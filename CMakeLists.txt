cmake_minimum_required(VERSION 2.8)
project(cappuccino CXX)

option(CAPPUCCINO_BUILD_EXAMPLES "Build the examples. Default=ON" ON)
option(CAPPUCCINO_BUILD_TESTS    "Build the tests. Default=ON" ON)
option(CAPPUCCINO_CODE_COVERAGE  "Enable code coverage, tests must also be enabled. Default=OFF" OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CAPPUCCINO_SOURCE_FILES
    inc/cappuccino/Allow.hpp src/Allow.cpp
    inc/cappuccino/Cappuccino.hpp
    inc/cappuccino/FifoCache.hpp
    inc/cappuccino/LfuCache.hpp
    inc/cappuccino/LfudaCache.hpp
    inc/cappuccino/Lock.hpp src/Lock.cpp
    inc/cappuccino/LruCache.hpp
    inc/cappuccino/MruCache.hpp
    inc/cappuccino/Peek.hpp src/Peek.cpp
    inc/cappuccino/RrCache.hpp
    inc/cappuccino/TlruCache.hpp
    inc/cappuccino/UtlruCache.hpp
)

add_library(${PROJECT_NAME} STATIC ${CAPPUCCINO_SOURCE_FILES})
set_target_properties(${PROJECT_NAME} PROPERTIES LINKER_LANGUAGE CXX)
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/inc)

if(${CMAKE_CXX_COMPILER_ID} MATCHES "GNU")
    target_compile_options(${PROJECT_NAME} PRIVATE
        -Wno-unknown-pragmas
        -Wall
        -Wextra
        -Weffc++
        -Werror
        -Wpedantic
        -pedantic-errors
    )
elseif(${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
    target_compile_options(${PROJECT_NAME} PRIVATE
        -Wall
        -Wextra
        -Weffc++
        -Werror
        -Wpedantic
        -pedantic-errors
    )
endif()

if(CAPPUCCINO_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(CAPPUCCINO_BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()
