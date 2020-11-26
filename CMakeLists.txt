cmake_minimum_required(VERSION 2.8)
project(cappuccino CXX)

# Set the githooks directory to auto format and update the readme.
message("git config core.hooksPath .githooks")
execute_process(
    COMMAND git config core.hooksPath .githooks
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(CAPPUCCINO_BUILD_EXAMPLES "Build the examples. Default=ON" ON)
option(CAPPUCCINO_BUILD_TESTS    "Build the tests. Default=ON" ON)
option(CAPPUCCINO_CODE_COVERAGE  "Enable code coverage, tests must also be enabled. Default=OFF" OFF)

set(CAPPUCCINO_SOURCE_FILES
    inc/cappuccino/allow.hpp src/allow.cpp
    inc/cappuccino/cappuccino.hpp
    inc/cappuccino/fifo_cache.hpp
    inc/cappuccino/lfu_cache.hpp
    inc/cappuccino/lfuda_cache.hpp
    inc/cappuccino/lock.hpp src/lock.cpp
    inc/cappuccino/lru_cache.hpp
    inc/cappuccino/mru_cache.hpp
    inc/cappuccino/peek.hpp src/peek.cpp
    inc/cappuccino/rr_cache.hpp
    inc/cappuccino/tlru_cache.hpp
    inc/cappuccino/ut_map.hpp
    inc/cappuccino/ut_set.hpp
    inc/cappuccino/utlru_cache.hpp
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
    if(CAPPUCCINO_CODE_COVERAGE)
        target_compile_options(${PROJECT_NAME} PRIVATE --coverage)
        target_link_libraries(${PROJECT_NAME} PRIVATE gcov)
    endif()

    enable_testing()
    add_subdirectory(test)
endif()
