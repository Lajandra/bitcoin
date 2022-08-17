// Copyright (c) 2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <node/chainstatemanager_args.h>

#include <kernel/chainstatemanager_opts.h>
#include <node/coins_view_args.h>
#include <node/database_args.h>

namespace node {
void ReadChainstateManagerArgs(const ArgsManager& args, kernel::ChainstateManagerOpts& options)
{
    ReadDatabaseArgs(args, options.block_tree_db);
    ReadDatabaseArgs(args, options.coins_db);
    ReadCoinsViewArgs(args, options.coins_view);
}
} // namespace node
