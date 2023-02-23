// Copyright (c) 2023 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <common/init.h>
#include <chainparams.h>
#include <util/system.h>

#include <optional>

namespace common {
std::optional<bilingual_str> InitConfig(ArgsManager& args)
{
    if (!CheckDataDirOption()) {
        return strprintf(_("Specified data directory \"%s\" does not exist."), args.GetArg("-datadir", ""));
    }
    std::string error;
    if (!args.ReadConfigFiles(error, true)) {
        return strprintf(_("Error reading configuration file: %s"), error);
    }
    // Check for chain settings (Params() calls are only valid after this clause)
    try {
        SelectParams(args.GetChainName());
    } catch (const std::exception& e) {
       return Untranslated(e.what());
    }
    return {};
}
} // namespace commond
