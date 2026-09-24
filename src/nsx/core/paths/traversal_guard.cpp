// SPDX-License-Identifier: GPL-3.0-only

#include "nsx/core/paths/traversal_guard.hpp"

#include <vector>

namespace nsx::core {

namespace detail {
namespace {

/// Split on '/', keeping empty pieces so `a//b` is visible as an empty
/// component rather than silently collapsing to `a/b`.
std::vector<std::string_view> split(std::string_view path)
{
    std::vector<std::string_view> out;
    std::string_view::size_type start = 0;

    while (true) {
        const std::string_view::size_type slash = path.find('/', start);
        if (slash == std::string_view::npos) {
            out.push_back(path.substr(start));
            break;
        }
        out.push_back(path.substr(start, slash - start));
        start = slash + 1;
    }
    return out;
}

bool isControl(char c)
{
    const auto u = static_cast<unsigned char>(c);
    return u < 0x20 || u == 0x7F;
}

std::string_view stripTrailingSlashes(std::string_view path)
{
    while (path.size() > 1 && path.back() == '/') {
        path.remove_suffix(1);
    }
    return path;
}

}  // namespace
}  // namespace detail

std::string_view describe(PathError error)
{
    switch (error) {
        case PathError::Empty:
            return "the path is empty";
        case PathError::AbsolutePath:
            return "the path is absolute and would ignore the destination";
        case PathError::VolumePrefix:
            return "the path contains a volume prefix";
        case PathError::ParentTraversal:
            return "the path contains a parent reference";
        case PathError::BackslashSeparator:
            return "the path contains a backslash";
        case PathError::NullByte:
            return "the path contains a null byte";
        case PathError::ControlCharacter:
            return "the path contains a control character";
        case PathError::EmptyComponent:
            return "the path contains an empty component";
        case PathError::DotComponent:
            return "the path contains a '.' component";
        case PathError::TrailingDotOrSpace:
            return "a path component ends in a dot or a space";
        case PathError::ComponentTooLong:
            return "a path component is too long";
        case PathError::PathTooLong:
            return "the path is too long";
    }
    return "unknown path error";
}

Result<std::monostate, PathError> validateEntryName(std::string_view entry)
{
    using R = Result<std::monostate, PathError>;

    if (entry.empty()) {
        return R::err(PathError::Empty);
    }

    // Character-level checks first, over the WHOLE string. A NUL or a backslash
    // anywhere makes the split below meaningless, so nothing may be concluded
    // from component structure until these have passed.
    for (const char c : entry) {
        if (c == '\0') {
            return R::err(PathError::NullByte);
        }
        if (c == '\\') {
            return R::err(PathError::BackslashSeparator);
        }
        if (c == ':') {
            return R::err(PathError::VolumePrefix);
        }
        if (detail::isControl(c)) {
            return R::err(PathError::ControlCharacter);
        }
    }

    if (entry.front() == '/') {
        return R::err(PathError::AbsolutePath);
    }
    if (entry.size() > kMaxPathLength) {
        return R::err(PathError::PathTooLong);
    }

    // A directory entry legitimately ends in '/'. Drop exactly one trailing
    // separator before splitting, so `a/b/` does not read as an empty final
    // component, and `a//` still does.
    std::string_view body = entry;
    if (body.back() == '/') {
        body.remove_suffix(1);
    }
    if (body.empty()) {
        return R::err(PathError::Empty);
    }

    for (const std::string_view component : detail::split(body)) {
        if (component.empty()) {
            return R::err(PathError::EmptyComponent);
        }
        if (component == ".") {
            return R::err(PathError::DotComponent);
        }
        if (component == "..") {
            return R::err(PathError::ParentTraversal);
        }
        if (component.size() > kMaxComponentLength) {
            return R::err(PathError::ComponentTooLong);
        }
        // FAT strips a trailing dot or space, so `foo.` and `foo` become the
        // same file. An archive carrying both writes one over the other while
        // every name comparison says they differ.
        if (component.back() == '.' || component.back() == ' ') {
            return R::err(PathError::TrailingDotOrSpace);
        }
    }

    return R::ok(std::monostate{});
}

Result<std::string, PathError> resolveUnder(std::string_view root, std::string_view entry)
{
    using R = Result<std::string, PathError>;

    if (root.empty()) {
        return R::err(PathError::Empty);
    }

    const Result<std::monostate, PathError> valid = validateEntryName(entry);
    if (!valid.hasValue()) {
        return R::err(valid.error());
    }

    const std::string_view base = detail::stripTrailingSlashes(root);

    std::string_view body = entry;
    const bool directory = body.back() == '/';
    if (directory) {
        body.remove_suffix(1);
    }

    std::string out;
    out.reserve(base.size() + 1 + body.size() + (directory ? 1 : 0));
    out.append(base);
    if (out.back() != '/') {
        out.push_back('/');
    }
    out.append(body);
    if (directory) {
        out.push_back('/');
    }

    if (out.size() > kMaxPathLength) {
        return R::err(PathError::PathTooLong);
    }

    return R::ok(std::move(out));
}

bool isWithin(std::string_view root, std::string_view path)
{
    if (root.empty() || path.empty()) {
        return false;
    }

    const std::vector<std::string_view> rootParts =
        detail::split(detail::stripTrailingSlashes(root));
    const std::vector<std::string_view> pathParts =
        detail::split(detail::stripTrailingSlashes(path));

    if (pathParts.size() < rootParts.size()) {
        return false;
    }

    // Component by component. A string prefix test would call /switch/nsx-evil
    // a child of /switch/nsx.
    for (std::size_t i = 0; i < rootParts.size(); ++i) {
        if (rootParts[i] != pathParts[i]) {
            return false;
        }
    }

    // Anything below must not re-introduce a parent reference: a caller may
    // hand us a path this module did not build.
    for (std::size_t i = rootParts.size(); i < pathParts.size(); ++i) {
        if (pathParts[i] == ".." || pathParts[i].empty()) {
            return false;
        }
    }

    return true;
}

}  // namespace nsx::core
