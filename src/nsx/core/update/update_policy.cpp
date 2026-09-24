// SPDX-License-Identifier: GPL-3.0-only

#include "nsx/core/update/update_policy.hpp"

namespace nsx::core {

UpdateDecision decideUpdate(const SemVer& installed, const UpdateManifest& manifest,
                            Channel channel)
{
    UpdateDecision out;
    out.target = manifest.version;

    // 1. Channel first. A stable client must not even consider a beta manifest,
    //    regardless of how much newer it is.
    if (manifest.channel != channel) {
        out.action = UpdateAction::WrongChannel;
        out.target.reset();
        out.reason = "manifest is for a different release channel";
        return out;
    }

    // 2. min_supported. Set above an older version when the SD layout or config
    //    format changed in a way the in-app path cannot bridge - the user is
    //    sent to the zip rather than walked into a broken update.
    if (compare(installed, manifest.minSupported) < 0) {
        out.action = UpdateAction::UnsupportedPath;
        out.reason = "installed " + installed.toString() + " is older than the minimum " +
                     manifest.minSupported.toString() + " this release can update from";
        return out;
    }

    // 3. Is it actually newer? Full SemVer precedence, so a prerelease never
    //    outranks its release and a two-digit minor does not wrap.
    if (!isNewerThan(manifest.version, installed)) {
        out.action = UpdateAction::UpToDate;
        out.target.reset();
        out.reason = "installed " + installed.toString() + " is current";
        return out;
    }

    // 4. Mandatory is a per-release publisher decision, not a compile-time one.
    out.action = manifest.mandatory ? UpdateAction::Mandatory : UpdateAction::Optional;
    out.reason =
        manifest.version.toString() + " is available (installed " + installed.toString() + ")";
    return out;
}

UpdateDecision unreadableManifest(std::string_view why)
{
    UpdateDecision out;
    out.action = UpdateAction::ManifestUnreadable;
    out.reason = std::string(why);
    return out;
}

std::string_view describe(UpdateAction action)
{
    switch (action) {
        case UpdateAction::UpToDate:
            return "up to date";
        case UpdateAction::Optional:
            return "an update is available";
        case UpdateAction::Mandatory:
            return "a required update is available";
        case UpdateAction::UnsupportedPath:
            return "too old to update in-app";
        case UpdateAction::WrongChannel:
            return "manifest is for another channel";
        case UpdateAction::ManifestUnreadable:
            return "update check unavailable";
    }
    return "unknown";
}

}  // namespace nsx::core
