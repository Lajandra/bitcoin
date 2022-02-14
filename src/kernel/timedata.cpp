// Copyright (c) 2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <kernel/timedata.h>

#include <kernel/context.h>
#include <util/time.h>

namespace kernel {
/**
 * "Never go to sea with two chronometers; take one or three."
 * Our three time sources are:
 *  - System clock
 *  - Median of other nodes clocks
 *  - The user (asking the user to fix the system clock if the first two disagree)
 */
int64_t GetTimeOffset(Context& context)
{
    LOCK(context.timeoffset_mutex);
    return context.time_offset;
}

int64_t GetAdjustedTime(Context& context)
{
    return GetTime() + GetTimeOffset(context);
}
} // namespace kernel
