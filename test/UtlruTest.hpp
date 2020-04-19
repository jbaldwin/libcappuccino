#pragma once

#include "catch.hpp"

#include <cappuccino/Cappuccino.hpp>

#include <chrono>
#include <thread>

using namespace cappuccino;

TEST_CASE("UtUtlru example")
{
    using namespace std::chrono_literals;

    // Create a cache with 2 items and a uniform TTL of 20ms.
    cappuccino::UtlruCache<uint64_t, std::string> cache { 20ms, 2 };

    // Insert "hello" and "world".
    cache.Insert(1, "Hello");
    cache.Insert(2, "World");

    {
        // Fetch the items from the catch, this will update their Utlru positions.
        auto hello = cache.Find(1);
        auto world = cache.Find(2);

        REQUIRE(hello.has_value());
        REQUIRE(world.has_value());

        REQUIRE(hello.value() == "Hello");
        REQUIRE(world.value() == "World");
    }

    // Insert "Hola", this will replace "Hello" since its the oldest Utlru item
    // and nothing has expired yet.
    cache.Insert(3, "Hola");

    {
        auto hola = cache.Find(3);
        auto hello = cache.Find(1); // Hello isn't in the cache anymore and will return an empty optional.
        auto world = cache.Find(2);

        REQUIRE(hola.has_value());
        REQUIRE_FALSE(hello.has_value());
        REQUIRE(world.has_value());
    }

    // Sleep for an ~order of magnitude longer than the TTL.
    std::this_thread::sleep_for(100ms);

    // Manually trigger a clean since no insert/delete is wanted.
    auto cleaned_count = cache.CleanExpiredValues();
    REQUIRE(cleaned_count == 2);
    REQUIRE(cache.empty());
}

TEST_CASE("Utlru Find doesn't exist")
{
    UtlruCache<uint64_t, std::string> cache { 50ms, 4 };
    REQUIRE_FALSE(cache.Find(100).has_value());
}

TEST_CASE("Utlru Insert Only")
{
    UtlruCache<uint64_t, std::string> cache { 50ms, 4 };

    REQUIRE(cache.Insert(1, "test", Allow::INSERT));
    auto value = cache.Find(1);
    REQUIRE(value.has_value());
    REQUIRE(value.value() == "test");

    REQUIRE_FALSE(cache.Insert(1, "test2", Allow::INSERT));
    value = cache.Find(1);
    REQUIRE(value.has_value());
    REQUIRE(value.value() == "test");
}

TEST_CASE("Utlru Update Only")
{
    UtlruCache<uint64_t, std::string> cache { 50ms, 4 };

    REQUIRE_FALSE(cache.Insert(1, "test", Allow::UPDATE));
    auto value = cache.Find(1);
    REQUIRE_FALSE(value.has_value());
}

TEST_CASE("Utlru Insert Or Update")
{
    UtlruCache<uint64_t, std::string> cache { 50ms, 4 };

    REQUIRE(cache.Insert(1, "test"));
    auto value = cache.Find(1);
    REQUIRE(value.has_value());
    REQUIRE(value.value() == "test");

    REQUIRE(cache.Insert(1, "test2"));
    value = cache.Find(1);
    REQUIRE(value.has_value());
    REQUIRE(value.value() == "test2");
}

TEST_CASE("Utlru InsertRange Insert Only")
{
    UtlruCache<uint64_t, std::string> cache { 50ms, 4 };

    {
        std::vector<std::pair<uint64_t, std::string>> inserts {
            {1, "test1"},
            {2, "test2"},
            {3, "test3"}
        };

        auto inserted = cache.InsertRange(std::move(inserts), Allow::INSERT);
        REQUIRE(inserted == 3);
    }

    REQUIRE(cache.size() == 3);

    REQUIRE(cache.Find(2).has_value());
    REQUIRE(cache.Find(2).value() == "test2"); // make 2 Utlru
    REQUIRE(cache.Find(1).has_value());
    REQUIRE(cache.Find(1).value() == "test1");
    REQUIRE(cache.Find(3).has_value());
    REQUIRE(cache.Find(3).value() == "test3");

    {
        std::vector<std::pair<uint64_t, std::string>> inserts {
            {1, "test1"},
            {2, "test2"},
            {3, "test3"},
            {4, "test4"}, // new
            {5, "test5"}, // new
        };

        auto inserted = cache.InsertRange(std::move(inserts), Allow::INSERT);
        REQUIRE(inserted == 2);
    }

    REQUIRE(cache.size() == 4);
    REQUIRE(cache.Find(1).has_value());
    REQUIRE(cache.Find(1).value() == "test1");
    REQUIRE_FALSE(cache.Find(2).has_value()); // evicted by Utlru policy
    REQUIRE(cache.Find(3).has_value());
    REQUIRE(cache.Find(3).value() == "test3");
    REQUIRE(cache.Find(4).has_value());
    REQUIRE(cache.Find(4).value() == "test4");
    REQUIRE(cache.Find(5).has_value());
    REQUIRE(cache.Find(5).value() == "test5");
}

TEST_CASE("Utlru InsertRange Update Only")
{
    UtlruCache<uint64_t, std::string> cache { 50ms, 4 };

    {
        std::vector<std::pair<uint64_t, std::string>> inserts {
            {1, "test1"},
            {2, "test2"},
            {3, "test3"}
        };

        auto inserted = cache.InsertRange(std::move(inserts), Allow::UPDATE);
        REQUIRE(inserted == 0);
    }

    REQUIRE(cache.size() == 0);
    REQUIRE_FALSE(cache.Find(1).has_value());
    REQUIRE_FALSE(cache.Find(2).has_value());
    REQUIRE_FALSE(cache.Find(3).has_value());
}

TEST_CASE("Utlru InsertRange Insert Or Update")
{
    UtlruCache<uint64_t, std::string> cache { 50ms, 4 };

    {
        std::vector<std::pair<uint64_t, std::string>> inserts {
            {1, "test1"},
            {2, "test2"},
            {3, "test3"}
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
            {2, "test2"}, // make 2 Utlru
            {1, "test1"},
            {3, "test3"},
            {4, "test4"}, // new
            {5, "test5"}, // new
        };

        auto inserted = cache.InsertRange(std::move(inserts));
        REQUIRE(inserted == 5);
    }

    REQUIRE(cache.size() == 4);
    REQUIRE(cache.Find(1).has_value());
    REQUIRE(cache.Find(1).value() == "test1");
    REQUIRE_FALSE(cache.Find(2).has_value()); // evicted by Utlru policy
    REQUIRE(cache.Find(3).has_value());
    REQUIRE(cache.Find(3).value() == "test3");
    REQUIRE(cache.Find(4).has_value());
    REQUIRE(cache.Find(4).value() == "test4");
    REQUIRE(cache.Find(5).has_value());
    REQUIRE(cache.Find(5).value() == "test5");
}

TEST_CASE("Utlru Delete")
{
    UtlruCache<uint64_t, std::string> cache { 50ms, 4 };

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

TEST_CASE("Utlru DeleteRange")
{
    UtlruCache<uint64_t, std::string> cache { 50ms, 4 };

    {
        std::vector<std::pair<uint64_t, std::string>> inserts {
            {1, "test1"},
            {2, "test2"},
            {3, "test3"}
        };

        auto inserted = cache.InsertRange(std::move(inserts));
        REQUIRE(inserted == 3);
    }

    REQUIRE(cache.size() == 3);
    REQUIRE(cache.Find(1).has_value());
    REQUIRE(cache.Find(2).has_value());
    REQUIRE(cache.Find(3).has_value());


    {
        std::vector<uint64_t> delete_keys { 1,3,4,5 };

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

TEST_CASE("Utlru FindRange")
{
    UtlruCache<uint64_t, std::string> cache { 50ms, 4 };

    {
        std::vector<std::pair<uint64_t, std::string>> inserts {
            {1, "test1"},
            {2, "test2"},
            {3, "test3"}
        };

        auto inserted = cache.InsertRange(std::move(inserts));
        REQUIRE(inserted == 3);
    }

    // Make sure all inserted keys exists via find range.
    {
        std::vector<uint64_t> keys{ 1,2,3 };
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
        std::vector<uint64_t> keys{ 1,3,4,5 };
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

TEST_CASE("Utlru FindRangeFill")
{
    UtlruCache<uint64_t, std::string> cache { 50ms, 4 };

    {
        std::vector<std::pair<uint64_t, std::string>> inserts {
            {1, "test1"},
            {2, "test2"},
            {3, "test3"}
        };

        auto inserted = cache.InsertRange(std::move(inserts));
        REQUIRE(inserted == 3);
    }

    // Make sure all inserted keys exists via find range.
    {
        std::vector<std::pair<uint64_t, std::optional<std::string>>> items {
            {1, std::nullopt},
            {2, std::nullopt},
            {3, std::nullopt},
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
            {1, std::nullopt},
            {3, std::nullopt},
            {4, std::nullopt},
            {5, std::nullopt},
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

TEST_CASE("Utlru empty")
{
    UtlruCache<uint64_t, std::string> cache { 50ms, 4 };

    REQUIRE(cache.empty());
    REQUIRE(cache.Insert(1, "test", Allow::INSERT));
    REQUIRE_FALSE(cache.empty());
    REQUIRE(cache.Delete(1));
    REQUIRE(cache.empty());
}

TEST_CASE("Utlru size + capacity")
{
    UtlruCache<uint64_t, std::string> cache { 50ms, 4 };

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

TEST_CASE("Utlru Find with Peek")
{
    UtlruCache<uint64_t, std::string> cache { 50ms, 4 };

    REQUIRE(cache.Insert(1, "Hello"));
    REQUIRE(cache.Insert(2, "World"));
    REQUIRE(cache.Insert(3, "Hola"));
    REQUIRE(cache.Insert(4, "Mondo"));

    REQUIRE(cache.Find(1, Peek::YES).has_value()); // doesn't move up to MRU
    REQUIRE(cache.Find(2, Peek::NO).has_value());
    REQUIRE(cache.Find(3, Peek::YES).has_value()); // doesn't move up to MRU
    REQUIRE(cache.Find(4, Peek::NO).has_value());

    REQUIRE(cache.Insert(5, "another one bites the dust1"));
    REQUIRE_FALSE(cache.Find(1).has_value());
    REQUIRE(cache.Insert(6, "another one bites the dust2"));
    REQUIRE_FALSE(cache.Find(3).has_value());
}

TEST_CASE("Utlru ttls")
{
    UtlruCache<uint64_t, std::string> cache { 20ms, 2 };

    REQUIRE(cache.Insert(1, "Hello"));
    REQUIRE(cache.Insert(2, "World"));

    std::this_thread::sleep_for(50ms);

    REQUIRE(cache.Insert(3, "Hola"));

    auto hello = cache.Find(1);
    auto world = cache.Find(2);
    auto hola  = cache.Find(3);

    REQUIRE_FALSE(hello.has_value());
    REQUIRE_FALSE(world.has_value());
    REQUIRE(hola.has_value());
    REQUIRE(hola.value() == "Hola");
}

TEST_CASE("Utlru Clean with only some expired")
{
    UtlruCache<uint64_t, std::string> cache { 25ms, 2 };

    REQUIRE(cache.Insert(1, "Hello"));
    REQUIRE(cache.Insert(2, "World"));

    std::this_thread::sleep_for(50ms);

    REQUIRE(cache.Insert(3, "Hola"));

    cache.CleanExpiredValues();

    auto hello = cache.Find(1);
    auto world = cache.Find(2);
    auto hola  = cache.Find(3);

    REQUIRE_FALSE(hello.has_value());
    REQUIRE_FALSE(world.has_value());
    REQUIRE(hola.has_value());
    REQUIRE(hola.value() == "Hola");
}