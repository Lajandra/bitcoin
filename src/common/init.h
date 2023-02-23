// Copyright (c) 2023 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_COMMON_INIT_H
#define BITCOIN_COMMON_INIT_H

#include <util/translation.h>

#include <optional>

class ArgsManager;

namespace common {
/* Read config files and create datadir if it does not exist. Return error on failure */
std::optional<bilingual_str> InitConfig(ArgsManager& args);
} // namespace common

#endif // BITCOIN_COMMON_INIT_H
