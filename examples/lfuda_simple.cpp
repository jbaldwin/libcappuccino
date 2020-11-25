#include "cappuccino/Cappuccino.hpp"

#include <iostream>
#include <thread>

using namespace std::chrono_literals;

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    // Create a cache with 2 items, 1s age time with halving dynamic aging ratio.
    cappuccino::LfudaCache<std::string, std::string> lfuda_cache{2, 1s, 0.5f};

    // Insert some data.
    lfuda_cache.Insert("foo", "Hello");
    lfuda_cache.Insert("bar", "World");

    // Make foo have a use count of 20
    for (size_t i = 1; i < 20; ++i)
    {
        auto foo                 = lfuda_cache.FindWithUseCount("foo");
        auto& [value, use_count] = foo.value();
        std::cout << "foo=" << value << " use_count=" << use_count << std::endl;
    }
    // Make bar have a use count of 22
    for (size_t i = 1; i < 22; ++i)
    {
        auto bar                 = lfuda_cache.FindWithUseCount("bar");
        auto& [value, use_count] = bar.value();
        std::cout << "bar=" << value << " use_count=" << use_count << std::endl;
    }

    // wait long enough to dynamically age.
    std::cout << "Waiting 2s to dynamically age the cache..." << std::endl;
    std::this_thread::sleep_for(2s);

    // Manually dynamic age to see its effect
    auto aged_count = lfuda_cache.DynamicallyAge();
    std::cout << "Manually dynamically aged " << aged_count << " items." << std::endl;
    {
        {
            auto foo                 = lfuda_cache.FindWithUseCount("foo");
            auto& [value, use_count] = foo.value();
            std::cout << "foo=" << value << " use_count=" << use_count << std::endl;
        }
        {
            auto bar                 = lfuda_cache.FindWithUseCount("bar");
            auto& [value, use_count] = bar.value();
            std::cout << "bar=" << value << " use_count=" << use_count << std::endl;
        }
    }

    std::cout << std::endl << "Inserting foobar..." << std::endl;

    // Insert foobar, foo should be replaced as it will dynamically age
    // down to 10, while bar will dynamically age down to 11.
    lfuda_cache.Insert("foobar", "Hello World");

    {
        auto foo    = lfuda_cache.FindWithUseCount("foo");
        auto bar    = lfuda_cache.FindWithUseCount("bar");
        auto foobar = lfuda_cache.FindWithUseCount("foobar");

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
