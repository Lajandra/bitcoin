// Copyright (c) 2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_KERNEL_TIMEDATA_H
#define BITCOIN_KERNEL_TIMEDATA_H

#include <stdint.h>

namespace kernel {
struct Context;
/** Functions to keep track of adjusted P2P time */
int64_t GetTimeOffset(Context& context);
int64_t GetAdjustedTime(Context& context);
} // namespace kernel

#endif // BITCOIN_KERNEL_TIMEDATA_H
