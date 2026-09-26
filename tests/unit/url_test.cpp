// SPDX-License-Identifier: GPL-3.0-only

#include <doctest/doctest.h>

#include "nsx/core/net/url.hpp"

TEST_SUITE("core::net::url")
{
    TEST_CASE("parseUrl decomposes standard and port-explicit URLs")
    {
        using nsx::core::net::parseUrl;

        const auto u1 = parseUrl("https://raw.githubusercontent.com/user/repo/release-metadata/update.json");
        REQUIRE(u1.has_value());
        CHECK(u1->scheme == "https");
        CHECK(u1->host == "raw.githubusercontent.com");
        CHECK(u1->port == 443);
        CHECK(u1->path == "/user/repo/release-metadata/update.json");
        CHECK(u1->query == "");

        const auto u2 = parseUrl("http://192.168.1.100:8080/api/v1/status?verbose=1");
        REQUIRE(u2.has_value());
        CHECK(u2->scheme == "http");
        CHECK(u2->host == "192.168.1.100");
        CHECK(u2->port == 8080);
        CHECK(u2->path == "/api/v1/status");
        CHECK(u2->query == "verbose=1");

        CHECK_FALSE(parseUrl("not_a_url").has_value());
        CHECK_FALSE(parseUrl("http://").has_value());
    }

    TEST_CASE("buildQuery creates valid query strings")
    {
        using nsx::core::net::buildQuery;

        CHECK(buildQuery({}) == "");
        CHECK(buildQuery({{"key", "val"}}) == "?key=val");
        CHECK(buildQuery({{"a", "1"}, {"b", "2"}}) == "?a=1&b=2");
    }

    TEST_CASE("formatIfNoneMatch wraps ETags cleanly")
    {
        using nsx::core::net::formatIfNoneMatch;

        CHECK(formatIfNoneMatch("abcdef") == "\"abcdef\"");
        CHECK(formatIfNoneMatch("\"abcdef\"") == "\"abcdef\"");
        CHECK(formatIfNoneMatch("") == "");
    }
}
