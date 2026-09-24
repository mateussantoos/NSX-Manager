// SPDX-License-Identifier: GPL-3.0-only

#include "nsx/core/firmware/firmware_set.hpp"

#include <algorithm>

namespace nsx::core {

namespace detail {
namespace {

bool endsWithNoCase(std::string_view text, std::string_view suffix)
{
    if (text.size() < suffix.size()) {
        return false;
    }
    const std::size_t offset = text.size() - suffix.size();
    for (std::size_t i = 0; i < suffix.size(); ++i) {
        const char a = text[offset + i];
        const char b = suffix[i];
        const char lowerA = (a >= 'A' && a <= 'Z') ? static_cast<char>(a + ('a' - 'A')) : a;
        if (lowerA != b) {
            return false;
        }
    }
    return true;
}

/// The last path component.
std::string_view baseName(std::string_view path)
{
    const std::string_view::size_type slash = path.find_last_of('/');
    return slash == std::string_view::npos ? path : path.substr(slash + 1);
}

}  // namespace
}  // namespace detail

bool isFirmwareContent(std::string_view name)
{
    constexpr std::string_view kSuffix = ".nca";

    // A name that is ONLY the suffix has no stem, so it is not a file this
    // application produced - firmware NCAs are named for their title id. It is
    // also a dotfile, which is the kind of thing a user puts somewhere
    // deliberately. Treating it as unrecognised means the install stops and
    // names it rather than deleting it, which is the direction to err in for
    // a function whose answer decides what gets removed.
    if (name.size() <= kSuffix.size()) {
        return false;
    }

    // Case-insensitive because the file system is: a set unpacked on a PC may
    // carry .NCA, and refusing to recognise it would make this application
    // refuse to clear a directory it filled itself.
    return detail::endsWithNoCase(name, kSuffix);
}

FirmwareDirectoryScan scanFirmwareDirectory(const std::vector<std::string>& names)
{
    FirmwareDirectoryScan out;

    for (const std::string& relative : names) {
        if (isFirmwareContent(detail::baseName(relative))) {
            ++out.firmwareFiles;
        }
        else {
            out.unexpected.push_back(relative);
        }
    }

    std::sort(out.unexpected.begin(), out.unexpected.end());
    return out;
}

bool looksLikeFirmware(const std::vector<std::string>& names)
{
    const FirmwareDirectoryScan scan = scanFirmwareDirectory(names);
    return scan.firmwareFiles > 0 && scan.onlyFirmware();
}

}  // namespace nsx::core
