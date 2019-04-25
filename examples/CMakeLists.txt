cmake_minimum_required(VERSION 3.0.2)
project(cappuccino_examples CXX)

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/${CMAKE_BUILD_TYPE}/bin)

### lfu_simple ###
project(lfu_simple CXX)
set(LFU_SIMPLE_SOURCE_FILES
    examples/lfu_simple.cpp
)
add_executable(${PROJECT_NAME} ${LFU_SIMPLE_SOURCE_FILES})
target_link_libraries(${PROJECT_NAME} PRIVATE cappuccino ${CAPPUCCINO_SYSTEM_LIBRARY_DEPENDENCIES})
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)

### lfuda_simple ###
project(lfuda_simple CXX)
set(LFUDA_SIMPLE_SOURCE_FILES
    examples/lfuda_simple.cpp
)
add_executable(${PROJECT_NAME} ${LFUDA_SIMPLE_SOURCE_FILES})
target_link_libraries(${PROJECT_NAME} PRIVATE cappuccino ${CAPPUCCINO_SYSTEM_LIBRARY_DEPENDENCIES})
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)

### lru_simple ###
project(lru_simple CXX)
set(LRU_SIMPLE_SOURCE_FILES
    examples/lru_simple.cpp
)
add_executable(${PROJECT_NAME} ${LRU_SIMPLE_SOURCE_FILES})
target_link_libraries(${PROJECT_NAME} PRIVATE cappuccino ${CAPPUCCINO_SYSTEM_LIBRARY_DEPENDENCIES})
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)

### tlru_simple ###
project(tlru_simple CXX)
set(TLRU_SIMPLE_SOURCE_FILES
    examples/tlru_simple.cpp
)
add_executable(${PROJECT_NAME} ${TLRU_SIMPLE_SOURCE_FILES})
target_link_libraries(${PROJECT_NAME} PRIVATE cappuccino ${CAPPUCCINO_SYSTEM_LIBRARY_DEPENDENCIES})
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)

### utlru_simple ###
project(utlru_simple CXX)
set(UTLRU_SIMPLE_SOURCE_FILES
    examples/utlru_simple.cpp
)
add_executable(${PROJECT_NAME} ${UTLRU_SIMPLE_SOURCE_FILES})
target_link_libraries(${PROJECT_NAME} PRIVATE cappuccino ${CAPPUCCINO_SYSTEM_LIBRARY_DEPENDENCIES})
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)

### bench ###
project(bench CXX)
set(TLRU_BENCH_SOURCE_FILES
    examples/bench.cpp
)
add_executable(${PROJECT_NAME} ${TLRU_BENCH_SOURCE_FILES})
target_link_libraries(${PROJECT_NAME} PRIVATE cappuccino ${CAPPUCCINO_SYSTEM_LIBRARY_DEPENDENCIES})
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)

### mru_simple ###
project(mru_simple CXX)
set(MRU_SIMPLE_SOURCE_FILES
    examples/mru_simple.cpp
)
add_executable(${PROJECT_NAME} ${MRU_SIMPLE_SOURCE_FILES})
target_link_libraries(${PROJECT_NAME} PRIVATE cappuccino ${CAPPUCCINO_SYSTEM_LIBRARY_DEPENDENCIES})
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)

### rr_simple ###
project(rr_simple CXX)
set(RR_SIMPLE_SOURCE_FILES
    examples/rr_simple.cpp
)
add_executable(${PROJECT_NAME} ${RR_SIMPLE_SOURCE_FILES})
target_link_libraries(${PROJECT_NAME} PRIVATE cappuccino ${CAPPUCCINO_SYSTEM_LIBRARY_DEPENDENCIES})
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)
