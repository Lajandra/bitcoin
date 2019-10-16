// Copyright (c) 2019 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_UTIL_SETTINGS_H
#define BITCOIN_UTIL_SETTINGS_H

#include <map>
#include <string>
#include <vector>

class UniValue;

namespace util {

//! Settings value type (string/integer/boolean/null variant).
//!
//! @note UniValue is used here for convenience and because it can be easily
//!       serialized in a readable format. But any other variant type could be
//!       substituted if there's a need to move away from UniValue.
using SettingsValue = UniValue;

//! Stored bitcoin settings. This struct combines settings from the command line
//! and a read-only configuration file.
struct Settings {
    //! Map of setting name to forced setting value.
    std::map<std::string, SettingsValue> forced_settings;
    //! Map of setting name to list of command line values.
    std::map<std::string, std::vector<SettingsValue>> command_line_options;
    //! Map of config section name and setting name to list of config file values.
    std::map<std::string, std::map<std::string, std::vector<SettingsValue>>> ro_config;
};

//! Get settings value from combined sources: forced settings, command line
//! arguments and the read-only config file.
//!
//! @param ignore_default_section_config - ignore values set in the top section
//!                                        of the config file (default section
//!                                        before any [section] keywords)
//! @param get_chain_name - enable special backwards compatible behavior
//!                         for GetChainName
SettingsValue GetSetting(const Settings& settings, const std::string& section, const std::string& name, bool ignore_default_section_config, bool get_chain_name);

//! Get combined setting value similar to GetSetting(), except if setting was
//! specified multiple times, return a list of all the values specified.
std::vector<SettingsValue> GetSettingsList(const Settings& settings, const std::string& section, const std::string& name, bool ignore_default_section_config);

//! Return true if there is a config value in the default section that would be
//! ignored when the `ignore_default_section_config` is true, and is not
//! overridden by a more specific command-line or network specific value.
//!
//! This is used to provide user warnings about values that might be getting
//! ignored unintentionally.
bool HasIgnoredDefaultSectionConfigValue(const Settings& settings, const std::string& section, const std::string& name);

//! Iterable list of settings that skips negated values.
struct SettingsSpan {
    SettingsSpan(const SettingsValue* data, size_t size) : data(data), size(size) {}
    SettingsSpan(const std::vector<SettingsValue>& vec);
    SettingsSpan(const SettingsValue& value) : SettingsSpan(&value, 1) {}
    SettingsSpan() {}
    const SettingsValue* begin() const; //<! Pointer to first non-negated value.
    const SettingsValue* end() const;   //<! Pointer to end of values.
    bool empty() const;                 //<! True if there are any non-negated values.
    bool last_negated() const;          //<! True if the last value is negated.
    size_t negated() const;             //<! Number of negated values.

    const SettingsValue* data = nullptr;
    size_t size = 0;
};

//! Map lookup helper.
template <typename Map, typename Key>
auto FindKey(Map&& map, Key&& key) -> decltype(&map.at(key))
{
    auto it = map.find(key);
    return it == map.end() ? nullptr : &it->second;
}

} // namespace util

#endif // BITCOIN_UTIL_SETTINGS_H
