// SPDX-License-Identifier: GPL-3.0-only

#include "nsx/domain/selfupdate/sd_file_store.hpp"

#include <cstdio>
#include <vector>

#include <sys/stat.h>
#include <sys/types.h>

#if __has_include(<sys/statvfs.h>)
#define NSX_HAVE_STATVFS 1
#include <sys/statvfs.h>
#else
#define NSX_HAVE_STATVFS 0
#endif

namespace nsx::domain {

namespace {

/// Large enough that an NRO copy is a few hundred reads, small enough that the
/// homebrew heap does not notice.
constexpr std::size_t kIoChunk = 64 * 1024;

}  // namespace

bool SdFileStore::exists(const std::string& path) const
{
    FILE* f = std::fopen(path.c_str(), "rb");
    if (f == nullptr) {
        return false;
    }
    std::fclose(f);
    return true;
}

std::optional<std::string> SdFileStore::readText(const std::string& path,
                                                 std::uint64_t maxBytes) const
{
    FILE* f = std::fopen(path.c_str(), "rb");
    if (f == nullptr) {
        return std::nullopt;
    }

    std::string out;
    std::vector<char> buf(kIoChunk);
    bool tooLarge = false;

    while (const std::size_t n = std::fread(buf.data(), 1, buf.size(), f)) {
        if (out.size() + n > maxBytes) {
            tooLarge = true;
            break;
        }
        out.append(buf.data(), n);
    }

    const bool ok = !tooLarge && std::ferror(f) == 0;
    std::fclose(f);

    if (!ok) {
        return std::nullopt;
    }
    return out;
}

bool SdFileStore::writeAtomic(const std::string& path, std::string_view data)
{
    const std::string tmp = path + ".tmp";

    FILE* f = std::fopen(tmp.c_str(), "wb");
    if (f == nullptr) {
        return false;
    }
    const bool written = data.empty() || std::fwrite(data.data(), 1, data.size(), f) == data.size();
    const bool flushed = std::fflush(f) == 0;
    std::fclose(f);

    if (!written || !flushed) {
        std::remove(tmp.c_str());
        return false;
    }

    // FatFs cannot rename onto an existing file.
    std::remove(path.c_str());
    if (std::rename(tmp.c_str(), path.c_str()) != 0) {
        std::remove(tmp.c_str());
        return false;
    }
    return true;
}

bool SdFileStore::remove(const std::string& path)
{
    std::remove(path.c_str());
    return !exists(path);
}

bool SdFileStore::makeDirectories(const std::string& path)
{
    if (path.empty()) {
        return false;
    }

    // Create each component in turn. mkdir on an existing directory fails with
    // EEXIST, which is success for our purposes, so the outcome is judged by
    // whether the final directory is there rather than by any return code.
    std::string partial;
    partial.reserve(path.size());

    for (std::string::size_type i = 0; i < path.size(); ++i) {
        partial.push_back(path[i]);
        const bool atSeparator = path[i] == '/' && i > 0;
        const bool atEnd = i + 1 == path.size();
        if (!atSeparator && !atEnd) {
            continue;
        }

        std::string component = partial;
        if (atSeparator) {
            component.pop_back();
        }
        if (component.empty() || component.back() == ':') {
            continue;  // volume root, e.g. "sdmc:"
        }
        (void)::mkdir(component.c_str(), 0777);
    }

    struct stat st
    {
    };

    return ::stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

bool SdFileStore::copyFile(const std::string& from, const std::string& to)
{
    FILE* in = std::fopen(from.c_str(), "rb");
    if (in == nullptr) {
        return false;
    }
    FILE* out = std::fopen(to.c_str(), "wb");
    if (out == nullptr) {
        std::fclose(in);
        return false;
    }

    std::vector<char> buf(kIoChunk);
    bool ok = true;
    while (true) {
        const std::size_t n = std::fread(buf.data(), 1, buf.size(), in);
        if (n == 0) {
            ok = std::ferror(in) == 0;
            break;
        }
        if (std::fwrite(buf.data(), 1, n, out) != n) {
            ok = false;
            break;
        }
    }

    ok = ok && (std::fflush(out) == 0);
    std::fclose(in);
    std::fclose(out);

    if (!ok) {
        std::remove(to.c_str());
    }
    return ok;
}

std::optional<core::Sha256::Digest> SdFileStore::digestOf(const std::string& path) const
{
    FILE* f = std::fopen(path.c_str(), "rb");
    if (f == nullptr) {
        return std::nullopt;
    }

    core::Sha256 hasher;
    std::vector<char> buf(kIoChunk);
    while (const std::size_t n = std::fread(buf.data(), 1, buf.size(), f)) {
        hasher.update(buf.data(), n);
    }
    const bool ok = std::ferror(f) == 0;
    std::fclose(f);

    if (!ok) {
        return std::nullopt;
    }
    return hasher.finish();
}

std::optional<std::uint64_t> SdFileStore::freeSpaceBytes(const std::string& dir) const
{
#if NSX_HAVE_STATVFS
    struct statvfs st
    {
    };

    if (::statvfs(dir.c_str(), &st) != 0) {
        return std::nullopt;
    }
    // f_frsize is the fragment size and is what f_bavail counts in; it is zero
    // on some implementations, in which case f_bsize is the intended value.
    const std::uint64_t unit = st.f_frsize != 0 ? static_cast<std::uint64_t>(st.f_frsize)
                                                : static_cast<std::uint64_t>(st.f_bsize);
    if (unit == 0) {
        return std::nullopt;
    }
    return unit * static_cast<std::uint64_t>(st.f_bavail);
#else
    (void)dir;
    // Unknown, never zero: the pre-flight treats this as "could not measure"
    // and proceeds rather than refusing an update it has no evidence against.
    return std::nullopt;
#endif
}

}  // namespace nsx::domain
