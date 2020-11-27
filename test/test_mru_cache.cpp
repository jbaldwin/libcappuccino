#include "catch.hpp"
#include <cappuccino/cappuccino.hpp>

using namespace cappuccino;

TEST_CASE("Mru example")
{
    // Create a cache with 2 items.
    mru_cache<uint64_t, std::string> cache{2};

    // Insert hello and world.
    cache.insert(1, "Hello");
    cache.insert(2, "World");

    {
        // Grab them
        auto hello = cache.find(1);
        auto world = cache.find(2);

        REQUIRE(hello.has_value());
        REQUIRE(world.has_value());

        REQUIRE(hello.value() == "Hello");
        REQUIRE(world.value() == "World");
    }

    // Insert hola, this will replace "World" since its the most recently used item.
    cache.insert(3, "Hola");

    {
        auto hola  = cache.find(3);
        auto hello = cache.find(1); // this will exist
        auto world = cache.find(2); // this value will no longer be available.

        REQUIRE(hola.has_value());
        REQUIRE(hello.has_value());
        REQUIRE_FALSE(world.has_value());
    }
}

TEST_CASE("Mru Find doesn't exist")
{
    mru_cache<uint64_t, std::string> cache{4};
    REQUIRE_FALSE(cache.find(100).has_value());
}

TEST_CASE("Mru Insert Only")
{
    mru_cache<uint64_t, std::string> cache{4};

    REQUIRE(cache.insert(1, "test", allow::insert));
    auto value = cache.find(1);
    REQUIRE(value.has_value());
    REQUIRE(value.value() == "test");

    REQUIRE_FALSE(cache.insert(1, "test2", allow::insert));
    value = cache.find(1);
    REQUIRE(value.has_value());
    REQUIRE(value.value() == "test");
}

TEST_CASE("Mru Update Only")
{
    mru_cache<uint64_t, std::string> cache{4};

    REQUIRE_FALSE(cache.insert(1, "test", allow::update));
    auto value = cache.find(1);
    REQUIRE_FALSE(value.has_value());
}

TEST_CASE("Mru Insert Or Update")
{
    mru_cache<uint64_t, std::string> cache{4};

    REQUIRE(cache.insert(1, "test"));
    auto value = cache.find(1);
    REQUIRE(value.has_value());
    REQUIRE(value.value() == "test");

    REQUIRE(cache.insert(1, "test2"));
    value = cache.find(1);
    REQUIRE(value.has_value());
    REQUIRE(value.value() == "test2");
}

TEST_CASE("Mru insert_range Insert Only")
{
    mru_cache<uint64_t, std::string> cache{4};

    {
        std::vector<std::pair<uint64_t, std::string>> inserts{{1, "test1"}, {2, "test2"}, {3, "test3"}};

        auto inserted = cache.insert_range(std::move(inserts), allow::insert);
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
    REQUIRE(cache.find(2).has_value());
    REQUIRE(cache.find(2).value() == "test2");
    REQUIRE(cache.find(3).has_value());
    REQUIRE(cache.find(3).value() == "test3");
    // REQUIRE(cache.find(4).has_value());
    // REQUIRE(cache.find(4).value() == "test4");
    REQUIRE_FALSE(cache.find(4).has_value());
    REQUIRE(cache.find(5).has_value());
    REQUIRE(cache.find(5).value() == "test5");
}

TEST_CASE("Mru insert_range Update Only")
{
    mru_cache<uint64_t, std::string> cache{4};

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

TEST_CASE("Mru insert_range Insert Or Update")
{
    mru_cache<uint64_t, std::string> cache{4};

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
            {1, "test1"},
            {2, "test2"},
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
    REQUIRE(cache.find(2).has_value());
    REQUIRE(cache.find(2).value() == "test2");
    REQUIRE(cache.find(3).has_value());
    REQUIRE(cache.find(3).value() == "test3");
    // REQUIRE(cache.find(4).has_value());
    // REQUIRE(cache.find(4).value() == "test4");
    REQUIRE_FALSE(cache.find(4).has_value()); // evicted by Mru policy
    REQUIRE(cache.find(5).has_value());
    REQUIRE(cache.find(5).value() == "test5");
}

TEST_CASE("Mru Delete")
{
    mru_cache<uint64_t, std::string> cache{4};

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

TEST_CASE("Mru DeleteRange")
{
    mru_cache<uint64_t, std::string> cache{4};

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

TEST_CASE("Mru FindRange")
{
    mru_cache<uint64_t, std::string> cache{4};

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

TEST_CASE("Mru find_range_fill")
{
    mru_cache<uint64_t, std::string> cache{4};

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

TEST_CASE("Mru empty")
{
    mru_cache<uint64_t, std::string> cache{4};

    REQUIRE(cache.empty());
    REQUIRE(cache.insert(1, "test", allow::insert));
    REQUIRE_FALSE(cache.empty());
    REQUIRE(cache.erase(1));
    REQUIRE(cache.empty());
}

TEST_CASE("Mru size + capacity")
{
    mru_cache<uint64_t, std::string> cache{4};

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

TEST_CASE("Mru Find with peek")
{
    mru_cache<uint64_t, std::string> cache{4};

    REQUIRE(cache.insert(1, "Hello"));
    REQUIRE(cache.insert(2, "World"));
    REQUIRE(cache.insert(3, "Hola"));
    REQUIRE(cache.insert(4, "Mondo"));

    REQUIRE(cache.find(1, peek::yes).has_value()); // doesn't move up to LRU
    REQUIRE(cache.find(2, peek::no).has_value());
    REQUIRE(cache.find(3, peek::yes).has_value()); // doesn't move up to LRU
    REQUIRE(cache.find(4, peek::no).has_value());

    REQUIRE(cache.insert(5, "another one bites the dust1"));
    REQUIRE_FALSE(cache.find(4).has_value());
    REQUIRE(cache.insert(6, "another one bites the dust2"));
    REQUIRE_FALSE(cache.find(5).has_value());
}