#include "cappuccino/Cappuccino.h"

#include <chrono>
#include <iostream>

using namespace cappuccino;

int main(int argc, char* argv[])
{
    using namespace std::chrono_literals;

    // Create a cache with 2 items.
    TlruCache<uint64_t, std::string> lru_cache { 3 };

    // Insert hello and world.
    lru_cache.Insert(1h, 1, "Hello");
    lru_cache.Insert(1h, 2, "World");
    lru_cache.Insert(1h, 3, "I should never be found!");

    {
        // Grab them
        auto hello = lru_cache.Find(1);
        auto world = lru_cache.Find(2);

        // Lets use them!
        std::cout << hello.value() << ", " << world.value() << "!" << std::endl;
    }

    // Insert hola, this will replace "Hello" since its the oldest lru item (nothing has expired).
    lru_cache.Insert(1h, 4, "Hola");

    {
        auto hola = lru_cache.Find(4); // hola was just inserted, it will be found
        auto hello = lru_cache.Find(1); // hello will also have a value, it is at the end of the lru list
        auto world = lru_cache.Find(2); // world is in the middle of our 3 lru list.
        auto nope = lru_cache.Find(3); // nope was lru'ed when hola was inserted since hello and world were fetched

        // Grab and print again!
        std::cout << hola.value() << ", " << world.value() << "!" << std::endl;

        // No value should be present in the cache for the hello key.
        if (!hello.has_value()) {
            std::cout << "hello impossibru!" << std::endl;
        }

        if (nope.has_value()) {
            std::cout << "nope impossibru!" << std::endl;
        }
    }

    return 0;
}
