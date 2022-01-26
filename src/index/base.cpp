// Copyright (c) 2017-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>
#include <common/args.h>
#include <index/base.h>
#include <interfaces/chain.h>
#include <interfaces/handler.h>
#include <kernel/chain.h>
#include <logging.h>
#include <node/abort.h>
#include <node/blockstorage.h>
#include <node/context.h>
#include <node/database_args.h>
#include <node/interface_ui.h>
#include <shutdown.h>
#include <tinyformat.h>
<<<<<<< HEAD
||||||| parent of cd3aa7ea3eb8 (indexes, refactor: Remove remaining CBlockIndex* uses in index Rewind methods)
#include <util/syscall_sandbox.h>
=======
#include <undo.h>
#include <util/syscall_sandbox.h>
>>>>>>> cd3aa7ea3eb8 (indexes, refactor: Remove remaining CBlockIndex* uses in index Rewind methods)
#include <util/thread.h>
#include <util/translation.h>
#include <validation.h> // For g_chainman
#include <warnings.h>

#include <string>
#include <utility>

constexpr uint8_t DB_BEST_BLOCK{'B'};

constexpr auto SYNC_LOG_INTERVAL{30s};
constexpr auto SYNC_LOCATOR_WRITE_INTERVAL{30s};

template <typename... Args>
void BaseIndex::FatalErrorf(const char* fmt, const Args&... args)
{
    auto message = tfm::format(fmt, args...);
    node::AbortNode(m_chain->context()->exit_status, message);
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
    std::chrono::steady_clock::time_point m_last_log_time{0s};
    std::chrono::steady_clock::time_point m_last_locator_write_time{0s};
};

void BaseIndexNotifications::blockConnected(const interfaces::BlockInfo& block)
{
    const CBlockIndex* pindex = WITH_LOCK(cs_main, return m_index.m_chainstate->m_blockman.LookupBlockIndex(block.hash));
    if (!block.data) {
        // Null block.data means block is the starting block at the beginning of
        // the sync. Set the best block to this starting block, and latch
        // m_synced to true if there are no blocks following it.
        assert(!m_index.m_best_block_index && !m_index.m_synced);
        m_index.SetBestBlockIndex(pindex);
        if (block.chain_tip) {
            m_index.m_synced = true;
        }
        return;
    }

    if (m_index.IgnoreBlockConnected(block)) return;

    const CBlockIndex* best_block_index = m_index.m_best_block_index.load();
    if (block.chain_tip && best_block_index != pindex->pprev && !m_index.Rewind(best_block_index, pindex->pprev)) {
        FatalError("%s: Failed to rewind index %s to a previous chain tip",
                   __func__, m_index.GetName());
        return;
    }

    interfaces::BlockInfo block_info = kernel::MakeBlockInfo(pindex, block.data);
    CBlockUndo block_undo;
    if (m_options.connect_undo_data && pindex->nHeight > 0) {
        if (!node::UndoReadFromDisk(block_undo, pindex)) {
            FatalError("%s: Failed to read block %s undo data from disk",
                       __func__, pindex->GetBlockHash().ToString());
            return m_index.Interrupt();
        }
        block_info.undo_data = &block_undo;
    }
    std::chrono::steady_clock::time_point current_time{0s};
    bool synced = m_index.m_synced;
    if (!synced) {
        current_time = std::chrono::steady_clock::now();
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
        // Setting the best block index is intentionally the last step of this
        // function, so BlockUntilSyncedToCurrentChain callers waiting for the
        // best block index to be updated can rely on the block being fully
        // processed, and the index object being safe to delete.
        m_index.SetBestBlockIndex(pindex);
    } else if (m_last_locator_write_time + SYNC_LOCATOR_WRITE_INTERVAL < current_time || m_index.m_interrupt) {
        auto locator = GetLocator(*m_index.m_chain, pindex->GetBlockHash());
        m_index.SetBestBlockIndex(pindex);
        m_last_locator_write_time = current_time;
        // No need to handle errors in Commit. If it fails, the error will be already be
        // logged. The best way to recover is to continue, as index cannot be corrupted by
        // a missed commit to disk for an advanced index state.
        m_index.Commit(locator);
    }
}

void BaseIndexNotifications::blockDisconnected(const interfaces::BlockInfo& block)
{
}

void BaseIndexNotifications::chainStateFlushed(const CBlockLocator& locator)
{
    if (m_index.IgnoreChainStateFlushed(locator)) return;

    // No need to handle errors in Commit. If it fails, the error will be already be logged. The
    // best way to recover is to continue, as index cannot be corrupted by a missed commit to disk
    // for an advanced index state.
    m_index.Commit(locator);
}

BaseIndex::DB::DB(const fs::path& path, size_t n_cache_size, bool f_memory, bool f_wipe, bool f_obfuscate) :
    CDBWrapper{DBParams{
        .path = path,
        .cache_bytes = n_cache_size,
        .memory_only = f_memory,
        .wipe_data = f_wipe,
        .obfuscate = f_obfuscate,
        .options = [] { DBOptions options; node::ReadDatabaseArgs(gArgs, options); return options; }()}}
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

BaseIndex::BaseIndex(std::unique_ptr<interfaces::Chain> chain, std::string name)
    : m_chain{std::move(chain)}, m_name{std::move(name)} {}

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
bool BaseIndex::Init()
{
    // m_chainstate member gives indexing code access to node internals. It is
    // removed in followup https://github.com/bitcoin/bitcoin/pull/24230
    m_chainstate = &m_chain->context()->chainman->ActiveChainstate();
    // Register to validation interface before setting the 'm_synced' flag, so that
    // callbacks are not missed once m_synced is true.
    RegisterValidationInterface(this);

    CBlockLocator locator;
    if (!GetDB().ReadBestBlock(locator)) {
        locator.SetNull();
    }

    LOCK(cs_main);
    CChain& active_chain = m_chainstate->m_chain;
    if (locator.IsNull()) {
        SetBestBlockIndex(nullptr);
    } else {
        // Setting the best block to the locator's top block. If it is not part of the
        // best chain, we will rewind to the fork point during index sync
        const CBlockIndex* locator_index{m_chainstate->m_blockman.LookupBlockIndex(locator.vHave.at(0))};
        if (!locator_index) {
            return InitError(strprintf(Untranslated("%s: best block of the index not found. Please rebuild the index."), GetName()));
        }
        SetBestBlockIndex(locator_index);
    }

    // Child init
    const CBlockIndex* start_block = m_best_block_index.load();
    if (!CustomInit(start_block ? std::make_optional(interfaces::BlockKey{start_block->GetBlockHash(), start_block->nHeight}) : std::nullopt)) {
        return false;
    }

    // Note: this will latch to true immediately if the user starts up with an empty
    // datadir and an index enabled. If this is the case, indexation will happen solely
    // via `BlockConnected` signals until, possibly, the next restart.
<<<<<<< HEAD
    m_synced = start_block == active_chain.Tip();
    m_init = true;
||||||| parent of d7eef9f49a82 (indexes, refactor: Remove index prune_violation code)
    m_synced = m_best_block_index.load() == active_chain.Tip();
    if (!m_synced) {
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
            return InitError(strprintf(Untranslated("%s best block of the index goes beyond pruned data. Please disable the index or reindex (which will download the whole blockchain again)"), GetName()));
        }
    }
=======
    m_synced = m_best_block_index.load() == active_chain.Tip();
    if (!m_synced) {
        if (!m_chain->hasDataFromTipDown(m_best_block_index.load())) {
            return InitError(strprintf(Untranslated("%s best block of the index goes beyond pruned data. Please disable the index or reindex (which will download the whole blockchain again)"), GetName()));
        }
    }
>>>>>>> d7eef9f49a82 (indexes, refactor: Remove index prune_violation code)
    return true;
}

||||||| parent of 42ba163fcdaa (indexes, refactor: Remove index Init method)
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

    // Note: this will latch to true immediately if the user starts up with an empty
    // datadir and an index enabled. If this is the case, indexation will happen solely
    // via `BlockConnected` signals until, possibly, the next restart.
    m_synced = m_best_block_index.load() == active_chain.Tip();
    if (!m_synced) {
        if (!m_chain->hasDataFromTipDown(m_best_block_index.load())) {
            return InitError(strprintf(Untranslated("%s best block of the index goes beyond pruned data. Please disable the index or reindex (which will download the whole blockchain again)"), GetName()));
        }
    }
    return true;
}

=======
>>>>>>> 42ba163fcdaa (indexes, refactor: Remove index Init method)
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
    const CBlockIndex* pindex = m_best_block_index.load();
    if (!m_synced) {
<<<<<<< HEAD
||||||| parent of 46bd09f9a92c (indexes, refactor: Remove remaining CBlockIndex* uses in index CustomAppend methods)
        auto& consensus_params = Params().GetConsensus();

=======
        auto& consensus_params = Params().GetConsensus();
        auto notifications = WITH_LOCK(m_mutex, return m_notifications);

<<<<<<< HEAD
>>>>>>> 46bd09f9a92c (indexes, refactor: Remove remaining CBlockIndex* uses in index CustomAppend methods)
        std::chrono::steady_clock::time_point last_log_time{0s};
        std::chrono::steady_clock::time_point last_locator_write_time{0s};
||||||| parent of b377b50cd6ba (indexes, refactor: Move more new block logic out of ThreadSync to blockConnected)
        std::chrono::steady_clock::time_point last_log_time{0s};
        std::chrono::steady_clock::time_point last_locator_write_time{0s};
=======
>>>>>>> b377b50cd6ba (indexes, refactor: Move more new block logic out of ThreadSync to blockConnected)
        while (true) {
            if (m_interrupt) {
                return;
            }

            {
                LOCK(cs_main);
                const CBlockIndex* pindex_next = NextSyncBlock(pindex, m_chainstate->m_chain);
                if (!pindex_next) {
                    SetBestBlockIndex(pindex);
                    m_synced = true;
                    // No need to handle errors in Commit. See rationale above.
                    Commit(GetLocator(*m_chain, pindex->GetBlockHash()));
                    break;
                }
                if (pindex_next->pprev != pindex && !Rewind(pindex, pindex_next->pprev)) {
                    FatalErrorf("%s: Failed to rewind index %s to a previous chain tip",
                               __func__, GetName());
                    return;
                }
                pindex = pindex_next;
            }

            CBlock block;
            interfaces::BlockInfo block_info = kernel::MakeBlockInfo(pindex);
<<<<<<< HEAD
            if (!m_chainstate->m_blockman.ReadBlockFromDisk(block, *pindex)) {
                FatalErrorf("%s: Failed to read block %s from disk",
||||||| parent of 46bd09f9a92c (indexes, refactor: Remove remaining CBlockIndex* uses in index CustomAppend methods)
            if (!ReadBlockFromDisk(block, pindex, consensus_params)) {
                FatalError("%s: Failed to read block %s from disk",
=======
            // Set chain_tip to false so blockConnected call does not set m_synced to true.
            block_info.chain_tip = false;
            if (!ReadBlockFromDisk(block, pindex, consensus_params)) {
                FatalError("%s: Failed to read block %s from disk",
>>>>>>> 46bd09f9a92c (indexes, refactor: Remove remaining CBlockIndex* uses in index CustomAppend methods)
                           __func__, pindex->GetBlockHash().ToString());
                return;
            } else {
                block_info.data = &block;
            }
<<<<<<< HEAD
            if (!CustomAppend(block_info)) {
                FatalErrorf("%s: Failed to write block %s to index database",
                           __func__, pindex->GetBlockHash().ToString());
                return;
            }
||||||| parent of 46bd09f9a92c (indexes, refactor: Remove remaining CBlockIndex* uses in index CustomAppend methods)
            if (!CustomAppend(block_info)) {
                FatalError("%s: Failed to write block %s to index database",
                           __func__, pindex->GetBlockHash().ToString());
                return;
            }
=======
            notifications->blockConnected(block_info);
>>>>>>> 46bd09f9a92c (indexes, refactor: Remove remaining CBlockIndex* uses in index CustomAppend methods)
        }
    }

    if (pindex) {
        LogPrintf("%s is enabled at height %d\n", GetName(), pindex->nHeight);
    } else {
        LogPrintf("%s is enabled\n", GetName());
    }
}

bool BaseIndex::Commit(const CBlockLocator& locator)
{
    // Don't commit anything if we haven't indexed any block yet
    // (this could happen if init is interrupted).
    bool ok = !locator.IsNull();
    if (ok) {
        CDBBatch batch(GetDB());
        ok = CustomCommit(batch);
        if (ok) {
            GetDB().WriteBestBlock(batch, locator);
            ok = GetDB().WriteBatch(batch);
        }
    }
    if (!ok) {
        return error("%s: Failed to commit latest %s state", __func__, GetName());
    }
    return true;
}

bool BaseIndex::Rewind(const CBlockIndex* current_tip, const CBlockIndex* new_tip)
{
    assert(current_tip == m_best_block_index);
    assert(current_tip->GetAncestor(new_tip->nHeight) == new_tip);

    const Consensus::Params& consensus_params = Params().GetConsensus();
    CBlock block;
    CBlockUndo block_undo;

    for (const CBlockIndex* iter_tip = current_tip; iter_tip != new_tip; iter_tip = iter_tip->pprev) {
        interfaces::BlockInfo block_info = kernel::MakeBlockInfo(iter_tip);
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
    SetBestBlockIndex(new_tip);
    if (!Commit(GetLocator(*m_chain, new_tip->GetBlockHash()))) {
        // If commit fails, revert the best block index to avoid corruption.
        SetBestBlockIndex(current_tip);
        return false;
    }

    return true;
}

bool BaseIndex::IgnoreBlockConnected(const interfaces::BlockInfo& block)
{
    // During initial sync, ignore validation interface notifications, only
    // process notifications from sync thread.
    if (!m_synced) {
        return block.chain_tip;
    }

    const CBlockIndex* pindex = WITH_LOCK(cs_main, return m_chainstate->m_blockman.LookupBlockIndex(block.hash));
    const CBlockIndex* best_block_index = m_best_block_index.load();
    if (!best_block_index) {
        if (pindex->nHeight != 0) {
            FatalErrorf("%s: First block connected is not the genesis block (height=%d)",
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
<<<<<<< HEAD
            return;
        }
        if (best_block_index != pindex->pprev && !Rewind(best_block_index, pindex->pprev)) {
            FatalErrorf("%s: Failed to rewind index %s to a previous chain tip",
                       __func__, GetName());
            return;
||||||| parent of 7eb0aa6c9846 (indexes, refactor: Remove index validationinterface hooks)
            return;
        }
        if (best_block_index != pindex->pprev && !Rewind(best_block_index, pindex->pprev)) {
            FatalError("%s: Failed to rewind index %s to a previous chain tip",
                       __func__, GetName());
            return;
=======
            return true;
>>>>>>> 7eb0aa6c9846 (indexes, refactor: Remove index validationinterface hooks)
        }
    }
<<<<<<< HEAD
    interfaces::BlockInfo block_info = kernel::MakeBlockInfo(pindex, block.get());
    if (CustomAppend(block_info)) {
        // Setting the best block index is intentionally the last step of this
        // function, so BlockUntilSyncedToCurrentChain callers waiting for the
        // best block index to be updated can rely on the block being fully
        // processed, and the index object being safe to delete.
        SetBestBlockIndex(pindex);
    } else {
        FatalErrorf("%s: Failed to write block %s to index",
                   __func__, pindex->GetBlockHash().ToString());
        return;
    }
||||||| parent of 7eb0aa6c9846 (indexes, refactor: Remove index validationinterface hooks)
    interfaces::BlockInfo block_info = kernel::MakeBlockInfo(pindex, block.get());
    if (CustomAppend(block_info)) {
        // Setting the best block index is intentionally the last step of this
        // function, so BlockUntilSyncedToCurrentChain callers waiting for the
        // best block index to be updated can rely on the block being fully
        // processed, and the index object being safe to delete.
        SetBestBlockIndex(pindex);
    } else {
        FatalError("%s: Failed to write block %s to index",
                   __func__, pindex->GetBlockHash().ToString());
        return;
    }
=======
    return false;
>>>>>>> 7eb0aa6c9846 (indexes, refactor: Remove index validationinterface hooks)
}

bool BaseIndex::IgnoreChainStateFlushed(const CBlockLocator& locator)
{
    if (!m_synced) {
        return true;
    }

    assert(!locator.IsNull());
    const uint256& locator_tip_hash = locator.vHave.front();
    const CBlockIndex* locator_tip_index;
    {
        LOCK(cs_main);
        locator_tip_index = m_chainstate->m_blockman.LookupBlockIndex(locator_tip_hash);
    }

    if (!locator_tip_index) {
        FatalErrorf("%s: First block (hash=%s) in locator was not found",
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
        LogPrintf("%s: WARNING: Locator contains block (hash=%s) not on known best " /* Continued */
                  "chain (tip=%s); not writing index locator\n",
                  __func__, locator_tip_hash.ToString(),
                  best_block_index->GetBlockHash().ToString());
        return true;
    }
    return false;
}

bool BaseIndex::BlockUntilSyncedToCurrentChain() const
{
    AssertLockNotHeld(cs_main);

    if (!m_synced) {
        return false;
    }

    {
        // Skip the queue-draining stuff if we know we're caught up with
        // m_chain.Tip().
        LOCK(cs_main);
        const CBlockIndex* chain_tip = m_chainstate->m_chain.Tip();
        const CBlockIndex* best_block_index = m_best_block_index.load();
        if (best_block_index->GetAncestor(chain_tip->nHeight) == chain_tip) {
            return true;
        }
    }

    LogPrintf("%s: %s is catching up on block notifications\n", __func__, GetName());
    SyncWithValidationInterfaceQueue();
    return true;
}

void BaseIndex::Interrupt()
{
    m_interrupt();
    LOCK(m_mutex);
    m_notifications.reset();
}

bool BaseIndex::StartBackgroundSync()
{
<<<<<<< HEAD
    if (!m_init) throw std::logic_error("Error: Cannot start a non-initialized index");
||||||| parent of 42ba163fcdaa (indexes, refactor: Remove index Init method)
    // m_chainstate member gives indexing code access to node internals. It is
    // removed in followup https://github.com/bitcoin/bitcoin/pull/24230
    m_chainstate = &m_chain->context()->chainman->ActiveChainstate();
    // Need to register this ValidationInterface before running Init(), so that
    // callbacks are not missed if Init sets m_synced to true.
    RegisterValidationInterface(this);
    if (!Init()) return false;

    const CBlockIndex* index = m_best_block_index.load();
    if (!CustomInit(index ? std::make_optional(interfaces::BlockKey{index->GetBlockHash(), index->nHeight}) : std::nullopt)) {
        return false;
    }
=======
    // m_chainstate member gives indexing code access to node internals. It is
    // removed in followup https://github.com/bitcoin/bitcoin/pull/24230
    m_chainstate = &m_chain->context()->chainman->ActiveChainstate();
    CBlockLocator locator;
    if (!GetDB().ReadBestBlock(locator)) {
        locator.SetNull();
    }

    auto options = CustomOptions();
    auto notifications = std::make_shared<BaseIndexNotifications>(*this);
    auto handler = m_chain->attachChain(notifications, locator, options);
    if (!handler) {
        return InitError(strprintf(Untranslated("%s best block of the index goes beyond pruned data. Please disable the index or reindex (which will download the whole blockchain again)"), GetName()));
    } else {
        LOCK(m_mutex);
        m_notifications = std::move(notifications);
        m_handler = std::move(handler);
    }

    const CBlockIndex* index = m_best_block_index.load();
    if (!CustomInit(index ? std::make_optional(interfaces::BlockKey{index->GetBlockHash(), index->nHeight}) : std::nullopt)) {
        return false;
    }
>>>>>>> 42ba163fcdaa (indexes, refactor: Remove index Init method)

    m_thread_sync = std::thread(&util::TraceThread, GetName(), [this] { ThreadSync(); });
    return true;
}

void BaseIndex::Stop()
{
    Interrupt();

    if (m_thread_sync.joinable()) {
        m_thread_sync.join();
    }

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
    if (const auto& pindex = m_best_block_index.load()) {
        summary.best_block_height = pindex->nHeight;
        summary.best_block_hash = pindex->GetBlockHash();
    } else {
        summary.best_block_height = 0;
        summary.best_block_hash = m_chain->getBlockHash(0);
    }
    return summary;
}

void BaseIndex::SetBestBlockIndex(const CBlockIndex* block)
{
    assert(!m_chainstate->m_blockman.IsPruneMode() || AllowPrune());

    if (AllowPrune() && block) {
        node::PruneLockInfo prune_lock;
        prune_lock.height_first = block->nHeight;
        WITH_LOCK(::cs_main, m_chainstate->m_blockman.UpdatePruneLock(GetName(), prune_lock));
    }

    // Intentionally set m_best_block_index as the last step in this function,
    // after updating prune locks above, and after making any other references
    // to *this, so the BlockUntilSyncedToCurrentChain function (which checks
    // m_best_block_index as an optimization) can be used to wait for the last
    // BlockConnected notification and safely assume that prune locks are
    // updated and that the index object is safe to delete.
    m_best_block_index = block;
}
