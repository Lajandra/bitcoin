<<<<<<< HEAD
// Copyright (c) 2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <node/chainstatemanager_args.h>

#include <arith_uint256.h>
#include <tinyformat.h>
#include <uint256.h>
#include <util/strencodings.h>
#include <util/system.h>
#include <util/translation.h>
#include <validation.h>

#include <chrono>
#include <optional>
#include <string>

namespace node {
std::optional<bilingual_str> ApplyArgsManOptions(const ArgsManager& args, ChainstateManager::Options& opts)
{
    if (auto value{args.GetBoolArg("-checkblockindex")}) opts.check_block_index = *value;

    if (auto value{args.GetBoolArg("-checkpoints")}) opts.checkpoints_enabled = *value;

    if (auto value{args.GetArg("-minimumchainwork")}) {
        if (!IsHexNumber(*value)) {
            return strprintf(Untranslated("Invalid non-hex (%s) minimum chain work value specified"), *value);
        }
        opts.minimum_chain_work = UintToArith256(uint256S(*value));
    }

    if (auto value{args.GetArg("-assumevalid")}) opts.assumed_valid_block = uint256S(*value);

    if (auto value{args.GetIntArg("-maxtipage")}) opts.max_tip_age = std::chrono::seconds{*value};

    return std::nullopt;
}
} // namespace node
||||||| parent of ee6058f7a0e (refactor, validation: Add ChainstateManagerOpts db options)
=======
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
>>>>>>> ee6058f7a0e (refactor, validation: Add ChainstateManagerOpts db options)
