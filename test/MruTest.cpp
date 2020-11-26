#include "catch.hpp"
#include <cappuccino/Cappuccino.hpp>

using namespace cappuccino;

TEST_CASE("Mru example")
{
    // Create a cache with 2 items.
    MruCache<uint64_t, std::string> mru_cache{2};

    // Insert hello and world.
    mru_cache.Insert(1, "Hello");
    mru_cache.Insert(2, "World");

    {
        // Grab them
        auto hello = mru_cache.Find(1);
        auto world = mru_cache.Find(2);

        REQUIRE(hello.has_value());
        REQUIRE(world.has_value());

        REQUIRE(hello.value() == "Hello");
        REQUIRE(world.value() == "World");
    }

    // Insert hola, this will replace "World" since its the most recently used item.
    mru_cache.Insert(3, "Hola");

    {
        auto hola  = mru_cache.Find(3);
        auto hello = mru_cache.Find(1); // this will exist
        auto world = mru_cache.Find(2); // this value will no longer be available.

        REQUIRE(hola.has_value());
        REQUIRE(hello.has_value());
        REQUIRE_FALSE(world.has_value());
    }
}

TEST_CASE("Mru Find doesn't exist")
{
    MruCache<uint64_t, std::string> cache{4};
    REQUIRE_FALSE(cache.Find(100).has_value());
}

TEST_CASE("Mru Insert Only")
{
    MruCache<uint64_t, std::string> cache{4};

    REQUIRE(cache.Insert(1, "test", Allow::INSERT));
    auto value = cache.Find(1);
    REQUIRE(value.has_value());
    REQUIRE(value.value() == "test");

    REQUIRE_FALSE(cache.Insert(1, "test2", Allow::INSERT));
    value = cache.Find(1);
    REQUIRE(value.has_value());
    REQUIRE(value.value() == "test");
}

TEST_CASE("Mru Update Only")
{
    MruCache<uint64_t, std::string> cache{4};

    REQUIRE_FALSE(cache.Insert(1, "test", Allow::UPDATE));
    auto value = cache.Find(1);
    REQUIRE_FALSE(value.has_value());
}

TEST_CASE("Mru Insert Or Update")
{
    MruCache<uint64_t, std::string> cache{4};

    REQUIRE(cache.Insert(1, "test"));
    auto value = cache.Find(1);
    REQUIRE(value.has_value());
    REQUIRE(value.value() == "test");

    REQUIRE(cache.Insert(1, "test2"));
    value = cache.Find(1);
    REQUIRE(value.has_value());
    REQUIRE(value.value() == "test2");
}

TEST_CASE("Mru InsertRange Insert Only")
{
    MruCache<uint64_t, std::string> cache{4};

    {
        std::vector<std::pair<uint64_t, std::string>> inserts{{1, "test1"}, {2, "test2"}, {3, "test3"}};

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
        std::vector<std::pair<uint64_t, std::string>> inserts{
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
    REQUIRE(cache.Find(2).has_value());
    REQUIRE(cache.Find(2).value() == "test2");
    REQUIRE(cache.Find(3).has_value());
    REQUIRE(cache.Find(3).value() == "test3");
    // REQUIRE(cache.Find(4).has_value());
    // REQUIRE(cache.Find(4).value() == "test4");
    REQUIRE_FALSE(cache.Find(4).has_value());
    REQUIRE(cache.Find(5).has_value());
    REQUIRE(cache.Find(5).value() == "test5");
}

TEST_CASE("Mru InsertRange Update Only")
{
    MruCache<uint64_t, std::string> cache{4};

    {
        std::vector<std::pair<uint64_t, std::string>> inserts{{1, "test1"}, {2, "test2"}, {3, "test3"}};

        auto inserted = cache.InsertRange(std::move(inserts), Allow::UPDATE);
        REQUIRE(inserted == 0);
    }

    REQUIRE(cache.size() == 0);
    REQUIRE_FALSE(cache.Find(1).has_value());
    REQUIRE_FALSE(cache.Find(2).has_value());
    REQUIRE_FALSE(cache.Find(3).has_value());
}

TEST_CASE("Mru InsertRange Insert Or Update")
{
    MruCache<uint64_t, std::string> cache{4};

    {
        std::vector<std::pair<uint64_t, std::string>> inserts{{1, "test1"}, {2, "test2"}, {3, "test3"}};

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
        std::vector<std::pair<uint64_t, std::string>> inserts{
            {1, "test1"},
            {2, "test2"},
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
    REQUIRE(cache.Find(2).has_value());
    REQUIRE(cache.Find(2).value() == "test2");
    REQUIRE(cache.Find(3).has_value());
    REQUIRE(cache.Find(3).value() == "test3");
    // REQUIRE(cache.Find(4).has_value());
    // REQUIRE(cache.Find(4).value() == "test4");
    REQUIRE_FALSE(cache.Find(4).has_value()); // evicted by Mru policy
    REQUIRE(cache.Find(5).has_value());
    REQUIRE(cache.Find(5).value() == "test5");
}

TEST_CASE("Mru Delete")
{
    MruCache<uint64_t, std::string> cache{4};

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

TEST_CASE("Mru DeleteRange")
{
    MruCache<uint64_t, std::string> cache{4};

    {
        std::vector<std::pair<uint64_t, std::string>> inserts{{1, "test1"}, {2, "test2"}, {3, "test3"}};

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

TEST_CASE("Mru FindRange")
{
    MruCache<uint64_t, std::string> cache{4};

    {
        std::vector<std::pair<uint64_t, std::string>> inserts{{1, "test1"}, {2, "test2"}, {3, "test3"}};

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

TEST_CASE("Mru FindRangeFill")
{
    MruCache<uint64_t, std::string> cache{4};

    {
        std::vector<std::pair<uint64_t, std::string>> inserts{{1, "test1"}, {2, "test2"}, {3, "test3"}};

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

TEST_CASE("Mru empty")
{
    MruCache<uint64_t, std::string> cache{4};

    REQUIRE(cache.empty());
    REQUIRE(cache.Insert(1, "test", Allow::INSERT));
    REQUIRE_FALSE(cache.empty());
    REQUIRE(cache.Delete(1));
    REQUIRE(cache.empty());
}

TEST_CASE("Mru size + capacity")
{
    MruCache<uint64_t, std::string> cache{4};

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

TEST_CASE("Mru Find with Peek")
{
    MruCache<uint64_t, std::string> cache{4};

    REQUIRE(cache.Insert(1, "Hello"));
    REQUIRE(cache.Insert(2, "World"));
    REQUIRE(cache.Insert(3, "Hola"));
    REQUIRE(cache.Insert(4, "Mondo"));

    REQUIRE(cache.Find(1, Peek::YES).has_value()); // doesn't move up to LRU
    REQUIRE(cache.Find(2, Peek::NO).has_value());
    REQUIRE(cache.Find(3, Peek::YES).has_value()); // doesn't move up to LRU
    REQUIRE(cache.Find(4, Peek::NO).has_value());

    REQUIRE(cache.Insert(5, "another one bites the dust1"));
    REQUIRE_FALSE(cache.Find(4).has_value());
    REQUIRE(cache.Insert(6, "another one bites the dust2"));
    REQUIRE_FALSE(cache.Find(5).has_value());
}