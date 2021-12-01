// Copyright (c) 2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_NODE_CHAINSTATE_H
#define BITCOIN_NODE_CHAINSTATE_H

#include <validation.h>

#include <cstdint>
#include <functional>
#include <optional>

class ChainstateManager;
class CTxMemPool;
namespace Consensus {
struct Params;
} // namespace Consensus

namespace node {

struct CacheSizes;

struct InitOptions
{
    CTxMemPool* mempool = nullptr;
    bool block_tree_db_in_memory = false;
    bool coins_db_in_memory = false;
    bool reset = false;
    bool prune = false;
    bool reindex = false;
    int64_t check_blocks = DEFAULT_CHECKBLOCKS;
    int64_t check_level = DEFAULT_CHECKLEVEL;
    std::function<bool()> check_interrupt;
    std::function<void()> coins_error_cb;
};

enum class InitStatus { SUCCESS, FAILURE, FAILURE_REINDEX, INTERRUPTED };

//! Status code and optional string.
using InitResult = std::tuple<InitStatus, bilingual_str>;

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
InitResult LoadChainstate(ChainstateManager& chainman,
                          const CacheSizes& cache_sizes,
                          const InitOptions& options = {});

InitResult VerifyLoadedChainstate(ChainstateManager& chainman,
                                  const InitOptions& options = {});
} // namespace node

#endif // BITCOIN_NODE_CHAINSTATE_H
