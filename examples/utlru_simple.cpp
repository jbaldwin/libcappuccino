#include "cappuccino/Cappuccino.hpp"

#include <chrono>
#include <iostream>

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    using namespace std::chrono_literals;

    // Create a cache with 2 items and a uniform TTL of 1 hour.
    cappuccino::UtlruCache<uint64_t, std::string> lru_cache { 1h, 2 };

    // Insert "hello" and "world".
    lru_cache.Insert(1, "Hello");
    lru_cache.Insert(2, "World");

    // Fetch the items from the catch, this will update their LRU positions.
    auto hello = lru_cache.Find(1);
    auto world = lru_cache.Find(2);

    std::cout << hello.value() << ", " << world.value() << "!" << std::endl;

    // Insert "hHla", this will replace "Hello" since its the oldest lru item
    // and nothing has expired yet.
    lru_cache.Insert(3, "Hola");

    auto hola = lru_cache.Find(3);
    hello = lru_cache.Find(1); // Hello isn't in the cache anymore and will return an empty optional.

    std::cout << hola.value() << ", " << world.value() << "!" << std::endl;

    // No value should be present in the cache for the hello key.
    if (!hello.has_value()) {
        std::cout << "Hello was LRU evicted from the cache.\n";
    }

    return 0;
}
