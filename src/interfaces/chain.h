// Copyright (c) 2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_INTERFACES_CHAIN_H
#define BITCOIN_INTERFACES_CHAIN_H

#include <optional.h>

#include <memory>
#include <stdint.h>
#include <string>
#include <vector>

class CBlock;
class CScheduler;
class uint256;
struct CBlockLocator;

namespace interfaces {

//! Interface for giving wallet processes access to blockchain state.
class Chain
{
public:
    virtual ~Chain() {}

    //! Interface for querying locked chain state, used by legacy code that
    //! assumes state won't change between calls. New code should avoid using
    //! the Lock interface and instead call higher-level Chain methods
    //! that return more information so the chain doesn't need to stay locked
    //! between calls.
    class Lock
    {
    public:
        virtual ~Lock() {}

        //! Get current chain height, not including genesis block (returns 0 if
        //! chain only contains genesis block, nothing if chain does not contain
        //! any blocks).
        virtual Optional<int> getHeight() = 0;

        //! Get block height above genesis block. Returns 0 for genesis block,
        //! 1 for following block, and so on. Returns nothing for a block not
        //! included in the current chain.
        virtual Optional<int> getBlockHeight(const uint256& hash) = 0;

        //! Get block depth. Returns 1 for chain tip, 2 for preceding block, and
        //! so on. Returns 0 for a block not included in the current chain.
        virtual int getBlockDepth(const uint256& hash) = 0;

        //! Get block hash.
        virtual uint256 getBlockHash(int height) = 0;

        //! Get block time.
        virtual int64_t getBlockTime(int height) = 0;

        //! Get block median time past.
        virtual int64_t getBlockMedianTimePast(int height) = 0;

        //! Check that the full block is available on disk (ie has not been
        //! pruned), and contains transactions.
        virtual bool haveBlockOnDisk(int height) = 0;

        //! Return height of the first block in the chain with timestamp equal
        //! or greater than the given time, or nothing if there is no block with
        //! a high enough timestamp.
        virtual Optional<int> findFirstBlockWithTime(int64_t time) = 0;

        //! Return height of the first block in the chain with timestamp equal
        //! or greater than the given time and height equal or greater than the
        //! given height, or nothing if there is no such block.
        //!
        //! Calling this with height 0 is equivalent to calling
        //! findFirstBlockWithTime, but less efficient because it requires a
        //! linear instead of a binary search.
        virtual Optional<int> findFirstBlockWithTimeAndHeight(int64_t time, int height) = 0;

        //! Return height of last block in the specified range which is pruned, or
        //! nothing if no block in the range is pruned. Range is inclusive.
        virtual Optional<int> findPruned(int start_height = 0, Optional<int> stop_height = nullopt) = 0;

        //! Return height of the highest block on the chain that is an ancestor
        //! of the specified block. Also return the height of the specified
        //! block as an optional output parameter.
        virtual Optional<int> findFork(const uint256& hash, Optional<int>* height) = 0;

        //! Return true if block hash points to the current chain tip, or to a
        //! possible descendant of the current chain tip that isn't currently
        //! connected.
        virtual bool isPotentialTip(const uint256& hash) = 0;

        //! Get locator for the current chain tip.
        virtual CBlockLocator getLocator() = 0;

        //! Return height of block on the chain using locator.
        virtual Optional<int> findLocatorFork(const CBlockLocator& locator) = 0;
    };

    //! Return Lock interface. Chain is locked when this is called, and
    //! unlocked when the returned interface is freed.
    virtual std::unique_ptr<Lock> lock(bool try_lock = false) = 0;

    //! Return Lock interface assuming chain is already locked. This
    //! method is temporary and is only used in a few places to avoid changing
    //! behavior while code is transitioned to use the Chain::Lock interface.
    virtual std::unique_ptr<Lock> assumeLocked() = 0;

    //! Return whether node has the block and optionally return block metadata or contents.
    virtual bool findBlock(const uint256& hash,
        CBlock* block = nullptr,
        int64_t* time = nullptr,
        int64_t* max_time = nullptr) = 0;

    //! Estimate fraction of total transactions verified if blocks up to
    //! given height are verified.
    virtual double guessVerificationProgress(const uint256& block_hash) = 0;
};

//! Interface to let node manage chain clients (wallets, or maybe tools for
//! monitoring and analysis in the future).
class ChainClient
{
public:
    virtual ~ChainClient() {}

    //! Register rpcs.
    virtual void registerRpcs() = 0;

    //! Check for errors before loading.
    virtual bool verify() = 0;

    //! Load saved state.
    virtual bool load() = 0;

    //! Start client execution and provide a scheduler.
    virtual void start(CScheduler& scheduler) = 0;

    //! Save state to disk.
    virtual void flush() = 0;

    //! Shut down client.
    virtual void stop() = 0;
};

//! Return implementation of Chain interface.
std::unique_ptr<Chain> MakeChain();

//! Return implementation of ChainClient interface for a wallet client. This
//! function will be undefined in builds where ENABLE_WALLET is false.
//!
//! Currently, wallets are the only chain clients. But in the future, other
//! types of chain clients could be added, such as tools for monitoring,
//! analysis, or fee estimation. These clients need to expose their own
//! MakeXXXClient functions returning their implementations of the ChainClient
//! interface.
std::unique_ptr<ChainClient> MakeWalletClient(Chain& chain, std::vector<std::string> wallet_filenames);

} // namespace interfaces

#endif // BITCOIN_INTERFACES_CHAIN_H
