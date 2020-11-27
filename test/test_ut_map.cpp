#include "catch.hpp"
#include <cappuccino/cappuccino.hpp>

#include <chrono>
#include <iostream>
#include <thread>
#include <variant>

using namespace cappuccino;
using namespace std::chrono_literals;

TEST_CASE("ut_map example")
{
    // Create a map with 2 items and a uniform TTL of 20ms.
    ut_map<std::string, bool> map{20ms};

    // Insert "hello" and "world".
    map.insert("Hello", false);
    map.insert("World", true);

    {
        // Fetch the items from the map.
        auto hello = map.find("Hello");
        auto world = map.find("World");

        REQUIRE(hello.has_value());
        REQUIRE(world.has_value());

        REQUIRE(hello.value() == false);
        REQUIRE(world.value() == true);
    }

    // Sleep for an ~order of magnitude longer than the TTL.
    std::this_thread::sleep_for(100ms);

    // Manually trigger a clean since no insert/delete is wanted.
    auto cleaned_count = map.clean_expired_values();
    REQUIRE(cleaned_count == 2);
    REQUIRE(map.empty());
}

TEST_CASE("ut_map Find doesn't exist")
{
    ut_map<uint64_t, std::string> map{50ms};
    REQUIRE_FALSE(map.find(100).has_value());
}

TEST_CASE("ut_map Insert Only")
{
    ut_map<uint64_t, std::string> map{50ms};

    REQUIRE(map.insert(1, "test", allow::insert));
    auto value = map.find(1);
    REQUIRE(value.has_value());
    REQUIRE(value.value() == "test");

    REQUIRE_FALSE(map.insert(1, "test2", allow::insert));
    value = map.find(1);
    REQUIRE(value.has_value());
    REQUIRE(value.value() == "test");
}

TEST_CASE("ut_map Update Only")
{
    ut_map<uint64_t, std::string> map{50ms};

    REQUIRE_FALSE(map.insert(1, "test", allow::update));
    auto value = map.find(1);
    REQUIRE_FALSE(value.has_value());
}

TEST_CASE("ut_map Insert Or Update")
{
    ut_map<uint64_t, std::string> map{50ms};

    REQUIRE(map.insert(1, "test"));
    auto value = map.find(1);
    REQUIRE(value.has_value());
    REQUIRE(value.value() == "test");

    REQUIRE(map.insert(1, "test2"));
    value = map.find(1);
    REQUIRE(value.has_value());
    REQUIRE(value.value() == "test2");
}

TEST_CASE("ut_map InsertRange Insert Only")
{
    ut_map<uint64_t, std::string> map{50ms};

    {
        std::vector<std::pair<uint64_t, std::string>> inserts{{1, "test1"}, {2, "test2"}, {3, "test3"}};

        auto inserted = map.insert_range(std::move(inserts), allow::insert);
        REQUIRE(inserted == 3);
    }

    REQUIRE(map.size() == 3);

    REQUIRE(map.find(2).has_value());
    REQUIRE(map.find(2).value() == "test2");
    REQUIRE(map.find(1).has_value());
    REQUIRE(map.find(1).value() == "test1");
    REQUIRE(map.find(3).has_value());
    REQUIRE(map.find(3).value() == "test3");

    {
        std::vector<std::pair<uint64_t, std::string>> inserts{
            {1, "test1"},
            {2, "test2"},
            {3, "test3"},
            {4, "test4"}, // new
            {5, "test5"}, // new
        };

        auto inserted = map.insert_range(std::move(inserts), allow::insert);
        REQUIRE(inserted == 2);
    }

    REQUIRE(map.size() == 5);
    REQUIRE(map.find(1).has_value());
    REQUIRE(map.find(1).value() == "test1");
    REQUIRE(map.find(2).has_value());
    REQUIRE(map.find(3).has_value());
    REQUIRE(map.find(3).value() == "test3");
    REQUIRE(map.find(4).has_value());
    REQUIRE(map.find(4).value() == "test4");
    REQUIRE(map.find(5).has_value());
    REQUIRE(map.find(5).value() == "test5");
}

TEST_CASE("ut_map InsertRange Update Only")
{
    ut_map<uint64_t, std::string> map{50ms};

    {
        std::vector<std::pair<uint64_t, std::string>> inserts{{1, "test1"}, {2, "test2"}, {3, "test3"}};

        auto inserted = map.insert_range(std::move(inserts), allow::update);
        REQUIRE(inserted == 0);
    }

    REQUIRE(map.size() == 0);
    REQUIRE_FALSE(map.find(1).has_value());
    REQUIRE_FALSE(map.find(2).has_value());
    REQUIRE_FALSE(map.find(3).has_value());
}

TEST_CASE("ut_map InsertRange Insert Or Update")
{
    ut_map<uint64_t, std::string> map{50ms};

    {
        std::vector<std::pair<uint64_t, std::string>> inserts{{1, "test1"}, {2, "test2"}, {3, "test3"}};

        auto inserted = map.insert_range(std::move(inserts));
        REQUIRE(inserted == 3);
    }

    REQUIRE(map.size() == 3);
    REQUIRE(map.find(1).has_value());
    REQUIRE(map.find(1).value() == "test1");
    REQUIRE(map.find(2).has_value());
    REQUIRE(map.find(2).value() == "test2");
    REQUIRE(map.find(3).has_value());
    REQUIRE(map.find(3).value() == "test3");

    {
        std::vector<std::pair<uint64_t, std::string>> inserts{
            {2, "test2"},
            {1, "test1"},
            {3, "test3"},
            {4, "test4"}, // new
            {5, "test5"}, // new
        };

        auto inserted = map.insert_range(std::move(inserts));
        REQUIRE(inserted == 5);
    }

    REQUIRE(map.size() == 5);
    REQUIRE(map.find(1).has_value());
    REQUIRE(map.find(1).value() == "test1");
    REQUIRE(map.find(2).has_value());
    REQUIRE(map.find(3).has_value());
    REQUIRE(map.find(3).value() == "test3");
    REQUIRE(map.find(4).has_value());
    REQUIRE(map.find(4).value() == "test4");
    REQUIRE(map.find(5).has_value());
    REQUIRE(map.find(5).value() == "test5");
}

TEST_CASE("ut_map Delete")
{
    ut_map<uint64_t, std::string> map{50ms};

    REQUIRE(map.insert(1, "test", allow::insert));
    auto value = map.find(1);
    REQUIRE(value.has_value());
    REQUIRE(value.value() == "test");
    REQUIRE(map.size() == 1);

    map.erase(1);
    value = map.find(1);
    REQUIRE_FALSE(value.has_value());
    REQUIRE(map.size() == 0);
    REQUIRE(map.empty());

    REQUIRE_FALSE(map.erase(200));
}

TEST_CASE("ut_map DeleteRange")
{
    ut_map<uint64_t, std::string> map{50ms};

    {
        std::vector<std::pair<uint64_t, std::string>> inserts{{1, "test1"}, {2, "test2"}, {3, "test3"}};

        auto inserted = map.insert_range(std::move(inserts));
        REQUIRE(inserted == 3);
    }

    REQUIRE(map.size() == 3);
    REQUIRE(map.find(1).has_value());
    REQUIRE(map.find(2).has_value());
    REQUIRE(map.find(3).has_value());

    {
        std::vector<uint64_t> delete_keys{1, 3, 4, 5};

        auto deleted = map.erase_range(delete_keys);
        REQUIRE(deleted == 2);
    }

    REQUIRE(map.size() == 1);
    REQUIRE_FALSE(map.find(1).has_value());
    REQUIRE(map.find(2).has_value());
    REQUIRE(map.find(2).value() == "test2");
    REQUIRE_FALSE(map.find(3).has_value());
    REQUIRE_FALSE(map.find(4).has_value());
    REQUIRE_FALSE(map.find(5).has_value());
}

TEST_CASE("ut_map FindRange")
{
    ut_map<uint64_t, std::string> map{50ms};

    {
        std::vector<std::pair<uint64_t, std::string>> inserts{{1, "test1"}, {2, "test2"}, {3, "test3"}};

        auto inserted = map.insert_range(std::move(inserts));
        REQUIRE(inserted == 3);
    }

    // Make sure all inserted keys exists via find range.
    {
        std::vector<uint64_t> keys{1, 2, 3};
        auto                  items = map.find_range(keys);

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
        auto                  items = map.find_range(keys);

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

TEST_CASE("ut_map FindRangeFill")
{
    ut_map<uint64_t, std::string> map{50ms};

    {
        std::vector<std::pair<uint64_t, std::string>> inserts{{1, "test1"}, {2, "test2"}, {3, "test3"}};

        auto inserted = map.insert_range(std::move(inserts));
        REQUIRE(inserted == 3);
    }

    // Make sure all inserted keys exists via find range.
    {
        std::vector<std::pair<uint64_t, std::optional<std::string>>> items{
            {1, std::nullopt},
            {2, std::nullopt},
            {3, std::nullopt},
        };
        map.find_range_fill(items);

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
        map.find_range_fill(items);

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

TEST_CASE("ut_map empty")
{
    ut_map<uint64_t, std::string> map{50ms};

    REQUIRE(map.empty());
    REQUIRE(map.insert(1, "test", allow::insert));
    REQUIRE_FALSE(map.empty());
    REQUIRE(map.erase(1));
    REQUIRE(map.empty());
}

TEST_CASE("ut_map size")
{
    ut_map<uint64_t, std::string> map{50ms};

    REQUIRE(map.insert(1, "test1"));
    REQUIRE(map.size() == 1);

    REQUIRE(map.insert(2, "test2"));
    REQUIRE(map.size() == 2);

    REQUIRE(map.insert(3, "test3"));
    REQUIRE(map.size() == 3);

    REQUIRE(map.insert(4, "test4"));
    REQUIRE(map.size() == 4);

    REQUIRE(map.insert(5, "test5"));
    REQUIRE(map.size() == 5);

    REQUIRE(map.insert(6, "test6"));
    REQUIRE(map.size() == 6);

    REQUIRE(map.erase(6));
    REQUIRE(map.size() == 5);
    REQUIRE_FALSE(map.empty());
}

TEST_CASE("ut_map ttls")
{
    ut_map<uint64_t, std::string> map{20ms};

    REQUIRE(map.insert(1, "Hello"));
    REQUIRE(map.insert(2, "World"));

    std::this_thread::sleep_for(50ms);

    REQUIRE(map.insert(3, "Hola"));

    auto hello = map.find(1);
    auto world = map.find(2);
    auto hola  = map.find(3);

    REQUIRE_FALSE(hello.has_value());
    REQUIRE_FALSE(world.has_value());
    REQUIRE(hola.has_value());
    REQUIRE(hola.value() == "Hola");
}

TEST_CASE("ut_map Clean with only some expired")
{
    ut_map<uint64_t, std::string> map{25ms};

    REQUIRE(map.insert(1, "Hello"));
    REQUIRE(map.insert(2, "World"));

    std::this_thread::sleep_for(50ms);

    REQUIRE(map.insert(3, "Hola"));

    map.clean_expired_values();

    auto hello = map.find(1);
    auto world = map.find(2);
    auto hola  = map.find(3);

    REQUIRE_FALSE(hello.has_value());
    REQUIRE_FALSE(world.has_value());
    REQUIRE(hola.has_value());
    REQUIRE(hola.value() == "Hola");
}

TEST_CASE("ut_map bulk insert some expire")
{
    ut_map<uint64_t, uint64_t> map{50ms};

    for (uint64_t i = 0; i < 100; ++i)
    {
        REQUIRE(map.insert(i, i));
    }

    REQUIRE(map.size() == 100);

    std::this_thread::sleep_for(250ms);

    map.clean_expired_values();
    REQUIRE(map.size() == 0);

    for (uint64_t i = 100; i < 200; ++i)
    {
        REQUIRE(map.insert(i, i));
    }

    REQUIRE(map.size() == 100);

    for (uint64_t i = 0; i < 100; ++i)
    {
        auto opt = map.find(i);
        REQUIRE_FALSE(opt.has_value());
    }

    for (uint64_t i = 100; i < 200; ++i)
    {
        REQUIRE(map.find(i).has_value());
    }

    REQUIRE(map.size() == 100);
}

TEST_CASE("ut_map update TTLs some expire")
{
    ut_map<std::string, uint64_t> map{}; // 100ms default

    REQUIRE(map.insert("Hello", 1));
    REQUIRE(map.insert("World", 2));

    std::this_thread::sleep_for(80ms);

    // Update "Hello" TTL, but not "World".
    REQUIRE(map.insert("Hello", 1, allow::update));
    REQUIRE_FALSE(map.insert("World", 1, allow::insert));

    // Total of ~160ms, "World" should expire.
    std::this_thread::sleep_for(80ms);

    auto hello = map.find("Hello");
    auto world = map.find("World");

    REQUIRE(map.size() == 1);
    REQUIRE(hello.has_value());
    REQUIRE(hello.value() == 1);
    REQUIRE_FALSE(world.has_value());

    // Total of ~240ms, "Hello" should expire.
    std::this_thread::sleep_for(80ms);

    hello = map.find("Hello");
    world = map.find("World");

    REQUIRE(map.empty());
    REQUIRE_FALSE(hello.has_value());
    REQUIRE_FALSE(world.has_value());
}

TEST_CASE("ut_map update element value")
{
    ut_map<std::string, uint64_t> map{}; // 100ms default

    REQUIRE(map.insert("Hello", 1));
    REQUIRE(map.insert("World", 2));

    std::this_thread::sleep_for(80ms);

    // Update "Hello", but not "World".
    REQUIRE(map.insert("Hello", 3, allow::update));
    REQUIRE_FALSE(map.insert("World", 4, allow::insert));

    auto hello = map.find("Hello");
    auto world = map.find("World");

    REQUIRE(hello.has_value());
    REQUIRE(hello.value() == 3);
    REQUIRE(world.has_value());
    REQUIRE(world.value() == 2);

    // Total of ~160ms, "World" should expire.
    std::this_thread::sleep_for(80ms);

    hello = map.find("Hello");
    world = map.find("World");

    REQUIRE(map.size() == 1);
    REQUIRE(hello.has_value());
    REQUIRE(hello.value() == 3); // updated value
    REQUIRE_FALSE(world.has_value());
}

TEST_CASE("ut_map inserts, updates, and insert_or_update")
{
    ut_map<std::string, uint64_t> map{}; // 100ms default

    REQUIRE(map.insert("Hello", 1, allow::insert));
    REQUIRE(map.insert("World", 2, allow::insert_or_update));
    REQUIRE(map.insert("Hola", 3)); // defaults to allow::insert_or_update

    REQUIRE_FALSE(map.insert("Friend", 4, allow::update));
    REQUIRE_FALSE(map.insert("Hello", 5, allow::insert));

    auto hello = map.find("Hello");
    auto world = map.find("World");
    auto hola  = map.find("Hola");
    auto frand = map.find("Friend");

    REQUIRE(hello.has_value());
    REQUIRE(hello.value() == 1);

    REQUIRE(world.has_value());
    REQUIRE(world.value() == 2);

    REQUIRE(hola.has_value());
    REQUIRE(hola.value() == 3);

    REQUIRE_FALSE(frand.has_value());

    REQUIRE(map.insert("Hello", 6, allow::update));

    hello = map.find("Hello");

    REQUIRE(hello.has_value());
    REQUIRE(hello.value() == 6);
}

TEST_CASE("ut_map Insert only long running test.")
{
    // This test is to make sure that an item that is continuously inserted into
    // the cache with allow::insert only and its the only item inserted that it
    // will eventually be evicted by its TTL.

    ut_map<std::string, std::monostate> cache{50ms};

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
