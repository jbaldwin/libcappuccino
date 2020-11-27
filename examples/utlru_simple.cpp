#include <cappuccino/cappuccino.hpp>

#include <chrono>
#include <iostream>

int main()
{
    using namespace std::chrono_literals;

    // Create a cache with 2 items and a uniform TTL of 1 hour.
    cappuccino::utlru_cache<uint64_t, std::string> cache{1h, 2};

    // Insert "hello" and "world".
    cache.insert(1, "Hello");
    cache.insert(2, "World");

    // Fetch the items from the catch, this will update their LRU positions.
    auto hello = cache.find(1);
    auto world = cache.find(2);

    std::cout << hello.value() << ", " << world.value() << "!" << std::endl;

    // Insert "hHla", this will replace "Hello" since its the oldest lru item
    // and nothing has expired yet.
    cache.insert(3, "Hola");

    auto hola = cache.find(3);
    hello     = cache.find(1); // Hello isn't in the cache anymore and will return an empty optional.

    std::cout << hola.value() << ", " << world.value() << "!" << std::endl;

    // No value should be present in the cache for the hello key.
    if (!hello.has_value())
    {
        std::cout << "Hello was LRU evicted from the cache.\n";
    }

    return 0;
}
