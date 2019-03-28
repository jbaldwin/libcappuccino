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

### lru_uniform_ttl_simple ###
set(LRU_UNIFORM_TTL_SIMPLE_SOURCE_FILES
    examples/lru_uniform_ttl_simple.cpp
    )
add_executable(lru_uniform_ttl_simple ${LRU_UNIFORM_TTL_SIMPLE_SOURCE_FILES})
target_link_libraries(lru_uniform_ttl_simple PRIVATE cappuccino ${CAPPUCCINO_SYSTEM_LIBRARY_DEPENDENCIES})

### lru_uniform_ttl_bench ###
set(LRU_UNIFORM_TTL_BENCH_SOURCE_FILES
    examples/lru_uniform_ttl_bench.cpp
    )
add_executable(lru_uniform_ttl_bench ${LRU_UNIFORM_TTL_BENCH_SOURCE_FILES})
target_link_libraries(lru_uniform_ttl_bench PRIVATE cappuccino ${CAPPUCCINO_SYSTEM_LIBRARY_DEPENDENCIES})
