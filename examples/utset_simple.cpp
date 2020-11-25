#include "cappuccino/Cappuccino.hpp"

#include <chrono>
#include <iostream>
#include <thread>

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    using namespace std::chrono_literals;

    // Create a set a uniform TTL of 10 millisecond.
    cappuccino::UtSet<std::string> ut_set{10ms};

    // Insert "hello" and "world".
    ut_set.Insert("Hello");
    ut_set.Insert("World");

    // Fetch the items from the nap.
    auto hello = ut_set.Find("Hello");
    auto world = ut_set.Find("World");

    if (hello && world)
    {
        std::cout << "Hello and World are in the set!" << std::endl;
    }
    else
    {
        std::cout << "Oops our elements weren't properly inserted!" << std::endl;
    }

    // Insert "Hola". This will keep expanding the set.
    ut_set.Insert("Hola");

    auto hola = ut_set.Find("Hola");
    hello     = ut_set.Find("Hello"); // Hello is still in the set.

    // All values should be present in the set.
    if (hello && world && hola)
    {
        std::cout << "Hello and World and Hola are all still in the set!" << std::endl;
    }
    else
    {
        std::cout << "Oops our elements were evicted!" << std::endl;
    }

    // Sleep for 10 times the TTL to evict the elements.
    std::this_thread::sleep_for(100ms);

    // No values should be present in the set now.
    hello = ut_set.Find("Hello");
    world = ut_set.Find("World");
    hola  = ut_set.Find("Hola");

    if (!hello && !world && !hola)
    {
        std::cout << "Everything is gone from the uniform time aware set!" << std::endl;
    }
    else
    {
        std::cout << "Oops our elements weren't properly evicted!" << std::endl;
    }

    return 0;
}
