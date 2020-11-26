#include <cappuccino/cappuccino.hpp>

#include <iostream>

int main()
{
    // Create a cache with 2 items.
    cappuccino::LruCache<uint64_t, std::string> lru_cache{2};

    // Insert hello and world.
    lru_cache.Insert(1, "Hello");
    lru_cache.Insert(2, "World");

    {
        // Grab them
        auto hello = lru_cache.Find(1);
        auto world = lru_cache.Find(2);

        // Lets use them!
        std::cout << hello.value() << ", " << world.value() << "!" << std::endl;
    }

    // Insert hola, this will replace "Hello" since its the oldest lru item.
    lru_cache.Insert(3, "Hola");

    {
        auto hola  = lru_cache.Find(3);
        auto hello = lru_cache.Find(1); // this will return an empty optional now
        auto world = lru_cache.Find(2); // this value should still be available!

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
