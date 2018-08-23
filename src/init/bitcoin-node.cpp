// Copyright (c) 2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>
#include <init.h>
#include <interfaces/chain.h>
#include <interfaces/echo.h>
#include <interfaces/init.h>
#include <interfaces/ipc.h>
#include <interfaces/node.h>
#include <ipc/context.h>
#include <node/context.h>
#include <util/check.h>
#include <util/memory.h>
#include <util/system.h>

#include <functional>
#include <memory>
#include <string>
#include <utility>

namespace ipc {
namespace capnp {
void SetupNodeServer(ipc::Context& context);
std::string GlobalArgsNetwork();
} // namespace capnp
} // namespace ipc

namespace init {
namespace {
const char* EXE_NAME = "bitcoin-node";

class BitcoinNodeInit : public interfaces::Init
{
public:
    BitcoinNodeInit(NodeContext& node, int argc, char* argv[])
        : m_node(node),
          m_ipc(interfaces::MakeIpc(argc, argv, EXE_NAME, *this, /* can_connect= */ false, /* can_listen= */ true))
    {
        m_node.args = &gArgs;
        m_node.init = this;
        m_ipc->context().init_process = [this] {
            // TODO in future PR: Refactor bitcoin startup code, dedup this with AppInit.
            SelectParams(ipc::capnp::GlobalArgsNetwork());
            InitLogging(*Assert(m_node.args));
            InitParameterInteraction(*Assert(m_node.args));
        };
        ipc::capnp::SetupNodeServer(m_ipc->context());
    }
    std::unique_ptr<interfaces::Echo> makeEcho() override { return interfaces::MakeEcho(); }
    std::unique_ptr<interfaces::Node> makeNode() override { return interfaces::MakeNode(m_node); }
    std::unique_ptr<interfaces::Chain> makeChain() override { return interfaces::MakeChain(m_node); }
    interfaces::Ipc* ipc() override { return m_ipc.get(); }
    NodeContext& m_node;
    std::unique_ptr<interfaces::Ipc> m_ipc;
};
} // namespace
} // namespace init

namespace interfaces {
std::unique_ptr<Init> MakeNodeInit(NodeContext& node, int argc, char* argv[], int& exit_status)
{
    auto init = MakeUnique<init::BitcoinNodeInit>(node, argc, argv);
    // Check if bitcoin-node is being invoked as an IPC server. If so, then
    // bypass normal execution and just respond to requests over the IPC
    // channel and return null.
    if (init->m_ipc->serveProcess(init::EXE_NAME, argc, argv, exit_status)) {
        return nullptr;
    }
    return init;
}
} // namespace interfaces
