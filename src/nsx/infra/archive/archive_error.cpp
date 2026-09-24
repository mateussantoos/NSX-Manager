// SPDX-License-Identifier: GPL-3.0-only

#include "nsx/infra/archive/archive_error.hpp"

namespace nsx::infra {

std::string_view describe(ArchiveError error)
{
    switch (error) {
        case ArchiveError::CannotOpen:
            return "the archive could not be opened";
        case ArchiveError::Empty:
            return "the archive is empty";
        case ArchiveError::UnsafeEntry:
            return "the archive contains an entry that would write outside the destination";
        case ArchiveError::TooLarge:
            return "the archive expands to more than the allowed size";
        case ArchiveError::DirectoryFailed:
            return "a destination directory could not be created";
        case ArchiveError::WriteFailed:
            return "a file could not be written";
        case ArchiveError::ReadFailed:
            return "an entry could not be decompressed";
        case ArchiveError::Aborted:
            return "cancelled";
    }
    return "unknown archive error";
}

}  // namespace nsx::infra
