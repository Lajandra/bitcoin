// Copyright (c) 2017-2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>
#include <index/base.h>
<<<<<<< HEAD
#include <interfaces/chain.h>
<<<<<<< HEAD
#include <kernel/chain.h>
||||||| parent of 05af3975a30 (indexes, refactor: Pass Chain interface instead of CChainState class to indexes)
=======
#include <interfaces/chain.h>
>>>>>>> 05af3975a30 (indexes, refactor: Pass Chain interface instead of CChainState class to indexes)
||||||| parent of 78e624078b3 (indexes, refactor: Remove index Init method)
=======
#include <interfaces/handler.h>
>>>>>>> 78e624078b3 (indexes, refactor: Remove index Init method)
#include <node/blockstorage.h>
#include <node/chain.h>
#include <node/interface_ui.h>
#include <shutdown.h>
#include <tinyformat.h>
#include <undo.h>
#include <util/syscall_sandbox.h>
#include <util/system.h>
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
    std::optional<interfaces::BlockKey> getBest()
    {
        LOCK(m_index.m_mutex);
        return m_index.m_best_block;
    }
    void setBest(const interfaces::BlockKey& block)
    {
        assert(!block.hash.IsNull());
        assert(block.height >= 0);
        m_index.SetBestBlock(block);
    }

    BaseIndex& m_index;
    std::optional<bool> m_init_result;
    std::chrono::steady_clock::time_point m_last_log_time{0s};
    std::chrono::steady_clock::time_point m_last_locator_write_time{0s};
    //! As blocks are disconnected, index is updated but not committed to until
    //! the next flush or block connection. start/end variables record the first
    //! and last disconnected blocks as an inclusive range. If start is set and
    //! end is null it means an error has occurred, and the index will stop
    //! trying to rewind and handle the error on the next new block or flush.
    std::optional<interfaces::BlockKey> m_rewind_start;
    std::optional<interfaces::BlockKey> m_rewind_end;
};

void BaseIndexNotifications::blockConnected(const interfaces::BlockInfo& block)
{
    if (!block.error.empty()) {
        FatalError("%s", block.error);
        return m_index.Interrupt();
    }
    if (!m_index.m_synced && !block.data) {
        // Before sync, attachChain will send an initial blockConnected event
        // without any block data, indicating the starting block (based on the
        // index locator) where the index was last synced. If the index is
        // already synced at this point, block.chain_tip will be true, and
        // m_synced can latch to true. Otherwise, more blockConnected events
        // will be sent with block data, followed by a final blockConnected
        // event without block data, to finish the sync and update
        // m_best_block and m_synced.
        if (block.height >= 0) {
            setBest({block.hash, block.height});
        } else {
            assert(!getBest());
        }
        if (block.chain_tip) {
            m_index.m_synced = true;
            if (block.height >= 0) {
                LogPrintf("%s is enabled at height %d\n", m_index.GetName(), block.height);
            } else {
                LogPrintf("%s is enabled\n", m_index.GetName());
            }
        }
        if (!m_init_result) {
            m_init_result = m_index.CustomInit(block.height >= 0 ? std::make_optional(interfaces::BlockKey{block.hash, block.height}) : std::nullopt);
            if (!*m_init_result) return m_index.Interrupt();
        }
    }

    if (!block.data) return;

    bool success = !m_rewind_start || m_rewind_end;
    if (m_rewind_start && success) {
        auto best_block = getBest();
        // Assert m_best_block is null or is parent of new connected block, or is
        // descendant of parent of new connected block.
        if (best_block && best_block->hash != *block.prev_hash) {
            uint256 best_ancestor_hash;
            assert(m_index.m_chain->findAncestorByHeight(best_block->hash, block.height - 1, FoundBlock().hash(best_ancestor_hash)));
            assert(best_ancestor_hash == *block.prev_hash);
        }
        setBest({*block.prev_hash, block.height-1});
        chainStateFlushed(GetLocator(*m_index.m_chain, *block.prev_hash));
        success = getBest()->hash == *block.prev_hash;
    }
    if (!success) {
        FatalError("%s: Failed to rewind index %s to a previous chain tip",
                   __func__, m_index.GetName());
        return m_index.Interrupt();
    }

    std::chrono::steady_clock::time_point current_time{0s};
    bool synced = m_index.m_synced;
    if (!synced) {
        current_time = std::chrono::steady_clock::now();
        if (m_last_log_time + SYNC_LOG_INTERVAL < current_time) {
            LogPrintf("Syncing %s with block chain from height %d\n",
                      m_index.GetName(), block.height);
            m_last_log_time = current_time;
        }
    }
    if (!m_index.CustomAppend(block)) {
        FatalError("%s: Failed to write block %s to index",
                   __func__, block.hash.ToString());
        return m_index.Interrupt();
    }
    // Only update m_best_block between flushes if synced. Unclear why
    // best block is not updated here before sync, but this has been
    // longstanding behavior since syncing was introduced in #13033 so care
    // should be taken if changing m_best_block semantics.
    assert(synced == m_index.m_synced);
    if (synced) {
        setBest({block.hash, block.height});
    } else if (m_last_locator_write_time + SYNC_LOCATOR_WRITE_INTERVAL < current_time) {
        setBest({block.hash, block.height});
        m_last_locator_write_time = current_time;
        // No need to handle errors in Commit. If it fails, the error will be already be
        // logged. The best way to recover is to continue, as index cannot be corrupted by
        // a missed commit to disk for an advanced index state.
        m_index.Commit(GetLocator(*m_index.m_chain, block.hash));
    }
}

void BaseIndexNotifications::blockDisconnected(const interfaces::BlockInfo& block)
{
    if (!block.error.empty()) {
        FatalError("%s", block.error);
        return m_index.Interrupt();
    }

    auto best_block = getBest();
    if (!m_rewind_start || m_rewind_end) m_rewind_end = best_block;
    if (!m_rewind_start) m_rewind_start = best_block;

    if (m_rewind_end && !m_index.CustomRemove(block)) {
        m_rewind_end = std::nullopt;
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
        setBest(*m_rewind_start);
    }
    m_rewind_start = std::nullopt;
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
<<<<<<< HEAD
            interfaces::BlockInfo block_info = kernel::MakeBlockInfo(pindex);
||||||| parent of 848bdd1b330 (indexes, refactor: Remove CBlockIndex* uses in index WriteBlock methods)
=======
            interfaces::BlockInfo block_info = node::MakeBlockInfo(pindex);
<<<<<<< HEAD
>>>>>>> 848bdd1b330 (indexes, refactor: Remove CBlockIndex* uses in index WriteBlock methods)
||||||| parent of 78e624078b3 (indexes, refactor: Remove index Init method)
=======
            block_info.chain_tip = false;
>>>>>>> 78e624078b3 (indexes, refactor: Remove index Init method)
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

||||||| parent of 5eb6e1d9f9d (indexes, refactor: Move sync thread from index to node)
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
>>>>>>> 5eb6e1d9f9d (indexes, refactor: Move sync thread from index to node)
bool BaseIndex::Commit(const CBlockLocator& locator)
{
    // Don't commit anything if we haven't indexed any block yet
    // (this could happen if init is interrupted).
<<<<<<< HEAD
    bool ok = m_best_block_index != nullptr;
    if (ok) {
        CDBBatch batch(GetDB());
        ok = CustomCommit(batch);
        if (ok) {
            GetDB().WriteBestBlock(batch, GetLocator(*m_chain, m_best_block_index.load()->GetBlockHash()));
            ok = GetDB().WriteBatch(batch);
        }
||||||| parent of 77c1c13a5ca (indexes, refactor: Add Commit CBlockLocator& argument)
    if (m_best_block_index == nullptr) {
        return false;
=======
    if (locator.IsNull()) {
        return false;
>>>>>>> 77c1c13a5ca (indexes, refactor: Add Commit CBlockLocator& argument)
    }
    if (!ok) {
        return error("%s: Failed to commit latest %s state", __func__, GetName());
    }
<<<<<<< HEAD
||||||| parent of 3c82fa533a3 (indexes, refactor: Remove CChainState use in index CommitInternal method)
    GetDB().WriteBestBlock(batch, m_chainstate->m_chain.GetLocator(m_best_block_index));
=======
    CDBBatch batch(GetDB());
    bool success = CustomCommit(batch);
    if (success) {
        GetDB().WriteBestBlock(batch, locator);
    }
    if (!success || !GetDB().WriteBatch(batch)) {
        return error("%s: Failed to commit latest %s state", __func__, GetName());
    }
>>>>>>> 3c82fa533a3 (indexes, refactor: Remove CChainState use in index CommitInternal method)
    return true;
}

<<<<<<< HEAD
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
    interfaces::BlockInfo block_info = kernel::MakeBlockInfo(pindex, block.get());
    if (CustomAppend(block_info)) {
||||||| parent of 848bdd1b330 (indexes, refactor: Remove CBlockIndex* uses in index WriteBlock methods)

    if (WriteBlock(*block, pindex)) {
=======
    interfaces::BlockInfo block_info = node::MakeBlockInfo(pindex, block.get());
    if (CustomAppend(block_info)) {
>>>>>>> 848bdd1b330 (indexes, refactor: Remove CBlockIndex* uses in index WriteBlock methods)
        SetBestBlockIndex(pindex);
    } else {
        FatalError("%s: Failed to write block %s to index",
                   __func__, pindex->GetBlockHash().ToString());
        return;
    }
||||||| parent of f9fdde531f5 (indexes, refactor: Remove index validation interface and block locator code)
    interfaces::BlockInfo block_info = node::MakeBlockInfo(pindex, block.get());
    if (CustomAppend(block_info)) {
        SetBestBlockIndex(pindex);
    } else {
        FatalError("%s: Failed to write block %s to index",
                   __func__, pindex->GetBlockHash().ToString());
        return;
    }
=======
    return false;
>>>>>>> f9fdde531f5 (indexes, refactor: Remove index validation interface and block locator code)
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

||||||| parent of feb6e30a45e (indexes: Rewrite chain sync logic, remove racy init)
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
>>>>>>> feb6e30a45e (indexes: Rewrite chain sync logic, remove racy init)
bool BaseIndex::BlockUntilSyncedToCurrentChain() const
{
    AssertLockNotHeld(cs_main);

    if (!m_synced) {
        return false;
    }

    if (const auto best_block = WITH_LOCK(m_mutex, return m_best_block)) {
        // Skip the queue-draining stuff if we know we're caught up with
        // m_chain.Tip().
        interfaces::BlockKey tip;
        uint256 ancestor;
        if (m_chain->getTip(FoundBlock().hash(tip.hash).height(tip.height)) &&
            m_chain->findAncestorByHeight(best_block->hash, tip.height, FoundBlock().hash(ancestor)) &&
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
<<<<<<< HEAD
<<<<<<< HEAD
    // m_chainstate member gives indexing code access to node internals. It is
    // removed in followup https://github.com/bitcoin/bitcoin/pull/24230
    m_chainstate = &m_chain->context()->chainman->ActiveChainstate();
||||||| parent of 05af3975a30 (indexes, refactor: Pass Chain interface instead of CChainState class to indexes)
    m_chainstate = &active_chainstate;
=======
    // m_chainstate member gives indexing code access to node internals. It
    // will be removed in upcoming commit
    m_chainstate = &m_chain->context()->chainman->ActiveChainstate();
<<<<<<< HEAD
>>>>>>> 05af3975a30 (indexes, refactor: Pass Chain interface instead of CChainState class to indexes)
    // Need to register this ValidationInterface before running Init(), so that
    // callbacks are not missed if Init sets m_synced to true.
    RegisterValidationInterface(this);
||||||| parent of f9fdde531f5 (indexes, refactor: Remove index validation interface and block locator code)
    // Need to register this ValidationInterface before running Init(), so that
    // callbacks are not missed if Init sets m_synced to true.
    RegisterValidationInterface(this);
=======
>>>>>>> f9fdde531f5 (indexes, refactor: Remove index validation interface and block locator code)
||||||| parent of 8ca844e3ad9 (Remove direct index -> node dependency)
    // m_chainstate member gives indexing code access to node internals. It
    // will be removed in upcoming commit
    m_chainstate = &m_chain->context()->chainman->ActiveChainstate();
=======
>>>>>>> 8ca844e3ad9 (Remove direct index -> node dependency)
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
    const auto best_block = WITH_LOCK(m_mutex, return m_best_block);
    summary.best_block_height = best_block ? best_block->height : 0;
    return summary;
}

void BaseIndex::SetBestBlock(const interfaces::BlockKey& block) {
    assert(!node::fPruneMode || AllowPrune());

    WITH_LOCK(m_mutex, m_best_block = block);
    if (AllowPrune()) {
        node::PruneLockInfo prune_lock;
        prune_lock.height_first = block.height;
        m_chain->updatePruneLock(GetName(), prune_lock);
    }
}
