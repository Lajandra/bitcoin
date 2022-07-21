// Copyright (c) 2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_NODE_CHAINSTATE_H
#define BITCOIN_NODE_CHAINSTATE_H

<<<<<<< HEAD
#include <util/translation.h>
||||||| parent of 81130462f61 (Update LoadChainstate to use util::Result)
=======
#include <util/result.h>
>>>>>>> 81130462f61 (Update LoadChainstate to use util::Result)
#include <validation.h>

#include <cstdint>
#include <functional>
#include <tuple>

class CTxMemPool;

namespace node {

struct CacheSizes;

struct ChainstateLoadOptions {
    CTxMemPool* mempool{nullptr};
    bool block_tree_db_in_memory{false};
    bool coins_db_in_memory{false};
    bool reindex{false};
    bool reindex_chainstate{false};
    bool prune{false};
    int64_t check_blocks{DEFAULT_CHECKBLOCKS};
    int64_t check_level{DEFAULT_CHECKLEVEL};
    std::function<bool()> check_interrupt;
    std::function<void()> coins_error_cb;
};

//! Chainstate load errors. Simple applications can just treat all errors as
//! failures. More complex applications may want to try reindexing in the
//! generic error case, and pass an interrupt callback and exit cleanly in the
//! interrupted case.
enum class ChainstateLoadError { FAILURE, FAILURE_INCOMPATIBLE_DB, INTERRUPTED };

/** This sequence can have 4 types of outcomes:
 *
 *  1. Success
 *  2. Shutdown requested
 *    - nothing failed but a shutdown was triggered in the middle of the
 *      sequence
 *  3. Soft failure
 *    - a failure that might be recovered from with a reindex
 *  4. Hard failure
 *    - a failure that definitively cannot be recovered from with a reindex
 *
 *  LoadChainstate returns a (status code, error string) tuple.
 */
util::Result<void, ChainstateLoadError> LoadChainstate(ChainstateManager& chainman, const CacheSizes& cache_sizes,
                                                       const ChainstateLoadOptions& options);
util::Result<void, ChainstateLoadError> VerifyLoadedChainstate(ChainstateManager& chainman, const ChainstateLoadOptions& options);
} // namespace node

#endif // BITCOIN_NODE_CHAINSTATE_H
