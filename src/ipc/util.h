#ifndef BITCOIN_IPC_UTIL_H
#define BITCOIN_IPC_UTIL_H

#include <memory>

namespace ipc {

//! Substitute for for C++14 std::make_unique.
template <typename T, typename... Args>
std::unique_ptr<T> MakeUnique(Args&&... args)
{
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

} // namespace ipc

#endif // BITCOIN_IPC_UTIL_H
