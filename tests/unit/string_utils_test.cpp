// SPDX-License-Identifier: GPL-3.0-only

#include <doctest/doctest.h>

#include "nsx/core/text/string_utils.hpp"

TEST_SUITE("core::text::string_utils")
{
    TEST_CASE("formatBytes formats varying sizes accurately")
    {
        using nsx::core::text::formatBytes;

        CHECK(formatBytes(0) == "0 B");
        CHECK(formatBytes(512) == "512 B");
        CHECK(formatBytes(1024) == "1.00 KB");
        CHECK(formatBytes(1536) == "1.50 KB");
        CHECK(formatBytes(1048576) == "1.00 MB");
        CHECK(formatBytes(1073741824) == "1.00 GB");
        CHECK(formatBytes(1099511627776ULL) == "1.00 TB");
    }

    TEST_CASE("trim removes whitespace correctly")
    {
        using namespace nsx::core::text;

        CHECK(trim("   hello   ") == "hello");
        CHECK(trim("\t\n world \r\n") == "world");
        CHECK(trim("clean") == "clean");
        CHECK(trim("   ") == "");
        CHECK(trim("") == "");
    }

    TEST_CASE("sanitize removes non-printable characters")
    {
        using nsx::core::text::sanitize;

        CHECK(sanitize("Hello\x01\x02World\x7F!") == "HelloWorld!");
        CHECK(sanitize("Valid Printable String 123") == "Valid Printable String 123");
    }

    TEST_CASE("split and join operate reciprocally")
    {
        using namespace nsx::core::text;

        const auto parts = split("apple,banana,orange,grape", ',');
        REQUIRE(parts.size() == 4);
        CHECK(parts[0] == "apple");
        CHECK(parts[1] == "banana");
        CHECK(parts[2] == "orange");
        CHECK(parts[3] == "grape");

        const auto joined = join(parts, ", ");
        CHECK(joined == "apple, banana, orange, grape");
    }
}
