#include <cappuccino/cappuccino.hpp>

#include <iostream>

int main()
{
    // Create a cache with 2 items.
    cappuccino::mru_cache<uint64_t, std::string> cache{2};

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

    // Insert hola, this will replace "World" since its the most recently used item.
    cache.insert(3, "Hola");

    {
        auto hola  = cache.find(3);
        auto hello = cache.find(1); // this will exist
        auto world = cache.find(2); // this value will no longer be available.

        // Grab and print again!
        std::cout << hola.value() << ", " << hello.value() << "!" << std::endl;

        // No value should be present in the cache for the world key.
        if (world.has_value())
        {
            std::cout << "impossibru!" << std::endl;
        }
    }

    return 0;
}
