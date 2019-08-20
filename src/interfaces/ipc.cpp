#include <interfaces/ipc.h>

#include <errno.h>
#include <fs.h>
#include <interfaces/init.h>
#include <logging.h>
#include <util/memory.h>
#include <util/strencodings.h>

#include <iostream>
#include <mp/util.h>
#include <netdb.h>
#include <sstream>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <tinyformat.h>
#include <util/memory.h>

namespace interfaces {
namespace {
class IpcProcessImpl : public IpcProcess
{
public:
    IpcProcessImpl(int argc, char* argv[], const char* exe_name, IpcProtocol& protocol)
        : m_argc(argc), m_argv(argv), m_exe_name(exe_name), m_protocol(protocol)
    {
    }
    int spawn(const std::string& new_exe_name, int& pid) override
    {
        return mp::SpawnProcess(pid, [&](int fd) {
            fs::path path = m_argc > 0 ? m_argv[0] : "";
            path.remove_filename();
            path.append(new_exe_name);
            return std::vector<std::string>{path.string(), "-ipcfd", std::to_string(fd)};
        });
    }
    int wait(int pid) override { return mp::WaitProcess(pid); }
    bool serve(int& exit_status) override
    {
        if (m_argc != 3 || strcmp(m_argv[1], "-ipcfd") != 0) {
            return false;
        }
        try {
            int32_t fd = -1;
            ParseInt32(m_argv[2], &fd);
            m_protocol.serve(fd);
            exit_status = EXIT_SUCCESS;
        } catch (std::exception& e) {
            std::cerr << e.what() << std::endl;
            exit_status = EXIT_FAILURE;
        }
        return true;
    }
    int connect(const fs::path& data_dir, const std::string& dest_exe_name, std::string& address) override;
    int bind(const fs::path& data_dir, std::string& address) override;
    int m_argc;
    char** m_argv;
    const char* m_exe_name;
    IpcProtocol& m_protocol;
};

bool ParseAddress(std::string& address,
    const fs::path& data_dir,
    const std::string& dest_exe_name,
    struct sockaddr_un& addr)
{
    if (address.compare(0, 4, "unix") == 0 && (address.size() == 4 || address[4] == ':')) {
        fs::path socket_dir = data_dir / "sockets";
        fs::path path;
        if (address.size() <= 5) {
            path = socket_dir / strprintf("%s.sock", dest_exe_name);
        } else {
            path = fs::absolute(address.substr(5), socket_dir);
        }
        std::string path_str = path.string();
        if (path_str.size() < sizeof(addr.sun_path) - 1) {
            memset(&addr, 0, sizeof(addr));
            addr.sun_family = AF_UNIX;
            strncpy(addr.sun_path, path.string().c_str(), sizeof(addr.sun_path));
            address = strprintf("unix:%s", path_str);
            return true;
        }
    }

    return false;
}

int IpcProcessImpl::connect(const fs::path& data_dir, const std::string& dest_exe_name, std::string& address)
{
    struct sockaddr_un addr;
    if (!ParseAddress(address, data_dir, dest_exe_name, addr)) {
        LogPrintf("Error: Invalid IPC connect address '%s'\n", address);
        return -1;
    }

    int fd;
    if ((fd = ::socket(addr.sun_family, SOCK_STREAM, 0)) == -1) {
        throw std::system_error(errno, std::system_category());
    }
    if (::connect(fd, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
        return fd;
    }
    int connect_error = errno;
    if (::close(fd) != 0) {
        LogPrintf("Error closing file descriptor %i: %s\n", fd, strerror(errno));
    }
    throw std::system_error(connect_error, std::system_category());
}

int IpcProcessImpl::bind(const fs::path& data_dir, std::string& address)
{
    struct sockaddr_un addr;
    if (!ParseAddress(address, data_dir, m_exe_name, addr)) {
        LogPrintf("Error: Invalid IPC bind address '%s'\n", address);
        return -1;
    }

    if (addr.sun_family == AF_UNIX) {
        fs::path path = addr.sun_path;
        fs::create_directories(path.parent_path());
        if (fs::symlink_status(path).type() == fs::socket_file) {
            fs::remove(path);
        }
    }

    int fd;
    if ((fd = ::socket(addr.sun_family, SOCK_STREAM, 0)) == -1) {
        throw std::system_error(errno, std::system_category());
    }

    if (::bind(fd, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
        return fd;
    }
    int bind_error = errno;
    if (::close(fd) != 0) {
        LogPrintf("Error closing file descriptor %i: %s\n", fd, strerror(errno));
    }
    throw std::system_error(bind_error, std::system_category());
}

} // namespace

std::unique_ptr<IpcProcess> MakeIpcProcess(int argc, char* argv[], const char* exe_name, IpcProtocol& protocol)
{
    return MakeUnique<IpcProcessImpl>(argc, argv, exe_name, protocol);
}
} // namespace interfaces
