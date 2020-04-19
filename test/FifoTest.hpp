#pragma once

#include "catch.hpp"

#include <cappuccino/Cappuccino.hpp>

using namespace cappuccino;

TEST_CASE("Fifo example")
{
    // Create a cache with 4 items.
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

TEST_CASE("Fifo Insert Only")
{
    FifoCache<uint64_t, std::string> fifo_cache { 4 };

    REQUIRE(fifo_cache.Insert(1, "test", InsertMethodEnum::INSERT));
    auto value = fifo_cache.Find(1);
    REQUIRE(value.has_value());
    REQUIRE(value.value() == "test");

    REQUIRE_FALSE(fifo_cache.Insert(1, "test2", InsertMethodEnum::INSERT));
    value = fifo_cache.Find(1);
    REQUIRE(value.has_value());
    REQUIRE(value.value() == "test");
}

TEST_CASE("Fifo Update Only")
{
    FifoCache<uint64_t, std::string> fifo_cache { 4 };

    REQUIRE_FALSE(fifo_cache.Insert(1, "test", InsertMethodEnum::UPDATE));
    auto value = fifo_cache.Find(1);
    REQUIRE_FALSE(value.has_value());
}

TEST_CASE("Fifo Insert Or Update")
{
    FifoCache<uint64_t, std::string> fifo_cache { 4 };

    REQUIRE(fifo_cache.Insert(1, "test"));
    auto value = fifo_cache.Find(1);
    REQUIRE(value.has_value());
    REQUIRE(value.value() == "test");

    REQUIRE(fifo_cache.Insert(1, "test2"));
    value = fifo_cache.Find(1);
    REQUIRE(value.has_value());
    REQUIRE(value.value() == "test2");
}

TEST_CASE("Fifo InsertRange Insert Only")
{
    FifoCache<uint64_t, std::string> fifo_cache { 4 };

    {
        std::vector<std::pair<uint64_t, std::string>> inserts {
            {1, "test1"},
            {2, "test2"},
            {3, "test3"}
        };

        auto inserted = fifo_cache.InsertRange(std::move(inserts), InsertMethodEnum::INSERT);
        REQUIRE(inserted == 3);
    }

    REQUIRE(fifo_cache.size() == 3);
    REQUIRE(fifo_cache.Find(1).has_value());
    REQUIRE(fifo_cache.Find(1).value() == "test1");
    REQUIRE(fifo_cache.Find(2).has_value());
    REQUIRE(fifo_cache.Find(2).value() == "test2");
    REQUIRE(fifo_cache.Find(3).has_value());
    REQUIRE(fifo_cache.Find(3).value() == "test3");

    {
        std::vector<std::pair<uint64_t, std::string>> inserts {
            {1, "test1"},
            {2, "test2"},
            {3, "test3"},
            {4, "test4"}, // new
            {5, "test5"}, // new
        };

        auto inserted = fifo_cache.InsertRange(std::move(inserts), InsertMethodEnum::INSERT);
        REQUIRE(inserted == 2);
    }

    REQUIRE(fifo_cache.size() == 4);
    REQUIRE_FALSE(fifo_cache.Find(1).has_value()); // evicted by fifo policy
    REQUIRE(fifo_cache.Find(2).has_value());
    REQUIRE(fifo_cache.Find(2).value() == "test2");
    REQUIRE(fifo_cache.Find(3).has_value());
    REQUIRE(fifo_cache.Find(3).value() == "test3");
    REQUIRE(fifo_cache.Find(4).has_value());
    REQUIRE(fifo_cache.Find(4).value() == "test4");
    REQUIRE(fifo_cache.Find(5).has_value());
    REQUIRE(fifo_cache.Find(5).value() == "test5");
}

TEST_CASE("Fifo InsertRange Update Only")
{
    FifoCache<uint64_t, std::string> fifo_cache { 4 };

    {
        std::vector<std::pair<uint64_t, std::string>> inserts {
            {1, "test1"},
            {2, "test2"},
            {3, "test3"}
        };

        auto inserted = fifo_cache.InsertRange(std::move(inserts), InsertMethodEnum::UPDATE);
        REQUIRE(inserted == 0);
    }

    REQUIRE(fifo_cache.size() == 0);
    REQUIRE_FALSE(fifo_cache.Find(1).has_value());
    REQUIRE_FALSE(fifo_cache.Find(2).has_value());
    REQUIRE_FALSE(fifo_cache.Find(3).has_value());
}

TEST_CASE("Fifo InsertRange Insert Or Update")
{
    FifoCache<uint64_t, std::string> fifo_cache { 4 };

    {
        std::vector<std::pair<uint64_t, std::string>> inserts {
            {1, "test1"},
            {2, "test2"},
            {3, "test3"}
        };

        auto inserted = fifo_cache.InsertRange(std::move(inserts));
        REQUIRE(inserted == 3);
    }

    REQUIRE(fifo_cache.size() == 3);
    REQUIRE(fifo_cache.Find(1).has_value());
    REQUIRE(fifo_cache.Find(1).value() == "test1");
    REQUIRE(fifo_cache.Find(2).has_value());
    REQUIRE(fifo_cache.Find(2).value() == "test2");
    REQUIRE(fifo_cache.Find(3).has_value());
    REQUIRE(fifo_cache.Find(3).value() == "test3");

    {
        std::vector<std::pair<uint64_t, std::string>> inserts {
            {1, "test1"},
            {2, "test2"},
            {3, "test3"},
            {4, "test4"}, // new
            {5, "test5"}, // new
        };

        auto inserted = fifo_cache.InsertRange(std::move(inserts));
        REQUIRE(inserted == 5);
    }

    REQUIRE(fifo_cache.size() == 4);
    REQUIRE_FALSE(fifo_cache.Find(1).has_value()); // evicted by fifo policy
    REQUIRE(fifo_cache.Find(2).has_value());
    REQUIRE(fifo_cache.Find(2).value() == "test2");
    REQUIRE(fifo_cache.Find(3).has_value());
    REQUIRE(fifo_cache.Find(3).value() == "test3");
    REQUIRE(fifo_cache.Find(4).has_value());
    REQUIRE(fifo_cache.Find(4).value() == "test4");
    REQUIRE(fifo_cache.Find(5).has_value());
    REQUIRE(fifo_cache.Find(5).value() == "test5");
}

TEST_CASE("Fifo Delete")
{
    FifoCache<uint64_t, std::string> fifo_cache { 4 };

    REQUIRE(fifo_cache.Insert(1, "test", InsertMethodEnum::INSERT));
    auto value = fifo_cache.Find(1);
    REQUIRE(value.has_value());
    REQUIRE(value.value() == "test");
    REQUIRE(fifo_cache.size() == 1);

    fifo_cache.Delete(1);
    value = fifo_cache.Find(1);
    REQUIRE_FALSE(value.has_value());
    REQUIRE(fifo_cache.size() == 0);
    REQUIRE(fifo_cache.empty());

    REQUIRE_FALSE(fifo_cache.Delete(200));
}

TEST_CASE("Fifo DeleteRange")
{
    FifoCache<uint64_t, std::string> fifo_cache { 4 };

    {
        std::vector<std::pair<uint64_t, std::string>> inserts {
            {1, "test1"},
            {2, "test2"},
            {3, "test3"}
        };

        auto inserted = fifo_cache.InsertRange(std::move(inserts));
        REQUIRE(inserted == 3);
    }

    REQUIRE(fifo_cache.size() == 3);
    REQUIRE(fifo_cache.Find(1).has_value());
    REQUIRE(fifo_cache.Find(2).has_value());
    REQUIRE(fifo_cache.Find(3).has_value());


    {
        std::vector<uint64_t> delete_keys { 1,3,4,5 };

        auto deleted = fifo_cache.DeleteRange(delete_keys);
        REQUIRE(deleted == 2);
    }

    REQUIRE(fifo_cache.size() == 1);
    REQUIRE_FALSE(fifo_cache.Find(1).has_value());
    REQUIRE(fifo_cache.Find(2).has_value());
    REQUIRE(fifo_cache.Find(2).value() == "test2");
    REQUIRE_FALSE(fifo_cache.Find(3).has_value());
    REQUIRE_FALSE(fifo_cache.Find(4).has_value());
    REQUIRE_FALSE(fifo_cache.Find(5).has_value());
}

TEST_CASE("Fifo FindRange")
{
    FifoCache<uint64_t, std::string> fifo_cache { 4 };

    {
        std::vector<std::pair<uint64_t, std::string>> inserts {
            {1, "test1"},
            {2, "test2"},
            {3, "test3"}
        };

        auto inserted = fifo_cache.InsertRange(std::move(inserts));
        REQUIRE(inserted == 3);
    }

    // Make sure all inserted keys exists via find range.
    {
        std::vector<uint64_t> keys{ 1,2,3 };
        auto items = fifo_cache.FindRange(keys);

        REQUIRE(items[0].first == 1);
        REQUIRE(items[0].second.has_value());
        REQUIRE(items[0].second.value() == "test1");
        REQUIRE(items[1].first == 2);
        REQUIRE(items[1].second.has_value());
        REQUIRE(items[1].second.value() == "test2");
        REQUIRE(items[2].first == 3);
        REQUIRE(items[2].second.has_value());
        REQUIRE(items[2].second.value() == "test3");
    }

    // Make sure keys not inserted are not found by find range.
    {
        std::vector<uint64_t> keys{ 1,3,4,5 };
        auto items = fifo_cache.FindRange(keys);

        REQUIRE(items[0].first == 1);
        REQUIRE(items[0].second.has_value());
        REQUIRE(items[0].second.value() == "test1");
        REQUIRE(items[1].first == 3);
        REQUIRE(items[1].second.has_value());
        REQUIRE(items[1].second.value() == "test3");
        REQUIRE(items[2].first == 4);
        REQUIRE_FALSE(items[2].second.has_value());
        REQUIRE(items[3].first == 5);
        REQUIRE_FALSE(items[3].second.has_value());
    }
}

TEST_CASE("Fifo FindRangeFill")
{
    FifoCache<uint64_t, std::string> fifo_cache { 4 };

    {
        std::vector<std::pair<uint64_t, std::string>> inserts {
            {1, "test1"},
            {2, "test2"},
            {3, "test3"}
        };

        auto inserted = fifo_cache.InsertRange(std::move(inserts));
        REQUIRE(inserted == 3);
    }

    // Make sure all inserted keys exists via find range.
    {
        std::vector<std::pair<uint64_t, std::optional<std::string>>> items {
            {1, std::nullopt},
            {2, std::nullopt},
            {3, std::nullopt},
        };
        fifo_cache.FindRangeFill(items);

        REQUIRE(items[0].first == 1);
        REQUIRE(items[0].second.has_value());
        REQUIRE(items[0].second.value() == "test1");
        REQUIRE(items[1].first == 2);
        REQUIRE(items[1].second.has_value());
        REQUIRE(items[1].second.value() == "test2");
        REQUIRE(items[2].first == 3);
        REQUIRE(items[2].second.has_value());
        REQUIRE(items[2].second.value() == "test3");
    }

    // Make sure keys not inserted are not found by find range.
    {
        std::vector<std::pair<uint64_t, std::optional<std::string>>> items {
            {1, std::nullopt},
            {3, std::nullopt},
            {4, std::nullopt},
            {5, std::nullopt},
        };
        fifo_cache.FindRangeFill(items);

        REQUIRE(items[0].first == 1);
        REQUIRE(items[0].second.has_value());
        REQUIRE(items[0].second.value() == "test1");
        REQUIRE(items[1].first == 3);
        REQUIRE(items[1].second.has_value());
        REQUIRE(items[1].second.value() == "test3");
        REQUIRE(items[2].first == 4);
        REQUIRE_FALSE(items[2].second.has_value());
        REQUIRE(items[3].first == 5);
        REQUIRE_FALSE(items[3].second.has_value());
    }
}

TEST_CASE("Fifo empty")
{
    FifoCache<uint64_t, std::string> fifo_cache { 4 };

    REQUIRE(fifo_cache.empty());
    REQUIRE(fifo_cache.Insert(1, "test", InsertMethodEnum::INSERT));
    REQUIRE_FALSE(fifo_cache.empty());
    REQUIRE(fifo_cache.Delete(1));
    REQUIRE(fifo_cache.empty());
}

TEST_CASE("Fifo size + capacity")
{
    FifoCache<uint64_t, std::string> fifo_cache { 4 };

    REQUIRE(fifo_cache.capacity() == 4);

    REQUIRE(fifo_cache.Insert(1, "test1"));
    REQUIRE(fifo_cache.size() == 1);

    REQUIRE(fifo_cache.Insert(2, "test2"));
    REQUIRE(fifo_cache.size() == 2);

    REQUIRE(fifo_cache.Insert(3, "test3"));
    REQUIRE(fifo_cache.size() == 3);

    REQUIRE(fifo_cache.Insert(4, "test4"));
    REQUIRE(fifo_cache.size() == 4);

    REQUIRE(fifo_cache.Insert(5, "test5"));
    REQUIRE(fifo_cache.size() == 4);

    REQUIRE(fifo_cache.Insert(6, "test6"));
    REQUIRE(fifo_cache.size() == 4);

    REQUIRE(fifo_cache.capacity() == 4);
}