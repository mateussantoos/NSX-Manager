// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <variant>

#include "nsx/core/result/result.hpp"

namespace nsx::core {

/// @brief Longest single path component accepted.
/// @details FatFs with long filenames tops out at 255 UTF-16 code units.
inline constexpr std::size_t kMaxComponentLength = 255;

/// @brief Longest whole path accepted, destination root included.
inline constexpr std::size_t kMaxPathLength = 1024;

/// @brief Why a path or an archive entry name was refused.
/// @since 0.3.0
enum class PathError
{
    Empty,               ///< Nothing to validate.
    AbsolutePath,        ///< Begins with a separator; would ignore the destination.
    VolumePrefix,        ///< Contains `:` - `sdmc:/` and `C:` are absolute.
    ParentTraversal,     ///< Contains a `..` component.
    BackslashSeparator,  ///< Contains `\`, which is a separator on some systems.
    NullByte,            ///< Contains NUL, which truncates in every C API.
    ControlCharacter,    ///< Contains a control character.
    EmptyComponent,      ///< Contains `//`.
    DotComponent,        ///< Contains a `.` component.
    TrailingDotOrSpace,  ///< A component ends in `.` or ` `; FAT strips both.
    ComponentTooLong,    ///< A component exceeds @ref kMaxComponentLength.
    PathTooLong          ///< The result exceeds @ref kMaxPathLength.
};

/// @brief A short English description of a refusal.
/// @param error The error to describe.
/// @return A description suitable for a log line.
/// @since 0.3.0
[[nodiscard]] std::string_view describe(PathError error);

/// @brief Whether an archive entry name may be extracted at all.
///
/// @details **Rejects rather than normalises, and that is the whole design.**
///          `a/../b` could be rewritten to `b` and accepted, but then the name
///          an entry carries no longer says where it lands, and every zip-slip
///          vulnerability on record is a bug in exactly that rewriting. An
///          archive has no legitimate need for `..`, `.`, `//`, a leading
///          separator or a volume prefix, so none of them is accepted and there
///          is no normalisation left to get wrong.
///
///          What is refused, and why each one is a real attack on a Switch:
///
///          - a `..` component - the zip-slip primitive itself;
///            `../../atmosphere/x` escapes any destination.
///          - a leading `/` - an absolute path ignores the destination.
///          - `:` anywhere - `sdmc:/atmosphere/x` is absolute **on the
///            console** wherever it appears, because the devoptab resolves the
///            prefix rather than the leading separator.
///          - `\` - a separator on a host and a legal filename character on
///            FatFs, so `..\..\x` traverses on one and is a file on the other.
///          - NUL - `safe.txt` followed by NUL and `../../evil` passes a string
///            check and truncates at the C API.
///          - a trailing `.` or space - FAT strips both, so `foo.` and `foo`
///            are one file: an overwrite primitive that survives a name
///            comparison.
///          - control characters - terminal escape sequences in a name that a
///            log or a UI will render.
///
///          Deliberately **not** refused: Windows reserved device names (`CON`,
///          `NUL`, `LPT1`). FatFs does not reserve them, the target is a
///          console, and refusing them would reject legitimate archive contents
///          to defend a platform this never runs on.
///
/// @param entry The entry name exactly as the archive carries it.
/// @return Nothing when it may be extracted, or the reason it may not.
/// @see docs/architecture/threat-model.md
/// @since 0.3.0
[[nodiscard]] Result<std::monostate, PathError> validateEntryName(std::string_view entry);

/// @brief Join a validated entry name onto a destination root.
///
/// @details Validates the entry first, then joins. The result is guaranteed to
///          satisfy @ref isWithin for the same root - an extractor that uses
///          this function cannot write outside its destination, whatever the
///          archive says.
///
/// @param root Absolute destination directory, without a trailing separator.
/// @param entry The entry name from the archive.
/// @return The absolute path to write to, or why the entry was refused.
/// @since 0.3.0
[[nodiscard]] Result<std::string, PathError> resolveUnder(std::string_view root,
                                                          std::string_view entry);

/// @brief Whether a path lies inside a directory.
///
/// @details Compares **component by component**, never as a string prefix:
///          `/switch/nsx` is a string prefix of `/switch/nsx-evil` while being
///          no parent of it, and a prefix test is how that distinction gets
///          lost.
///
///          Case-sensitive, even though FAT is not. The conservative direction:
///          a case difference reports "not within", which refuses. Refusing
///          cannot be used to escape, since escaping needs `..` and `..` is
///          refused outright.
///
/// @param root The directory.
/// @param path The path to test.
/// @return True when @p path is @p root itself or lies beneath it.
/// @since 0.3.0
[[nodiscard]] bool isWithin(std::string_view root, std::string_view path);

}  // namespace nsx::core
