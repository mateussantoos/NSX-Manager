// SPDX-License-Identifier: GPL-3.0-only

#include <doctest/doctest.h>

#include "nsx/platform/network/network_service.hpp"

TEST_SUITE("platform::network::network_service")
{
    TEST_CASE("MockNetworkService queries and mutates connection telemetry")
    {
        using namespace nsx::platform::network;

        ConnectionInfo initial{};
        initial.isConnected = true;
        initial.medium = ConnectionMedium::Wifi;
        initial.ipAddress = "192.168.1.150";
        initial.subnetMask = "255.255.255.0";
        initial.gateway = "192.168.1.1";
        initial.ssid = "Home_Network_5G";
        initial.wifiSignalBars = 3;
        initial.wifiSignalPercent = 100;

        MockNetworkService service(initial);
        const auto info = service.queryConnectionInfo();

        CHECK(info.isConnected);
        CHECK(info.medium == ConnectionMedium::Wifi);
        CHECK(info.ipAddress == "192.168.1.150");
        CHECK(info.subnetMask == "255.255.255.0");
        CHECK(info.gateway == "192.168.1.1");
        CHECK(info.ssid == "Home_Network_5G");
        CHECK(info.wifiSignalBars == 3);
        CHECK(info.wifiSignalPercent == 100);

        // Mutate to disconnected
        ConnectionInfo disconnected{};
        disconnected.isConnected = false;
        disconnected.medium = ConnectionMedium::None;
        service.setConnectionInfo(disconnected);

        const auto discInfo = service.queryConnectionInfo();
        CHECK_FALSE(discInfo.isConnected);
        CHECK(discInfo.medium == ConnectionMedium::None);
    }
}
