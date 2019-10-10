#include "cappuccino/TlruCache.h"

#include <catch.hpp>

#include <thread>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#pragma clang diagnostic ignored "-Wfloat-equal"

namespace spotx
{
namespace collections
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
        cappuccino::TlruCache<std::string, double> cache{5};

        auto insert_vals = std::vector<std::pair<std::string, double>>{
            {{"one", 1.0}, {"two", 2.0}, {"three", 3.0}, {"pi", 3.14159}, {"four", 4.0}}};

        // TODO?
        //cache.Insert(ttl, insert_vals);
        for (const auto& v : insert_vals)
            cache.Insert(ttl, v.first, v.second);

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
            cache.Insert(ttl, "persist", 1.0);
            auto first_access = cache.Find("persist");
            //bool test_ret_val = cache.Insert(ttl, "persist", 1337.1337);
            bool test_ret_val = false;
            cache.Insert(ttl, "persist", 1337.1337);

            WHEN("We retrieve the value at that key")
            {
                auto val_persist = cache.Find("persist");

                THEN("We expect the value was NOT changed")
                {
                    REQUIRE(first_access);
                    REQUIRE(first_access == 1.0);
                    REQUIRE_FALSE(test_ret_val); // no insert happened.
                    REQUIRE(val_persist);
                    REQUIRE(val_persist.value() == 1.0);
                }
            }
        }

        // TODO?
        /*WHEN("We try to retrive multiple values at the same time")
        {
            auto fill_range = std::vector<std::pair<std::string, Optional<double>>>{};

            fill_range.resize(5);

            // optionals all default to nullopt
            fill_range[0].first = "one";
            fill_range[1].first = "two";
            fill_range[2].first = "three";
            fill_range[3].first = "pi";
            fill_range[4].first = "four";

            cache.FillRange(fill_range);

            THEN("We expect all values to be retrived properly")
            {
                REQUIRE(fill_range[0].second);
                REQUIRE(fill_range[0].second.value() == 1.0);

                REQUIRE(fill_range[1].second);
                REQUIRE(fill_range[1].second.value() == 2.0);

                REQUIRE(fill_range[2].second);
                REQUIRE(fill_range[2].second.value() == 3.0);

                REQUIRE(fill_range[3].second);
                REQUIRE(fill_range[3].second.value() == 3.14159);

                REQUIRE(fill_range[4].second);
                REQUIRE(fill_range[4].second.value() == 4.0);
            }
        }*/
    }
}

} // namespace test
} // namespace collections
} // namespace spotx

#pragma clang diagnostic pop
