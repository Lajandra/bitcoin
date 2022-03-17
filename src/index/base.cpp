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

constexpr int64_t SYNC_LOG_INTERVAL = 30; // seconds
constexpr int64_t SYNC_LOCATOR_WRITE_INTERVAL = 30; // seconds

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
    if (!m_index.m_synced) {
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
    if (m_index.m_synced) {
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
    m_chain->waitForNotifications();
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
