// Copyright (c) 2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_RPC_SERVER_UTIL_H
#define BITCOIN_RPC_SERVER_UTIL_H

#include <any>

class ArgsManager;
class CBlockPolicyEstimator;
class CConnman;
class CTxMemPool;
<<<<<<< HEAD
class ChainstateManager;
||||||| parent of 56c37b37299 (Add src/node/* code to node:: namespace)
struct NodeContext;
=======
>>>>>>> 56c37b37299 (Add src/node/* code to node:: namespace)
class PeerManager;
<<<<<<< HEAD
struct NodeContext;
||||||| parent of 56c37b37299 (Add src/node/* code to node:: namespace)
=======
namespace node {
struct NodeContext;
} // namespace node
>>>>>>> 56c37b37299 (Add src/node/* code to node:: namespace)

node::NodeContext& EnsureAnyNodeContext(const std::any& context);
CTxMemPool& EnsureMemPool(const node::NodeContext& node);
CTxMemPool& EnsureAnyMemPool(const std::any& context);
<<<<<<< HEAD
ArgsManager& EnsureArgsman(const NodeContext& node);
ArgsManager& EnsureAnyArgsman(const std::any& context);
ChainstateManager& EnsureChainman(const NodeContext& node);
||||||| parent of 56c37b37299 (Add src/node/* code to node:: namespace)
ChainstateManager& EnsureChainman(const NodeContext& node);
=======
ChainstateManager& EnsureChainman(const node::NodeContext& node);
>>>>>>> 56c37b37299 (Add src/node/* code to node:: namespace)
ChainstateManager& EnsureAnyChainman(const std::any& context);
CBlockPolicyEstimator& EnsureFeeEstimator(const node::NodeContext& node);
CBlockPolicyEstimator& EnsureAnyFeeEstimator(const std::any& context);
CConnman& EnsureConnman(const node::NodeContext& node);
PeerManager& EnsurePeerman(const node::NodeContext& node);

#endif // BITCOIN_RPC_SERVER_UTIL_H
