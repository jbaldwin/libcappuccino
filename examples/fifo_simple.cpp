#include "cappuccino/Cappuccino.hpp"

#include <iostream>

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    // Create a cache with 2 items.
    cappuccino::FifoCache<uint64_t, std::string> fifo_cache{4};

    // Insert some data.
    fifo_cache.Insert(1, "one");
    fifo_cache.Insert(2, "two");
    fifo_cache.Insert(3, "three");
    fifo_cache.Insert(4, "four");

    {
        auto one   = fifo_cache.Find(1);
        auto two   = fifo_cache.Find(2);
        auto three = fifo_cache.Find(3);
        auto four  = fifo_cache.Find(4);

        if (one.has_value() && two.has_value() && three.has_value() && four.has_value())
        {
            std::cout << "1, 2, 3, 4 all exist in the cache." << std::endl;
        }
    }

    fifo_cache.Insert(5, "five");

    {
        auto one   = fifo_cache.Find(1);
        auto two   = fifo_cache.Find(2);
        auto three = fifo_cache.Find(3);
        auto four  = fifo_cache.Find(4);
        auto five  = fifo_cache.Find(5);

        if (one.has_value())
        {
            std::cout << "One isn't supposed to be here!" << std::endl;
        }

        if (two.has_value() && three.has_value() && four.has_value() && five.has_value())
        {
            std::cout << "2, 3, 4, 5 all exist in the cache." << std::endl;
        }
    }

    fifo_cache.Insert(6, "six");

    {
        auto two   = fifo_cache.Find(2);
        auto three = fifo_cache.Find(3);
        auto four  = fifo_cache.Find(4);
        auto five  = fifo_cache.Find(5);
        auto six   = fifo_cache.Find(6);

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
