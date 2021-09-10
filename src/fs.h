// Copyright (c) 2017-2019 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_FS_H
#define BITCOIN_FS_H

#include <stdio.h>
#include <string>
#if defined WIN32 && defined __GLIBCXX__
#include <ext/stdio_filebuf.h>
#endif

#include <filesystem>
#include <fstream>

/** Filesystem operations and types */
namespace fs {

using namespace std::filesystem;

/**
 * Custom path class which inherits from the standard path class, but disables
 * error-prone std::string conversion functions that we never want to call on
 * windows.
 *
 * The default std::string conversion methods are safe on unix but work badly on
 * windows because they assume that windows paths (which are unicode internally)
 * are supposed to be encoded in the current windows "code page"" encoding when
 * they are stored in std::string. This is never what we want because we do not
 * have any application code that handles strings in the windows code page. We
 * only have code that treats std::strings as 8-bit strings with no particular
 * encoding, or as UTF-8 strings.
 *
 * The PathFromString and PathToString functions below can be used as substitutes
 * for the disabled unsafe methods when generic platform-native strings are
 * required. And the fs::u8path() function and fs::path::u8string() method can
 * be used as substitutes when UTF-8 strings specifically are required.
 */
class path : public std::filesystem::path
{
public:
    using std::filesystem::path::path;
    path(std::filesystem::path path) : std::filesystem::path::path(std::move(path)) {}
    path(const std::string& string) = delete;
    path& operator=(std::string&) = delete;
    std::string string() const = delete;
};

static inline path operator+(path p1, path p2)
{
    p1 += std::move(p2);
    return p1;
}

/**
 * Convert path object to std::string representation capable of representing all
 * paths supported on the platform, being round-tripped, and being passed to
 * POSIX APIs.
 *
 * Generally, this function should not be be used to format paths for display or
 * logging or to convert to JSON. These cases usually require UTF-8
 * reprepresentations, and fs::path::u8string should be used instead. This
 * function should also generally not be used for string manipulation. It
 * generally better to manipulate path strings and avoid narrow<->wide character
 * conversion using fs::path::native().
 *
 * Design notes:
 *
 * - On POSIX, where paths natively are 8-bit strings that assume no particular
 *   encoding, this is equivalent to calling fs::path::string().
 *
 * - On Windows, where paths are natively unicode, this is equivalent to calling
 *   fs::path::u8string().
 *
 * - On POSIX, to support roundtrip fs::path <-> std::string conversion, it is
 *   important that this uses path::string() not path::u8string(), because
 *   path::string() returns the path verbatim, while path::u8string() tries to
 *   return the path as UTF-8 and could mangle it or invoke platform-specific
 *   behavior.
 *
 * - On Windows, to support roundtrip fs::path <--> std::string conversion, it
 *   is important that this uses path::u8string() not path::string(), because
 *   paths on Windows are natively unicode and can be represented in UTF-8 while
 *   path::string() will return different strings depending on the current
 *   windows code page, and can choose an encoding not capable of representing
 *   every file path.
 */
static inline std::string PathToString(const std::filesystem::path& path)
{
#ifdef WIN32
    return path.u8string();
#else
    static_assert(std::is_same<path::string_type, std::string>::value, "PathFromString not implemented on this platform");
    return path.string();
#endif
}

/**
 * Inverse of PathToString above. Can be used to construct fs::path objects from
 * platform-specific path strings coming from command line arguments,
 * environment variables, or POSIX APIs. For UTF-8 strings from unicode sources
 * like JSON data, or Qt string objects, this function should not be used and
 * it's more appropriate to call call fs::u8path() instead.
 */
static inline path PathFromString(const std::string& string)
{
#ifdef WIN32
    return u8path(string);
#else
    return std::filesystem::path(string);
#endif
}
}

/** Bridge operations to C stdio */
namespace fsbridge {
    FILE *fopen(const fs::path& p, const char *mode);

    /**
     * Helper function for joining two paths
     *
     * @param[in] base  Base path
     * @param[in] path  Path to combine with base
     * @returns path unchanged if it is an absolute path, otherwise returns base joined with path. Returns base unchanged if path is empty.
     * @pre  Base path must be absolute
     * @post Returned path will always be absolute
     */
    fs::path AbsPathJoin(const fs::path& base, const fs::path& path);

    class FileLock
    {
    public:
        FileLock() = delete;
        FileLock(const FileLock&) = delete;
        FileLock(FileLock&&) = delete;
        explicit FileLock(const fs::path& file);
        ~FileLock();
        bool TryLock();
        std::string GetReason() { return reason; }

    private:
        std::string reason;
#ifndef WIN32
        int fd = -1;
#else
        void* hFile = (void*)-1; // INVALID_HANDLE_VALUE
#endif
    };

    std::string get_filesystem_error_message(const fs::filesystem_error& e);

    typedef std::ifstream ifstream;
    typedef std::ofstream ofstream;
};

#endif // BITCOIN_FS_H
