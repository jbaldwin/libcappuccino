#include "catch.hpp"
#include <cappuccino/cappuccino.hpp>

#include <chrono>
#include <iostream>
#include <thread>
#include <variant>

using namespace cappuccino;
using namespace std::chrono_literals;

TEST_CASE("UtUtlru example")
{
    // Create a cache with 2 items and a uniform TTL of 20ms.
    utlru_cache<uint64_t, std::string> cache{20ms, 2};

    // Insert "hello" and "world".
    cache.insert(1, "Hello");
    cache.insert(2, "World");

    {
        // Fetch the items from the catch, this will update their Utlru positions.
        auto hello = cache.find(1);
        auto world = cache.find(2);

        REQUIRE(hello.has_value());
        REQUIRE(world.has_value());

        REQUIRE(hello.value() == "Hello");
        REQUIRE(world.value() == "World");
    }

    // Insert "Hola", this will replace "Hello" since its the oldest Utlru item
    // and nothing has expired yet.
    cache.insert(3, "Hola");

    {
        auto hola  = cache.find(3);
        auto hello = cache.find(1); // Hello isn't in the cache anymore and will return an empty optional.
        auto world = cache.find(2);

        REQUIRE(hola.has_value());
        REQUIRE_FALSE(hello.has_value());
        REQUIRE(world.has_value());
    }

    // Sleep for an ~order of magnitude longer than the TTL.
    std::this_thread::sleep_for(100ms);

    // Manually trigger a clean since no insert/delete is wanted.
    auto cleaned_count = cache.clean_expired_values();
    REQUIRE(cleaned_count == 2);
    REQUIRE(cache.empty());
}

TEST_CASE("Utlru Find doesn't exist")
{
    utlru_cache<uint64_t, std::string> cache{50ms, 4};
    REQUIRE_FALSE(cache.find(100).has_value());
}

TEST_CASE("Utlru Insert Only")
{
    utlru_cache<uint64_t, std::string> cache{50ms, 4};

    REQUIRE(cache.insert(1, "test", allow::insert));
    auto value = cache.find(1);
    REQUIRE(value.has_value());
    REQUIRE(value.value() == "test");

    REQUIRE_FALSE(cache.insert(1, "test2", allow::insert));
    value = cache.find(1);
    REQUIRE(value.has_value());
    REQUIRE(value.value() == "test");
}

TEST_CASE("Utlru Update Only")
{
    utlru_cache<uint64_t, std::string> cache{50ms, 4};

    REQUIRE_FALSE(cache.insert(1, "test", allow::update));
    auto value = cache.find(1);
    REQUIRE_FALSE(value.has_value());
}

TEST_CASE("Utlru Insert Or Update")
{
    utlru_cache<uint64_t, std::string> cache{50ms, 4};

    REQUIRE(cache.insert(1, "test"));
    auto value = cache.find(1);
    REQUIRE(value.has_value());
    REQUIRE(value.value() == "test");

    REQUIRE(cache.insert(1, "test2"));
    value = cache.find(1);
    REQUIRE(value.has_value());
    REQUIRE(value.value() == "test2");
}

TEST_CASE("Utlru InsertRange Insert Only")
{
    utlru_cache<uint64_t, std::string> cache{50ms, 4};

    {
        std::vector<std::pair<uint64_t, std::string>> inserts{{1, "test1"}, {2, "test2"}, {3, "test3"}};

        auto inserted = cache.insert_range(std::move(inserts), allow::insert);
        REQUIRE(inserted == 3);
    }

    REQUIRE(cache.size() == 3);

    REQUIRE(cache.find(2).has_value());
    REQUIRE(cache.find(2).value() == "test2"); // make 2 Utlru
    REQUIRE(cache.find(1).has_value());
    REQUIRE(cache.find(1).value() == "test1");
    REQUIRE(cache.find(3).has_value());
    REQUIRE(cache.find(3).value() == "test3");

    {
        std::vector<std::pair<uint64_t, std::string>> inserts{
            {1, "test1"},
            {2, "test2"},
            {3, "test3"},
            {4, "test4"}, // new
            {5, "test5"}, // new
        };

        auto inserted = cache.insert_range(std::move(inserts), allow::insert);
        REQUIRE(inserted == 2);
    }

    REQUIRE(cache.size() == 4);
    REQUIRE(cache.find(1).has_value());
    REQUIRE(cache.find(1).value() == "test1");
    REQUIRE_FALSE(cache.find(2).has_value()); // evicted by Utlru policy
    REQUIRE(cache.find(3).has_value());
    REQUIRE(cache.find(3).value() == "test3");
    REQUIRE(cache.find(4).has_value());
    REQUIRE(cache.find(4).value() == "test4");
    REQUIRE(cache.find(5).has_value());
    REQUIRE(cache.find(5).value() == "test5");
}

TEST_CASE("Utlru InsertRange Update Only")
{
    utlru_cache<uint64_t, std::string> cache{50ms, 4};

    {
        std::vector<std::pair<uint64_t, std::string>> inserts{{1, "test1"}, {2, "test2"}, {3, "test3"}};

        auto inserted = cache.insert_range(std::move(inserts), allow::update);
        REQUIRE(inserted == 0);
    }

    REQUIRE(cache.size() == 0);
    REQUIRE_FALSE(cache.find(1).has_value());
    REQUIRE_FALSE(cache.find(2).has_value());
    REQUIRE_FALSE(cache.find(3).has_value());
}

TEST_CASE("Utlru InsertRange Insert Or Update")
{
    utlru_cache<uint64_t, std::string> cache{50ms, 4};

    {
        std::vector<std::pair<uint64_t, std::string>> inserts{{1, "test1"}, {2, "test2"}, {3, "test3"}};

        auto inserted = cache.insert_range(std::move(inserts));
        REQUIRE(inserted == 3);
    }

    REQUIRE(cache.size() == 3);
    REQUIRE(cache.find(1).has_value());
    REQUIRE(cache.find(1).value() == "test1");
    REQUIRE(cache.find(2).has_value());
    REQUIRE(cache.find(2).value() == "test2");
    REQUIRE(cache.find(3).has_value());
    REQUIRE(cache.find(3).value() == "test3");

    {
        std::vector<std::pair<uint64_t, std::string>> inserts{
            {2, "test2"}, // make 2 Utlru
            {1, "test1"},
            {3, "test3"},
            {4, "test4"}, // new
            {5, "test5"}, // new
        };

        auto inserted = cache.insert_range(std::move(inserts));
        REQUIRE(inserted == 5);
    }

    REQUIRE(cache.size() == 4);
    REQUIRE(cache.find(1).has_value());
    REQUIRE(cache.find(1).value() == "test1");
    REQUIRE_FALSE(cache.find(2).has_value()); // evicted by Utlru policy
    REQUIRE(cache.find(3).has_value());
    REQUIRE(cache.find(3).value() == "test3");
    REQUIRE(cache.find(4).has_value());
    REQUIRE(cache.find(4).value() == "test4");
    REQUIRE(cache.find(5).has_value());
    REQUIRE(cache.find(5).value() == "test5");
}

TEST_CASE("Utlru Delete")
{
    utlru_cache<uint64_t, std::string> cache{50ms, 4};

    REQUIRE(cache.insert(1, "test", allow::insert));
    auto value = cache.find(1);
    REQUIRE(value.has_value());
    REQUIRE(value.value() == "test");
    REQUIRE(cache.size() == 1);

    cache.erase(1);
    value = cache.find(1);
    REQUIRE_FALSE(value.has_value());
    REQUIRE(cache.size() == 0);
    REQUIRE(cache.empty());

    REQUIRE_FALSE(cache.erase(200));
}

TEST_CASE("Utlru DeleteRange")
{
    utlru_cache<uint64_t, std::string> cache{50ms, 4};

    {
        std::vector<std::pair<uint64_t, std::string>> inserts{{1, "test1"}, {2, "test2"}, {3, "test3"}};

        auto inserted = cache.insert_range(std::move(inserts));
        REQUIRE(inserted == 3);
    }

    REQUIRE(cache.size() == 3);
    REQUIRE(cache.find(1).has_value());
    REQUIRE(cache.find(2).has_value());
    REQUIRE(cache.find(3).has_value());

    {
        std::vector<uint64_t> delete_keys{1, 3, 4, 5};

        auto deleted = cache.erase_range(delete_keys);
        REQUIRE(deleted == 2);
    }

    REQUIRE(cache.size() == 1);
    REQUIRE_FALSE(cache.find(1).has_value());
    REQUIRE(cache.find(2).has_value());
    REQUIRE(cache.find(2).value() == "test2");
    REQUIRE_FALSE(cache.find(3).has_value());
    REQUIRE_FALSE(cache.find(4).has_value());
    REQUIRE_FALSE(cache.find(5).has_value());
}

TEST_CASE("Utlru FindRange")
{
    utlru_cache<uint64_t, std::string> cache{50ms, 4};

    {
        std::vector<std::pair<uint64_t, std::string>> inserts{{1, "test1"}, {2, "test2"}, {3, "test3"}};

        auto inserted = cache.insert_range(std::move(inserts));
        REQUIRE(inserted == 3);
    }

    // Make sure all inserted keys exists via find range.
    {
        std::vector<uint64_t> keys{1, 2, 3};
        auto                  items = cache.find_range(keys);

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
        auto                  items = cache.find_range(keys);

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
    utlru_cache<uint64_t, std::string> cache{50ms, 4};

    {
        std::vector<std::pair<uint64_t, std::string>> inserts{{1, "test1"}, {2, "test2"}, {3, "test3"}};

        auto inserted = cache.insert_range(std::move(inserts));
        REQUIRE(inserted == 3);
    }

    // Make sure all inserted keys exists via find range.
    {
        std::vector<std::pair<uint64_t, std::optional<std::string>>> items{
            {1, std::nullopt},
            {2, std::nullopt},
            {3, std::nullopt},
        };
        cache.find_range_fill(items);

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
        cache.find_range_fill(items);

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
    utlru_cache<uint64_t, std::string> cache{50ms, 4};

    REQUIRE(cache.empty());
    REQUIRE(cache.insert(1, "test", allow::insert));
    REQUIRE_FALSE(cache.empty());
    REQUIRE(cache.erase(1));
    REQUIRE(cache.empty());
}

TEST_CASE("Utlru size + capacity")
{
    utlru_cache<uint64_t, std::string> cache{50ms, 4};

    REQUIRE(cache.capacity() == 4);

    REQUIRE(cache.insert(1, "test1"));
    REQUIRE(cache.size() == 1);

    REQUIRE(cache.insert(2, "test2"));
    REQUIRE(cache.size() == 2);

    REQUIRE(cache.insert(3, "test3"));
    REQUIRE(cache.size() == 3);

    REQUIRE(cache.insert(4, "test4"));
    REQUIRE(cache.size() == 4);

    REQUIRE(cache.insert(5, "test5"));
    REQUIRE(cache.size() == 4);

    REQUIRE(cache.insert(6, "test6"));
    REQUIRE(cache.size() == 4);

    REQUIRE(cache.capacity() == 4);
}

TEST_CASE("Utlru Find with peek")
{
    utlru_cache<uint64_t, std::string> cache{50ms, 4};

    REQUIRE(cache.insert(1, "Hello"));
    REQUIRE(cache.insert(2, "World"));
    REQUIRE(cache.insert(3, "Hola"));
    REQUIRE(cache.insert(4, "Mondo"));

    REQUIRE(cache.find(1, peek::yes).has_value()); // doesn't move up to MRU
    REQUIRE(cache.find(2, peek::no).has_value());
    REQUIRE(cache.find(3, peek::yes).has_value()); // doesn't move up to MRU
    REQUIRE(cache.find(4, peek::no).has_value());

    REQUIRE(cache.insert(5, "another one bites the dust1"));
    REQUIRE_FALSE(cache.find(1).has_value());
    REQUIRE(cache.insert(6, "another one bites the dust2"));
    REQUIRE_FALSE(cache.find(3).has_value());
}

TEST_CASE("Utlru ttls")
{
    utlru_cache<uint64_t, std::string> cache{20ms, 2};

    REQUIRE(cache.insert(1, "Hello"));
    REQUIRE(cache.insert(2, "World"));

    std::this_thread::sleep_for(50ms);

    REQUIRE(cache.insert(3, "Hola"));

    auto hello = cache.find(1);
    auto world = cache.find(2);
    auto hola  = cache.find(3);

    REQUIRE_FALSE(hello.has_value());
    REQUIRE_FALSE(world.has_value());
    REQUIRE(hola.has_value());
    REQUIRE(hola.value() == "Hola");
}

TEST_CASE("Utlru Clean with only some expired")
{
    utlru_cache<uint64_t, std::string> cache{25ms, 2};

    REQUIRE(cache.insert(1, "Hello"));
    REQUIRE(cache.insert(2, "World"));

    std::this_thread::sleep_for(50ms);

    REQUIRE(cache.insert(3, "Hola"));

    cache.clean_expired_values();

    auto hello = cache.find(1);
    auto world = cache.find(2);
    auto hola  = cache.find(3);

    REQUIRE_FALSE(hello.has_value());
    REQUIRE_FALSE(world.has_value());
    REQUIRE(hola.has_value());
    REQUIRE(hola.value() == "Hola");
}

TEST_CASE("utlru_cache Insert only long running test.")
{
    // This test is to make sure that an item that is continuously inserted into the cache
    // with allow::insert only and its the only item inserted that it will eventually
    // be evicted by its TTL.

    utlru_cache<std::string, std::monostate> cache{50ms, 128};

    uint64_t inserted{0};
    uint64_t blocked{0};

    auto start = std::chrono::steady_clock::now();

    while (inserted < 5)
    {
        if (cache.insert("test-key", std::monostate{}, allow::insert))
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
