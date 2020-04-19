#include "cappuccino/Cappuccino.hpp"

#include <iostream>

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    // Create a cache with 2 items.
    cappuccino::RrCache<uint64_t, std::string> rr_cache { 2 };

    // Insert hello and world.
    rr_cache.Insert(1, "Hello");
    rr_cache.Insert(2, "World");

    {
        // Grab them
        auto hello = rr_cache.Find(1);
        auto world = rr_cache.Find(2);

        // Lets use them!
        std::cout << hello.value() << ", " << world.value() << "!" << std::endl;
    }

    // Insert hola, this will replace "Hello" or "World", we don't know!
    rr_cache.Insert(3, "Hola");

    {
        auto hola = rr_cache.Find(3); // This will be in the cache.
        auto hello = rr_cache.Find(1); // This might be in the cache?
        auto world = rr_cache.Find(2); // This might be in the cache?

        std::cout << hola.value() << ", ";

        if (hello.has_value()) {
            std::cout << hello.value();
        } else if (world.has_value()) {
            std::cout << world.value();
        } else {
            std::cout << "impossibru!";
        }
        std::cout << std::endl;
    }

    return 0;
}
