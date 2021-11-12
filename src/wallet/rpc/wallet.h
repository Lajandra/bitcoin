// Copyright (c) 2016-2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_WALLET_RPC_WALLET_H
#define BITCOIN_WALLET_RPC_WALLET_H

#include <span.h>

class CRPCCommand;

namespace wallet {
Span<const CRPCCommand> GetWalletRPCCommands();

<<<<<<< HEAD:src/wallet/rpc/wallet.h
#endif // BITCOIN_WALLET_RPC_WALLET_H
||||||| parent of 64c8123c6e3 (Add src/wallet/* code to wallet:: namespace):src/wallet/rpcwallet.h
RPCHelpMan getaddressinfo();
RPCHelpMan signrawtransactionwithwallet();
#endif // BITCOIN_WALLET_RPCWALLET_H
=======
RPCHelpMan getaddressinfo();
RPCHelpMan signrawtransactionwithwallet();
} // namespace wallet

#endif // BITCOIN_WALLET_RPCWALLET_H
>>>>>>> 64c8123c6e3 (Add src/wallet/* code to wallet:: namespace):src/wallet/rpcwallet.h
