// Copyright (c) 2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <interfaces/init.h>

namespace interfaces {
std::unique_ptr<Echo> Init::makeEcho() { return {}; }
} // namespace interfaces
