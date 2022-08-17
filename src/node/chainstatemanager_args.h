// Copyright (c) 2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_NODE_CHAINSTATEMANAGER_ARGS_H
#define BITCOIN_NODE_CHAINSTATEMANAGER_ARGS_H

class ArgsManager;
namespace kernel {
struct ChainstateManagerOpts;
} // kernel

namespace node {
void ReadChainstateManagerArgs(const ArgsManager& args, kernel::ChainstateManagerOpts& options);
} // namespace node

#endif // BITCOIN_NODE_CHAINSTATEMANAGER_ARGS_H
