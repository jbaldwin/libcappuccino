cmake_minimum_required(VERSION 3.0.2)
project(cappuccino_examples CXX)

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/${CMAKE_BUILD_TYPE}/bin)

### lru_simple ###
set(LRU_SIMPLE_SOURCE_FILES
    examples/lru_simple.cpp
)
add_executable(lru_simple ${LRU_SIMPLE_SOURCE_FILES})
target_link_libraries(lru_simple PRIVATE cappuccino ${CAPPUCCINO_SYSTEM_LIBRARY_DEPENDENCIES})

### lru_bench ###
set(LRU_BENCH_SOURCE_FILES
    examples/lru_bench.cpp
)
add_executable(lru_bench ${LRU_BENCH_SOURCE_FILES})
target_link_libraries(lru_bench PRIVATE cappuccino ${CAPPUCCINO_SYSTEM_LIBRARY_DEPENDENCIES})
