// SPDX-License-Identifier: GPL-3.0-only

#include "nsx/domain/motd/motd_service.hpp"

#include <nlohmann/json.hpp>

#include "nsx/core/version/semver.hpp"

namespace nsx::domain::motd {

MotdService::MotdService(std::string currentAppVersion) : m_appVersion(std::move(currentAppVersion))
{
}

bool MotdService::parseAndCache(std::string_view jsonManifest)
{
    const auto doc = nlohmann::json::parse(jsonManifest, nullptr, false);
    if (doc.is_discarded() || !doc.is_object()) {
        return false;
    }

    if (!doc.contains("motd") || !doc["motd"].is_object()) {
        m_cachedBulletin = std::nullopt;
        return false;
    }

    const auto& motdJson = doc["motd"];
    if (!motdJson.contains("id") || !motdJson.contains("title") || !motdJson.contains("message")) {
        return false;
    }

    MessageOfTheDay bulletin;
    bulletin.id = motdJson["id"].get<std::string>();
    bulletin.title = motdJson["title"].get<std::string>();
    bulletin.message = motdJson["message"].get<std::string>();

    if (motdJson.contains("severity") && motdJson["severity"].is_string()) {
        bulletin.severity = motdJson["severity"].get<std::string>();
    }

    if (motdJson.contains("min_app_version") && motdJson["min_app_version"].is_string()) {
        bulletin.minAppVersion = motdJson["min_app_version"].get<std::string>();
        if (!m_appVersion.empty()) {
            const auto appSem = core::parseSemVer(m_appVersion);
            const auto minSem = core::parseSemVer(bulletin.minAppVersion);
            if (appSem.hasValue() && minSem.hasValue() &&
                core::compare(appSem.value(), minSem.value()) < 0) {
                // Application version is below required minimum, suppress bulletin
                return false;
            }
        }
    }

    m_cachedBulletin = std::move(bulletin);
    return true;
}

std::optional<MessageOfTheDay> MotdService::currentBulletin() const
{
    if (!m_cachedBulletin) {
        return std::nullopt;
    }
    if (isDismissed(m_cachedBulletin->id)) {
        return std::nullopt;
    }
    return m_cachedBulletin;
}

bool MotdService::isDismissed(std::string_view bulletinId) const
{
    return m_dismissedIds.find(std::string(bulletinId)) != m_dismissedIds.end();
}

void MotdService::dismiss(std::string_view bulletinId)
{
    m_dismissedIds.insert(std::string(bulletinId));
}

void MotdService::resetDismissals()
{
    m_dismissedIds.clear();
}

}  // namespace nsx::domain::motd
