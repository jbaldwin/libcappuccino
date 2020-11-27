#include <cappuccino/cappuccino.hpp>

#include <iostream>

int main()
{
    // Create a cache with 2 items.
    cappuccino::lru_cache<uint64_t, std::string> cache{2};

    // Insert hello and world.
    cache.insert(1, "Hello");
    cache.insert(2, "World");

    {
        // Grab them
        auto hello = cache.find(1);
        auto world = cache.find(2);

        // Lets use them!
        std::cout << hello.value() << ", " << world.value() << "!" << std::endl;
    }

    // Insert hola, this will replace "Hello" since its the oldest lru item.
    cache.insert(3, "Hola");

    {
        auto hola  = cache.find(3);
        auto hello = cache.find(1); // this will return an empty optional now
        auto world = cache.find(2); // this value should still be available!

        // Grab and print again!
        std::cout << hola.value() << ", " << world.value() << "!" << std::endl;

        // No value should be present in the cache for the hello key.
        if (hello.has_value())
        {
            std::cout << "impossibru!" << std::endl;
        }
    }

    return 0;
}
