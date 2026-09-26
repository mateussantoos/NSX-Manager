// SPDX-License-Identifier: GPL-3.0-only

#include "nsx/platform/network/network_service.hpp"

#include <algorithm>
#include <cstdio>

#ifdef __SWITCH__
#include <switch.h>
#endif

namespace nsx::platform::network {

namespace {

std::string formatIpv4(const unsigned char addr[4])
{
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%u.%u.%u.%u", addr[0], addr[1], addr[2], addr[3]);
    return std::string(buf);
}

}  // namespace

ConnectionInfo DefaultNetworkService::queryConnectionInfo() const
{
#ifdef __SWITCH__
    ConnectionInfo info{};
    NifmInternetConnectionType connType{};
    u32 wifiStrength = 0;
    NifmInternetConnectionStatus connStatus{};

    if (R_SUCCEEDED(nifmGetInternetConnectionStatus(&connType, &wifiStrength, &connStatus))) {
        info.isConnected = (connStatus == NifmInternetConnectionStatus_Connected);
        if (connType == NifmInternetConnectionType_WiFi) {
            info.medium = ConnectionMedium::Wifi;
            info.wifiSignalBars = static_cast<int>(wifiStrength);
            info.wifiSignalPercent = std::clamp((info.wifiSignalBars * 100) / 3, 0, 100);
        }
        else if (connType == NifmInternetConnectionType_Ethernet) {
            info.medium = ConnectionMedium::Ethernet;
            info.wifiSignalBars = 3;
            info.wifiSignalPercent = 100;
        }
    }

    NifmNetworkProfileData profile{};
    if (R_SUCCEEDED(nifmGetCurrentNetworkProfile(&profile))) {
        if (info.medium == ConnectionMedium::Wifi) {
            const auto maxLen = sizeof(profile.wireless_setting_data.ssid);
            const auto len = std::min<std::size_t>(profile.wireless_setting_data.ssid_len, maxLen);
            info.ssid = std::string(profile.wireless_setting_data.ssid, len);
        }

        const auto& ipSetting = profile.ip_setting_data.ip_address_setting;
        info.ipAddress = formatIpv4(ipSetting.current_addr.addr);
        info.subnetMask = formatIpv4(ipSetting.subnet_mask.addr);
        info.gateway = formatIpv4(ipSetting.gateway.addr);
    }

    return info;
#else
    ConnectionInfo info{};
    info.isConnected = true;
    info.medium = ConnectionMedium::Ethernet;
    constexpr unsigned char kLocalhost[4] = {127, 0, 0, 1};
    constexpr unsigned char kSubnet[4] = {255, 255, 255, 0};
    info.ipAddress = formatIpv4(kLocalhost);
    info.subnetMask = formatIpv4(kSubnet);
    info.gateway = formatIpv4(kLocalhost);
    info.wifiSignalBars = 3;
    info.wifiSignalPercent = 100;
    return info;
#endif
}

}  // namespace nsx::platform::network
