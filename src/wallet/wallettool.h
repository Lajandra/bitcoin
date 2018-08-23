// Copyright (c) 2016-2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_WALLET_WALLETTOOL_H
#define BITCOIN_WALLET_WALLETTOOL_H

#include <string>

class ArgsManager;

namespace interfaces {
class Chain;
} // namespace interfaces

namespace WalletTool {

<<<<<<< HEAD
bool ExecuteWalletToolFunc(const ArgsManager& args, const std::string& command);
||||||| parent of dd5bbe8cdcf (multiprocess: Add -ipcconnect and -ipcbind options)
void WalletShowInfo(CWallet* wallet_instance);
bool ExecuteWalletToolFunc(const ArgsManager& args, const std::string& command);
=======
void WalletShowInfo(CWallet* wallet_instance);
bool ExecuteWalletToolFunc(const ArgsManager& args, interfaces::Chain* chain, const std::string& command);
>>>>>>> dd5bbe8cdcf (multiprocess: Add -ipcconnect and -ipcbind options)

} // namespace WalletTool

#endif // BITCOIN_WALLET_WALLETTOOL_H
