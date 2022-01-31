// Copyright (c) 2017-2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>
#include <index/base.h>
#include <interfaces/chain.h>
#include <interfaces/handler.h>
#include <node/blockstorage.h>
#include <node/chain.h>
#include <node/context.h>
#include <node/ui_interface.h>
#include <shutdown.h>
#include <tinyformat.h>
#include <undo.h>
#include <util/syscall_sandbox.h>
#include <util/thread.h>
#include <util/translation.h>
#include <validation.h> // For g_chainman
#include <warnings.h>

using interfaces::FoundBlock;
using node::ReadBlockFromDisk;

constexpr uint8_t DB_BEST_BLOCK{'B'};

constexpr auto SYNC_LOG_INTERVAL{30s};
constexpr auto SYNC_LOCATOR_WRITE_INTERVAL{30s};

template <typename... Args>
static void FatalError(const char* fmt, const Args&... args)
{
    std::string strMessage = tfm::format(fmt, args...);
    SetMiscWarning(Untranslated(strMessage));
    LogPrintf("*** %s\n", strMessage);
    AbortError(_("A fatal internal error occurred, see debug.log for details"));
    StartShutdown();
}

CBlockLocator GetLocator(interfaces::Chain& chain, const uint256& block_hash)
{
    CBlockLocator locator;
    bool found = chain.findBlock(block_hash, interfaces::FoundBlock().locator(locator));
    assert(found);
    assert(!locator.IsNull());
    return locator;
}

class BaseIndexNotifications : public interfaces::Chain::Notifications
{
public:
    BaseIndexNotifications(BaseIndex& index) : m_index(index) {}
    void blockConnected(const interfaces::BlockInfo& block) override;
    void blockDisconnected(const interfaces::BlockInfo& block) override;
    void chainStateFlushed(const CBlockLocator& locator) override;

    BaseIndex& m_index;
    interfaces::Chain::NotifyOptions m_options = m_index.CustomOptions();
    std::optional<bool> m_init_result;
    int64_t m_last_log_time = 0;
    int64_t m_last_locator_write_time = 0;
    //! As blocks are disconnected, index is updated but not committed to until
    //! the next flush or block connection. start/end variables record the first
    //! and last disconnected blocks as an inclusive range. If start is set and
    //! end is null it means an error has occurred, and the index will stop
    //! trying to rewind and handle the error on the next new block or flush.
    const CBlockIndex* m_rewind_start = nullptr;
    const CBlockIndex* m_rewind_end = nullptr;
};

void BaseIndexNotifications::blockConnected(const interfaces::BlockInfo& block)
{
    if (!block.error.empty()) {
        FatalError("%s", block.error);
        return m_index.Interrupt();
    }
    const CBlockIndex* pindex = WITH_LOCK(cs_main, return m_index.m_chainstate->m_blockman.LookupBlockIndex(block.hash));
    if (!m_index.m_synced && !block.data) {
        // Before sync, attachChain will send an initial blockConnected event
        // without any block data, indicating the starting block (based on the
        // index locator) where the index was last synced. If the index is
        // already synced at this point, block.chain_tip will be true, and
        // m_synced can latch to true. Otherwise, more blockConnected events
        // will be sent with block data, followed by a final blockConnected
        // event without block data, to finish the sync and update
        // m_best_block_index and m_synced.
        m_index.m_best_block_index = pindex;
        if (block.chain_tip) {
            m_index.m_synced = true;
            if (pindex) {
                LogPrintf("%s is enabled at height %d\n", m_index.GetName(), pindex->nHeight);
            } else {
                LogPrintf("%s is enabled\n", m_index.GetName());
            }
        }
        if (!m_init_result) {
            m_init_result = m_index.CustomInit(block.height >= 0 ? std::make_optional(interfaces::BlockKey{block.hash, block.height}) : std::nullopt);
        }
    }

    if (!block.data) return;

    bool success = !m_rewind_start || m_rewind_end;
    if (m_rewind_start && success) {
        const CBlockIndex* best_block_index = m_index.m_best_block_index.load();
        assert(!best_block_index || best_block_index->GetAncestor(pindex->nHeight - 1) == pindex->pprev);
        m_index.m_best_block_index = pindex->pprev;
        chainStateFlushed(GetLocator(*m_index.m_chain, pindex->pprev->GetBlockHash()));
        success = m_index.m_best_block_index == pindex->pprev;
    }
    if (!success) {
        FatalError("%s: Failed to rewind index %s to a previous chain tip",
                   __func__, m_index.GetName());
        return m_index.Interrupt();
    }

    interfaces::BlockInfo block_info = node::MakeBlockInfo(pindex, block.data);
    CBlockUndo block_undo;
    if (m_options.connect_undo_data && pindex->nHeight > 0) {
        if (!node::UndoReadFromDisk(block_undo, pindex)) {
            FatalError("%s: Failed to read block %s undo data from disk",
                       __func__, pindex->GetBlockHash().ToString());
            return m_index.Interrupt();
        }
        block_info.undo_data = &block_undo;
    }
    int64_t current_time = 0;
    bool synced = m_index.m_synced;
    if (!synced) {
        current_time = GetTime();
        if (m_last_log_time + SYNC_LOG_INTERVAL < current_time) {
            LogPrintf("Syncing %s with block chain from height %d\n",
                      m_index.GetName(), pindex->nHeight);
            m_last_log_time = current_time;
        }
    }
    if (!m_index.CustomAppend(block_info)) {
        FatalError("%s: Failed to write block %s to index",
                   __func__, pindex->GetBlockHash().ToString());
        return m_index.Interrupt();
    }
    // Only update m_best_block_index between flushes if synced. Unclear why
    // best block is not updated here before sync, but this has been
    // longstanding behavior since syncing was introduced in #13033 so care
    // should be taken if changing m_best_block_index semantics.
    assert(synced == m_index.m_synced);
    if (synced) {
        m_index.m_best_block_index = pindex;
    } else if (m_last_locator_write_time + SYNC_LOCATOR_WRITE_INTERVAL < current_time) {
        m_index.m_best_block_index = pindex;
        m_last_locator_write_time = current_time;
        // No need to handle errors in Commit. If it fails, the error will be already be
        // logged. The best way to recover is to continue, as index cannot be corrupted by
        // a missed commit to disk for an advanced index state.
        m_index.Commit(GetLocator(*m_index.m_chain, pindex->GetBlockHash()));
    }
}

void BaseIndexNotifications::blockDisconnected(const interfaces::BlockInfo& block)
{
    if (!block.error.empty()) {
        FatalError("%s", block.error);
        return m_index.Interrupt();
    }

    const CBlockIndex* pindex = WITH_LOCK(cs_main, return m_index.m_chainstate->m_blockman.LookupBlockIndex(block.hash));
    if (!m_rewind_start || m_rewind_end) m_rewind_end = pindex;
    if (!m_rewind_start) m_rewind_start = pindex;

    CBlockUndo block_undo;
    interfaces::BlockInfo block_info = node::MakeBlockInfo(pindex, block.data);
    if (m_options.disconnect_undo_data && block.height > 0) {
        if (!node::UndoReadFromDisk(block_undo, pindex)) {
            m_rewind_end = nullptr;
        }
        block_info.undo_data = &block_undo;
    }
    if (m_rewind_end && !m_index.CustomRemove(block_info)) {
        m_rewind_end = nullptr;
    }
}

void BaseIndexNotifications::chainStateFlushed(const CBlockLocator& locator)
{
    // No need to handle errors in Commit. If it fails, the error will be already be logged. The
    // best way to recover is to continue, as index cannot be corrupted by a missed commit to disk
    // for an advanced index state.
    // In the case of a reorg, ensure persisted block locator is not stale.
    // Pruning has a minimum of 288 blocks-to-keep and getting the index
    // out of sync may be possible but a users fault.
    // In case we reorg beyond the pruned depth, ReadBlockFromDisk would
    // throw and lead to a graceful shutdown
    if (!m_index.Commit(locator) && m_rewind_start) {
        // If commit fails, revert the best block index to avoid corruption.
        m_index.m_best_block_index = m_rewind_start;
    }
    m_rewind_start = nullptr;
}

BaseIndex::DB::DB(const fs::path& path, size_t n_cache_size, bool f_memory, bool f_wipe, bool f_obfuscate) :
    CDBWrapper(path, n_cache_size, f_memory, f_wipe, f_obfuscate)
{}

bool BaseIndex::DB::ReadBestBlock(CBlockLocator& locator) const
{
    bool success = Read(DB_BEST_BLOCK, locator);
    if (!success) {
        locator.SetNull();
    }
    return success;
}

void BaseIndex::DB::WriteBestBlock(CDBBatch& batch, const CBlockLocator& locator)
{
    batch.Write(DB_BEST_BLOCK, locator);
}

BaseIndex::BaseIndex(std::unique_ptr<interfaces::Chain> chain)
    : m_chain{std::move(chain)} {}

BaseIndex::~BaseIndex()
{
    //! Assert Stop() was called before this base destructor. Notification
    //! handlers call pure virtual methods like GetName(), so if they are still
    //! being called at this point, they would segfault.
    LOCK(m_mutex);
    assert(!m_notifications);
    assert(!m_handler);
}

<<<<<<< HEAD
<<<<<<< HEAD
bool BaseIndex::Init()
{
    CBlockLocator locator;
    if (!GetDB().ReadBestBlock(locator)) {
        locator.SetNull();
    }

    LOCK(cs_main);
    CChain& active_chain = m_chainstate->m_chain;
    if (locator.IsNull()) {
        SetBestBlockIndex(nullptr);
    } else {
        SetBestBlockIndex(m_chainstate->FindForkInGlobalIndex(locator));
    }
    m_synced = m_best_block_index.load() == active_chain.Tip();
    if (!m_synced) {
<<<<<<< HEAD
        bool prune_violation = false;
        if (!m_best_block_index) {
            // index is not built yet
            // make sure we have all block data back to the genesis
            prune_violation = m_chainstate->m_blockman.GetFirstStoredBlock(*active_chain.Tip()) != active_chain.Genesis();
        }
        // in case the index has a best block set and is not fully synced
        // check if we have the required blocks to continue building the index
        else {
            const CBlockIndex* block_to_test = m_best_block_index.load();
            if (!active_chain.Contains(block_to_test)) {
                // if the bestblock is not part of the mainchain, find the fork
                // and make sure we have all data down to the fork
                block_to_test = active_chain.FindFork(block_to_test);
            }
            const CBlockIndex* block = active_chain.Tip();
            prune_violation = true;
            // check backwards from the tip if we have all block data until we reach the indexes bestblock
            while (block_to_test && block && (block->nStatus & BLOCK_HAVE_DATA)) {
                if (block_to_test == block) {
                    prune_violation = false;
                    break;
                }
                // block->pprev must exist at this point, since block_to_test is part of the chain
                // and thus must be encountered when going backwards from the tip
                assert(block->pprev);
                block = block->pprev;
            }
        }
        if (prune_violation) {
||||||| parent of dbdc7ed5a75 (indexes, refactor: Remove index prune_violation code)
        bool prune_violation = false;
        if (!m_best_block_index) {
            // index is not built yet
            // make sure we have all block data back to the genesis
            const CBlockIndex* block = active_chain.Tip();
            while (block->pprev && (block->pprev->nStatus & BLOCK_HAVE_DATA)) {
                block = block->pprev;
            }
            prune_violation = block != active_chain.Genesis();
        }
        // in case the index has a best block set and is not fully synced
        // check if we have the required blocks to continue building the index
        else {
            const CBlockIndex* block_to_test = m_best_block_index.load();
            if (!active_chain.Contains(block_to_test)) {
                // if the bestblock is not part of the mainchain, find the fork
                // and make sure we have all data down to the fork
                block_to_test = active_chain.FindFork(block_to_test);
            }
            const CBlockIndex* block = active_chain.Tip();
            prune_violation = true;
            // check backwards from the tip if we have all block data until we reach the indexes bestblock
            while (block_to_test && block && (block->nStatus & BLOCK_HAVE_DATA)) {
                if (block_to_test == block) {
                    prune_violation = false;
                    break;
                }
                // block->pprev must exist at this point, since block_to_test is part of the chain
                // and thus must be encountered when going backwards from the tip
                assert(block->pprev);
                block = block->pprev;
            }
        }
        if (prune_violation) {
=======
        if (!m_chain->checkBlocks(m_best_block_index.load())) {
>>>>>>> dbdc7ed5a75 (indexes, refactor: Remove index prune_violation code)
            return InitError(strprintf(Untranslated("%s best block of the index goes beyond pruned data. Please disable the index or reindex (which will download the whole blockchain again)"), GetName()));
        }
    }
    return true;
}

||||||| parent of 8cb6c143805 (indexes, refactor: Remove index Init method)
bool BaseIndex::Init()
{
    CBlockLocator locator;
    if (!GetDB().ReadBestBlock(locator)) {
        locator.SetNull();
    }

    LOCK(cs_main);
    CChain& active_chain = m_chainstate->m_chain;
    if (locator.IsNull()) {
        m_best_block_index = nullptr;
    } else {
        m_best_block_index = m_chainstate->FindForkInGlobalIndex(locator);
    }
    m_synced = m_best_block_index.load() == active_chain.Tip();
    if (!m_synced) {
        if (!m_chain->checkBlocks(m_best_block_index.load())) {
            return InitError(strprintf(Untranslated("%s best block of the index goes beyond pruned data. Please disable the index or reindex (which will download the whole blockchain again)"), GetName()));
        }
    }
    return true;
}

=======
>>>>>>> 8cb6c143805 (indexes, refactor: Remove index Init method)
static const CBlockIndex* NextSyncBlock(const CBlockIndex* pindex_prev, CChain& chain) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    AssertLockHeld(cs_main);

    if (!pindex_prev) {
        return chain.Genesis();
    }

    const CBlockIndex* pindex = chain.Next(pindex_prev);
    if (pindex) {
        return pindex;
    }

    return chain.Next(chain.FindFork(pindex_prev));
}

void BaseIndex::ThreadSync()
{
    SetSyscallSandboxPolicy(SyscallSandboxPolicy::TX_INDEX);
    const CBlockIndex* pindex = m_best_block_index.load();
    if (!m_synced) {
        auto& consensus_params = Params().GetConsensus();
        auto notifications = WITH_LOCK(m_mutex, return m_notifications);

<<<<<<< HEAD
        std::chrono::steady_clock::time_point last_log_time{0s};
        std::chrono::steady_clock::time_point last_locator_write_time{0s};
||||||| parent of 1819bdb7cb0 (indexes, refactor: Move more new block logic out of ThreadSync to blockConnected)
        int64_t last_log_time = 0;
        int64_t last_locator_write_time = 0;
=======
>>>>>>> 1819bdb7cb0 (indexes, refactor: Move more new block logic out of ThreadSync to blockConnected)
        while (true) {
            if (m_interrupt) {
<<<<<<< HEAD
                SetBestBlockIndex(pindex);
                // No need to handle errors in Commit. If it fails, the error will be already be
                // logged. The best way to recover is to continue, as index cannot be corrupted by
                // a missed commit to disk for an advanced index state.
                Commit(GetLocator(*m_chain, pindex->GetBlockHash()));
||||||| parent of 1819bdb7cb0 (indexes, refactor: Move more new block logic out of ThreadSync to blockConnected)
                m_best_block_index = pindex;
                // No need to handle errors in Commit. If it fails, the error will be already be
                // logged. The best way to recover is to continue, as index cannot be corrupted by
                // a missed commit to disk for an advanced index state.
                Commit(GetLocator(*m_chain, pindex->GetBlockHash()));
=======
>>>>>>> 1819bdb7cb0 (indexes, refactor: Move more new block logic out of ThreadSync to blockConnected)
                return;
            }

            {
                LOCK(cs_main);
                const CBlockIndex* pindex_next = NextSyncBlock(pindex, m_chainstate->m_chain);
                if (!pindex_next) {
<<<<<<< HEAD
                    SetBestBlockIndex(pindex);
                    m_synced = true;
                    // No need to handle errors in Commit. See rationale above.
                    Commit(GetLocator(*m_chain, pindex->GetBlockHash()));
||||||| parent of 1871ff04dcb (indexes, refactor: Move Commit logic out of ThreadSync to notification handlers)
                    m_best_block_index = pindex;
                    m_synced = true;
                    // No need to handle errors in Commit. See rationale above.
                    Commit(GetLocator(*m_chain, pindex->GetBlockHash()));
=======
                    assert(pindex);
                    notifications->blockConnected(node::MakeBlockInfo(pindex));
                    notifications->chainStateFlushed(GetLocator(*m_chain, pindex->GetBlockHash()));
>>>>>>> 1871ff04dcb (indexes, refactor: Move Commit logic out of ThreadSync to notification handlers)
                    break;
                }
                if (pindex_next->pprev != pindex) {
                    const CBlockIndex* current_tip = pindex;
                    const CBlockIndex* new_tip = pindex_next->pprev;
                    for (const CBlockIndex* iter_tip = current_tip; iter_tip != new_tip; iter_tip = iter_tip->pprev) {
                        CBlock block;
                        interfaces::BlockInfo block_info = node::MakeBlockInfo(iter_tip);
                        block_info.chain_tip = false;
                        if (!ReadBlockFromDisk(block, iter_tip, consensus_params)) {
                            block_info.error = strprintf("%s: Failed to read block %s from disk",
                                       __func__, iter_tip->GetBlockHash().ToString());
                        } else {
                            block_info.data = &block;
                        }
                        notifications->blockDisconnected(block_info);
                        if (m_interrupt) break;
                    }
                }
                pindex = pindex_next;
            }

<<<<<<< HEAD
            auto current_time{std::chrono::steady_clock::now()};
            if (last_log_time + SYNC_LOG_INTERVAL < current_time) {
                LogPrintf("Syncing %s with block chain from height %d\n",
                          GetName(), pindex->nHeight);
                last_log_time = current_time;
            }

            if (last_locator_write_time + SYNC_LOCATOR_WRITE_INTERVAL < current_time) {
                SetBestBlockIndex(pindex);
                last_locator_write_time = current_time;
                // No need to handle errors in Commit. See rationale above.
                Commit(GetLocator(*m_chain, pindex->GetBlockHash()));
            }

||||||| parent of 1819bdb7cb0 (indexes, refactor: Move more new block logic out of ThreadSync to blockConnected)
            int64_t current_time = GetTime();
            if (last_log_time + SYNC_LOG_INTERVAL < current_time) {
                LogPrintf("Syncing %s with block chain from height %d\n",
                          GetName(), pindex->nHeight);
                last_log_time = current_time;
            }

            if (last_locator_write_time + SYNC_LOCATOR_WRITE_INTERVAL < current_time) {
                m_best_block_index = pindex;
                last_locator_write_time = current_time;
                // No need to handle errors in Commit. See rationale above.
                Commit(GetLocator(*m_chain, pindex->GetBlockHash()));
            }

=======
>>>>>>> 1819bdb7cb0 (indexes, refactor: Move more new block logic out of ThreadSync to blockConnected)
            CBlock block;
            interfaces::BlockInfo block_info = node::MakeBlockInfo(pindex);
            block_info.chain_tip = false;
            if (!ReadBlockFromDisk(block, pindex, consensus_params)) {
                block_info.error = strprintf("%s: Failed to read block %s from disk",
                           __func__, pindex->GetBlockHash().ToString());
            } else {
                block_info.data = &block;
            }
            notifications->blockConnected(block_info);
        }
    }

    if (pindex) {
        LogPrintf("%s is enabled at height %d\n", GetName(), pindex->nHeight);
    } else {
        LogPrintf("%s is enabled\n", GetName());
    }
}

||||||| parent of c6375a96f88 (indexes, refactor: Move sync thread from index to node)
static const CBlockIndex* NextSyncBlock(const CBlockIndex* pindex_prev, CChain& chain) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    AssertLockHeld(cs_main);

    if (!pindex_prev) {
        return chain.Genesis();
    }

    const CBlockIndex* pindex = chain.Next(pindex_prev);
    if (pindex) {
        return pindex;
    }

    return chain.Next(chain.FindFork(pindex_prev));
}

void BaseIndex::ThreadSync()
{
    SetSyscallSandboxPolicy(SyscallSandboxPolicy::TX_INDEX);
    const CBlockIndex* pindex = m_best_block_index.load();
    if (!m_synced) {
        auto& consensus_params = Params().GetConsensus();
        auto notifications = WITH_LOCK(m_mutex, return m_notifications);

        while (true) {
            if (m_interrupt) {
                return;
            }

            {
                LOCK(cs_main);
                const CBlockIndex* pindex_next = NextSyncBlock(pindex, m_chainstate->m_chain);
                if (!pindex_next) {
                    assert(pindex);
                    notifications->blockConnected(node::MakeBlockInfo(pindex));
                    notifications->chainStateFlushed(GetLocator(*m_chain, pindex->GetBlockHash()));
                    break;
                }
                if (pindex_next->pprev != pindex) {
                    const CBlockIndex* current_tip = pindex;
                    const CBlockIndex* new_tip = pindex_next->pprev;
                    for (const CBlockIndex* iter_tip = current_tip; iter_tip != new_tip; iter_tip = iter_tip->pprev) {
                        CBlock block;
                        interfaces::BlockInfo block_info = node::MakeBlockInfo(iter_tip);
                        block_info.chain_tip = false;
                        if (!ReadBlockFromDisk(block, iter_tip, consensus_params)) {
                            block_info.error = strprintf("%s: Failed to read block %s from disk",
                                       __func__, iter_tip->GetBlockHash().ToString());
                        } else {
                            block_info.data = &block;
                        }
                        notifications->blockDisconnected(block_info);
                        if (m_interrupt) break;
                    }
                }
                pindex = pindex_next;
            }

            CBlock block;
            interfaces::BlockInfo block_info = node::MakeBlockInfo(pindex);
            block_info.chain_tip = false;
            if (!ReadBlockFromDisk(block, pindex, consensus_params)) {
                block_info.error = strprintf("%s: Failed to read block %s from disk",
                           __func__, pindex->GetBlockHash().ToString());
            } else {
                block_info.data = &block;
            }
            notifications->blockConnected(block_info);
        }
    }

    if (pindex) {
        LogPrintf("%s is enabled at height %d\n", GetName(), pindex->nHeight);
    } else {
        LogPrintf("%s is enabled\n", GetName());
    }
}

=======
>>>>>>> c6375a96f88 (indexes, refactor: Move sync thread from index to node)
bool BaseIndex::Commit(const CBlockLocator& locator)
{
    // Don't commit anything if we haven't indexed any block yet
    // (this could happen if init is interrupted).
    if (locator.IsNull()) {
        return false;
    }
    CDBBatch batch(GetDB());
    bool success = CustomCommit(batch);
    if (success) {
        GetDB().WriteBestBlock(batch, locator);
    }
    if (!success || !GetDB().WriteBatch(batch)) {
        return error("%s: Failed to commit latest %s state", __func__, GetName());
    }
    return true;
}

<<<<<<< HEAD
<<<<<<< HEAD
bool BaseIndex::Rewind(const CBlockIndex* current_tip, const CBlockIndex* new_tip)
{
    assert(current_tip == m_best_block_index);
    assert(current_tip->GetAncestor(new_tip->nHeight) == new_tip);

    const Consensus::Params& consensus_params = Params().GetConsensus();
    CBlock block;
    CBlockUndo block_undo;

    for (const CBlockIndex* iter_tip = current_tip; iter_tip != new_tip; iter_tip = iter_tip->pprev) {
        interfaces::BlockInfo block_info = node::MakeBlockInfo(iter_tip);
        if (CustomOptions().disconnect_data) {
            if (!ReadBlockFromDisk(block, iter_tip, consensus_params)) {
                return error("%s: Failed to read block %s from disk",
                             __func__, iter_tip->GetBlockHash().ToString());
            }
            block_info.data = &block;
	}
        if (CustomOptions().disconnect_undo_data && iter_tip->nHeight > 0) {
            if (!node::UndoReadFromDisk(block_undo, iter_tip)) {
                return false;
            }
            block_info.undo_data = &block_undo;
        }
        if (!CustomRemove(block_info)) {
            return false;
        }
    }

    // In the case of a reorg, ensure persisted block locator is not stale.
    // Pruning has a minimum of 288 blocks-to-keep and getting the index
    // out of sync may be possible but a users fault.
    // In case we reorg beyond the pruned depth, ReadBlockFromDisk would
    // throw and lead to a graceful shutdown
<<<<<<< HEAD
    SetBestBlockIndex(new_tip);
    if (!Commit()) {
||||||| parent of 4389f0cdc8a (indexes, refactor: Add Commit CBlockLocator& argument)
    m_best_block_index = new_tip;
    if (!Commit()) {
=======
    m_best_block_index = new_tip;
    if (!Commit(GetLocator(*m_chain, new_tip->GetBlockHash()))) {
>>>>>>> 4389f0cdc8a (indexes, refactor: Add Commit CBlockLocator& argument)
        // If commit fails, revert the best block index to avoid corruption.
        SetBestBlockIndex(current_tip);
        return false;
    }

    return true;
}

||||||| parent of 068e10fde56 (indexes, refactor: Move Rewind logic out of Rewind to blockDisconnected and ThreadSync)
bool BaseIndex::Rewind(const CBlockIndex* current_tip, const CBlockIndex* new_tip)
{
    assert(current_tip == m_best_block_index);
    assert(current_tip->GetAncestor(new_tip->nHeight) == new_tip);

    const Consensus::Params& consensus_params = Params().GetConsensus();
    CBlock block;
    CBlockUndo block_undo;

    for (const CBlockIndex* iter_tip = current_tip; iter_tip != new_tip; iter_tip = iter_tip->pprev) {
        interfaces::BlockInfo block_info = node::MakeBlockInfo(iter_tip);
        if (CustomOptions().disconnect_data) {
            if (!ReadBlockFromDisk(block, iter_tip, consensus_params)) {
                return error("%s: Failed to read block %s from disk",
                             __func__, iter_tip->GetBlockHash().ToString());
            }
            block_info.data = &block;
	}
        if (CustomOptions().disconnect_undo_data && iter_tip->nHeight > 0) {
            if (!node::UndoReadFromDisk(block_undo, iter_tip)) {
                return false;
            }
            block_info.undo_data = &block_undo;
        }
        if (!CustomRemove(block_info)) {
            return false;
        }
    }

    // In the case of a reorg, ensure persisted block locator is not stale.
    // Pruning has a minimum of 288 blocks-to-keep and getting the index
    // out of sync may be possible but a users fault.
    // In case we reorg beyond the pruned depth, ReadBlockFromDisk would
    // throw and lead to a graceful shutdown
    m_best_block_index = new_tip;
    if (!Commit(GetLocator(*m_chain, new_tip->GetBlockHash()))) {
        // If commit fails, revert the best block index to avoid corruption.
        m_best_block_index = current_tip;
        return false;
    }

    return true;
}

=======
>>>>>>> 068e10fde56 (indexes, refactor: Move Rewind logic out of Rewind to blockDisconnected and ThreadSync)
bool BaseIndex::IgnoreBlockConnected(const interfaces::BlockInfo& block)
{
    // During initial sync, ignore validation interface notifications, only
    // process notifications from sync thread.
    if (!m_synced) return block.chain_tip;

    const CBlockIndex* pindex = WITH_LOCK(cs_main, return m_chainstate->m_blockman.LookupBlockIndex(block.hash));
    const CBlockIndex* best_block_index = m_best_block_index.load();
    if (!best_block_index) {
        if (pindex->nHeight != 0) {
            FatalError("%s: First block connected is not the genesis block (height=%d)",
                       __func__, pindex->nHeight);
            return true;
        }
    } else {
        // Ensure block connects to an ancestor of the current best block. This should be the case
        // most of the time, but may not be immediately after the sync thread catches up and sets
        // m_synced. Consider the case where there is a reorg and the blocks on the stale branch are
        // in the ValidationInterface queue backlog even after the sync thread has caught up to the
        // new chain tip. In this unlikely event, log a warning and let the queue clear.
        if (best_block_index->GetAncestor(pindex->nHeight - 1) != pindex->pprev) {
            LogPrintf("%s: WARNING: Block %s does not connect to an ancestor of " /* Continued */
                      "known best chain (tip=%s); not updating index\n",
                      __func__, pindex->GetBlockHash().ToString(),
                      best_block_index->GetBlockHash().ToString());
            return true;
        }
    }
<<<<<<< HEAD
<<<<<<< HEAD

    if (WriteBlock(*block, pindex)) {
        SetBestBlockIndex(pindex);
||||||| parent of 5d80dc9a356 (indexes, refactor: Remove CBlockIndex* uses in index WriteBlock methods)

    if (WriteBlock(*block, pindex)) {
        m_best_block_index = pindex;
=======
    interfaces::BlockInfo block_info = node::MakeBlockInfo(pindex, block.get());
    if (CustomAppend(block_info)) {
        m_best_block_index = pindex;
>>>>>>> 5d80dc9a356 (indexes, refactor: Remove CBlockIndex* uses in index WriteBlock methods)
    } else {
        FatalError("%s: Failed to write block %s to index",
                   __func__, pindex->GetBlockHash().ToString());
        return;
    }
||||||| parent of dd832e81e91 (indexes, refactor: Remove index validation interface and block locator code)
    interfaces::BlockInfo block_info = node::MakeBlockInfo(pindex, block.get());
    if (CustomAppend(block_info)) {
        m_best_block_index = pindex;
    } else {
        FatalError("%s: Failed to write block %s to index",
                   __func__, pindex->GetBlockHash().ToString());
        return;
    }
=======
    return false;
>>>>>>> dd832e81e91 (indexes, refactor: Remove index validation interface and block locator code)
}

bool BaseIndex::IgnoreChainStateFlushed(const CBlockLocator& locator)
{
    assert(!locator.IsNull());
    const uint256& locator_tip_hash = locator.vHave.front();
    const CBlockIndex* locator_tip_index;
    {
        LOCK(cs_main);
        locator_tip_index = m_chainstate->m_blockman.LookupBlockIndex(locator_tip_hash);
    }

    if (!locator_tip_index) {
        FatalError("%s: First block (hash=%s) in locator was not found",
                   __func__, locator_tip_hash.ToString());
        return true;
    }

    // This checks that ChainStateFlushed callbacks are received after BlockConnected. The check may fail
    // immediately after the sync thread catches up and sets m_synced. Consider the case where
    // there is a reorg and the blocks on the stale branch are in the ValidationInterface queue
    // backlog even after the sync thread has caught up to the new chain tip. In this unlikely
    // event, log a warning and let the queue clear.
    const CBlockIndex* best_block_index = m_best_block_index.load();
    if (best_block_index->GetAncestor(locator_tip_index->nHeight) != locator_tip_index) {
        if (m_synced) {
            LogPrintf("%s: WARNING: Locator contains block (hash=%s) not on known best " /* Continued */
                      "chain (tip=%s); not writing index locator\n",
                      __func__, locator_tip_hash.ToString(),
                      best_block_index->GetBlockHash().ToString());
        }
        return true;
    }
    return false;
}

||||||| parent of 9bf661cc127 (indexes: Rewrite chain sync logic, remove racy init)
bool BaseIndex::IgnoreBlockConnected(const interfaces::BlockInfo& block)
{
    // During initial sync, ignore validation interface notifications, only
    // process notifications from sync thread.
    if (!m_synced) return block.chain_tip;

    const CBlockIndex* pindex = WITH_LOCK(cs_main, return m_chainstate->m_blockman.LookupBlockIndex(block.hash));
    const CBlockIndex* best_block_index = m_best_block_index.load();
    if (!best_block_index) {
        if (pindex->nHeight != 0) {
            FatalError("%s: First block connected is not the genesis block (height=%d)",
                       __func__, pindex->nHeight);
            return true;
        }
    } else {
        // Ensure block connects to an ancestor of the current best block. This should be the case
        // most of the time, but may not be immediately after the sync thread catches up and sets
        // m_synced. Consider the case where there is a reorg and the blocks on the stale branch are
        // in the ValidationInterface queue backlog even after the sync thread has caught up to the
        // new chain tip. In this unlikely event, log a warning and let the queue clear.
        if (best_block_index->GetAncestor(pindex->nHeight - 1) != pindex->pprev) {
            LogPrintf("%s: WARNING: Block %s does not connect to an ancestor of " /* Continued */
                      "known best chain (tip=%s); not updating index\n",
                      __func__, pindex->GetBlockHash().ToString(),
                      best_block_index->GetBlockHash().ToString());
            return true;
        }
    }
    return false;
}

bool BaseIndex::IgnoreChainStateFlushed(const CBlockLocator& locator)
{
    assert(!locator.IsNull());
    const uint256& locator_tip_hash = locator.vHave.front();
    const CBlockIndex* locator_tip_index;
    {
        LOCK(cs_main);
        locator_tip_index = m_chainstate->m_blockman.LookupBlockIndex(locator_tip_hash);
    }

    if (!locator_tip_index) {
        FatalError("%s: First block (hash=%s) in locator was not found",
                   __func__, locator_tip_hash.ToString());
        return true;
    }

    // This checks that ChainStateFlushed callbacks are received after BlockConnected. The check may fail
    // immediately after the sync thread catches up and sets m_synced. Consider the case where
    // there is a reorg and the blocks on the stale branch are in the ValidationInterface queue
    // backlog even after the sync thread has caught up to the new chain tip. In this unlikely
    // event, log a warning and let the queue clear.
    const CBlockIndex* best_block_index = m_best_block_index.load();
    if (best_block_index->GetAncestor(locator_tip_index->nHeight) != locator_tip_index) {
        if (m_synced) {
            LogPrintf("%s: WARNING: Locator contains block (hash=%s) not on known best " /* Continued */
                      "chain (tip=%s); not writing index locator\n",
                      __func__, locator_tip_hash.ToString(),
                      best_block_index->GetBlockHash().ToString());
        }
        return true;
    }
    return false;
}

=======
>>>>>>> 9bf661cc127 (indexes: Rewrite chain sync logic, remove racy init)
bool BaseIndex::BlockUntilSyncedToCurrentChain() const
{
    AssertLockNotHeld(cs_main);

    if (!m_synced) {
        return false;
    }

    if (const CBlockIndex* index = m_best_block_index.load()) {
        interfaces::BlockKey best_block{index->GetBlockHash(), index->nHeight};
        // Skip the queue-draining stuff if we know we're caught up with
        // m_chain.Tip().
        interfaces::BlockKey tip;
        uint256 ancestor;
        if (m_chain->getTip(FoundBlock().hash(tip.hash).height(tip.height)) &&
            m_chain->findAncestorByHeight(best_block.hash, tip.height, FoundBlock().hash(ancestor)) &&
            ancestor == tip.hash) {
            return true;
        }
    }

    LogPrintf("%s: %s is catching up on block notifications\n", __func__, GetName());
    m_chain->waitForPendingNotifications();
    return true;
}

void BaseIndex::Interrupt()
{
    LOCK(m_mutex);
    if (m_handler) m_handler->interrupt();
    m_notifications.reset();
}

bool BaseIndex::Start()
{
    // m_chainstate member gives indexing code access to node internals. It
    // will be removed in upcoming commit
    m_chainstate = &m_chain->context()->chainman->ActiveChainstate();

    CBlockLocator locator;
    if (!GetDB().ReadBestBlock(locator)) {
        locator.SetNull();
    }

    auto options = CustomOptions();
    options.thread_name = GetName();
    auto notifications = std::make_shared<BaseIndexNotifications>(*this);
    auto handler = m_chain->attachChain(notifications, locator, options);
    if (!handler) {
        return InitError(strprintf(Untranslated("%s best block of the index goes beyond pruned data. Please disable the index or reindex (which will download the whole blockchain again)"), GetName()));
    } else {
        LOCK(m_mutex);
        m_notifications = std::move(notifications);
        m_handler = std::move(handler);
        assert(m_notifications->m_init_result.has_value());
        return m_notifications->m_init_result.value();
    }
}

void BaseIndex::Stop()
{
    Interrupt();
    // Call handler destructor after releasing m_mutex. Locking the mutex is
    // required to access m_handler, but the lock should not be held while
    // destroying the handler, because the handler destructor waits for the last
    // notification to be processed, so holding the lock would deadlock if that
    // last notification also needs the lock.
    auto handler = WITH_LOCK(m_mutex, return std::move(m_handler));
}

IndexSummary BaseIndex::GetSummary() const
{
    IndexSummary summary{};
    summary.name = GetName();
    summary.synced = m_synced;
    summary.best_block_height = m_best_block_index ? m_best_block_index.load()->nHeight : 0;
    return summary;
}

void BaseIndex::SetBestBlockIndex(const CBlockIndex* block) {
    assert(!node::fPruneMode || AllowPrune());

    m_best_block_index = block;
    if (AllowPrune() && block) {
        node::PruneLockInfo prune_lock;
        prune_lock.height_first = block->nHeight;
        WITH_LOCK(::cs_main, m_chainstate->m_blockman.UpdatePruneLock(GetName(), prune_lock));
    }
}
