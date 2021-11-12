// Copyright (c) 2016-2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_WALLET_RPCWALLET_H
#define BITCOIN_WALLET_RPCWALLET_H

#include <span.h>

#include <any>
#include <memory>
#include <string>
#include <vector>

class CRPCCommand;
class JSONRPCRequest;
class UniValue;
class CTransaction;
struct PartiallySignedTransaction;

namespace wallet {
class CWallet;
class LegacyScriptPubKeyMan;
struct WalletContext;

Span<const CRPCCommand> GetWalletRPCCommands();

/**
 * Figures out what wallet, if any, to use for a JSONRPCRequest.
 *
 * @param[in] request JSONRPCRequest that wishes to access a wallet
 * @return nullptr if no wallet should be used, or a pointer to the CWallet
 */
std::shared_ptr<CWallet> GetWalletForJSONRPCRequest(const JSONRPCRequest& request);

void EnsureWalletIsUnlocked(const CWallet&);
WalletContext& EnsureWalletContext(const std::any& context);
LegacyScriptPubKeyMan& EnsureLegacyScriptPubKeyMan(CWallet& wallet, bool also_create = false);
const LegacyScriptPubKeyMan& EnsureConstLegacyScriptPubKeyMan(const CWallet& wallet);

RPCHelpMan getaddressinfo();
RPCHelpMan signrawtransactionwithwallet();
<<<<<<< HEAD
#endif // BITCOIN_WALLET_RPCWALLET_H
||||||| parent of 8c85a48eac8 (Add src/wallet/* code to wallet:: namespace)
#endif //BITCOIN_WALLET_RPCWALLET_H
=======
}  // namespace wallet
#endif //BITCOIN_WALLET_RPCWALLET_H
>>>>>>> 8c85a48eac8 (Add src/wallet/* code to wallet:: namespace)
