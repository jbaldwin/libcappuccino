#include "cappuccino/Cappuccino.h"

#include <iostream>

using namespace cappuccino;

int main()
{
    // Create a cache with 2 items.
    LfuCache<std::string, std::string> lfu_cache { 2 };

    // Insert some data.
    lfu_cache.Insert("foo", "Hello");
    lfu_cache.Insert("bar", "World");

    // Touch foo twice.
    auto foo1 = lfu_cache.Find("foo");
    auto foo2 = lfu_cache.Find("foo");

    // Touch bar once.
    auto bar1 = lfu_cache.Find("bar");

    // Insert foobar, bar should be replaced.
    lfu_cache.Insert("foobar", "Hello World");

    auto bar2 = lfu_cache.FindWithUseCount("bar");
    if (bar2.has_value()) {
        std::cout << "bar2 should not have a value!" << std::endl;
    }

    auto foo3 = lfu_cache.FindWithUseCount("foo");
    auto foobar = lfu_cache.FindWithUseCount("foobar");

    if (foo3.has_value()) {
        auto& [value, use_count] = foo3.value();
        std::cout << "foo=" << value << " use_count=" << use_count << std::endl;
    }

    if (foobar.has_value()) {
        auto& [value, use_count] = foobar.value();
        std::cout << "foobar=" << value << " use_count=" << use_count << std::endl;
    }

    return 0;
}
