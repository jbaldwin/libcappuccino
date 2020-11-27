#include <cappuccino/cappuccino.hpp>

int main()
{
    using namespace std::chrono_literals;
    using namespace cappuccino;
    // Uniform TTL LRU cache with a 1 hour TTL and 200 element cache capacity.
    // The key is uint64_t and the value is std::string.
    UtlruCache<uint64_t, std::string> lru_cache{1h, 200};

    lru_cache.Insert(1, "Hello", allow::insert); // OK
    lru_cache.Insert(1, "Hello", allow::insert); // ERROR! already exists

    // Note that allow::insert can succeed if the item is TTLed out
    // in certain types of caches that support TTLs.

    lru_cache.Insert(1, "Hola", allow::update);  // OK exists
    lru_cache.Insert(2, "World", allow::update); // ERROR! doesn't exist

    lru_cache.Insert(2, "World"); // OK, parameter defaults to allow::insert_or_update
    return 0;
}
