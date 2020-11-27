#include <cappuccino/cappuccino.hpp>

#include <chrono>
#include <iostream>
#include <thread>

int main()
{
    using namespace std::chrono_literals;

    // Create a set a uniform TTL of 10 millisecond.
    cappuccino::ut_set<std::string> set{10ms};

    // Insert "hello" and "world".
    set.insert("Hello");
    set.insert("World");

    // Fetch the items from the nap.
    auto hello = set.find("Hello");
    auto world = set.find("World");

    if (hello && world)
    {
        std::cout << "Hello and World are in the set!" << std::endl;
    }
    else
    {
        std::cout << "Oops our elements weren't properly inserted!" << std::endl;
    }

    // Insert "Hola". This will keep expanding the set.
    set.insert("Hola");

    auto hola = set.find("Hola");
    hello     = set.find("Hello"); // Hello is still in the set.

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
    hello = set.find("Hello");
    world = set.find("World");
    hola  = set.find("Hola");

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
