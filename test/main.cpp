#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include <cappuccino/cappuccino.hpp>

using namespace cappuccino;

TEST_CASE("InsertMethod to_string()")
{
    REQUIRE(to_string(Allow::INSERT) == "INSERT");
    REQUIRE(to_string(Allow::UPDATE) == "UPDATE");
    REQUIRE(to_string(Allow::INSERT_OR_UPDATE) == "INSERT_OR_UPDATE");
    REQUIRE(to_string(static_cast<Allow>(5000)) == "INVALID_VALUE");
}

TEST_CASE("SyncImpl to_string()")
{
    REQUIRE(to_string(Sync::YES) == "YES");
    REQUIRE(to_string(Sync::NO) == "NO");
    REQUIRE(to_string(static_cast<Sync>(5000)) == "INVALID_VALUE");
}

TEST_CASE("Peek to_string()")
{
    REQUIRE(to_string(Peek::YES) == "YES");
    REQUIRE(to_string(Peek::NO) == "NO");
    REQUIRE(to_string(static_cast<Peek>(5000)) == "INVALID_VALUE");
}
