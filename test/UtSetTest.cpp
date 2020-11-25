#include "catch.hpp"
#include <cappuccino/Cappuccino.hpp>

#include <chrono>
#include <iostream>
#include <thread>

using namespace cappuccino;
using namespace std::chrono_literals;

TEST_CASE("UtSet example")
{
    // Create a set with 2 items and a uniform TTL of 20ms.
    UtSet<std::string> set{20ms};

    // Insert "hello" and "world".
    set.Insert("Hello");
    set.Insert("World");

    {
        // Fetch the items from the set.
        auto hello = set.Find("Hello");
        auto world = set.Find("World");

        REQUIRE(hello);
        REQUIRE(world);
    }

    // Sleep for an ~order of magnitude longer than the TTL.
    std::this_thread::sleep_for(100ms);

    // Manually trigger a clean since no insert/delete is wanted.
    auto cleaned_count = set.CleanExpiredValues();
    REQUIRE(cleaned_count == 2);
    REQUIRE(set.empty());
}

TEST_CASE("UtSet Find doesn't exist")
{
    UtSet<uint64_t> set{50ms};
    REQUIRE_FALSE(set.Find(100));
}

TEST_CASE("UtSet Update Only")
{
    UtSet<uint64_t> set{50ms};

    REQUIRE_FALSE(set.Insert(1, Allow::UPDATE));
    REQUIRE_FALSE(set.Find(1));
}

TEST_CASE("UtSet Insert Or Update")
{
    UtSet<uint64_t> set{50ms};

    REQUIRE(set.Insert(1));
    REQUIRE(set.Find(1));

    REQUIRE(set.Insert(1));
    REQUIRE(set.Find(1));
}

TEST_CASE("UtSet InsertRange Insert Only")
{
    UtSet<uint64_t> set{50ms};

    {
        std::vector<uint64_t> inserts{1, 2, 3};

        auto inserted = set.InsertRange(std::move(inserts), Allow::INSERT);
        REQUIRE(inserted == 3);
    }

    REQUIRE(set.size() == 3);

    REQUIRE(set.Find(2));
    REQUIRE(set.Find(1));
    REQUIRE(set.Find(3));

    {
        std::vector<uint64_t> inserts{1, 2, 3, 4, 5};

        auto inserted = set.InsertRange(std::move(inserts), Allow::INSERT);
        REQUIRE(inserted == 2);
    }

    REQUIRE(set.size() == 5);
    REQUIRE(set.Find(1));
    REQUIRE(set.Find(2));
    REQUIRE(set.Find(3));
    REQUIRE(set.Find(4));
    REQUIRE(set.Find(5));
}

TEST_CASE("UtSet InsertRange Update Only")
{
    UtSet<uint64_t> set{50ms};

    {
        std::vector<uint64_t> inserts{1, 2, 3};

        auto inserted = set.InsertRange(std::move(inserts), Allow::UPDATE);
        REQUIRE(inserted == 0);
    }

    REQUIRE(set.size() == 0);
    REQUIRE_FALSE(set.Find(1));
    REQUIRE_FALSE(set.Find(2));
    REQUIRE_FALSE(set.Find(3));
}

TEST_CASE("UtSet InsertRange Insert Or Update")
{
    UtSet<uint64_t> set{50ms};

    {
        std::vector<uint64_t> inserts{1, 2, 3};

        auto inserted = set.InsertRange(std::move(inserts));
        REQUIRE(inserted == 3);
    }

    REQUIRE(set.size() == 3);
    REQUIRE(set.Find(1));
    REQUIRE(set.Find(2));
    REQUIRE(set.Find(3));

    {
        std::vector<uint64_t> inserts{2, 1, 3, 4, 5};

        auto inserted = set.InsertRange(std::move(inserts));
        REQUIRE(inserted == 5);
    }

    REQUIRE(set.size() == 5);
    REQUIRE(set.Find(1));
    REQUIRE(set.Find(2));
    REQUIRE(set.Find(3));
    REQUIRE(set.Find(4));
    REQUIRE(set.Find(5));
}

TEST_CASE("UtSet Delete")
{
    UtSet<uint64_t> set{50ms};

    REQUIRE(set.Insert(1, Allow::INSERT));
    REQUIRE(set.Find(1));
    REQUIRE(set.size() == 1);

    set.Delete(1);
    REQUIRE_FALSE(set.Find(1));
    REQUIRE(set.size() == 0);
    REQUIRE(set.empty());

    REQUIRE_FALSE(set.Delete(200));
}

TEST_CASE("UtSet DeleteRange")
{
    UtSet<uint64_t> set{50ms};

    {
        std::vector<uint64_t> inserts{1, 2, 3};

        auto inserted = set.InsertRange(std::move(inserts));
        REQUIRE(inserted == 3);
    }

    REQUIRE(set.size() == 3);
    REQUIRE(set.Find(1));
    REQUIRE(set.Find(2));
    REQUIRE(set.Find(3));

    {
        std::vector<uint64_t> delete_keys{1, 3, 4, 5};

        auto deleted = set.DeleteRange(delete_keys);
        REQUIRE(deleted == 2);
    }

    REQUIRE(set.size() == 1);
    REQUIRE_FALSE(set.Find(1));
    REQUIRE(set.Find(2));
    REQUIRE_FALSE(set.Find(3));
    REQUIRE_FALSE(set.Find(4));
    REQUIRE_FALSE(set.Find(5));
}

TEST_CASE("UtSet FindRange")
{
    UtSet<uint64_t> set{50ms};

    {
        std::vector<uint64_t> inserts{1, 2, 3};

        auto inserted = set.InsertRange(std::move(inserts));
        REQUIRE(inserted == 3);
    }

    // Make sure all inserted keys exists via find range.
    {
        std::vector<uint64_t> keys{1, 2, 3};
        auto                  items = set.FindRange(keys);

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
        auto                  items = set.FindRange(keys);

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

TEST_CASE("UtSet FindRangeFill")
{
    UtSet<uint64_t> set{50ms};

    {
        std::vector<uint64_t> inserts{1, 2, 3};

        auto inserted = set.InsertRange(std::move(inserts));
        REQUIRE(inserted == 3);
    }

    // Make sure all inserted keys exists via find range.
    {
        std::vector<std::pair<uint64_t, bool>> items{
            {1, false},
            {2, false},
            {3, false},
        };
        set.FindRangeFill(items);

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
        set.FindRangeFill(items);

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

TEST_CASE("UtSet empty")
{
    UtSet<uint64_t> set{50ms};

    REQUIRE(set.empty());
    REQUIRE(set.Insert(1, Allow::INSERT));
    REQUIRE_FALSE(set.empty());
    REQUIRE(set.Delete(1));
    REQUIRE(set.empty());
}

TEST_CASE("UtSet size")
{
    UtSet<uint64_t> set{50ms};

    REQUIRE(set.Insert(1));
    REQUIRE(set.size() == 1);

    REQUIRE(set.Insert(2));
    REQUIRE(set.size() == 2);

    REQUIRE(set.Insert(3));
    REQUIRE(set.size() == 3);

    REQUIRE(set.Insert(4));
    REQUIRE(set.size() == 4);

    REQUIRE(set.Insert(5));
    REQUIRE(set.size() == 5);

    REQUIRE(set.Insert(6));
    REQUIRE(set.size() == 6);

    REQUIRE(set.Delete(6));
    REQUIRE(set.size() == 5);
    REQUIRE_FALSE(set.empty());
}

TEST_CASE("UtSet ttls")
{
    UtSet<uint64_t> set{20ms};

    REQUIRE(set.Insert(1));
    REQUIRE(set.Insert(2));

    std::this_thread::sleep_for(50ms);

    REQUIRE(set.Insert(3));

    REQUIRE_FALSE(set.Find(1));
    REQUIRE_FALSE(set.Find(2));
    REQUIRE(set.Find(3));
}

TEST_CASE("UtSet Clean with only some expired")
{
    UtSet<uint64_t> set{25ms};

    REQUIRE(set.Insert(1));
    REQUIRE(set.Insert(2));

    std::this_thread::sleep_for(50ms);

    REQUIRE(set.Insert(3));

    set.CleanExpiredValues();

    REQUIRE_FALSE(set.Find(1));
    REQUIRE_FALSE(set.Find(2));
    REQUIRE(set.Find(3));
}

TEST_CASE("UtSet bulk insert and some expire")
{
    UtSet<uint64_t> set{25ms};

    for (uint64_t i = 0; i < 100; ++i)
    {
        REQUIRE(set.Insert(i));
    }

    REQUIRE(set.size() == 100);

    std::this_thread::sleep_for(50ms);

    set.CleanExpiredValues();
    REQUIRE(set.size() == 0);
    REQUIRE(set.empty());

    for (uint64_t i = 100; i < 200; ++i)
    {
        REQUIRE(set.Insert(i));
    }

    REQUIRE(set.size() == 100);

    for (uint64_t i = 0; i < 100; ++i)
    {
        REQUIRE_FALSE(set.Find(i));
    }

    for (uint64_t i = 100; i < 200; ++i)
    {
        REQUIRE(set.Find(i));
    }

    REQUIRE(set.size() == 100);
}

TEST_CASE("UtSet update TTLs some expire")
{
    UtSet<std::string> set{}; // 100ms default

    REQUIRE(set.Insert("Hello"));
    REQUIRE(set.Insert("World"));

    std::this_thread::sleep_for(80ms);

    // Update "Hello" TTL, but not "World".
    REQUIRE(set.Insert("Hello", Allow::UPDATE));
    REQUIRE_FALSE(set.Insert("World", Allow::INSERT));

    // Total of ~160ms, "World" should expire.
    std::this_thread::sleep_for(80ms);

    REQUIRE(set.Find("Hello"));
    REQUIRE_FALSE(set.Find("World"));
    REQUIRE(set.size() == 1);

    // Total of ~240ms, "Hello" should expire.
    std::this_thread::sleep_for(80ms);

    REQUIRE_FALSE(set.Find("Hello"));
    REQUIRE_FALSE(set.Find("World"));
    REQUIRE(set.empty());
}

TEST_CASE("UtSet inserts, updates, and insert_or_update")
{
    UtSet<std::string> set{};

    REQUIRE(set.Insert("Hello", Allow::INSERT));
    REQUIRE(set.Insert("World", Allow::INSERT_OR_UPDATE));
    REQUIRE(set.Insert("Hola")); // defaults to Allow::INSERT_OR_UPDATE

    REQUIRE_FALSE(set.Insert("Friend", Allow::UPDATE));
    REQUIRE_FALSE(set.Insert("Hello", Allow::INSERT));

    REQUIRE(set.Find("Hello"));
    REQUIRE(set.Find("World"));
    REQUIRE(set.Find("Hola"));
    REQUIRE_FALSE(set.Find("Friend"));

    REQUIRE(set.Insert("Hello", Allow::UPDATE));
    REQUIRE(set.Find("Hello"));
}

TEST_CASE("UtSet Insert only long running test.")
{
    // This test is to make sure that an item that is continuously inserted into
    // the cache with Allow::INSERT only and its the only item inserted that it
    // will eventually be evicted by its TTL.

    UtSet<std::string> cache{1s};

    uint64_t inserted{0};
    uint64_t blocked{0};

    auto start = std::chrono::steady_clock::now();

    while (inserted < 5)
    {
        if (cache.Insert("test-key", Allow::INSERT))
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
    REQUIRE(elapsed >= std::chrono::milliseconds{4000});
}
