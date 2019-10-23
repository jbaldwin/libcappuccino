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
        std::chrono::seconds                       ttl{1};
        cappuccino::TlruCache<std::string, double> cache{5};

        bool inserted1 = cache.Insert(ttl, "one", 1.0); // first insert
        bool inserted2 = cache.Insert(ttl, "two", 2.0);
        bool inserted3 = cache.Insert(ttl, "three", 3.0);
        bool inserted4 = cache.Insert(ttl, "four", 4.0);
        bool inserted5 = cache.Insert(ttl, "five", 5.0); // last insert

        WHEN("Values were inserted")
        {
            THEN("Insert() returns success")
            {
                REQUIRE(inserted1);
                REQUIRE(inserted2);
                REQUIRE(inserted3);
                REQUIRE(inserted4);
                REQUIRE(inserted5);
            }
        }

        WHEN("We try to add a 6th entry the first one entered should be deleted.")
        {
            bool inserted6 = cache.Insert(ttl, "six", 6.0);

            auto val_one  = cache.Find("one");  // should be deleted
            auto val_five = cache.Find("five"); // should be there
            auto val_six  = cache.Find("six");  // should be there

            THEN("The first entry should be deleted")
            {
                REQUIRE(inserted6);
                REQUIRE_FALSE(val_one); // should be an empty optional.

                REQUIRE(val_five.has_value());
                REQUIRE(val_five.value() == 5.0);

                REQUIRE(val_six.has_value());
                REQUIRE(val_six.value() == 6.0);
            }
        }
    }
}

SCENARIO("The TlruCache can store values in a limited space with both TTL and LRU disposal", "[cappuccino][tlrucache]")
{
    using CacheType = cappuccino::TlruCache<std::string, double>;
    std::chrono::seconds ttl{1};

    GIVEN("A filled TlruCache of 5 doubles with 1s TTL")
    {
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
            std::this_thread::sleep_for(ttl);

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
                bool insert = cache.Insert(ttl, "five", 5.0);

                auto val_five = cache.Find("five");
                auto val_pi   = cache.Find("pi");

                THEN("We expect to be able to retrieve it, and the LRU to work properly")
                {
                    REQUIRE(insert);

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
            bool first_insert  = cache.Insert(ttl, "persist", 1.0);
            auto first_access  = cache.Find("persist");
            bool test_ret_val1 = cache.Insert(ttl, "persist", 1337.1337);
            bool test_ret_val2 = cache.Insert(ttl, "persist", 1234.1234);

            WHEN("We retrieve the value at that key")
            {
                auto val_persist = cache.Find("persist");

                THEN("We expect the value was updated")
                {
                    REQUIRE(first_insert);
                    REQUIRE(first_access);
                    REQUIRE(first_access == 1.0);
                    REQUIRE_FALSE(test_ret_val1); // no insert happened.
                    REQUIRE_FALSE(test_ret_val2); // no insert happened.
                    REQUIRE(val_persist);
                    REQUIRE(val_persist.value() == 1234.1234);
                }
            }
        }

        WHEN("We try to retrive multiple values at the same time")
        {
            auto keys = std::vector<std::string>{"one", "two", "three", "pi", "four", "unknown"};

            auto result_list = cache.FindRange(keys);

            THEN("We expect all values to be retrived properly")
            {
                REQUIRE(result_list.size() == keys.size());

                REQUIRE(result_list[0].second.value() == 1.0);
                REQUIRE(result_list[1].second.value() == 2.0);
                REQUIRE(result_list[2].second.value() == 3.0);
                REQUIRE(result_list[3].second.value() == 3.14159);
                REQUIRE(result_list[4].second.value() == 4.0);
                REQUIRE(!result_list[5].second.has_value());
            }
        }

        WHEN("We insert an existing key, wait 1s, and try to insert the same key")
        {
            bool result1 = cache.Insert(ttl, "one", 12.3);

            std::this_thread::sleep_for(ttl);

            bool result2 = cache.Insert(ttl, "one", 45.6);

            THEN("We expect the first insert to fail due to existing key, then the second to succeed on expired key")
            {
                REQUIRE_FALSE(result1);
                REQUIRE(result2);
            }
        }
    }

    GIVEN("A non-full TlruCache using InsertOnly()")
    {
        CacheType cache{5};

        WHEN("We insert keys in cache")
        {
            THEN("We expect first insert succeed and repeating the insert to fail")
            {
                REQUIRE(cache.InsertOnly(ttl, "one", 1.0));
                REQUIRE_FALSE(cache.InsertOnly(ttl, "one", 2.0));
                REQUIRE(cache.Find("one").value() == 1.0);
            }
        }

       WHEN("We wait 1s for entry to expire")
       {
            std::this_thread::sleep_for(ttl);
            
            THEN("Repeating the insert succeeds again since it expired")
            {
                REQUIRE(cache.InsertOnly(ttl, "one", 2.0));
                REQUIRE_FALSE(cache.InsertOnly(ttl, "one", 3.0));
                REQUIRE(cache.Find("one").value() == 2.0);
            }
       }
    }
}

} // namespace test
} // namespace cappuccino

#pragma clang diagnostic pop
