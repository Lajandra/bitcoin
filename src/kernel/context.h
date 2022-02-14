// Copyright (c) 2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_KERNEL_CONTEXT_H
#define BITCOIN_KERNEL_CONTEXT_H

#include <sync.h>

#include <stdint.h>

namespace kernel {
//! Context struct holding global the kernel library's global state, and passed
//! to external libbitcoin_kernel functions which need access to this state. The
//! kernel libary API is a work in progress, so state organization and member
//! list will evolve over time.
//!
//! State stored directly in this struct should be simple. More complex state
//! should be stored to std::unique_ptr members pointing to opaque types.
struct Context {
    Mutex timeoffset_mutex;
    int64_t time_offset GUARDED_BY(timeoffset_mutex) = 0;
};
} // namespace kernel

#endif // BITCOIN_KERNEL_CONTEXT_H
