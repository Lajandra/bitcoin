// Copyright (c) 2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_IPC_CAPNP_NODE_TYPES_H
#define BITCOIN_IPC_CAPNP_NODE_TYPES_H

#include <ipc/capnp/common.capnp.proxy-types.h>
#include <ipc/capnp/context.h>
#include <ipc/capnp/node.capnp.proxy.h>
#include <ipc/capnp/wallet.capnp.proxy-types.h>

class CNodeStats;
struct CNodeStateStats;

//! Specialization of rpcSetTimerInterfaceIfUnset needed because it takes a
//! RPCTimerInterface* argument, which requires custom code to provide a
//! compatible timer.
template <>
struct mp::ProxyServerMethodTraits<ipc::capnp::messages::Node::RpcSetTimerInterfaceIfUnsetParams>
{
    using Context = ServerContext<ipc::capnp::messages::Node,
                                  ipc::capnp::messages::Node::RpcSetTimerInterfaceIfUnsetParams,
                                  ipc::capnp::messages::Node::RpcSetTimerInterfaceIfUnsetResults>;
    static void invoke(Context& context);
};

//! Specialization of rpcUnsetTimerInterface needed because it takes a
//! RPCTimerInterface* argument, which requires custom code to provide a
//! compatible timer.
template <>
struct mp::ProxyServerMethodTraits<ipc::capnp::messages::Node::RpcUnsetTimerInterfaceParams>
{
    using Context = ServerContext<ipc::capnp::messages::Node,
                                  ipc::capnp::messages::Node::RpcUnsetTimerInterfaceParams,
                                  ipc::capnp::messages::Node::RpcUnsetTimerInterfaceResults>;
    static void invoke(Context& context);
};

namespace mp {
template <typename Accessor, typename ServerContext, typename Fn, typename... Args>
void CustomPassField(TypeList<int, const char* const*>, ServerContext& server_context, const Fn& fn, Args&&... args)
{
    const auto& params = server_context.call_context.getParams();
    const auto& value = Accessor::get(params);
    std::vector<const char*> argv(value.size());
    size_t i = 0;
    for (const auto arg : value) {
        argv[i] = arg.cStr();
        ++i;
    }
    return fn.invoke(server_context, std::forward<Args>(args)..., argv.size(), argv.data());
}

template <typename Output>
void CustomBuildField(TypeList<int, const char* const*>,
                      Priority<1>,
                      InvokeContext& invoke_context,
                      int argc,
                      const char* const* argv,
                      Output&& output)
{
    auto args = output.init(argc);
    for (int i = 0; i < argc; ++i) {
        args.set(i, argv[i]);
    }
}

template <typename InvokeContext>
static inline ::capnp::Void BuildPrimitive(InvokeContext& invoke_context, RPCTimerInterface*, TypeList<::capnp::Void>)
{
    return {};
}

//! RPCTimerInterface* server-side argument handling. Skips argument so it can
//! be handled by ProxyServerCustom code.
template <typename Accessor, typename ServerContext, typename Fn, typename... Args>
void CustomPassField(TypeList<RPCTimerInterface*>, ServerContext& server_context, const Fn& fn, Args&&... args)
{
    fn.invoke(server_context, std::forward<Args>(args)...);
}

template <typename Value, typename Output>
void CustomBuildField(TypeList<std::tuple<CNodeStats, bool, CNodeStateStats>>,
                      Priority<1>,
                      InvokeContext& invoke_context,
                      Value&& stats,
                      Output&& output)
{
    // FIXME should pass message_builder instead of builder below to avoid
    // calling output.set twice Need ValueBuilder class analogous to
    // ValueReader for this
    BuildField(TypeList<CNodeStats>(), invoke_context, output, std::get<0>(stats));
    if (std::get<1>(stats)) {
        auto message_builder = output.init();
        using Accessor = ProxyStruct<ipc::capnp::messages::NodeStats>::StateStatsAccessor;
        StructField<Accessor, ipc::capnp::messages::NodeStats::Builder> field_output{message_builder};
        BuildField(TypeList<CNodeStateStats>(), invoke_context, field_output, std::get<2>(stats));
    }
}

void CustomReadMessage(InvokeContext& invoke_context,
                       ipc::capnp::messages::NodeStats::Reader const& reader,
                       std::tuple<CNodeStats, bool, CNodeStateStats>& node_stats);

template <typename Value, typename Output>
void CustomBuildField(TypeList<CSubNet>, Priority<1>, InvokeContext& invoke_context, Value&& subnet, Output&& output)
{
    std::string subnet_str = subnet.ToString();
    auto result = output.init(subnet_str.size());
    memcpy(result.begin(), subnet_str.data(), subnet_str.size());
}
} // namespace mp

#endif // BITCOIN_IPC_CAPNP_NODE_TYPES_H
