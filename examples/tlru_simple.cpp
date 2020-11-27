#include <cappuccino/cappuccino.hpp>

#include <chrono>
#include <iostream>

int main()
{
    using namespace std::chrono_literals;

    // Create a cache with up to 3 items.
    cappuccino::tlru_cache<uint64_t, std::string> cache{3};

    // Insert "hello", "world" with different TTLs.
    cache.insert(1h, 1, "Hello");
    cache.insert(2h, 2, "World");

    // Insert a third value to fill the cache.
    cache.insert(3h, 3, "nope");

    {
        // Grab hello and world, this update their LRU positions.
        auto hello = cache.find(1);
        auto world = cache.find(2);

        std::cout << hello.value() << ", " << world.value() << "!" << std::endl;
    }

    // Insert "hola", this will replace "nope" since its the oldest lru item,
    // nothing has expired at this time.
    cache.insert(30min, 4, "Hola");

    {
        auto hola  = cache.find(4); // "hola" was just inserted, it will be found
        auto hello = cache.find(1); // "hello" will also have a value, it is at the end of the lru list
        auto world = cache.find(2); // "world" is in the middle of our 3 lru list.
        auto nope  = cache.find(3); // "nope" was lru'ed when "hola" was inserted
                                    // since "hello" and "world were fetched

        if (hola.has_value())
        {
            std::cout << hola.value() << "\n";
        }

        if (hello.has_value())
        {
            std::cout << hello.value() << "\n";
        }

        if (world.has_value())
        {
            std::cout << world.value() << "\n";
        }

        if (!nope.has_value())
        {
            std::cout << "Nope was LRU'ed out of the cache.\n";
        }
    }

    return 0;
}
