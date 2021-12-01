// Copyright (c) 2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_NODE_CHAINSTATE_H
#define BITCOIN_NODE_CHAINSTATE_H

#include <validation.h>

#include <cstdint>
#include <functional>
#include <tuple>

class ChainstateManager;
namespace Consensus {
    struct Params;
}
class CTxMemPool;
struct CacheSizes;
struct bilingual_str;

enum class InitStatus { SUCCESS, FAILURE, INTERRUPTED };

//! Status code and optional string.
using InitResult = std::tuple<InitStatus, bilingual_str>;

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
    std::function<int64_t()> get_unix_time_seconds;
};

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
InitResult LoadChainstate(
    ChainstateManager& chainman,
    const Consensus::Params& consensus_params,
    const CacheSizes& cache_sizes,
    const InitOptions& options = {});

InitResult VerifyLoadedChainstate(
    ChainstateManager& chainman,
    const Consensus::Params& consensus_params,
    const InitOptions& options = {});

#endif // BITCOIN_NODE_CHAINSTATE_H
