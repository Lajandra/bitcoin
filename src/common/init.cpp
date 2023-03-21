// Copyright (c) 2023 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <common/init.h>
#include <chainparams.h>
#include <fs.h>
#include <tinyformat.h>
#include <util/system.h>
#include <util/translation.h>

#include <algorithm>
#include <exception>
#include <optional>

namespace common {
std::optional<ConfigError> InitConfig(ArgsManager& args, SettingsAbortFn settings_abort_fn)
{
    try {
        if (!CheckDataDirOption(args)) {
            return ConfigError{ConfigStatus::FAILED, strprintf(_("Specified data directory \"%s\" does not exist."), args.GetArg("-datadir", ""))};
        }

        const fs::path data_dir_path{args.GetDataDirBase()};
        const fs::path config_path = args.GetConfigFilePath();

        std::string error;
        if (!args.ReadConfigFiles(error, true)) {
            return ConfigError{ConfigStatus::FAILED, strprintf(_("Error reading configuration file: %s"), error)};
        }

        // Check for chain settings (Params() calls are only valid after this clause)
        SelectParams(args.GetChainName());

        // Create datadir if it does not exist.
        // Note: it is important to call GetDataDirBase() again after calling
        // ReadConfigFiles() because the config file can specify a new datadir.
        // Specifying a different datadir is allowed so a user to can place a
        // bitcoin.conf in the default datadir location (see GetDefaultDataDir)
        // that points to other storage locations, while allowing CLI tools to
        // be called without -conf or -datadir arguments.
        const auto base_path{args.GetDataDirBase()};
        if (!fs::exists(base_path)) {
            // When creating a *new* datadir, also create a "wallets" subdirectory,
            // whether or not the wallet is enabled now, so if the wallet is enabled
            // in the future, it will use the "wallets" subdirectory for creating
            // and listing wallets, rather than the top-level directory where
            // wallets could be mixed up with other files. For backwards
            // compatibility, wallet code will use the "wallets" subdirectory only
            // if it already exists, but never create it itself. There is discussion
            // in https://github.com/bitcoin/bitcoin/issues/16220 about ways to
            // change wallet code so it would no longer be necessary to create
            // "wallets" subdirectories here.
            fs::create_directories(base_path / "wallets");
        }
        const auto net_path{args.GetDataDirNet()};
        if (!fs::exists(net_path)) {
            fs::create_directories(net_path / "wallets");
        }


        const fs::path new_config_path = base_path / BITCOIN_CONF_FILENAME;
        if (fs::exists(new_config_path) && !fs::equivalent(config_path, new_config_path)) {
            const std::string cli_config_path = args.GetArg("-conf", "");
            std::string config_source = cli_config_path.empty()
                ? strprintf("data directory %s", fs::quoted(data_dir_path))
                : strprintf("command line argument %s", std::quoted("-conf=" + cli_config_path));
            std::string error = strprintf(
                "Data directory %1$s contains a %2$s file which is ignored, because a different configuration file "
                "%3$s from %4$s is being used instead. Possible ways to resolve this would be to:\n"
                "- Delete or rename the %2$s file in data directory %1$s.\n"
                "- Change current datadir= or conf= options to specify one configuration file, not two, and use "
                "includeconf= to merge any other configuration files.\n"
                "- Set warnignoredconf=1 option to ignore the %2$s file in data directory %1$s with a "
                "warning instead of an error.",
                fs::quoted(base_path),
                fs::quoted(BITCOIN_CONF_FILENAME),
                fs::quoted(config_path),
                config_source);
            if (args.GetBoolArg("-warnignoredconf", false)) {
                LogPrintf("Warning: %s\n", error);
            } else {
                return ConfigError{ConfigStatus::FAILED, Untranslated(error)};
            }
        }

        // Create settings.json if -nosettings was not specified.
        if (args.GetSettingsPath()) {
            std::vector<std::string> details;
            if (!args.ReadSettingsFile(&details)) {
                const bilingual_str& message = _("Settings file could not be read");
                if (!settings_abort_fn) {
                    return ConfigError{ConfigStatus::FAILED, message, details};
                } else if (settings_abort_fn(message, details)) {
                    return ConfigError{ConfigStatus::ABORTED, message, details};
                } else {
                    details.clear(); // User chose to ignore the error and proceed.
                }
            }
            if (!args.WriteSettingsFile(&details)) {
                const bilingual_str& message = _("Settings file could not be written");
                return ConfigError{ConfigStatus::FAILED_WRITE, message, details};
            }
        }
    } catch (const std::exception& e) {
        return ConfigError{ConfigStatus::FAILED, Untranslated(e.what())};
    }
    return {};
}
} // namespace common
