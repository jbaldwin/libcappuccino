#include <cappuccino/cappuccino.hpp>

#include <iostream>

int main()
{
    // Create a cache with 2 items.
    cappuccino::rr_cache<uint64_t, std::string> cache{2};

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

    // Insert hola, this will replace "Hello" or "World", we don't know!
    cache.insert(3, "Hola");

    {
        auto hola  = cache.find(3); // This will be in the cache.
        auto hello = cache.find(1); // This might be in the cache?
        auto world = cache.find(2); // This might be in the cache?

        std::cout << hola.value() << ", ";

        if (hello.has_value())
        {
            std::cout << hello.value();
        }
        else if (world.has_value())
        {
            std::cout << world.value();
        }
        else
        {
            std::cout << "impossibru!";
        }
        std::cout << std::endl;
    }

    return 0;
}
