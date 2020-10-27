#pragma once

#include "catch.hpp"

#include <cappuccino/Cappuccino.hpp>

#include <chrono>
#include <thread>

using namespace cappuccino;

TEST_CASE("UtMap example")
{
    using namespace std::chrono_literals;

    // Create a map with 2 items and a uniform TTL of 20ms.
    UtMap<std::string, bool> map { 20ms };

    // Insert "hello" and "world".
    map.Insert("Hello", false);
    map.Insert("World", true);

    {
        // Fetch the items from the map.
        auto hello = map.Find("Hello");
        auto world = map.Find("World");

        REQUIRE(hello.has_value());
        REQUIRE(world.has_value());

        REQUIRE(hello.value() == false);
        REQUIRE(world.value() == true);
    }

    // Sleep for an ~order of magnitude longer than the TTL.
    std::this_thread::sleep_for(100ms);

    // Manually trigger a clean since no insert/delete is wanted.
    auto cleaned_count = map.CleanExpiredValues();
    REQUIRE(cleaned_count == 2);
    REQUIRE(map.empty());
}

TEST_CASE("UtMap Find doesn't exist")
{
    UtMap<uint64_t, std::string> map { 50ms };
    REQUIRE_FALSE(map.Find(100).has_value());
}

TEST_CASE("UtMap Insert Only")
{
    UtMap<uint64_t, std::string> map { 50ms };

    REQUIRE(map.Insert(1, "test", Allow::INSERT));
    auto value = map.Find(1);
    REQUIRE(value.has_value());
    REQUIRE(value.value() == "test");

    REQUIRE_FALSE(map.Insert(1, "test2", Allow::INSERT));
    value = map.Find(1);
    REQUIRE(value.has_value());
    REQUIRE(value.value() == "test");
}

TEST_CASE("UtMap Update Only")
{
    UtMap<uint64_t, std::string> map { 50ms };

    REQUIRE_FALSE(map.Insert(1, "test", Allow::UPDATE));
    auto value = map.Find(1);
    REQUIRE_FALSE(value.has_value());
}

TEST_CASE("UtMap Insert Or Update")
{
    UtMap<uint64_t, std::string> map { 50ms };

    REQUIRE(map.Insert(1, "test"));
    auto value = map.Find(1);
    REQUIRE(value.has_value());
    REQUIRE(value.value() == "test");

    REQUIRE(map.Insert(1, "test2"));
    value = map.Find(1);
    REQUIRE(value.has_value());
    REQUIRE(value.value() == "test2");
}

TEST_CASE("UtMap InsertRange Insert Only")
{
    UtMap<uint64_t, std::string> map { 50ms };

    {
        std::vector<std::pair<uint64_t, std::string>> inserts {
            { 1, "test1" },
            { 2, "test2" },
            { 3, "test3" }
        };

        auto inserted = map.InsertRange(std::move(inserts), Allow::INSERT);
        REQUIRE(inserted == 3);
    }

    REQUIRE(map.size() == 3);

    REQUIRE(map.Find(2).has_value());
    REQUIRE(map.Find(2).value() == "test2");
    REQUIRE(map.Find(1).has_value());
    REQUIRE(map.Find(1).value() == "test1");
    REQUIRE(map.Find(3).has_value());
    REQUIRE(map.Find(3).value() == "test3");

    {
        std::vector<std::pair<uint64_t, std::string>> inserts {
            { 1, "test1" },
            { 2, "test2" },
            { 3, "test3" },
            { 4, "test4" }, // new
            { 5, "test5" }, // new
        };

        auto inserted = map.InsertRange(std::move(inserts), Allow::INSERT);
        REQUIRE(inserted == 2);
    }

    REQUIRE(map.size() == 5);
    REQUIRE(map.Find(1).has_value());
    REQUIRE(map.Find(1).value() == "test1");
    REQUIRE(map.Find(2).has_value());
    REQUIRE(map.Find(3).has_value());
    REQUIRE(map.Find(3).value() == "test3");
    REQUIRE(map.Find(4).has_value());
    REQUIRE(map.Find(4).value() == "test4");
    REQUIRE(map.Find(5).has_value());
    REQUIRE(map.Find(5).value() == "test5");
}

TEST_CASE("UtMap InsertRange Update Only")
{
    UtMap<uint64_t, std::string> map { 50ms };

    {
        std::vector<std::pair<uint64_t, std::string>> inserts {
            { 1, "test1" },
            { 2, "test2" },
            { 3, "test3" }
        };

        auto inserted = map.InsertRange(std::move(inserts), Allow::UPDATE);
        REQUIRE(inserted == 0);
    }

    REQUIRE(map.size() == 0);
    REQUIRE_FALSE(map.Find(1).has_value());
    REQUIRE_FALSE(map.Find(2).has_value());
    REQUIRE_FALSE(map.Find(3).has_value());
}

TEST_CASE("UtMap InsertRange Insert Or Update")
{
    UtMap<uint64_t, std::string> map { 50ms };

    {
        std::vector<std::pair<uint64_t, std::string>> inserts {
            { 1, "test1" },
            { 2, "test2" },
            { 3, "test3" }
        };

        auto inserted = map.InsertRange(std::move(inserts));
        REQUIRE(inserted == 3);
    }

    REQUIRE(map.size() == 3);
    REQUIRE(map.Find(1).has_value());
    REQUIRE(map.Find(1).value() == "test1");
    REQUIRE(map.Find(2).has_value());
    REQUIRE(map.Find(2).value() == "test2");
    REQUIRE(map.Find(3).has_value());
    REQUIRE(map.Find(3).value() == "test3");

    {
        std::vector<std::pair<uint64_t, std::string>> inserts {
            { 2, "test2" },
            { 1, "test1" },
            { 3, "test3" },
            { 4, "test4" }, // new
            { 5, "test5" }, // new
        };

        auto inserted = map.InsertRange(std::move(inserts));
        REQUIRE(inserted == 5);
    }

    REQUIRE(map.size() == 5);
    REQUIRE(map.Find(1).has_value());
    REQUIRE(map.Find(1).value() == "test1");
    REQUIRE(map.Find(2).has_value());
    REQUIRE(map.Find(3).has_value());
    REQUIRE(map.Find(3).value() == "test3");
    REQUIRE(map.Find(4).has_value());
    REQUIRE(map.Find(4).value() == "test4");
    REQUIRE(map.Find(5).has_value());
    REQUIRE(map.Find(5).value() == "test5");
}

TEST_CASE("UtMap Delete")
{
    UtMap<uint64_t, std::string> map { 50ms };

    REQUIRE(map.Insert(1, "test", Allow::INSERT));
    auto value = map.Find(1);
    REQUIRE(value.has_value());
    REQUIRE(value.value() == "test");
    REQUIRE(map.size() == 1);

    map.Delete(1);
    value = map.Find(1);
    REQUIRE_FALSE(value.has_value());
    REQUIRE(map.size() == 0);
    REQUIRE(map.empty());

    REQUIRE_FALSE(map.Delete(200));
}

TEST_CASE("UtMap DeleteRange")
{
    UtMap<uint64_t, std::string> map { 50ms };

    {
        std::vector<std::pair<uint64_t, std::string>> inserts {
            { 1, "test1" },
            { 2, "test2" },
            { 3, "test3" }
        };

        auto inserted = map.InsertRange(std::move(inserts));
        REQUIRE(inserted == 3);
    }

    REQUIRE(map.size() == 3);
    REQUIRE(map.Find(1).has_value());
    REQUIRE(map.Find(2).has_value());
    REQUIRE(map.Find(3).has_value());

    {
        std::vector<uint64_t> delete_keys { 1, 3, 4, 5 };

        auto deleted = map.DeleteRange(delete_keys);
        REQUIRE(deleted == 2);
    }

    REQUIRE(map.size() == 1);
    REQUIRE_FALSE(map.Find(1).has_value());
    REQUIRE(map.Find(2).has_value());
    REQUIRE(map.Find(2).value() == "test2");
    REQUIRE_FALSE(map.Find(3).has_value());
    REQUIRE_FALSE(map.Find(4).has_value());
    REQUIRE_FALSE(map.Find(5).has_value());
}

TEST_CASE("UtMap FindRange")
{
    UtMap<uint64_t, std::string> map { 50ms };

    {
        std::vector<std::pair<uint64_t, std::string>> inserts {
            { 1, "test1" },
            { 2, "test2" },
            { 3, "test3" }
        };

        auto inserted = map.InsertRange(std::move(inserts));
        REQUIRE(inserted == 3);
    }

    // Make sure all inserted keys exists via find range.
    {
        std::vector<uint64_t> keys { 1, 2, 3 };
        auto items = map.FindRange(keys);

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
        auto items = map.FindRange(keys);

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

TEST_CASE("UtMap FindRangeFill")
{
    UtMap<uint64_t, std::string> map { 50ms };

    {
        std::vector<std::pair<uint64_t, std::string>> inserts {
            { 1, "test1" },
            { 2, "test2" },
            { 3, "test3" }
        };

        auto inserted = map.InsertRange(std::move(inserts));
        REQUIRE(inserted == 3);
    }

    // Make sure all inserted keys exists via find range.
    {
        std::vector<std::pair<uint64_t, std::optional<std::string>>> items {
            { 1, std::nullopt },
            { 2, std::nullopt },
            { 3, std::nullopt },
        };
        map.FindRangeFill(items);

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
        map.FindRangeFill(items);

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

TEST_CASE("UtMap empty")
{
    UtMap<uint64_t, std::string> map { 50ms };

    REQUIRE(map.empty());
    REQUIRE(map.Insert(1, "test", Allow::INSERT));
    REQUIRE_FALSE(map.empty());
    REQUIRE(map.Delete(1));
    REQUIRE(map.empty());
}

TEST_CASE("UtMap size")
{
    UtMap<uint64_t, std::string> map { 50ms };

    REQUIRE(map.Insert(1, "test1"));
    REQUIRE(map.size() == 1);

    REQUIRE(map.Insert(2, "test2"));
    REQUIRE(map.size() == 2);

    REQUIRE(map.Insert(3, "test3"));
    REQUIRE(map.size() == 3);

    REQUIRE(map.Insert(4, "test4"));
    REQUIRE(map.size() == 4);

    REQUIRE(map.Insert(5, "test5"));
    REQUIRE(map.size() == 5);

    REQUIRE(map.Insert(6, "test6"));
    REQUIRE(map.size() == 6);

    REQUIRE(map.Delete(6));
    REQUIRE(map.size() == 5);
    REQUIRE_FALSE(map.empty());
}

TEST_CASE("UtMap ttls")
{
    UtMap<uint64_t, std::string> map { 20ms };

    REQUIRE(map.Insert(1, "Hello"));
    REQUIRE(map.Insert(2, "World"));

    std::this_thread::sleep_for(50ms);

    REQUIRE(map.Insert(3, "Hola"));

    auto hello = map.Find(1);
    auto world = map.Find(2);
    auto hola = map.Find(3);

    REQUIRE_FALSE(hello.has_value());
    REQUIRE_FALSE(world.has_value());
    REQUIRE(hola.has_value());
    REQUIRE(hola.value() == "Hola");
}

TEST_CASE("UtMap Clean with only some expired")
{
    UtMap<uint64_t, std::string> map { 25ms };

    REQUIRE(map.Insert(1, "Hello"));
    REQUIRE(map.Insert(2, "World"));

    std::this_thread::sleep_for(50ms);

    REQUIRE(map.Insert(3, "Hola"));

    map.CleanExpiredValues();

    auto hello = map.Find(1);
    auto world = map.Find(2);
    auto hola = map.Find(3);

    REQUIRE_FALSE(hello.has_value());
    REQUIRE_FALSE(world.has_value());
    REQUIRE(hola.has_value());
    REQUIRE(hola.value() == "Hola");
}

TEST_CASE("UtMap bulk insert some expire")
{
    UtMap<uint64_t, uint64_t> map { 50ms };

    for (uint64_t i = 0; i < 100; ++i) {
        REQUIRE(map.Insert(i, i));
    }

    REQUIRE(map.size() == 100);

    std::this_thread::sleep_for(250ms);

    map.CleanExpiredValues();
    REQUIRE(map.size() == 0);

    for (uint64_t i = 100; i < 200; ++i) {
        REQUIRE(map.Insert(i, i));
    }

    REQUIRE(map.size() == 100);

    for (uint64_t i = 0; i < 100; ++i) {
        auto opt = map.Find(i);
        REQUIRE_FALSE(opt.has_value());
    }

    for (uint64_t i = 100; i < 200; ++i) {
        REQUIRE(map.Find(i).has_value());
    }

    REQUIRE(map.size() == 100);
}

TEST_CASE("UtMap update TTLs some expire")
{
    UtMap<std::string, uint64_t> map {}; // 100ms default

    REQUIRE(map.Insert("Hello", 1));
    REQUIRE(map.Insert("World", 2));

    std::this_thread::sleep_for(80ms);

    // Update "Hello" TTL, but not "World".
    REQUIRE(map.Insert("Hello", 1, Allow::UPDATE));
    REQUIRE_FALSE(map.Insert("World", 1, Allow::INSERT));

    // Total of ~160ms, "World" should expire.
    std::this_thread::sleep_for(80ms);

    auto hello = map.Find("Hello");
    auto world = map.Find("World");

    REQUIRE(map.size() == 1);
    REQUIRE(hello.has_value());
    REQUIRE(hello.value() == 1);
    REQUIRE_FALSE(world.has_value());

    // Total of ~240ms, "Hello" should expire.
    std::this_thread::sleep_for(80ms);

    hello = map.Find("Hello");
    world = map.Find("World");

    REQUIRE(map.empty());
    REQUIRE_FALSE(hello.has_value());
    REQUIRE_FALSE(world.has_value());
}

TEST_CASE("UtMap update element value")
{
    UtMap<std::string, uint64_t> map {}; // 100ms default

    REQUIRE(map.Insert("Hello", 1));
    REQUIRE(map.Insert("World", 2));

    std::this_thread::sleep_for(80ms);

    // Update "Hello", but not "World".
    REQUIRE(map.Insert("Hello", 3, Allow::UPDATE));
    REQUIRE_FALSE(map.Insert("World", 4, Allow::INSERT));

    auto hello = map.Find("Hello");
    auto world = map.Find("World");

    REQUIRE(hello.has_value());
    REQUIRE(hello.value() == 3);
    REQUIRE(world.has_value());
    REQUIRE(world.value() == 2);

    // Total of ~160ms, "World" should expire.
    std::this_thread::sleep_for(80ms);

    hello = map.Find("Hello");
    world = map.Find("World");

    REQUIRE(map.size() == 1);
    REQUIRE(hello.has_value());
    REQUIRE(hello.value() == 3); // updated value
    REQUIRE_FALSE(world.has_value());
}

TEST_CASE("UtMap inserts, updates, and insert_or_update")
{
    UtMap<std::string, uint64_t> map {}; // 100ms default

    REQUIRE(map.Insert("Hello", 1, Allow::INSERT));
    REQUIRE(map.Insert("World", 2, Allow::INSERT_OR_UPDATE));
    REQUIRE(map.Insert("Hola", 3)); // defaults to Allow::INSERT_OR_UPDATE

    REQUIRE_FALSE(map.Insert("Friend", 4, Allow::UPDATE));
    REQUIRE_FALSE(map.Insert("Hello", 5, Allow::INSERT));

    auto hello = map.Find("Hello");
    auto world = map.Find("World");
    auto hola = map.Find("Hola");
    auto frand = map.Find("Friend");

    REQUIRE(hello.has_value());
    REQUIRE(hello.value() == 1);

    REQUIRE(world.has_value());
    REQUIRE(world.value() == 2);

    REQUIRE(hola.has_value());
    REQUIRE(hola.value() == 3);

    REQUIRE_FALSE(frand.has_value());

    REQUIRE(map.Insert("Hello", 6, Allow::UPDATE));

    hello = map.Find("Hello");

    REQUIRE(hello.has_value());
    REQUIRE(hello.value() == 6);
}