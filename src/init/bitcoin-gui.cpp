// Copyright (c) 2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <interfaces/init.h>
#include <interfaces/ipc.h>

#include <memory>

namespace ipc {
namespace capnp {
void SetupNodeClient(ipc::Context& context);
} // namespace capnp
} // namespace ipc

namespace init {
namespace {
const char* EXE_NAME = "bitcoin-gui";

class BitcoinGuiInit : public interfaces::Init
{
public:
    BitcoinGuiInit(const char* arg0) : m_ipc(interfaces::MakeIpc(EXE_NAME, ".gui", arg0, *this))
    {
<<<<<<< HEAD
        m_node.args = &gArgs;
        m_node.init = this;
    }
    std::unique_ptr<interfaces::Node> makeNode() override { return interfaces::MakeNode(m_node); }
    std::unique_ptr<interfaces::Chain> makeChain() override { return interfaces::MakeChain(m_node); }
    std::unique_ptr<interfaces::WalletLoader> makeWalletLoader(interfaces::Chain& chain) override
    {
        return MakeWalletLoader(chain, *Assert(m_node.args));
||||||| parent of 4f4a927b801 (Make bitcoin-gui spawn a bitcoin-node process)
        m_node.args = &gArgs;
        m_node.init = this;
    }
    std::unique_ptr<interfaces::Node> makeNode() override { return interfaces::MakeNode(m_node); }
    std::unique_ptr<interfaces::Chain> makeChain() override { return interfaces::MakeChain(m_node); }
    std::unique_ptr<interfaces::WalletClient> makeWalletClient(interfaces::Chain& chain) override
    {
        return MakeWalletClient(chain, *Assert(m_node.args));
=======
        ipc::capnp::SetupNodeClient(m_ipc->context());
>>>>>>> 4f4a927b801 (Make bitcoin-gui spawn a bitcoin-node process)
    }
    interfaces::Ipc* ipc() override { return m_ipc.get(); }
    std::unique_ptr<interfaces::Ipc> m_ipc;
};
} // namespace
} // namespace init

namespace interfaces {
std::unique_ptr<Init> MakeGuiInit(int argc, char* argv[])
{
    return std::make_unique<init::BitcoinGuiInit>(argc > 0 ? argv[0] : "");
}
} // namespace interfaces
