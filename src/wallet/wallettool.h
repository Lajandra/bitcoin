// Copyright (c) 2016-2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_WALLET_WALLETTOOL_H
#define BITCOIN_WALLET_WALLETTOOL_H

#include <string>

class ArgsManager;

<<<<<<< HEAD
namespace wallet {
||||||| parent of 40c88d90c0b (multiprocess: Add -ipcconnect and -ipcbind options)
=======
namespace interfaces {
class Chain;
} // namespace interfaces

>>>>>>> 40c88d90c0b (multiprocess: Add -ipcconnect and -ipcbind options)
namespace WalletTool {

bool ExecuteWalletToolFunc(const ArgsManager& args, interfaces::Chain* chain, const std::string& command);

} // namespace WalletTool
} // namespace wallet

#endif // BITCOIN_WALLET_WALLETTOOL_H
