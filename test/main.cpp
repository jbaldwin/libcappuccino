#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include <cappuccino/cappuccino.hpp>

using namespace cappuccino;

TEST_CASE("InsertMethod to_string()")
{
    REQUIRE(to_string(allow::insert) == "insert");
    REQUIRE(to_string(allow::update) == "update");
    REQUIRE(to_string(allow::insert_or_update) == "insert_or_update");
    REQUIRE(to_string(static_cast<allow>(5000)) == "invalid_value");
}

TEST_CASE("syncImpl to_string()")
{
    REQUIRE(to_string(sync::yes) == "yes");
    REQUIRE(to_string(sync::no) == "no");
    REQUIRE(to_string(static_cast<cappuccino::sync>(5000)) == "invalid_value");
}

TEST_CASE("Peek to_string()")
{
    REQUIRE(to_string(Peek::YES) == "YES");
    REQUIRE(to_string(Peek::NO) == "NO");
    REQUIRE(to_string(static_cast<Peek>(5000)) == "INVALID_VALUE");
}
