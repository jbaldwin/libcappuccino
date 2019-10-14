#include "cappuccino/TlruCache.h"

#include <catch.hpp>

#include <thread>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#pragma clang diagnostic ignored "-Wfloat-equal"

namespace cappuccino
{
namespace test
{
SCENARIO("Test the TlruCache's insertion and deletion order.", "[cappuccino][tlrucache]")
{
    GIVEN("A filled TlruCache of 5 doubles with 1s TTL")
    {
        std::chrono::seconds ttl{1};
        cappuccino::TlruCache<std::string, double> cache{5};

        cache.Insert(ttl, "one", 1.0); // first insert
        cache.Insert(ttl, "two", 2.0);
        cache.Insert(ttl, "three", 3.0);
        cache.Insert(ttl, "four", 4.0);
        cache.Insert(ttl, "five", 5.0); // last insert

        WHEN("We try to add a 6th entry the first one entered should be deleted.")
        {
            cache.Insert(ttl, "six", 6.0);

            auto val_one  = cache.Find("one");  // should be deleted
            auto val_five = cache.Find("five"); // should be there
            auto val_six  = cache.Find("six");  // should be there

            THEN("The first entry should be deleted")
            {
                REQUIRE_FALSE(val_one); // should be an empty optional.

                REQUIRE(val_five.has_value());
                REQUIRE(val_five.value() == 5.0);

                REQUIRE(val_six.has_value());
                REQUIRE(val_six.value() == 6.0);
            }
        }
    }
}

SCENARIO("The TlruCache can storage values in a limited space with both TTL and LRU disposal", "[cappuccino][tlrucache]")
{
    GIVEN("A filled TlruCache of 5 doubles with 1s TTL")
    {
        std::chrono::seconds ttl{1};
        using CacheType = cappuccino::TlruCache<std::string, double>;
        CacheType cache{5};

        auto insert_vals = std::vector<CacheType::KeyValue>{
            {{ttl, "one", 1.0}, {ttl, "two", 2.0}, {ttl, "three", 3.0}, {ttl, "pi", 3.14159}, {ttl, "four", 4.0}}};

        cache.InsertRange(insert_vals);

        WHEN("We request an item we have inserted in the cache")
        {
            auto val_one = cache.Find("one");
            auto val_pi  = cache.Find("pi");

            THEN("We expect the values to be present and correct")
            {
                REQUIRE(val_one);
                REQUIRE(val_one.value() == 1.0);

                REQUIRE(val_pi);
                REQUIRE(val_pi.value() == 3.14159);
            }
        }

        WHEN("We wait 1s before reading the values")
        {
            std::this_thread::sleep_for(
                std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds{1}));

            auto val_two   = cache.Find("two");
            auto val_three = cache.Find("three");

            THEN("We expect the cache does not return a value")
            {
                REQUIRE_FALSE(val_two);
                REQUIRE_FALSE(val_three);
            }
        }

        WHEN("We access the last resource used")
        {
            // pi is not accessed recently
            auto val_one   = cache.Find("one");
            auto val_two   = cache.Find("two");
            auto val_three = cache.Find("three");
            auto val_four  = cache.Find("four");

            WHEN("We insert a new value (beyond the limit)")
            {
                cache.Insert(ttl, "five", 5.0);

                auto val_five = cache.Find("five");
                auto val_pi   = cache.Find("pi");

                THEN("We expect to be able to retrieve it, and the LRU to work properly")
                {
                    REQUIRE(val_one);
                    REQUIRE(val_one.value() == 1.0);
                    REQUIRE(val_two);
                    REQUIRE(val_two.value() == 2.0);
                    REQUIRE(val_three);
                    REQUIRE(val_three.value() == 3.0);
                    REQUIRE(val_four);
                    REQUIRE(val_four.value() == 4.0);
                    REQUIRE(val_five);
                    REQUIRE(val_five.value() == 5.0);

                    // pi was deleted to make room for the new "five" entry
                    REQUIRE_FALSE(val_pi);
                }
            }
        }

        WHEN("We try to insert a value already in the cache")
        {
            bool first_insert = cache.Insert(ttl, "persist", 1.0);
            auto first_access = cache.Find("persist");
            bool test_ret_val1 = cache.Insert(ttl, "persist", 1337.1337);
            bool test_ret_val2 = cache.Insert(ttl, "persist", 1337.1337);

            WHEN("We retrieve the value at that key")
            {
                auto val_persist = cache.Find("persist");

                THEN("We expect the value was NOT changed")
                {
                    REQUIRE(first_insert);
                    REQUIRE(first_access);
                    REQUIRE(first_access == 1.0);
                    REQUIRE_FALSE(test_ret_val1); // no insert happened.
                    REQUIRE_FALSE(test_ret_val2); // no insert happened.
                    REQUIRE(val_persist);
                    REQUIRE(val_persist.value() == 1.0);
                }
            }
        }

        WHEN("We try to retrive multiple values at the same time")
        {
            auto keys = std::vector<std::string>{"one", "two", "three", "pi", "four", "unknown"};

            auto result_map = cache.FindRange(keys);

            THEN("We expect all values to be retrived properly")
            {
                auto iter = result_map.find("one");
                REQUIRE((iter != result_map.end() && iter->second.value() == 1.0));

                iter = result_map.find("two");
                REQUIRE((iter != result_map.end() && iter->second.value() == 2.0));

                iter = result_map.find("three");
                REQUIRE((iter != result_map.end() && iter->second.value() == 3.0));

                iter = result_map.find("pi");
                REQUIRE((iter != result_map.end() && iter->second.value() == 3.14159));

                iter = result_map.find("four");
                REQUIRE((iter != result_map.end() && iter->second.value() == 4.0));

                iter = result_map.find("unknown");
                REQUIRE((iter != result_map.end() && !iter->second.has_value()));
            }
        }
    }
}

} // namespace test
} // namespace cappuccino

#pragma clang diagnostic pop
