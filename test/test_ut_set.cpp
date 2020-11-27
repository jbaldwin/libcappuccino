#include "catch.hpp"
#include <cappuccino/cappuccino.hpp>

#include <chrono>
#include <iostream>
#include <thread>

using namespace cappuccino;
using namespace std::chrono_literals;

TEST_CASE("ut_set example")
{
    // Create a set with 2 items and a uniform TTL of 20ms.
    ut_set<std::string> set{20ms};

    // Insert "hello" and "world".
    set.insert("Hello");
    set.insert("World");

    {
        // Fetch the items from the set.
        auto hello = set.find("Hello");
        auto world = set.find("World");

        REQUIRE(hello);
        REQUIRE(world);
    }

    // Sleep for an ~order of magnitude longer than the TTL.
    std::this_thread::sleep_for(100ms);

    // Manually trigger a clean since no insert/delete is wanted.
    auto cleaned_count = set.clean_expired_values();
    REQUIRE(cleaned_count == 2);
    REQUIRE(set.empty());
}

TEST_CASE("ut_set Find doesn't exist")
{
    ut_set<uint64_t> set{50ms};
    REQUIRE_FALSE(set.find(100));
}

TEST_CASE("ut_set Update Only")
{
    ut_set<uint64_t> set{50ms};

    REQUIRE_FALSE(set.insert(1, allow::update));
    REQUIRE_FALSE(set.find(1));
}

TEST_CASE("ut_set Insert Or Update")
{
    ut_set<uint64_t> set{50ms};

    REQUIRE(set.insert(1));
    REQUIRE(set.find(1));

    REQUIRE(set.insert(1));
    REQUIRE(set.find(1));
}

TEST_CASE("ut_set InsertRange Insert Only")
{
    ut_set<uint64_t> set{50ms};

    {
        std::vector<uint64_t> inserts{1, 2, 3};

        auto inserted = set.insert_range(std::move(inserts), allow::insert);
        REQUIRE(inserted == 3);
    }

    REQUIRE(set.size() == 3);

    REQUIRE(set.find(2));
    REQUIRE(set.find(1));
    REQUIRE(set.find(3));

    {
        std::vector<uint64_t> inserts{1, 2, 3, 4, 5};

        auto inserted = set.insert_range(std::move(inserts), allow::insert);
        REQUIRE(inserted == 2);
    }

    REQUIRE(set.size() == 5);
    REQUIRE(set.find(1));
    REQUIRE(set.find(2));
    REQUIRE(set.find(3));
    REQUIRE(set.find(4));
    REQUIRE(set.find(5));
}

TEST_CASE("ut_set InsertRange Update Only")
{
    ut_set<uint64_t> set{50ms};

    {
        std::vector<uint64_t> inserts{1, 2, 3};

        auto inserted = set.insert_range(std::move(inserts), allow::update);
        REQUIRE(inserted == 0);
    }

    REQUIRE(set.size() == 0);
    REQUIRE_FALSE(set.find(1));
    REQUIRE_FALSE(set.find(2));
    REQUIRE_FALSE(set.find(3));
}

TEST_CASE("ut_set InsertRange Insert Or Update")
{
    ut_set<uint64_t> set{50ms};

    {
        std::vector<uint64_t> inserts{1, 2, 3};

        auto inserted = set.insert_range(std::move(inserts));
        REQUIRE(inserted == 3);
    }

    REQUIRE(set.size() == 3);
    REQUIRE(set.find(1));
    REQUIRE(set.find(2));
    REQUIRE(set.find(3));

    {
        std::vector<uint64_t> inserts{2, 1, 3, 4, 5};

        auto inserted = set.insert_range(std::move(inserts));
        REQUIRE(inserted == 5);
    }

    REQUIRE(set.size() == 5);
    REQUIRE(set.find(1));
    REQUIRE(set.find(2));
    REQUIRE(set.find(3));
    REQUIRE(set.find(4));
    REQUIRE(set.find(5));
}

TEST_CASE("ut_set Delete")
{
    ut_set<uint64_t> set{50ms};

    REQUIRE(set.insert(1, allow::insert));
    REQUIRE(set.find(1));
    REQUIRE(set.size() == 1);

    set.erase(1);
    REQUIRE_FALSE(set.find(1));
    REQUIRE(set.size() == 0);
    REQUIRE(set.empty());

    REQUIRE_FALSE(set.erase(200));
}

TEST_CASE("ut_set DeleteRange")
{
    ut_set<uint64_t> set{50ms};

    {
        std::vector<uint64_t> inserts{1, 2, 3};

        auto inserted = set.insert_range(std::move(inserts));
        REQUIRE(inserted == 3);
    }

    REQUIRE(set.size() == 3);
    REQUIRE(set.find(1));
    REQUIRE(set.find(2));
    REQUIRE(set.find(3));

    {
        std::vector<uint64_t> delete_keys{1, 3, 4, 5};

        auto deleted = set.erase_range(delete_keys);
        REQUIRE(deleted == 2);
    }

    REQUIRE(set.size() == 1);
    REQUIRE_FALSE(set.find(1));
    REQUIRE(set.find(2));
    REQUIRE_FALSE(set.find(3));
    REQUIRE_FALSE(set.find(4));
    REQUIRE_FALSE(set.find(5));
}

TEST_CASE("ut_set FindRange")
{
    ut_set<uint64_t> set{50ms};

    {
        std::vector<uint64_t> inserts{1, 2, 3};

        auto inserted = set.insert_range(std::move(inserts));
        REQUIRE(inserted == 3);
    }

    // Make sure all inserted keys exists via find range.
    {
        std::vector<uint64_t> keys{1, 2, 3};
        auto                  items = set.find_range(keys);

        REQUIRE(items[0].first == 1);
        REQUIRE(items[0].second);
        REQUIRE(items[1].first == 2);
        REQUIRE(items[1].second);
        REQUIRE(items[2].first == 3);
        REQUIRE(items[2].second);
    }

    // Make sure keys not inserted are not found by find range.
    {
        std::vector<uint64_t> keys{1, 3, 4, 5};
        auto                  items = set.find_range(keys);

        REQUIRE(items[0].first == 1);
        REQUIRE(items[0].second);
        REQUIRE(items[1].first == 3);
        REQUIRE(items[1].second);
        REQUIRE(items[2].first == 4);
        REQUIRE_FALSE(items[2].second);
        REQUIRE(items[3].first == 5);
        REQUIRE_FALSE(items[3].second);
    }
}

TEST_CASE("ut_set FindRangeFill")
{
    ut_set<uint64_t> set{50ms};

    {
        std::vector<uint64_t> inserts{1, 2, 3};

        auto inserted = set.insert_range(std::move(inserts));
        REQUIRE(inserted == 3);
    }

    // Make sure all inserted keys exists via find range.
    {
        std::vector<std::pair<uint64_t, bool>> items{
            {1, false},
            {2, false},
            {3, false},
        };
        set.find_range_fill(items);

        REQUIRE(items[0].first == 1);
        REQUIRE(items[0].second);
        REQUIRE(items[1].first == 2);
        REQUIRE(items[1].second);
        REQUIRE(items[2].first == 3);
        REQUIRE(items[2].second);
    }

    // Make sure keys not inserted are not found by find range.
    {
        std::vector<std::pair<uint64_t, bool>> items{
            {1, false},
            {3, false},
            {4, false},
            {5, false},
        };
        set.find_range_fill(items);

        REQUIRE(items[0].first == 1);
        REQUIRE(items[0].second);
        REQUIRE(items[1].first == 3);
        REQUIRE(items[1].second);
        REQUIRE(items[2].first == 4);
        REQUIRE_FALSE(items[2].second);
        REQUIRE(items[3].first == 5);
        REQUIRE_FALSE(items[3].second);
    }
}

TEST_CASE("ut_set empty")
{
    ut_set<uint64_t> set{50ms};

    REQUIRE(set.empty());
    REQUIRE(set.insert(1, allow::insert));
    REQUIRE_FALSE(set.empty());
    REQUIRE(set.erase(1));
    REQUIRE(set.empty());
}

TEST_CASE("ut_set size")
{
    ut_set<uint64_t> set{50ms};

    REQUIRE(set.insert(1));
    REQUIRE(set.size() == 1);

    REQUIRE(set.insert(2));
    REQUIRE(set.size() == 2);

    REQUIRE(set.insert(3));
    REQUIRE(set.size() == 3);

    REQUIRE(set.insert(4));
    REQUIRE(set.size() == 4);

    REQUIRE(set.insert(5));
    REQUIRE(set.size() == 5);

    REQUIRE(set.insert(6));
    REQUIRE(set.size() == 6);

    REQUIRE(set.erase(6));
    REQUIRE(set.size() == 5);
    REQUIRE_FALSE(set.empty());
}

TEST_CASE("ut_set ttls")
{
    ut_set<uint64_t> set{20ms};

    REQUIRE(set.insert(1));
    REQUIRE(set.insert(2));

    std::this_thread::sleep_for(50ms);

    REQUIRE(set.insert(3));

    REQUIRE_FALSE(set.find(1));
    REQUIRE_FALSE(set.find(2));
    REQUIRE(set.find(3));
}

TEST_CASE("ut_set Clean with only some expired")
{
    ut_set<uint64_t> set{25ms};

    REQUIRE(set.insert(1));
    REQUIRE(set.insert(2));

    std::this_thread::sleep_for(50ms);

    REQUIRE(set.insert(3));

    set.clean_expired_values();

    REQUIRE_FALSE(set.find(1));
    REQUIRE_FALSE(set.find(2));
    REQUIRE(set.find(3));
}

TEST_CASE("ut_set bulk insert and some expire")
{
    ut_set<uint64_t> set{25ms};

    for (uint64_t i = 0; i < 100; ++i)
    {
        REQUIRE(set.insert(i));
    }

    REQUIRE(set.size() == 100);

    std::this_thread::sleep_for(50ms);

    set.clean_expired_values();
    REQUIRE(set.size() == 0);
    REQUIRE(set.empty());

    for (uint64_t i = 100; i < 200; ++i)
    {
        REQUIRE(set.insert(i));
    }

    REQUIRE(set.size() == 100);

    for (uint64_t i = 0; i < 100; ++i)
    {
        REQUIRE_FALSE(set.find(i));
    }

    for (uint64_t i = 100; i < 200; ++i)
    {
        REQUIRE(set.find(i));
    }

    REQUIRE(set.size() == 100);
}

TEST_CASE("ut_set update TTLs some expire")
{
    ut_set<std::string> set{}; // 100ms default

    REQUIRE(set.insert("Hello"));
    REQUIRE(set.insert("World"));

    std::this_thread::sleep_for(80ms);

    // Update "Hello" TTL, but not "World".
    REQUIRE(set.insert("Hello", allow::update));
    REQUIRE_FALSE(set.insert("World", allow::insert));

    // Total of ~160ms, "World" should expire.
    std::this_thread::sleep_for(80ms);

    REQUIRE(set.find("Hello"));
    REQUIRE_FALSE(set.find("World"));
    REQUIRE(set.size() == 1);

    // Total of ~240ms, "Hello" should expire.
    std::this_thread::sleep_for(80ms);

    REQUIRE_FALSE(set.find("Hello"));
    REQUIRE_FALSE(set.find("World"));
    REQUIRE(set.empty());
}

TEST_CASE("ut_set inserts, updates, and insert_or_update")
{
    ut_set<std::string> set{};

    REQUIRE(set.insert("Hello", allow::insert));
    REQUIRE(set.insert("World", allow::insert_or_update));
    REQUIRE(set.insert("Hola")); // defaults to allow::insert_or_update

    REQUIRE_FALSE(set.insert("Friend", allow::update));
    REQUIRE_FALSE(set.insert("Hello", allow::insert));

    REQUIRE(set.find("Hello"));
    REQUIRE(set.find("World"));
    REQUIRE(set.find("Hola"));
    REQUIRE_FALSE(set.find("Friend"));

    REQUIRE(set.insert("Hello", allow::update));
    REQUIRE(set.find("Hello"));
}

TEST_CASE("ut_set Insert only long running test.")
{
    // This test is to make sure that an item that is continuously inserted into
    // the cache with allow::insert only and its the only item inserted that it
    // will eventually be evicted by its TTL.

    ut_set<std::string> cache{50ms};

    uint64_t inserted{0};
    uint64_t blocked{0};

    auto start = std::chrono::steady_clock::now();

    while (inserted < 5)
    {
        if (cache.insert("test-key", allow::insert))
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
