cmake_minimum_required(VERSION 2.8)
project(cappuccino CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CAPPUCCINO_SOURCE_FILES
    inc/cappuccino/Cappuccino.hpp
    inc/cappuccino/CappuccinoLock.hpp
    inc/cappuccino/FifoCache.hpp
    inc/cappuccino/LfuCache.hpp
    inc/cappuccino/LfudaCache.hpp
    inc/cappuccino/LruCache.hpp
    inc/cappuccino/MruCache.hpp
    inc/cappuccino/RrCache.hpp
    inc/cappuccino/SyncImplEnum.hpp src/SyncImplEnum.cpp
    inc/cappuccino/TlruCache.hpp
    inc/cappuccino/UtlruCache.hpp
)

add_library(${PROJECT_NAME} STATIC ${CAPPUCCINO_SOURCE_FILES})
set_target_properties(${PROJECT_NAME} PROPERTIES LINKER_LANGUAGE CXX)
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/inc)

if(${CMAKE_CXX_COMPILER_ID} MATCHES "GNU")
    target_compile_options(
        ${PROJECT_NAME} PRIVATE
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

add_subdirectory(examples)
enable_testing()
add_subdirectory(test)
