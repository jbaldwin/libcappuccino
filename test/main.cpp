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

TEST_CASE("peek to_string()")
{
    REQUIRE(to_string(peek::yes) == "yes");
    REQUIRE(to_string(peek::no) == "no");
    REQUIRE(to_string(static_cast<peek>(5000)) == "invalid_value");
}
