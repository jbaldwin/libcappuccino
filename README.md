libcappuccino - C++17 Cache Data Structure Library.
===================================================

[![CI](https://github.com/jbaldwin/libcappuccino/workflows/build-release-test/badge.svg)](https://github.com/jbaldwin/libcappuccino/workflows/build-release-test/badge.svg)
[![language][badge.language]][language]
[![license][badge.license]][license]

[badge.language]: https://img.shields.io/badge/language-C%2B%2B17-yellow.svg
[badge.license]: https://img.shields.io/badge/license-Apache--2.0-blue

[language]: https://en.wikipedia.org/wiki/C%2B%2B17
[license]: https://en.wikipedia.org/wiki/Apache_License

https://github.com/jbaldwin/libcappuccino

**libcappuccino** is licensed under the Apache 2.0 license.

# Overview #
* Thread safe cache datastructures.
** Can disable thread safety for singly threaded apps.
* The following eviction policies are currently supported:
** First in first out (FIFO).
** Least frequently used (LFU).
** Least frequently used with dynamic aging (LFUDA).
** Least recently used (LRU).
** Most recently used (MRU).
** Random Replacement (RR).
** Time aware least recently used (TLRU).
** Uniform time aware least recently used (UTLRU).

# Usage #

## Examples

See all of the examples under the examples/ directory.  Below are some simple examples
to get your started on using libcappuccino.


### Indivudal item TTL Least Recently Used Example
This example provides a individual item TTL LRU cache.  This means each item placed in the cache
can have its own TTL value and the eviction policy is TTL expired first and then LRU second.  This cache
is useful when items should have various TTLs applied to them.  Use the Uniform TTL LRU if every item
has the same TTL as it is more efficient on CPU and memory usage.  This type of cache could be compared
to the likes of how Redis/Memcached work but in memory of the application rather than a separate
process.

```C++
#include "cappuccino/Cappuccino.hpp"

#include <chrono>
#include <iostream>

int main(int argc, char* argv[])
{
    using namespace std::chrono_literals;

    // Create a cache with up to 3 items.
    cappuccino::TlruCache<uint64_t, std::string> lru_cache { 3 };

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
        auto hola  = lru_cache.Find(4); // "hola" was just inserted, it will be found
        auto hello = lru_cache.Find(1); // "hello" will also have a value, it is at the end of the lru list
        auto world = lru_cache.Find(2); // "world" is in the middle of our 3 lru list.
        auto nope  = lru_cache.Find(3); // "nope" was lru'ed when "hola" was inserted since "hello" and "world were fetched

        if(hola.has_value()) {
            std::cout << hola.value() << "\n";
        }

        if(hello.has_value()) {
            std::cout << hello.value() << "\n";
        }

        if(world.has_value()) {
            std::cout << world.value() << "\n";
        }

        if(!nope.has_value()) {
            std::cout << "Nope was LRU'ed out of the cache.\n";
        }
    }

    return 0;
}
```

#### Uniform TTL Least Recently Used Example
This example provides a uniform item TTL LRU cache.  Every item placed into the cache has the same
TTL value with the eviction policy being LRU.  This cache is useful when items should have the same
TTL value applied to them.

```C++
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
```

## Requirements
    C++17 compiler (g++/clang++)
    CMake
    make and/or ninja

## Instructions

### Building
    # This will produce a shared and static library to link against your project.
    mkdir Release && cd Release
    cmake -DCMAKE_BUILD_TYPE=Release ..
    cmake --build .

### CMake Projects

#### add_subdirectory()
To use within your cmake project you can clone the project or use git submodules and then `add_subdirectory` in the parent project's `CMakeList.txt`,
assuming the cappuccino code is in a `libcappuccino/` subdirectory of the parent project:

    add_subdirectory(libcappuccino)

To link to the `<project_name>` then use the following:

    add_executable(<project_name> main.cpp)
    target_link_libraries(<project_name> PRIVATE cappuccino)

Include cappuccino in the project's code by simply including `#include <cappuccino/Cappuccino.hpp>` as needed.

#### FetchContent
CMake can also include the project directly via a `FetchContent` declaration.  In your project's `CMakeLists.txt`
include the following code to download the git repository and make it available to link to.

    cmake_minimum_required(VERSION 3.11)

    # ... cmake project stuff ...

    include(FetchContent)
    FetchContent_Declare(
        cappuccino
        GIT_REPOSITORY https://github.com/jbaldwin/libcappuccino.git
        GIT_TAG        <TAG_OR_GIT_HASH>
    )
    FetchContent_MakeAvailable(cappuccino)

    # ... cmake project more stuff ...

    target_link_libraries(${PROJECT_NAME} PRIVATE cappuccino)

## Benchmarks

... todo ...

## Contributing and Testing

This project has a GitHub Actions CI implementation to compile and run unit tests.

Currently tested distros:
* ubuntu:latest
* fedora:latest

Currently tested compilers:
* g++-9
* clang-9

Contributing should ideally be a single commit if possible.  Any new feature should include relevant tests and examples
are welcome if understanding how the feature works is difficult or provides some additional value the tests otherwise cannot.

CMake is setup to understand how to run the tests.  Building and then running `ctest` will
execute the tests locally.

```bash
mkdir Release && cd Release
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build .
ctest -V
```

## Support

File bug reports, feature requests and questions using [GitHub Issues](https://github.com/jbaldwin/libcappuccino/issues)

Copyright © 2017-2020, Josh Baldwin
