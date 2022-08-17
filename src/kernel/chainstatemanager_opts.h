// Copyright (c) 2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_KERNEL_CHAINSTATEMANAGER_OPTS_H
#define BITCOIN_KERNEL_CHAINSTATEMANAGER_OPTS_H

<<<<<<< HEAD
#include <arith_uint256.h>
#include <uint256.h>
||||||| parent of ee6058f7a0e (refactor, validation: Add ChainstateManagerOpts db options)
=======
#include <dbwrapper.h>
#include <txdb.h>
>>>>>>> ee6058f7a0e (refactor, validation: Add ChainstateManagerOpts db options)
#include <util/time.h>

#include <cstdint>
#include <functional>
#include <optional>

class CChainParams;

static constexpr bool DEFAULT_CHECKPOINTS_ENABLED{true};
static constexpr auto DEFAULT_MAX_TIP_AGE{24h};

namespace kernel {

/**
 * An options struct for `ChainstateManager`, more ergonomically referred to as
 * `ChainstateManager::Options` due to the using-declaration in
 * `ChainstateManager`.
 */
struct ChainstateManagerOpts {
    const CChainParams& chainparams;
    const std::function<NodeClock::time_point()> adjusted_time_callback{nullptr};
<<<<<<< HEAD
    std::optional<bool> check_block_index{};
    bool checkpoints_enabled{DEFAULT_CHECKPOINTS_ENABLED};
    //! If set, it will override the minimum work we will assume exists on some valid chain.
    std::optional<arith_uint256> minimum_chain_work;
    //! If set, it will override the block hash whose ancestors we will assume to have valid scripts without checking them.
    std::optional<uint256> assumed_valid_block;
    //! If the tip is older than this, the node is considered to be in initial block download.
    std::chrono::seconds max_tip_age{DEFAULT_MAX_TIP_AGE};
||||||| parent of ee6058f7a0e (refactor, validation: Add ChainstateManagerOpts db options)
=======
    fs::path datadir;
    DBOptions block_tree_db{};
    DBOptions coins_db{};
    CoinsViewOptions coins_view{};
>>>>>>> ee6058f7a0e (refactor, validation: Add ChainstateManagerOpts db options)
};

} // namespace kernel

#endif // BITCOIN_KERNEL_CHAINSTATEMANAGER_OPTS_H
