#pragma once

#include "catch.hpp"

#include <cappuccino/Cappuccino.h>

using namespace cappuccino;

TEST_CASE("Fifo example")
{
    // Create a cache with 2 items.
    FifoCache<uint64_t, std::string> fifo_cache { 4 };

    // Insert some data.
    fifo_cache.Insert(1, "one");
    fifo_cache.Insert(2, "two");
    fifo_cache.Insert(3, "three");
    fifo_cache.Insert(4, "four");

    {
        auto one = fifo_cache.Find(1);
        auto two = fifo_cache.Find(2);
        auto three = fifo_cache.Find(3);
        auto four = fifo_cache.Find(4);

        REQUIRE(one.has_value());
        REQUIRE(two.has_value());
        REQUIRE(three.has_value());
        REQUIRE(four.has_value());
    }

    fifo_cache.Insert(5, "five");

    {
        auto one = fifo_cache.Find(1);
        auto two = fifo_cache.Find(2);
        auto three = fifo_cache.Find(3);
        auto four = fifo_cache.Find(4);
        auto five = fifo_cache.Find(5);

        REQUIRE(!one.has_value());

        REQUIRE(two.has_value());
        REQUIRE(three.has_value());
        REQUIRE(four.has_value());
        REQUIRE(five.has_value());
    }

    fifo_cache.Insert(6, "six");

    {
        auto two = fifo_cache.Find(2);
        auto three = fifo_cache.Find(3);
        auto four = fifo_cache.Find(4);
        auto five = fifo_cache.Find(5);
        auto six = fifo_cache.Find(6);

        REQUIRE(!two.has_value());

        REQUIRE(three.has_value());
        REQUIRE(four.has_value());
        REQUIRE(five.has_value());
        REQUIRE(six.has_value());
    }
}
