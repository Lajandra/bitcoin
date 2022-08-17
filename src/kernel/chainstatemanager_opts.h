// Copyright (c) 2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_KERNEL_CHAINSTATEMANAGER_OPTS_H
#define BITCOIN_KERNEL_CHAINSTATEMANAGER_OPTS_H

<<<<<<< HEAD
#include <util/time.h>

||||||| parent of 370189fe960 (refactor, kernel: Remove gArgs accesses from dbwrapper and txdb)
=======
#include <dbwrapper.h>
#include <txdb.h>

>>>>>>> 370189fe960 (refactor, kernel: Remove gArgs accesses from dbwrapper and txdb)
#include <cstdint>
#include <functional>

class CChainParams;

namespace kernel {

/**
 * An options struct for `ChainstateManager`, more ergonomically referred to as
 * `ChainstateManager::Options` due to the using-declaration in
 * `ChainstateManager`.
 */
struct ChainstateManagerOpts {
    const CChainParams& chainparams;
<<<<<<< HEAD
    const std::function<NodeClock::time_point()> adjusted_time_callback{nullptr};
||||||| parent of 370189fe960 (refactor, kernel: Remove gArgs accesses from dbwrapper and txdb)
    const std::function<int64_t()> adjusted_time_callback{nullptr};
=======
    const std::function<int64_t()> adjusted_time_callback{nullptr};
    fs::path datadir;
    DBOptions block_tree_db;
    DBOptions coins_db;
    CoinsViewOptions coins_view;
>>>>>>> 370189fe960 (refactor, kernel: Remove gArgs accesses from dbwrapper and txdb)
};

} // namespace kernel

#endif // BITCOIN_KERNEL_CHAINSTATEMANAGER_OPTS_H
