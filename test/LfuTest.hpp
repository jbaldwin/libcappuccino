#pragma once

#include "catch.hpp"

#include <cappuccino/Cappuccino.hpp>

using namespace cappuccino;

TEST_CASE("Lfu example")
{
    // Create a cache with 2 items.
    LfuCache<uint64_t, std::string> cache { 2 };

    // Insert some data.
    cache.Insert(1, "Hello");
    cache.Insert(2, "World");

    // Touch 1 twice.
    auto foo1 = cache.Find(1);
    auto foo2 = cache.Find(1);

    // Touch 2 once.
    auto bar1 = cache.Find(2);

    // Insert 3, 2 should be replaced.
    cache.Insert(3, "Hello World");

    auto bar2 = cache.FindWithUseCount(2);
    REQUIRE_FALSE(bar2.has_value());

    auto foo3 = cache.FindWithUseCount(1);
    auto foobar = cache.FindWithUseCount(3);

    REQUIRE(foo3.has_value());
    {
        auto& [value, use_count] = foo3.value();
        REQUIRE(value == "Hello");
        REQUIRE(use_count == 4); // insert, three finds
    }

    REQUIRE(foobar.has_value());
    {
        auto& [value, use_count] = foobar.value();
        REQUIRE(value == "Hello World");
        REQUIRE(use_count == 2); // insert, one find
    }
}

TEST_CASE("Lfu Find doesn't exist")
{
    LfuCache<uint64_t, std::string> cache { 4 };
    REQUIRE_FALSE(cache.Find(100).has_value());
}

TEST_CASE("Lfu Insert Only")
{
    LfuCache<uint64_t, std::string> cache { 4 };

    REQUIRE(cache.Insert(1, "test", Allow::INSERT));
    auto value = cache.Find(1);
    REQUIRE(value.has_value());
    REQUIRE(value.value() == "test");

    REQUIRE_FALSE(cache.Insert(1, "test2", Allow::INSERT));
    value = cache.Find(1);
    REQUIRE(value.has_value());
    REQUIRE(value.value() == "test");
}

TEST_CASE("Lfu Update Only")
{
    LfuCache<uint64_t, std::string> cache { 4 };

    REQUIRE_FALSE(cache.Insert(1, "test", Allow::UPDATE));
    auto value = cache.Find(1);
    REQUIRE_FALSE(value.has_value());
}

TEST_CASE("Lfu Insert Or Update")
{
    LfuCache<uint64_t, std::string> cache { 4 };

    REQUIRE(cache.Insert(1, "test"));
    auto value = cache.Find(1);
    REQUIRE(value.has_value());
    REQUIRE(value.value() == "test");

    REQUIRE(cache.Insert(1, "test2"));
    value = cache.Find(1);
    REQUIRE(value.has_value());
    REQUIRE(value.value() == "test2");
}

TEST_CASE("Lfu InsertRange Insert Only")
{
    LfuCache<uint64_t, std::string> cache { 4 };

    {
        std::vector<std::pair<uint64_t, std::string>> inserts {
            { 1, "test1" },
            { 2, "test2" },
            { 3, "test3" }
        };

        auto inserted = cache.InsertRange(std::move(inserts), Allow::INSERT);
        REQUIRE(inserted == 3);
    }

    REQUIRE(cache.size() == 3);
    REQUIRE(cache.Find(1).has_value());
    REQUIRE(cache.Find(1).value() == "test1");
    REQUIRE(cache.Find(2).has_value());
    REQUIRE(cache.Find(2).value() == "test2");
    REQUIRE(cache.Find(3).has_value());
    REQUIRE(cache.Find(3).value() == "test3");

    {
        std::vector<std::pair<uint64_t, std::string>> inserts {
            { 1, "test1" },
            { 2, "test2" },
            { 3, "test3" },
            { 4, "test4" }, // new
            { 5, "test5" }, // new
        };

        auto inserted = cache.InsertRange(std::move(inserts), Allow::INSERT);
        REQUIRE(inserted == 2);
    }

    REQUIRE(cache.size() == 4);
    REQUIRE(cache.Find(1).has_value());
    REQUIRE(cache.Find(1).value() == "test1");
    REQUIRE(cache.Find(2).has_value());
    REQUIRE(cache.Find(2).value() == "test2");
    REQUIRE(cache.Find(3).has_value());
    REQUIRE(cache.Find(3).value() == "test3");
    REQUIRE_FALSE(cache.Find(4).has_value()); // evicted by lfu policy
    REQUIRE(cache.Find(5).has_value());
    REQUIRE(cache.Find(5).value() == "test5");
}

TEST_CASE("Lfu InsertRange Update Only")
{
    LfuCache<uint64_t, std::string> cache { 4 };

    {
        std::vector<std::pair<uint64_t, std::string>> inserts {
            { 1, "test1" },
            { 2, "test2" },
            { 3, "test3" }
        };

        auto inserted = cache.InsertRange(std::move(inserts), Allow::UPDATE);
        REQUIRE(inserted == 0);
    }

    REQUIRE(cache.size() == 0);
    REQUIRE_FALSE(cache.Find(1).has_value());
    REQUIRE_FALSE(cache.Find(2).has_value());
    REQUIRE_FALSE(cache.Find(3).has_value());
}

TEST_CASE("Lfu InsertRange Insert Or Update")
{
    LfuCache<uint64_t, std::string> cache { 4 };

    {
        std::vector<std::pair<uint64_t, std::string>> inserts {
            { 1, "test1" },
            { 2, "test2" },
            { 3, "test3" }
        };

        auto inserted = cache.InsertRange(std::move(inserts));
        REQUIRE(inserted == 3);
    }

    REQUIRE(cache.size() == 3);
    REQUIRE(cache.Find(1).has_value());
    REQUIRE(cache.Find(1).value() == "test1");
    REQUIRE(cache.Find(2).has_value());
    REQUIRE(cache.Find(2).value() == "test2");
    REQUIRE(cache.Find(3).has_value());
    REQUIRE(cache.Find(3).value() == "test3");

    {
        std::vector<std::pair<uint64_t, std::string>> inserts {
            { 1, "test1" },
            { 2, "test2" },
            { 3, "test3" },
            { 4, "test4" }, // new
            { 5, "test5" }, // new
        };

        auto inserted = cache.InsertRange(std::move(inserts));
        REQUIRE(inserted == 5);
    }

    REQUIRE(cache.size() == 4);
    REQUIRE(cache.Find(1).has_value());
    REQUIRE(cache.Find(1).value() == "test1");
    REQUIRE(cache.Find(2).has_value());
    REQUIRE(cache.Find(2).value() == "test2");
    REQUIRE(cache.Find(3).has_value());
    REQUIRE(cache.Find(3).value() == "test3");
    REQUIRE_FALSE(cache.Find(4).has_value()); // evicted by lfu policy
    REQUIRE(cache.Find(5).has_value());
    REQUIRE(cache.Find(5).value() == "test5");
}

TEST_CASE("Lfu Delete")
{
    LfuCache<uint64_t, std::string> cache { 4 };

    REQUIRE(cache.Insert(1, "test", Allow::INSERT));
    auto value = cache.Find(1);
    REQUIRE(value.has_value());
    REQUIRE(value.value() == "test");
    REQUIRE(cache.size() == 1);

    cache.Delete(1);
    value = cache.Find(1);
    REQUIRE_FALSE(value.has_value());
    REQUIRE(cache.size() == 0);
    REQUIRE(cache.empty());

    REQUIRE_FALSE(cache.Delete(200));
}

TEST_CASE("Lfu DeleteRange")
{
    LfuCache<uint64_t, std::string> cache { 4 };

    {
        std::vector<std::pair<uint64_t, std::string>> inserts {
            { 1, "test1" },
            { 2, "test2" },
            { 3, "test3" }
        };

        auto inserted = cache.InsertRange(std::move(inserts));
        REQUIRE(inserted == 3);
    }

    REQUIRE(cache.size() == 3);
    REQUIRE(cache.Find(1).has_value());
    REQUIRE(cache.Find(2).has_value());
    REQUIRE(cache.Find(3).has_value());

    {
        std::vector<uint64_t> delete_keys { 1, 3, 4, 5 };

        auto deleted = cache.DeleteRange(delete_keys);
        REQUIRE(deleted == 2);
    }

    REQUIRE(cache.size() == 1);
    REQUIRE_FALSE(cache.Find(1).has_value());
    REQUIRE(cache.Find(2).has_value());
    REQUIRE(cache.Find(2).value() == "test2");
    REQUIRE_FALSE(cache.Find(3).has_value());
    REQUIRE_FALSE(cache.Find(4).has_value());
    REQUIRE_FALSE(cache.Find(5).has_value());
}

TEST_CASE("Lfu FindRange")
{
    LfuCache<uint64_t, std::string> cache { 4 };

    {
        std::vector<std::pair<uint64_t, std::string>> inserts {
            { 1, "test1" },
            { 2, "test2" },
            { 3, "test3" }
        };

        auto inserted = cache.InsertRange(std::move(inserts));
        REQUIRE(inserted == 3);
    }

    // Make sure all inserted keys exists via find range.
    {
        std::vector<uint64_t> keys { 1, 2, 3 };
        auto items = cache.FindRange(keys);

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
        std::vector<uint64_t> keys { 1, 3, 4, 5 };
        auto items = cache.FindRange(keys);

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

TEST_CASE("Lfu FindRangeFill")
{
    LfuCache<uint64_t, std::string> cache { 4 };

    {
        std::vector<std::pair<uint64_t, std::string>> inserts {
            { 1, "test1" },
            { 2, "test2" },
            { 3, "test3" }
        };

        auto inserted = cache.InsertRange(std::move(inserts));
        REQUIRE(inserted == 3);
    }

    // Make sure all inserted keys exists via find range.
    {
        std::vector<std::pair<uint64_t, std::optional<std::string>>> items {
            { 1, std::nullopt },
            { 2, std::nullopt },
            { 3, std::nullopt },
        };
        cache.FindRangeFill(items);

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
            { 1, std::nullopt },
            { 3, std::nullopt },
            { 4, std::nullopt },
            { 5, std::nullopt },
        };
        cache.FindRangeFill(items);

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

TEST_CASE("Lfu empty")
{
    LfuCache<uint64_t, std::string> cache { 4 };

    REQUIRE(cache.empty());
    REQUIRE(cache.Insert(1, "test", Allow::INSERT));
    REQUIRE_FALSE(cache.empty());
    REQUIRE(cache.Delete(1));
    REQUIRE(cache.empty());
}

TEST_CASE("Lfu size + capacity")
{
    LfuCache<uint64_t, std::string> cache { 4 };

    REQUIRE(cache.capacity() == 4);

    REQUIRE(cache.Insert(1, "test1"));
    REQUIRE(cache.size() == 1);

    REQUIRE(cache.Insert(2, "test2"));
    REQUIRE(cache.size() == 2);

    REQUIRE(cache.Insert(3, "test3"));
    REQUIRE(cache.size() == 3);

    REQUIRE(cache.Insert(4, "test4"));
    REQUIRE(cache.size() == 4);

    REQUIRE(cache.Insert(5, "test5"));
    REQUIRE(cache.size() == 4);

    REQUIRE(cache.Insert(6, "test6"));
    REQUIRE(cache.size() == 4);

    REQUIRE(cache.capacity() == 4);
}

TEST_CASE("Lfu smallest use count")
{
    LfuCache<uint64_t, std::string> cache { 1 };

    // Insert counts as a use.
    cache.Insert(1, "test1");

    // Find counts as a use.
    auto element = cache.FindWithUseCount(1);
    REQUIRE(element.has_value());
    const auto& [value, use_count] = element.value();
    REQUIRE(value == "test1");
    REQUIRE(use_count == 2);
}
