# libcappuccino - C++17 Cache and Associative Data Structure Library

[![CI](https://github.com/jbaldwin/libcappuccino/workflows/build/badge.svg)](https://github.com/jbaldwin/libcappuccino/workflows/build/badge.svg)
[![Coverage Status](https://coveralls.io/repos/github/jbaldwin/libcappuccino/badge.svg?branch=master)](https://coveralls.io/github/jbaldwin/libcappuccino?branch=master)
[![Total alerts](https://img.shields.io/lgtm/alerts/g/jbaldwin/libcappuccino.svg?logo=lgtm&logoWidth=18)](https://lgtm.com/projects/g/jbaldwin/libcappuccino/alerts/)
[![Codacy Badge](https://app.codacy.com/project/badge/Grade/8ecca4da783a437eba8c62964fed59ba)](https://www.codacy.com/gh/jbaldwin/libcappuccino/dashboard?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=jbaldwin/libcappuccino&amp;utm_campaign=Badge_Grade)
[![language][badge.language]][language]
[![license][badge.license]][license]

https://github.com/jbaldwin/libcappuccino

**libcappuccino** is licensed under the Apache 2.0 license.

## Overview
*   Thread safe cache and associative datastructures.
  *   Can disable thread safety for singly threaded apps.
*   The following eviction policies are currently supported:
  *   Cache (Fixed size contiguous memory).
    *   First in first out (FIFO).
    *   Least frequently used (LFU).
    *   Least frequently used with dynamic aging (LFUDA).
    *   Least recently used (LRU).
    *   Most recently used (MRU).
    *   Random Replacement (RR).
    *   Time aware least recently used (TLRU).
    *   Uniform time aware least recently used (UTLRU).
  *   Associative (Dynamic size non-contiguous memory).
    *   Uniform time aware set (UTSET).
    *   Uniform time aware map (UTMAP).

## Usage

### Examples

See all of the examples under the examples/ directory.  Below are some simple examples
to get your started on using libcappuccino.

#### Indivudal item TTL Least Recently Used Example
This example provides a individual item TTL LRU cache.  This means each item placed in the cache
can have its own TTL value and the eviction policy is TTL expired first and then LRU second.  This cache
is useful when items should have various TTLs applied to them.  Use the Uniform TTL LRU if every item
has the same TTL as it is more efficient on CPU and memory usage.  This type of cache could be compared
to the likes of how Redis/Memcached work but in memory of the application rather than a separate
process.

```C++
    #include <cappuccino/cappuccino.hpp>
    
    #include <chrono>
    #include <iostream>
    
    int main()
    {
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
            auto hola  = lru_cache.Find(4); // "hola" was just inserted, it will be found
            auto hello = lru_cache.Find(1); // "hello" will also have a value, it is at the end of the lru list
            auto world = lru_cache.Find(2); // "world" is in the middle of our 3 lru list.
            auto nope  = lru_cache.Find(3); // "nope" was lru'ed when "hola" was inserted
                                            // since "hello" and "world were fetched
    
            if (hola.has_value())
            {
                std::cout << hola.value() << "\n";
            }
    
            if (hello.has_value())
            {
                std::cout << hello.value() << "\n";
            }
    
            if (world.has_value())
            {
                std::cout << world.value() << "\n";
            }
    
            if (!nope.has_value())
            {
                std::cout << "Nope was LRU'ed out of the cache.\n";
            }
        }
    
        return 0;
    }
```

##### Insert, Update, Insert Or Update
Each `Insert()` method on the various caches takes an optional parameter `Allow` that tells the cache
how the insert call should behave.  By default this parameter is set to allow `INSERT_OR_UPDATE` on the
elements being inserted into the cache.  This means if they do not exist in the cache they will be added
and if they do exists the values will be udpated and any metadata like TTLs will be adjusted/reset.  The
caller can also specify this parameter as just `INSERT` to only allow the item to be added if it doesn't
already exist or as `UPDATE` to only change the item in the cahce if it already exists.

```C++
    #include <cappuccino/cappuccino.hpp>
    
    int main()
    {
        using namespace std::chrono_literals;
        using namespace cappuccino;
        // Uniform TTL LRU cache with a 1 hour TTL and 200 element cache capacity.
        // The key is uint64_t and the value is std::string.
        UtlruCache<uint64_t, std::string> lru_cache{1h, 200};
    
        lru_cache.Insert(1, "Hello", Allow::INSERT); // OK
        lru_cache.Insert(1, "Hello", Allow::INSERT); // ERROR! already exists
    
        // Note that Allow::INSERT can succeed if the item is TTLed out
        // in certain types of caches that support TTLs.
    
        lru_cache.Insert(1, "Hola", Allow::UPDATE);  // OK exists
        lru_cache.Insert(2, "World", Allow::UPDATE); // ERROR! doesn't exist
    
        lru_cache.Insert(2, "World"); // OK, parameter defaults to Allow::INSERT_OR_UPDATE
        return 0;
    }
```

### Requirements
    C++17 compiler (g++/clang++)
    CMake
    make and/or ninja

### Instructions

#### Building
    # This will produce a shared and static library to link against your project.
    mkdir Release && cd Release
    cmake -DCMAKE_BUILD_TYPE=Release ..
    cmake --build .

#### CMake Projects

##### add_subdirectory()
To use within your cmake project you can clone the project or use git submodules and then `add_subdirectory` in the parent project's `CMakeList.txt`,
assuming the cappuccino code is in a `libcappuccino/` subdirectory of the parent project:

```bash
    add_subdirectory(libcappuccino)
```

To link to the `<project_name>` then use the following:

    add_executable(<project_name> main.cpp)
    target_link_libraries(<project_name> PRIVATE cappuccino)

Include cappuccino in the project's code by simply including `#include <cappuccino/Cappuccino.hpp>` as needed.

##### FetchContent
CMake can also include the project directly via a `FetchContent` declaration.  In your project's `CMakeLists.txt`
include the following code to download the git repository and make it available to link to.

```bash
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
```

### Contributing and Testing

This project has a GitHub Actions CI implementation to compile and run unit tests.

Currently tested distros:
*   ubuntu:latest
*   fedora:latest

Currently tested compilers:
*   g++-9
*   clang-9

Contributing a new feature should include relevant tests.  Examples
are welcome if understanding how the feature works is difficult or provides some additional value the tests otherwise cannot.

CMake is setup to understand how to run the tests.  Building and then running `ctest` will
execute the tests locally.

```bash
mkdir Release && cd Release
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build .
ctest -VV
```

### Support

File bug reports, feature requests and questions using [GitHub Issues](https://github.com/jbaldwin/libcappuccino/issues)

Copyright © 2017-2020, Josh Baldwin

[badge.language]: https://img.shields.io/badge/language-C%2B%2B17-yellow.svg
[badge.license]: https://img.shields.io/badge/license-Apache--2.0-blue

[language]: https://en.wikipedia.org/wiki/C%2B%2B17
[license]: https://en.wikipedia.org/wiki/Apache_License
