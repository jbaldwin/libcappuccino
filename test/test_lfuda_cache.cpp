#include "catch.hpp"
#include <cappuccino/cappuccino.hpp>

#include <thread>

using namespace cappuccino;
using namespace std::chrono_literals;

TEST_CASE("Lfuda example")
{
    // Create a cache with 2 items, 10ms age time with halving dynamic aging ratio.
    lfuda_cache<uint64_t, std::string> cache{2, 10ms, 0.5f};

    // Insert some data.
    cache.insert(1, "Hello");
    cache.insert(2, "World");

    // Make 1 have a use count of 20
    for (size_t i = 1; i < 20; ++i)
    {
        cache.find(1);
    }
    // Make 2 have a use count of 22
    for (size_t i = 1; i < 22; ++i)
    {
        cache.find(2);
    }

    // wait an ~order of magnitude long enough to dynamically age.
    std::this_thread::sleep_for(50ms);

    // // Manually dynamic age to see its effect
    auto aged_count = cache.dynamically_age();
    REQUIRE(aged_count == 2);
    {
        {
            auto foo = cache.find_with_use_count(1);
            REQUIRE(foo.has_value());
            const auto& [value, use_count] = foo.value();
            REQUIRE(value == "Hello");
            REQUIRE(use_count == 11); // dynamic age 20 => 10 + 1 find
        }
        {
            auto bar = cache.find_with_use_count(2);
            REQUIRE(bar.has_value());
            const auto& [value, use_count] = bar.value();
            REQUIRE(value == "World");
            REQUIRE(use_count == 12); // dynamic age 22 => 11 + 1 find
        }
    }

    // Insert 3, 1 should be replaced as it will dynamically age
    // down to 10, while 2 will dynamically age down to 11.
    cache.insert(3, "Hello World");

    {
        auto foo    = cache.find_with_use_count(1);
        auto bar    = cache.find_with_use_count(2);
        auto foobar = cache.find_with_use_count(3);

        REQUIRE_FALSE(foo.has_value());
        REQUIRE(bar.has_value());
        REQUIRE(foobar.has_value());

        {
            const auto& [value, use_count] = bar.value();
            REQUIRE(value == "World");
            REQUIRE(use_count == 13);
        }
        {
            const auto& [value, use_count] = foobar.value();
            REQUIRE(value == "Hello World");
            REQUIRE(use_count == 2);
        }
    }
}

TEST_CASE("Lfuda Find doesn't exist")
{
    lfuda_cache<uint64_t, std::string> cache{4, 1s, 0.5f};
    REQUIRE_FALSE(cache.find(100).has_value());
}

TEST_CASE("Lfuda Insert Only")
{
    lfuda_cache<uint64_t, std::string> cache{4, 1s, 0.5f};

    REQUIRE(cache.insert(1, "test", allow::insert));
    auto value = cache.find(1);
    REQUIRE(value.has_value());
    REQUIRE(value.value() == "test");

    REQUIRE_FALSE(cache.insert(1, "test2", allow::insert));
    value = cache.find(1);
    REQUIRE(value.has_value());
    REQUIRE(value.value() == "test");
}

TEST_CASE("Lfuda Update Only")
{
    lfuda_cache<uint64_t, std::string> cache{4, 1s, 0.5f};

    REQUIRE_FALSE(cache.insert(1, "test", allow::update));
    auto value = cache.find(1);
    REQUIRE_FALSE(value.has_value());
}

TEST_CASE("Lfuda Insert Or Update")
{
    lfuda_cache<uint64_t, std::string> cache{4, 1s, 0.5f};

    REQUIRE(cache.insert(1, "test"));
    auto value = cache.find(1);
    REQUIRE(value.has_value());
    REQUIRE(value.value() == "test");

    REQUIRE(cache.insert(1, "test2"));
    value = cache.find(1);
    REQUIRE(value.has_value());
    REQUIRE(value.value() == "test2");
}

TEST_CASE("Lfuda InsertRange Insert Only")
{
    lfuda_cache<uint64_t, std::string> cache{4, 1s, 0.5f};

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
    REQUIRE_FALSE(cache.find(4).has_value()); // evicted by lfu policy
    REQUIRE(cache.find(5).has_value());
    REQUIRE(cache.find(5).value() == "test5");
}

TEST_CASE("Lfuda InsertRange Update Only")
{
    lfuda_cache<uint64_t, std::string> cache{4, 1s, 0.5f};

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

TEST_CASE("Lfuda InsertRange Insert Or Update")
{
    lfuda_cache<uint64_t, std::string> cache{4, 1s, 0.5f};

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
    REQUIRE_FALSE(cache.find(4).has_value()); // evicted by lfu policy
    REQUIRE(cache.find(5).has_value());
    REQUIRE(cache.find(5).value() == "test5");
}

TEST_CASE("Lfuda Delete")
{
    lfuda_cache<uint64_t, std::string> cache{4, 1s, 0.5f};

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

TEST_CASE("Lfuda DeleteRange")
{
    lfuda_cache<uint64_t, std::string> cache{4, 1s, 0.5f};

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

TEST_CASE("Lfuda FindRange")
{
    lfuda_cache<uint64_t, std::string> cache{4, 1s, 0.5f};

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

TEST_CASE("Lfuda FindRangeFill")
{
    lfuda_cache<uint64_t, std::string> cache{4, 1s, 0.5f};

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

TEST_CASE("Lfuda empty")
{
    lfuda_cache<uint64_t, std::string> cache{4, 1s, 0.5f};

    REQUIRE(cache.empty());
    REQUIRE(cache.insert(1, "test", allow::insert));
    REQUIRE_FALSE(cache.empty());
    REQUIRE(cache.erase(1));
    REQUIRE(cache.empty());
}

TEST_CASE("Lfuda size + capacity")
{
    lfuda_cache<uint64_t, std::string> cache{4, 1s, 0.5f};

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