#include <cappuccino/cappuccino.hpp>

#include <iostream>
#include <thread>

using namespace std::chrono_literals;

int main()
{
    // Create a cache with 2 items, 1s age time with halving dynamic aging ratio.
    cappuccino::lfuda_cache<std::string, std::string> cache{2, 1s, 0.5f};

    // Insert some data.
    cache.insert("foo", "Hello");
    cache.insert("bar", "World");

    // Make foo have a use count of 20
    for (size_t i = 1; i < 20; ++i)
    {
        auto foo                 = cache.find_with_use_count("foo");
        auto& [value, use_count] = foo.value();
        std::cout << "foo=" << value << " use_count=" << use_count << std::endl;
    }
    // Make bar have a use count of 22
    for (size_t i = 1; i < 22; ++i)
    {
        auto bar                 = cache.find_with_use_count("bar");
        auto& [value, use_count] = bar.value();
        std::cout << "bar=" << value << " use_count=" << use_count << std::endl;
    }

    // wait long enough to dynamically age.
    std::cout << "Waiting 2s to dynamically age the cache..." << std::endl;
    std::this_thread::sleep_for(2s);

    // Manually dynamic age to see its effect
    auto aged_count = cache.dynamically_age();
    std::cout << "Manually dynamically aged " << aged_count << " items." << std::endl;
    {
        {
            auto foo                 = cache.find_with_use_count("foo");
            auto& [value, use_count] = foo.value();
            std::cout << "foo=" << value << " use_count=" << use_count << std::endl;
        }
        {
            auto bar                 = cache.find_with_use_count("bar");
            auto& [value, use_count] = bar.value();
            std::cout << "bar=" << value << " use_count=" << use_count << std::endl;
        }
    }

    std::cout << std::endl << "Inserting foobar..." << std::endl;

    // Insert foobar, foo should be replaced as it will dynamically age
    // down to 10, while bar will dynamically age down to 11.
    cache.insert("foobar", "Hello World");

    {
        auto foo    = cache.find_with_use_count("foo");
        auto bar    = cache.find_with_use_count("bar");
        auto foobar = cache.find_with_use_count("foobar");

        if (foo.has_value())
        {
            std::cout << "foo should not have a value!" << std::endl;
        }

        if (bar.has_value())
        {
            auto& [value, use_count] = bar.value();
            std::cout << "bar=" << value << " use_count=" << use_count << std::endl;
        }

        if (foobar.has_value())
        {
            auto& [value, use_count] = foobar.value();
            std::cout << "foobar=" << value << " use_count=" << use_count << std::endl;
        }
    }

    return 0;
}
