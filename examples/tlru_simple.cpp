#include "cappuccino/Cappuccino.hpp"

#include <chrono>
#include <iostream>

int main() {
  using namespace std::chrono_literals;

  // Create a cache with up to 3 items.
  cappuccino::TlruCache<uint64_t, std::string> lru_cache{3};

  // Insert "hello", "world" with different TTLs.
  lru_cache.Insert(1h, 1, "Hello");
  lru_cache.Insert(2h, 2, "World");

  // Insert a third value to fill the cache.
  lru_cache.Insert(3h, 3, "nope");

  {
    // Grab hello and world, this update their LRU positions.
    auto hello = lru_cache.Find(1);
    auto world = lru_cache.Find(2);

    std::cout << hello.value() << ", " << world.value() << "!" << std::endl;
  }

  // Insert "hola", this will replace "nope" since its the oldest lru item,
  // nothing has expired at this time.
  lru_cache.Insert(30min, 4, "Hola");

  {
    auto hola = lru_cache.Find(4); // "hola" was just inserted, it will be found
    auto hello = lru_cache.Find(
        1); // "hello" will also have a value, it is at the end of the lru list
    auto world =
        lru_cache.Find(2); // "world" is in the middle of our 3 lru list.
    auto nope = lru_cache.Find(3); // "nope" was lru'ed when "hola" was inserted
                                   // since "hello" and "world were fetched

    if (hola.has_value()) {
      std::cout << hola.value() << "\n";
    }

    if (hello.has_value()) {
      std::cout << hello.value() << "\n";
    }

    if (world.has_value()) {
      std::cout << world.value() << "\n";
    }

    if (!nope.has_value()) {
      std::cout << "Nope was LRU'ed out of the cache.\n";
    }
  }

  return 0;
}
