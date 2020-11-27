#include <cappuccino/cappuccino.hpp>

#include <iostream>

int main()
{
    // Create a cache with 2 items.
    cappuccino::fifo_cache<uint64_t, std::string> cache{4};

    // Insert some data.
    cache.insert(1, "one");
    cache.insert(2, "two");
    cache.insert(3, "three");
    cache.insert(4, "four");

    {
        auto one   = cache.find(1);
        auto two   = cache.find(2);
        auto three = cache.find(3);
        auto four  = cache.find(4);

        if (one.has_value() && two.has_value() && three.has_value() && four.has_value())
        {
            std::cout << "1, 2, 3, 4 all exist in the cache." << std::endl;
        }
    }

    cache.insert(5, "five");

    {
        auto one   = cache.find(1);
        auto two   = cache.find(2);
        auto three = cache.find(3);
        auto four  = cache.find(4);
        auto five  = cache.find(5);

        if (one.has_value())
        {
            std::cout << "One isn't supposed to be here!" << std::endl;
        }

        if (two.has_value() && three.has_value() && four.has_value() && five.has_value())
        {
            std::cout << "2, 3, 4, 5 all exist in the cache." << std::endl;
        }
    }

    cache.insert(6, "six");

    {
        auto two   = cache.find(2);
        auto three = cache.find(3);
        auto four  = cache.find(4);
        auto five  = cache.find(5);
        auto six   = cache.find(6);

        if (two.has_value())
        {
            std::cout << "Two isn't supposed to be here!" << std::endl;
        }

        if (three.has_value() && four.has_value() && five.has_value() && six.has_value())
        {
            std::cout << "3, 4, 5, 6 all exist in the cache." << std::endl;
        }
    }

    return 0;
}
