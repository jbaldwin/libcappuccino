#include "cappuccino/Cappuccino.hpp"

#include <chrono>
#include <iostream>
#include <thread>

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    using namespace std::chrono_literals;

    // Create a map a uniform TTL of 10 millisecond.
    cappuccino::UtMap<uint64_t, std::string> ut_map{10ms};

    // Insert "hello" and "world".
    ut_map.Insert(1, "Hello");
    ut_map.Insert(2, "World");

    // Fetch the items from the nap.
    auto hello = ut_map.Find(1);
    auto world = ut_map.Find(2);

    std::cout << hello.value() << ", " << world.value() << "!" << std::endl;

    // Insert "Hola". This will keep expanding the map.
    ut_map.Insert(3, "Hola");

    auto hola = ut_map.Find(3);
    hello     = ut_map.Find(1); // Hello is still in the map.

    // All values should be present in the map.
    std::cout << hello.value() << ", " << world.value() << " and " << hola.value() << "!" << std::endl;

    // Sleep for 10 times the TTL to evict the elements.
    std::this_thread::sleep_for(100ms);

    // No values should be present in the map now.
    hello = ut_map.Find(1);
    world = ut_map.Find(2);
    hola  = ut_map.Find(3);

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
