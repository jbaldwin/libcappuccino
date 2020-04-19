#pragma once

#include "catch.hpp"

#include <cappuccino/Cappuccino.hpp>

using namespace cappuccino;

TEST_CASE("Lru example")
{
    // Create a cache with 2 items.
    cappuccino::LruCache<uint64_t, std::string> lru_cache { 2 };

    // Insert hello and world.
    lru_cache.Insert(1, "Hello");
    lru_cache.Insert(2, "World");

    {
        // Grab them
        auto hello = lru_cache.Find(1);
        auto world = lru_cache.Find(2);

        REQUIRE(hello.has_value());
        REQUIRE(world.has_value());

        REQUIRE(hello.value() == "Hello");
        REQUIRE(world.value() == "World");
    }

    // Insert hola, this will replace "Hello" since its the oldest lru item.
    lru_cache.Insert(3, "Hola");

    {
        auto hola = lru_cache.Find(3);
        auto hello = lru_cache.Find(1); // this will return an empty optional now
        auto world = lru_cache.Find(2); // this value should still be available!

        REQUIRE(hola.has_value());
        REQUIRE_FALSE(hello.has_value());
        REQUIRE(world.has_value());
    }
}

TEST_CASE("Lru Find doesn't exist")
{
    LruCache<uint64_t, std::string> cache { 4 };
    REQUIRE_FALSE(cache.Find(100).has_value());
}

TEST_CASE("Lru Insert Only")
{
    LruCache<uint64_t, std::string> cache { 4 };

    REQUIRE(cache.Insert(1, "test", Allow::INSERT));
    auto value = cache.Find(1);
    REQUIRE(value.has_value());
    REQUIRE(value.value() == "test");

    REQUIRE_FALSE(cache.Insert(1, "test2", Allow::INSERT));
    value = cache.Find(1);
    REQUIRE(value.has_value());
    REQUIRE(value.value() == "test");
}

TEST_CASE("Lru Update Only")
{
    LruCache<uint64_t, std::string> cache { 4 };

    REQUIRE_FALSE(cache.Insert(1, "test", Allow::UPDATE));
    auto value = cache.Find(1);
    REQUIRE_FALSE(value.has_value());
}

TEST_CASE("Lru Insert Or Update")
{
    LruCache<uint64_t, std::string> cache { 4 };

    REQUIRE(cache.Insert(1, "test"));
    auto value = cache.Find(1);
    REQUIRE(value.has_value());
    REQUIRE(value.value() == "test");

    REQUIRE(cache.Insert(1, "test2"));
    value = cache.Find(1);
    REQUIRE(value.has_value());
    REQUIRE(value.value() == "test2");
}

TEST_CASE("Lru InsertRange Insert Only")
{
    LruCache<uint64_t, std::string> cache { 4 };

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
    REQUIRE(cache.Find(2).value() == "test2"); // make 2 LRU
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
    REQUIRE_FALSE(cache.Find(2).has_value()); // evicted by lru policy
    REQUIRE(cache.Find(3).has_value());
    REQUIRE(cache.Find(3).value() == "test3");
    REQUIRE(cache.Find(4).has_value());
    REQUIRE(cache.Find(4).value() == "test4");
    REQUIRE(cache.Find(5).has_value());
    REQUIRE(cache.Find(5).value() == "test5");
}

TEST_CASE("Lru InsertRange Update Only")
{
    LruCache<uint64_t, std::string> cache { 4 };

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

TEST_CASE("Lru InsertRange Insert Or Update")
{
    LruCache<uint64_t, std::string> cache { 4 };

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
            {2, "test2"}, // make 2 LRU
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
    REQUIRE_FALSE(cache.Find(2).has_value()); // evicted by lru policy
    REQUIRE(cache.Find(3).has_value());
    REQUIRE(cache.Find(3).value() == "test3");
    REQUIRE(cache.Find(4).has_value());
    REQUIRE(cache.Find(4).value() == "test4");
    REQUIRE(cache.Find(5).has_value());
    REQUIRE(cache.Find(5).value() == "test5");
}

TEST_CASE("Lru Delete")
{
    LruCache<uint64_t, std::string> cache { 4 };

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

TEST_CASE("Lru DeleteRange")
{
    LruCache<uint64_t, std::string> cache { 4 };

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

TEST_CASE("Lru FindRange")
{
    LruCache<uint64_t, std::string> cache { 4 };

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

TEST_CASE("Lru FindRangeFill")
{
    LruCache<uint64_t, std::string> cache { 4 };

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

TEST_CASE("Lru empty")
{
    LruCache<uint64_t, std::string> cache { 4 };

    REQUIRE(cache.empty());
    REQUIRE(cache.Insert(1, "test", Allow::INSERT));
    REQUIRE_FALSE(cache.empty());
    REQUIRE(cache.Delete(1));
    REQUIRE(cache.empty());
}

TEST_CASE("Lru size + capacity")
{
    LruCache<uint64_t, std::string> cache { 4 };

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

TEST_CASE("Lru Find with Peek")
{
    LruCache<uint64_t, std::string> cache { 4 };

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