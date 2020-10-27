#pragma once

#include "catch.hpp"

#include <cappuccino/Cappuccino.hpp>

#include <thread>

using namespace cappuccino;
using namespace std::chrono_literals;

TEST_CASE("Lfuda example")
{
    // Create a cache with 2 items, 10ms age time with halving dynamic aging ratio.
    LfudaCache<uint64_t, std::string> cache { 2, 10ms, 0.5f };

    // Insert some data.
    cache.Insert(1, "Hello");
    cache.Insert(2, "World");

    // Make 1 have a use count of 20
    for (size_t i = 1; i < 20; ++i) {
        cache.Find(1);
    }
    // Make 2 have a use count of 22
    for (size_t i = 1; i < 22; ++i) {
        cache.Find(2);
    }

    // wait an ~order of magnitude long enough to dynamically age.
    std::this_thread::sleep_for(50ms);

    // // Manually dynamic age to see its effect
    auto aged_count = cache.DynamicallyAge();
    REQUIRE(aged_count == 2);
    {
        {
            auto foo = cache.FindWithUseCount(1);
            REQUIRE(foo.has_value());
            const auto& [value, use_count] = foo.value();
            REQUIRE(value == "Hello");
            REQUIRE(use_count == 11); // dynamic age 20 => 10 + 1 find
        }
        {
            auto bar = cache.FindWithUseCount(2);
            REQUIRE(bar.has_value());
            const auto& [value, use_count] = bar.value();
            REQUIRE(value == "World");
            REQUIRE(use_count == 12); // dynamic age 22 => 11 + 1 find
        }
    }

    // Insert 3, 1 should be replaced as it will dynamically age
    // down to 10, while 2 will dynamically age down to 11.
    cache.Insert(3, "Hello World");

    {
        auto foo = cache.FindWithUseCount(1);
        auto bar = cache.FindWithUseCount(2);
        auto foobar = cache.FindWithUseCount(3);

        REQUIRE_FALSE(foo.has_value());
        REQUIRE(bar.has_value());
        REQUIRE(foobar.has_value());

        {
            const auto& [value, use_count] = bar.value();
            REQUIRE(value == "World");
            REQUIRE(use_count == 13);
        }
        {
            const auto& [value, use_count] = foobar.value();
            REQUIRE(value == "Hello World");
            REQUIRE(use_count == 2);
        }
    }
}

TEST_CASE("Lfuda Find doesn't exist")
{
    LfudaCache<uint64_t, std::string> cache { 4, 1s, 0.5f };
    REQUIRE_FALSE(cache.Find(100).has_value());
}

TEST_CASE("Lfuda Insert Only")
{
    LfudaCache<uint64_t, std::string> cache { 4, 1s, 0.5f };

    REQUIRE(cache.Insert(1, "test", Allow::INSERT));
    auto value = cache.Find(1);
    REQUIRE(value.has_value());
    REQUIRE(value.value() == "test");

    REQUIRE_FALSE(cache.Insert(1, "test2", Allow::INSERT));
    value = cache.Find(1);
    REQUIRE(value.has_value());
    REQUIRE(value.value() == "test");
}

TEST_CASE("Lfuda Update Only")
{
    LfudaCache<uint64_t, std::string> cache { 4, 1s, 0.5f };

    REQUIRE_FALSE(cache.Insert(1, "test", Allow::UPDATE));
    auto value = cache.Find(1);
    REQUIRE_FALSE(value.has_value());
}

TEST_CASE("Lfuda Insert Or Update")
{
    LfudaCache<uint64_t, std::string> cache { 4, 1s, 0.5f };

    REQUIRE(cache.Insert(1, "test"));
    auto value = cache.Find(1);
    REQUIRE(value.has_value());
    REQUIRE(value.value() == "test");

    REQUIRE(cache.Insert(1, "test2"));
    value = cache.Find(1);
    REQUIRE(value.has_value());
    REQUIRE(value.value() == "test2");
}

TEST_CASE("Lfuda InsertRange Insert Only")
{
    LfudaCache<uint64_t, std::string> cache { 4, 1s, 0.5f };

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

TEST_CASE("Lfuda InsertRange Update Only")
{
    LfudaCache<uint64_t, std::string> cache { 4, 1s, 0.5f };

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

TEST_CASE("Lfuda InsertRange Insert Or Update")
{
    LfudaCache<uint64_t, std::string> cache { 4, 1s, 0.5f };

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

TEST_CASE("Lfuda Delete")
{
    LfudaCache<uint64_t, std::string> cache { 4, 1s, 0.5f };

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

TEST_CASE("Lfuda DeleteRange")
{
    LfudaCache<uint64_t, std::string> cache { 4, 1s, 0.5f };

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

TEST_CASE("Lfuda FindRange")
{
    LfudaCache<uint64_t, std::string> cache { 4, 1s, 0.5f };

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

TEST_CASE("Lfuda FindRangeFill")
{
    LfudaCache<uint64_t, std::string> cache { 4, 1s, 0.5f };

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

TEST_CASE("Lfuda empty")
{
    LfudaCache<uint64_t, std::string> cache { 4, 1s, 0.5f };

    REQUIRE(cache.empty());
    REQUIRE(cache.Insert(1, "test", Allow::INSERT));
    REQUIRE_FALSE(cache.empty());
    REQUIRE(cache.Delete(1));
    REQUIRE(cache.empty());
}

TEST_CASE("Lfuda size + capacity")
{
    LfudaCache<uint64_t, std::string> cache { 4, 1s, 0.5f };

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