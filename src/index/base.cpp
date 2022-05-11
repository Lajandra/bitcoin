// Copyright (c) 2017-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>
#include <index/base.h>
#include <interfaces/chain.h>
#include <interfaces/handler.h>
#include <kernel/chain.h>
#include <node/blockstorage.h>
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

#include <string>
#include <utility>

using interfaces::FoundBlock;

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
    bool found = chain.findBlock(block_hash, FoundBlock().locator(locator));
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
    //! the next flush or block connection. m_rewind_start points to the first
    //! block that has been disconnected and not flushed yet. m_rewind_error
    //! is set if a block failed to disconnect.
    std::optional<interfaces::BlockKey> m_rewind_start;
    bool m_rewind_error = false;
};

void BaseIndexNotifications::blockConnected(const interfaces::BlockInfo& block)
{
    if (!block.error.empty()) {
        FatalError("%s", block.error);
        return m_index.Interrupt();
    }
    if (!block.data) {
        // Null block.data means block is the starting block at the beginning
        // of the sync, or the ending block at the end of a sync. In either
        // case, set the best block to this block, and latch m_synced to true
        // if there are no blocks following it.
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
        return;
    }

    bool rewind_ok = !m_rewind_start || !m_rewind_error;
    if (m_rewind_start && rewind_ok) {
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
        rewind_ok = getBest()->hash == *block.prev_hash;
    }

    if (!rewind_ok) {
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
        // Setting the best block index is intentionally the last step of this
        // function, so BlockUntilSyncedToCurrentChain callers waiting for the
        // best block index to be updated can rely on the block being fully
        // processed, and the index object being safe to delete.
        setBest({block.hash, block.height});
    } else if (m_last_locator_write_time + SYNC_LOCATOR_WRITE_INTERVAL < current_time) {
        auto locator = GetLocator(*m_index.m_chain, block.hash);
        setBest({block.hash, block.height});
        m_last_locator_write_time = current_time;
        // No need to handle errors in Commit. If it fails, the error will be already be
        // logged. The best way to recover is to continue, as index cannot be corrupted by
        // a missed commit to disk for an advanced index state.
        m_index.Commit(locator);
    }
}

void BaseIndexNotifications::blockDisconnected(const interfaces::BlockInfo& block)
{
    if (!block.error.empty()) {
        FatalError("%s", block.error);
        return m_index.Interrupt();
    }

    auto best_block = getBest();
    if (!m_rewind_start) m_rewind_start = best_block;
    if (!m_rewind_error) m_rewind_error = !m_index.CustomRemove(block);
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
    m_rewind_error = false;
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

<<<<<<< HEAD
void BaseIndex::SetBestBlockIndex(const CBlockIndex* block)
{
    assert(!m_chainstate->m_blockman.IsPruneMode() || AllowPrune());
||||||| parent of 572ae290111 (indexes, refactor: Remove remaining CBlockIndex* pointers from indexing code)
void BaseIndex::SetBestBlockIndex(const CBlockIndex* block) {
    assert(!node::fPruneMode || AllowPrune());
=======
void BaseIndex::SetBestBlock(const interfaces::BlockKey& block) {
    assert(!node::fPruneMode || AllowPrune());
>>>>>>> 572ae290111 (indexes, refactor: Remove remaining CBlockIndex* pointers from indexing code)

    if (AllowPrune()) {
        node::PruneLockInfo prune_lock;
        prune_lock.height_first = block.height;
        m_chain->updatePruneLock(GetName(), prune_lock);
    }

    // Intentionally set m_best_block as the last step in this function,
    // after updating prune locks above, and after making any other references
    // to *this, so the BlockUntilSyncedToCurrentChain function (which checks
    // m_best_block as an optimization) can be used to wait for the last
    // BlockConnected notification and safely assume that prune locks are
    // updated and that the index object is safe to delete.
    WITH_LOCK(m_mutex, m_best_block = block);
}
