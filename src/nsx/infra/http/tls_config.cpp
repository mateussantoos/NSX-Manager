// SPDX-License-Identifier: GPL-3.0-only

#include "nsx/infra/http/tls_config.hpp"

namespace nsx::infra {

std::string_view describe(HttpError error)
{
    switch (error) {
        case HttpError::None:
            return "no error";
        case HttpError::NoNetwork:
            return "no network connection";
        case HttpError::DnsFailure:
            return "could not resolve the host";
        case HttpError::ConnectFailed:
            return "could not connect to the host";
        case HttpError::Timeout:
            return "the connection timed out";
        case HttpError::TlsVerifyFailed:
            return "the server certificate could not be verified";
        case HttpError::TlsClockSkew:
            return "the server certificate is not valid yet or has expired";
        case HttpError::InsecureUrl:
            return "refused a non-https URL";
        case HttpError::HttpStatus:
            return "the server returned an error status";
        case HttpError::RateLimited:
            return "rate limited by the server";
        case HttpError::TooLarge:
            return "the response was larger than allowed";
        case HttpError::WriteFailed:
            return "could not write to the SD card";
        case HttpError::Aborted:
            return "cancelled";
        case HttpError::Internal:
            return "internal error";
    }
    return "unknown error";
}

std::string_view advise(HttpError error)
{
    switch (error) {
        case HttpError::NoNetwork:
            return "Connect to Wi-Fi and try again.";
        case HttpError::DnsFailure:
            return "Check your network settings. A DNS blocker may be blocking more than "
                   "intended.";
        case HttpError::TlsClockSkew:
            // The single most useful message this application can produce. It is
            // also the alternative to what the predecessor did, which was to turn
            // verification off on every request.
            return "Set your console's date and time, then try again.";
        case HttpError::TlsVerifyFailed:
            return "The connection could not be trusted, so it was refused.";
        case HttpError::RateLimited:
            return "Too many requests. Try again later.";
        case HttpError::TooLarge:
        case HttpError::WriteFailed:
            return "Free some space on your SD card and try again.";
        case HttpError::ConnectFailed:
        case HttpError::Timeout:
        case HttpError::HttpStatus:
            return "Check your connection and try again.";
        case HttpError::None:
        case HttpError::InsecureUrl:
        case HttpError::Aborted:
        case HttpError::Internal:
            return "";
    }
    return "";
}

bool isHttpsUrl(std::string_view url)
{
    constexpr std::string_view kScheme = "https://";
    return url.size() > kScheme.size() && url.compare(0, kScheme.size(), kScheme) == 0;
}

}  // namespace nsx::infra
