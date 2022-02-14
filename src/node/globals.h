// Copyright (c) 2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_NODE_GLOBALS_H
#define BITCOIN_NODE_GLOBALS_H

namespace kernel {
struct Context;
} // namespace kernel

namespace node {
//! Global kernel context. As with all global variables, prefer not to use this
//! in new code! Instead use \ref NodeContext::kernel instead, pass
//! kernel::Context& directly arguments to functions that need to access kernel
//! state.
extern kernel::Context g_kernel;
} // namespace node

#endif // BITCOIN_NODE_GLOBALS_H
