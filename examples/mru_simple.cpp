#include "cappuccino/Cappuccino.h"

#include <iostream>

using namespace cappuccino;

int main()
{
    // Create a cache with 2 items.
    MruCache<uint64_t, std::string> mru_cache { 2 };

    // Insert hello and world.
    mru_cache.Insert(1, "Hello");
    mru_cache.Insert(2, "World");

    {
        // Grab them
        auto hello = mru_cache.Find(1);
        auto world = mru_cache.Find(2);

        // Lets use them!
        std::cout << hello.value() << ", " << world.value() << "!" << std::endl;
    }

    // Insert hola, this will replace "World" since its the most recently used item.
    mru_cache.Insert(3, "Hola");

    {
        auto hola = mru_cache.Find(3);
        auto hello = mru_cache.Find(1); // this will exist
        auto world = mru_cache.Find(2); // this value will no longer be available.

        // Grab and print again!
        std::cout << hola.value() << ", " << hello.value() << "!" << std::endl;

        // No value should be present in the cache for the world key.
        if (world.has_value()) {
            std::cout << "impossibru!" << std::endl;
        }
    }

    return 0;
}
