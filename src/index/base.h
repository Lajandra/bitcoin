// Copyright (c) 2017-2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_INDEX_BASE_H
#define BITCOIN_INDEX_BASE_H

#include <dbwrapper.h>
#include <interfaces/chain.h>
#include <interfaces/handler.h>
#include <threadinterrupt.h>
#include <validationinterface.h>

class BaseIndexNotifications;
class CBlock;
class CChainState;
namespace interfaces {
class Chain;
} // namespace interfaces

struct IndexSummary {
    std::string name;
    bool synced{false};
    int best_block_height{0};
};

/**
 * Base class for indices of blockchain data. This handles block connected and
 * disconnected notifications and ensures blocks are indexed sequentially
 * according to their position in the active chain.
 */
class BaseIndex
{
protected:
    /**
     * The database stores a block locator of the chain the database is synced to
     * so that the index can efficiently determine the point it last stopped at.
     * A locator is used instead of a simple hash of the chain tip because blocks
     * and block index entries may not be flushed to disk until after this database
     * is updated.
    */
    class DB : public CDBWrapper
    {
    public:
        DB(const fs::path& path, size_t n_cache_size,
           bool f_memory = false, bool f_wipe = false, bool f_obfuscate = false);

        /// Read block locator of the chain that the index is in sync with.
        bool ReadBestBlock(CBlockLocator& locator) const;

        /// Write block locator of the chain that the index is in sync with.
        void WriteBestBlock(CDBBatch& batch, const CBlockLocator& locator);
    };

private:
    /// Whether the index is in sync with the main chain. The flag is flipped
    /// from false to true once, after which point this starts processing
    /// ValidationInterface notifications to stay in sync.
    std::atomic<bool> m_synced{false};

    /// The last block in the chain that the index is in sync with.
    std::optional<interfaces::BlockKey> m_best_block GUARDED_BY(m_mutex);

    /// Read best block locator and check that data needed to sync has not been pruned.
    bool Init();

    /// Write the current index state (eg. chain block locator and subclass-specific items) to disk.
    ///
    /// Recommendations for error handling:
    /// If called on a successor of the previous committed best block in the index, the index can
    /// continue processing without risk of corruption, though the index state will need to catch up
    /// from further behind on reboot. If the new state is not a successor of the previous state (due
    /// to a chain reorganization), the index must halt until Commit succeeds or else it could end up
    /// getting corrupted.
<<<<<<< HEAD
    bool Commit();
<<<<<<< HEAD

    virtual bool AllowPrune() const = 0;

||||||| parent of 7f03da8d12b (indexes, refactor: Remove CBlockIndex* uses in index Rewind methods)
=======
||||||| parent of 4389f0cdc8a (indexes, refactor: Add Commit CBlockLocator& argument)
    bool Commit();
=======
    bool Commit(const CBlockLocator& locator);
>>>>>>> 4389f0cdc8a (indexes, refactor: Add Commit CBlockLocator& argument)

    /// Loop over disconnected blocks and call CustomRemove.
    bool Rewind(const interfaces::BlockKey& current_tip, const interfaces::BlockKey& new_tip);

<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
>>>>>>> 7f03da8d12b (indexes, refactor: Remove CBlockIndex* uses in index Rewind methods)
||||||| parent of 8cb6c143805 (indexes, refactor: Remove index Init method)
=======
||||||| parent of dd832e81e91 (indexes, refactor: Remove index validation interface and block locator code)
=======
    Mutex m_mutex;
>>>>>>> dd832e81e91 (indexes, refactor: Remove index validation interface and block locator code)
||||||| parent of 3b3f66ec486 (indexes, refactor: Remove remaining CBlockIndex* pointers from indexing code)
    Mutex m_mutex;
=======
    mutable Mutex m_mutex;
>>>>>>> 3b3f66ec486 (indexes, refactor: Remove remaining CBlockIndex* pointers from indexing code)
    friend class BaseIndexNotifications;
    std::shared_ptr<BaseIndexNotifications> m_notifications GUARDED_BY(m_mutex);
    std::unique_ptr<interfaces::Handler> m_handler GUARDED_BY(m_mutex);

>>>>>>> 8cb6c143805 (indexes, refactor: Remove index Init method)
protected:
    std::unique_ptr<interfaces::Chain> m_chain;

    /// Return custom notification options for index.
    [[nodiscard]] virtual interfaces::Chain::NotifyOptions CustomOptions() { return {}; }

    /// Initialize internal state from the database and block index.
    [[nodiscard]] virtual bool CustomInit(const std::optional<interfaces::BlockKey>& block) { return true; }

    /// Write update index entries for a newly connected block.
    [[nodiscard]] virtual bool CustomAppend(const interfaces::BlockInfo& block) { return true; }

    /// Virtual method called internally by Commit that can be overridden to atomically
    /// commit more index state.
    virtual bool CustomCommit(CDBBatch& batch) { return true; }

    /// Rewind index by one block during a chain reorg.
    [[nodiscard]] virtual bool CustomRemove(const interfaces::BlockInfo& block) { return true; }

    virtual DB& GetDB() const = 0;

    /// Get the name of the index for display in logs.
    virtual const char* GetName() const = 0;

    /// Update the internal best block index as well as the prune lock.
    void SetBestBlockIndex(const CBlockIndex* block);

public:
    BaseIndex(std::unique_ptr<interfaces::Chain> chain);
    virtual ~BaseIndex();

    /// Blocks the current thread until the index is caught up to the current
    /// state of the block chain. This only blocks if the index has gotten in
    /// sync once and only needs to process blocks in the ValidationInterface
    /// queue. If the index is catching up from far behind, this method does
    /// not block and immediately returns false.
    bool BlockUntilSyncedToCurrentChain() const LOCKS_EXCLUDED(::cs_main);

    void Interrupt();

    /// Start initializes the sync state and registers the instance as a
    /// ValidationInterface so that it stays in sync with blockchain updates.
    [[nodiscard]] bool Start();

    /// Stops the instance from staying in sync with blockchain updates.
    void Stop();

    /// Get a summary of the index and its state.
    IndexSummary GetSummary() const;
};

#endif // BITCOIN_INDEX_BASE_H
