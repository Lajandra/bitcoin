// Copyright (c) 2016-2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_WALLET_RPCWALLET_H
#define BITCOIN_WALLET_RPCWALLET_H

#include <span.h>

class CRPCCommand;
<<<<<<< HEAD
||||||| parent of d72688f16dd (Add src/wallet/* code to wallet:: namespace)
class CWallet;
class JSONRPCRequest;
class LegacyScriptPubKeyMan;
class UniValue;
class CTransaction;
struct PartiallySignedTransaction;
struct WalletContext;
=======
class JSONRPCRequest;
class UniValue;
class CTransaction;
struct PartiallySignedTransaction;

namespace wallet {
class CWallet;
class LegacyScriptPubKeyMan;
struct WalletContext;
>>>>>>> d72688f16dd (Add src/wallet/* code to wallet:: namespace)

Span<const CRPCCommand> GetWalletRPCCommands();

RPCHelpMan getaddressinfo();
RPCHelpMan signrawtransactionwithwallet();
} // namespace wallet
#endif // BITCOIN_WALLET_RPCWALLET_H
