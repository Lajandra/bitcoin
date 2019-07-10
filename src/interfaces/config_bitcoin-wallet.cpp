#include <interfaces/config.h>

#include <interfaces/capnp/ipc.h>
#include <interfaces/wallet.h>

namespace interfaces {

const Config g_config = {
    "bitcoin-wallet" /* exe_name */,
    ".wallet" /* log_suffix */,
    &MakeIpcProcess,
    &capnp::MakeCapnpProtocol,
    nullptr /* make_node */,
    &MakeWalletClient,
    nullptr /* make_node_client */,
    nullptr /* make_node_server */,
};

} // namespace interfaces
