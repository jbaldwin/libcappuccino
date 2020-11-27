#include <cappuccino/cappuccino.hpp>

#include <iostream>

int main()
{
    // Create a cache with 2 items.
    cappuccino::lfu_cache<std::string, std::string> cache{2};

    // Insert some data.
    cache.insert("foo", "Hello");
    cache.insert("bar", "World");

    // Touch foo twice.
    auto foo1 = cache.find("foo");
    auto foo2 = cache.find("foo");

    (void)foo1;
    (void)foo2;

    // Touch bar once.
    auto bar1 = cache.find("bar");

    (void)bar1;

    // Insert foobar, bar should be replaced.
    cache.insert("foobar", "Hello World");

    auto bar2 = cache.find_with_use_count("bar");
    if (bar2.has_value())
    {
        std::cout << "bar2 should not have a value!" << std::endl;
    }

    auto foo3   = cache.find_with_use_count("foo");
    auto foobar = cache.find_with_use_count("foobar");

    if (foo3.has_value())
    {
        auto& [value, use_count] = foo3.value();
        std::cout << "foo=" << value << " use_count=" << use_count << std::endl;
    }

    if (foobar.has_value())
    {
        auto& [value, use_count] = foobar.value();
        std::cout << "foobar=" << value << " use_count=" << use_count << std::endl;
    }

    return 0;
}
