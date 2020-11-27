#include <cappuccino/cappuccino.hpp>

#include <chrono>
#include <iostream>
#include <thread>

int main()
{
    using namespace std::chrono_literals;

    // Create a map a uniform TTL of 10 millisecond.
    cappuccino::ut_map<uint64_t, std::string> map{10ms};

    // Insert "hello" and "world".
    map.insert(1, "Hello");
    map.insert(2, "World");

    // Fetch the items from the nap.
    auto hello = map.find(1);
    auto world = map.find(2);

    std::cout << hello.value() << ", " << world.value() << "!" << std::endl;

    // Insert "Hola". This will keep expanding the map.
    map.insert(3, "Hola");

    auto hola = map.find(3);
    hello     = map.find(1); // Hello is still in the map.

    // All values should be present in the map.
    std::cout << hello.value() << ", " << world.value() << " and " << hola.value() << "!" << std::endl;

    // Sleep for 10 times the TTL to evict the elements.
    std::this_thread::sleep_for(100ms);

    // No values should be present in the map now.
    hello = map.find(1);
    world = map.find(2);
    hola  = map.find(3);

    if (!hello.has_value() && !world.has_value() && !hola.has_value())
    {
        std::cout << "Everything is gone from the uniform time aware map!" << std::endl;
    }
    else
    {
        std::cout << "Oops our elements weren't properly evicted!" << std::endl;
    }

    return 0;
}
