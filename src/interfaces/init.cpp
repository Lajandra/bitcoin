// Copyright (c) 2019 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <interfaces/init.h>

#include <interfaces/ipc.h>
#include <interfaces/node.h>
#include <logging.h>
#include <util/memory.h>

namespace interfaces {
namespace {
//! Close hook that encapsulates a function to be called on close.
class CloseFn : public CloseHook
{
public:
    explicit CloseFn(std::function<void()> fn) : m_fn(std::move(fn)) {}
    void onClose(Base& interface) override { m_fn(); }
    std::function<void()> m_fn;
};
} // namespace

LocalInit::LocalInit(const char* exe_name, const char* log_suffix) : m_exe_name(exe_name), m_log_suffix(log_suffix) {}
LocalInit::~LocalInit() {}
NodeContext& LocalInit::node()
{
    throw std::logic_error("Node accessor function called from non-node binary (gui, wallet, or test program)");
}

void SpawnProcess(IpcProcess& process,
    IpcProtocol& protocol,
    const std::string& new_exe_name,
    const MakeClientFn& make_client)
{
    int pid;
    int fd = process.spawn(new_exe_name, pid);
    std::unique_ptr<Init> init = protocol.connect(fd);
    Base& base = make_client(*init);
    base.addCloseHook(MakeUnique<CloseFn>([&process, new_exe_name, pid] {
        int status = process.wait(pid);
        LogPrint(::BCLog::IPC, "%s pid %i exited with status %i\n", new_exe_name, pid, status);
    }));
    base.addCloseHook(MakeUnique<Deleter<std::unique_ptr<Init>>>(std::move(init)));
}
} // namespace interfaces
