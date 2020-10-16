#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include <cappuccino/Cappuccino.hpp>

#include "FifoTest.hpp"
#include "LfuTest.hpp"
#include "LfudaTest.hpp"
#include "LruTest.hpp"
#include "MruTest.hpp"
#include "RrTest.hpp"
#include "TlruTest.hpp"
#include "UtMapTest.hpp"
#include "UtSetTest.hpp"
#include "UtlruTest.hpp"

TEST_CASE("InsertMethod to_string()")
{
    REQUIRE(cappuccino::to_string(Allow::INSERT) == "INSERT");
    REQUIRE(cappuccino::to_string(Allow::UPDATE) == "UPDATE");
    REQUIRE(cappuccino::to_string(Allow::INSERT_OR_UPDATE) == "INSERT_OR_UPDATE");
    REQUIRE(cappuccino::to_string(static_cast<Allow>(5000)) == "INVALID_VALUE");
}

TEST_CASE("SyncImpl to_string()")
{
    REQUIRE(cappuccino::to_string(Sync::YES) == "YES");
    REQUIRE(cappuccino::to_string(Sync::NO) == "NO");
    REQUIRE(cappuccino::to_string(static_cast<Sync>(5000)) == "INVALID_VALUE");
}

TEST_CASE("Peek to_string()")
{
    REQUIRE(cappuccino::to_string(Peek::YES) == "YES");
    REQUIRE(cappuccino::to_string(Peek::NO) == "NO");
    REQUIRE(cappuccino::to_string(static_cast<Sync>(5000)) == "INVALID_VALUE");
}
