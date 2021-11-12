// Copyright (c) 2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_RPC_NET_H
#define BITCOIN_RPC_NET_H

class CConnman;
class PeerManager;
namespace node {
struct NodeContext;
}  // namespace node

CConnman& EnsureConnman(const node::NodeContext& node);
PeerManager& EnsurePeerman(const node::NodeContext& node);

#endif // BITCOIN_RPC_NET_H
