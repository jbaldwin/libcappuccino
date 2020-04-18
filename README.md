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


### LRU Example
```C++
... todo ...
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
