#include "catch.hpp"
#include <cappuccino/cappuccino.hpp>

#include <chrono>
#include <iostream>
#include <thread>
#include <variant>

using namespace cappuccino;
using namespace std::chrono_literals;

TEST_CASE("TTlru example")
{
    // Create a cache with up to 3 items.
    TlruCache<uint64_t, std::string> Tlru_cache{3};

    // Insert "hello", "world" with different TTLs.
    Tlru_cache.Insert(1min, 1, "Hello");
    Tlru_cache.Insert(2min, 2, "World");

    // Insert a third value to fill the cache.
    Tlru_cache.Insert(3min, 3, "nope");

    {
        // Grab hello and world, this update their Tlru positions.
        auto hello = Tlru_cache.Find(1);
        auto world = Tlru_cache.Find(2);

        REQUIRE(hello.has_value());
        REQUIRE(world.has_value());
    }

    // Insert "hola", this will replace "nope" since its the oldest Tlru item,
    // nothing has expired at this time.
    Tlru_cache.Insert(10ms, 4, "Hola");

    {
        auto hola  = Tlru_cache.Find(4); // "hola" was just inserted, it will be found
        auto hello = Tlru_cache.Find(1); // "hello" will also have a value, it is at the end of the Tlru list
        auto world = Tlru_cache.Find(2); // "world" is in the middle of our 3 Tlru list.
        auto nope =
            Tlru_cache.Find(3); // "nope" was Tlru'ed when "hola" was inserted since "hello" and "world were fetched

        REQUIRE(hola.has_value());
        REQUIRE(hello.has_value());
        REQUIRE(world.has_value());
        REQUIRE_FALSE(nope.has_value());
    }

    // Sleep for an order of magnitude longer than the TTL on 'hola'.
    std::this_thread::sleep_for(100ms);

    {
        auto hola  = Tlru_cache.Find(4);
        auto hello = Tlru_cache.Find(1);
        auto world = Tlru_cache.Find(2);

        // Only hola will be evicted due to its TTL expiring.
        REQUIRE_FALSE(hola.has_value());
        REQUIRE(hello.has_value());
        REQUIRE(world.has_value());
    }
}

TEST_CASE("Tlru Find doesn't exist")
{
    TlruCache<uint64_t, std::string> cache{4};
    REQUIRE_FALSE(cache.Find(100).has_value());
}

TEST_CASE("Tlru Insert Only")
{
    TlruCache<uint64_t, std::string> cache{4};

    REQUIRE(cache.Insert(1min, 1, "test", allow::insert));
    auto value = cache.Find(1);
    REQUIRE(value.has_value());
    REQUIRE(value.value() == "test");

    REQUIRE_FALSE(cache.Insert(1min, 1, "test2", allow::insert));
    value = cache.Find(1);
    REQUIRE(value.has_value());
    REQUIRE(value.value() == "test");
}

TEST_CASE("Tlru Update Only")
{
    TlruCache<uint64_t, std::string> cache{4};

    REQUIRE_FALSE(cache.Insert(1min, 1, "test", allow::update));
    auto value = cache.Find(1);
    REQUIRE_FALSE(value.has_value());
}

TEST_CASE("Tlru Insert Or Update")
{
    TlruCache<uint64_t, std::string> cache{4};

    REQUIRE(cache.Insert(1min, 1, "test"));
    auto value = cache.Find(1);
    REQUIRE(value.has_value());
    REQUIRE(value.value() == "test");

    REQUIRE(cache.Insert(1min, 1, "test2"));
    value = cache.Find(1);
    REQUIRE(value.has_value());
    REQUIRE(value.value() == "test2");
}

TEST_CASE("Tlru InsertRange Insert Only")
{
    TlruCache<uint64_t, std::string> cache{4};

    {
        std::vector<std::tuple<std::chrono::minutes, uint64_t, std::string>> inserts{
            {1min, 1, "test1"}, {1min, 2, "test2"}, {1min, 3, "test3"}};

        auto inserted = cache.InsertRange(std::move(inserts), allow::insert);
        REQUIRE(inserted == 3);
    }

    REQUIRE(cache.size() == 3);

    REQUIRE(cache.Find(2).has_value());
    REQUIRE(cache.Find(2).value() == "test2"); // make 2 Tlru
    REQUIRE(cache.Find(1).has_value());
    REQUIRE(cache.Find(1).value() == "test1");
    REQUIRE(cache.Find(3).has_value());
    REQUIRE(cache.Find(3).value() == "test3");

    {
        std::vector<std::tuple<std::chrono::minutes, uint64_t, std::string>> inserts{
            {1min, 1, "test1"},
            {1min, 2, "test2"},
            {1min, 3, "test3"},
            {1min, 4, "test4"}, // new
            {1min, 5, "test5"}, // new
        };

        auto inserted = cache.InsertRange(std::move(inserts), allow::insert);
        REQUIRE(inserted == 2);
    }

    REQUIRE(cache.size() == 4);
    REQUIRE(cache.Find(1).has_value());
    REQUIRE(cache.Find(1).value() == "test1");
    REQUIRE_FALSE(cache.Find(2).has_value()); // evicted by Tlru policy
    REQUIRE(cache.Find(3).has_value());
    REQUIRE(cache.Find(3).value() == "test3");
    REQUIRE(cache.Find(4).has_value());
    REQUIRE(cache.Find(4).value() == "test4");
    REQUIRE(cache.Find(5).has_value());
    REQUIRE(cache.Find(5).value() == "test5");
}

TEST_CASE("Tlru InsertRange Update Only")
{
    TlruCache<uint64_t, std::string> cache{4};

    {
        std::vector<std::tuple<std::chrono::minutes, uint64_t, std::string>> inserts{
            {1min, 1, "test1"}, {1min, 2, "test2"}, {1min, 3, "test3"}};

        auto inserted = cache.InsertRange(std::move(inserts), allow::update);
        REQUIRE(inserted == 0);
    }

    REQUIRE(cache.size() == 0);
    REQUIRE_FALSE(cache.Find(1).has_value());
    REQUIRE_FALSE(cache.Find(2).has_value());
    REQUIRE_FALSE(cache.Find(3).has_value());
}

TEST_CASE("Tlru InsertRange Insert Or Update")
{
    TlruCache<uint64_t, std::string> cache{4};

    {
        std::vector<std::tuple<std::chrono::minutes, uint64_t, std::string>> inserts{
            {1min, 1, "test1"}, {1min, 2, "test2"}, {1min, 3, "test3"}};

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
        std::vector<std::tuple<std::chrono::minutes, uint64_t, std::string>> inserts{
            {1min, 2, "test2"}, // make 2 Tlru
            {1min, 1, "test1"},
            {1min, 3, "test3"},
            {1min, 4, "test4"}, // new
            {1min, 5, "test5"}, // new
        };

        auto inserted = cache.InsertRange(std::move(inserts));
        REQUIRE(inserted == 5);
    }

    REQUIRE(cache.size() == 4);
    REQUIRE(cache.Find(1).has_value());
    REQUIRE(cache.Find(1).value() == "test1");
    REQUIRE_FALSE(cache.Find(2).has_value()); // evicted by Tlru policy
    REQUIRE(cache.Find(3).has_value());
    REQUIRE(cache.Find(3).value() == "test3");
    REQUIRE(cache.Find(4).has_value());
    REQUIRE(cache.Find(4).value() == "test4");
    REQUIRE(cache.Find(5).has_value());
    REQUIRE(cache.Find(5).value() == "test5");
}

TEST_CASE("Tlru Delete")
{
    TlruCache<uint64_t, std::string> cache{4};

    REQUIRE(cache.Insert(1min, 1, "test", allow::insert));
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

TEST_CASE("Tlru DeleteRange")
{
    TlruCache<uint64_t, std::string> cache{4};

    {
        std::vector<std::tuple<std::chrono::minutes, uint64_t, std::string>> inserts{
            {1min, 1, "test1"}, {1min, 2, "test2"}, {1min, 3, "test3"}};

        auto inserted = cache.InsertRange(std::move(inserts));
        REQUIRE(inserted == 3);
    }

    REQUIRE(cache.size() == 3);
    REQUIRE(cache.Find(1).has_value());
    REQUIRE(cache.Find(2).has_value());
    REQUIRE(cache.Find(3).has_value());

    {
        std::vector<uint64_t> delete_keys{1, 3, 4, 5};

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

TEST_CASE("Tlru FindRange")
{
    TlruCache<uint64_t, std::string> cache{4};

    {
        std::vector<std::tuple<std::chrono::minutes, uint64_t, std::string>> inserts{
            {1min, 1, "test1"}, {1min, 2, "test2"}, {1min, 3, "test3"}};

        auto inserted = cache.InsertRange(std::move(inserts));
        REQUIRE(inserted == 3);
    }

    // Make sure all inserted keys exists via find range.
    {
        std::vector<uint64_t> keys{1, 2, 3};
        auto                  items = cache.FindRange(keys);

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
        std::vector<uint64_t> keys{1, 3, 4, 5};
        auto                  items = cache.FindRange(keys);

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

TEST_CASE("Tlru FindRangeFill")
{
    TlruCache<uint64_t, std::string> cache{4};

    {
        std::vector<std::tuple<std::chrono::minutes, uint64_t, std::string>> inserts{
            {1min, 1, "test1"}, {1min, 2, "test2"}, {1min, 3, "test3"}};

        auto inserted = cache.InsertRange(std::move(inserts));
        REQUIRE(inserted == 3);
    }

    // Make sure all inserted keys exists via find range.
    {
        std::vector<std::pair<uint64_t, std::optional<std::string>>> items{
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
        std::vector<std::pair<uint64_t, std::optional<std::string>>> items{
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

TEST_CASE("Tlru empty")
{
    TlruCache<uint64_t, std::string> cache{4};

    REQUIRE(cache.empty());
    REQUIRE(cache.Insert(1min, 1, "test", allow::insert));
    REQUIRE_FALSE(cache.empty());
    REQUIRE(cache.Delete(1));
    REQUIRE(cache.empty());
}

TEST_CASE("Tlru size + capacity")
{
    TlruCache<uint64_t, std::string> cache{4};

    REQUIRE(cache.capacity() == 4);

    REQUIRE(cache.Insert(1min, 1, "test1"));
    REQUIRE(cache.size() == 1);

    REQUIRE(cache.Insert(1min, 2, "test2"));
    REQUIRE(cache.size() == 2);

    REQUIRE(cache.Insert(1min, 3, "test3"));
    REQUIRE(cache.size() == 3);

    REQUIRE(cache.Insert(1min, 4, "test4"));
    REQUIRE(cache.size() == 4);

    REQUIRE(cache.Insert(1min, 5, "test5"));
    REQUIRE(cache.size() == 4);

    REQUIRE(cache.Insert(1min, 6, "test6"));
    REQUIRE(cache.size() == 4);

    REQUIRE(cache.capacity() == 4);
}

TEST_CASE("Tlru Find with Peek")
{
    TlruCache<uint64_t, std::string> cache{4};

    REQUIRE(cache.Insert(1min, 1, "Hello"));
    REQUIRE(cache.Insert(1min, 2, "World"));
    REQUIRE(cache.Insert(1min, 3, "Hola"));
    REQUIRE(cache.Insert(1min, 4, "Mondo"));

    REQUIRE(cache.Find(1, Peek::YES).has_value()); // doesn't move up to MRU
    REQUIRE(cache.Find(2, Peek::NO).has_value());
    REQUIRE(cache.Find(3, Peek::YES).has_value()); // doesn't move up to MRU
    REQUIRE(cache.Find(4, Peek::NO).has_value());

    REQUIRE(cache.Insert(1min, 5, "another one bites the dust1"));
    REQUIRE_FALSE(cache.Find(1).has_value());
    REQUIRE(cache.Insert(1min, 6, "another one bites the dust2"));
    REQUIRE_FALSE(cache.Find(3).has_value());
}

TEST_CASE("Tlru differnet ttls")
{
    TlruCache<uint64_t, std::string> cache{2};

    REQUIRE(cache.Insert(10ms, 1, "Hello"));
    REQUIRE(cache.Insert(100ms, 2, "World"));

    REQUIRE(cache.Find(1).has_value()); // Make sure its most recently used but less TTL

    std::this_thread::sleep_for(50ms);

    REQUIRE(cache.Insert(100ms, 3, "Hola"));

    auto hello = cache.Find(1);
    auto world = cache.Find(2);
    auto hola  = cache.Find(3);

    REQUIRE_FALSE(hello.has_value());
    REQUIRE(world.has_value());
    REQUIRE(hola.has_value());
    REQUIRE(world.value() == "World");
    REQUIRE(hola.value() == "Hola");
}

TEST_CASE("TlruCache Insert only long running test.")
{
    // This test is to make sure that an item that is continuously inserted into the cache
    // with allow::insert only and its the only item inserted that it will eventually
    // be evicted by its TTL.

    TlruCache<std::string, std::monostate> cache{128};

    uint64_t inserted{0};
    uint64_t blocked{0};

    auto start = std::chrono::steady_clock::now();

    while (inserted < 5)
    {
        if (cache.Insert(50ms, "test-key", std::monostate{}, allow::insert))
        {
            ++inserted;
            std::cout << "inserted=" << inserted << "\n";
            std::cout << "blocked=" << blocked << "\n";
        }
        else
        {
            ++blocked;
        }
        std::this_thread::sleep_for(1ms);
    }

    auto stop = std::chrono::steady_clock::now();

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);

    std::cout << "total_inserted=" << inserted << "\n";
    std::cout << "total_blocked=" << blocked << "\n";
    std::cout << "total_elapsed=" << elapsed.count() << "\n\n";

    REQUIRE(inserted == 5);
    REQUIRE(blocked > inserted);
    REQUIRE(elapsed >= std::chrono::milliseconds{200});
}
