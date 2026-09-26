// SPDX-License-Identifier: GPL-3.0-only

#include <doctest/doctest.h>

#include "nsx/domain/motd/motd_service.hpp"

TEST_SUITE("domain::motd::motd_service")
{
    TEST_CASE("MotdService parses, caches, and dismisses bulletins")
    {
        nsx::domain::motd::MotdService service("0.3.0");

        const char* manifestJson = R"json({
            "schema_version": 1,
            "product": "nsx-manager",
            "version": "0.3.0",
            "motd": {
                "id": "ams-24-warning",
                "severity": "warning",
                "title": "Firmware Warning",
                "message": "Horizon OS 24.0.0 is not yet compatible with current Atmosphere. Do not update via Daybreak.",
                "min_app_version": "0.2.0"
            }
        })json";

        REQUIRE(service.parseAndCache(manifestJson));

        auto bulletin = service.currentBulletin();
        REQUIRE(bulletin.has_value());
        CHECK(bulletin->id == "ams-24-warning");
        CHECK(bulletin->severity == "warning");
        CHECK(bulletin->title == "Firmware Warning");
        CHECK(bulletin->message.find("Horizon OS 24.0.0") != std::string::npos);

        // Test dismissal
        CHECK_FALSE(service.isDismissed("ams-24-warning"));
        service.dismiss("ams-24-warning");
        CHECK(service.isDismissed("ams-24-warning"));
        CHECK_FALSE(service.currentBulletin().has_value());

        // Test reset
        service.resetDismissals();
        CHECK_FALSE(service.isDismissed("ams-24-warning"));
        CHECK(service.currentBulletin().has_value());
    }

    TEST_CASE("MotdService enforces min_app_version requirement")
    {
        // App version 0.1.0 is lower than min_app_version 0.2.0
        nsx::domain::motd::MotdService service("0.1.0");

        const char* manifestJson = R"json({
            "schema_version": 1,
            "motd": {
                "id": "new-feature-note",
                "title": "Welcome",
                "message": "Enjoy the new dashboard layout!",
                "min_app_version": "0.2.0"
            }
        })json";

        CHECK_FALSE(service.parseAndCache(manifestJson));
        CHECK_FALSE(service.currentBulletin().has_value());
    }
}
