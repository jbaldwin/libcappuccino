cmake_minimum_required(VERSION 2.8)
project(cappuccino_examples CXX)

### allow_example ###
project(cap_allow_simple CXX)
add_executable(${PROJECT_NAME} allow_example.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE cappuccino)

### fifo_simple ###
project(cap_fifo_simple CXX)
add_executable(${PROJECT_NAME} fifo_simple.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE cappuccino)

### lfu_simple ###
project(cap_lfu_simple CXX)
add_executable(${PROJECT_NAME} lfu_simple.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE cappuccino)

### lfuda_simple ###
project(cap_lfuda_simple CXX)
add_executable(${PROJECT_NAME} lfuda_simple.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE cappuccino)

### lru_simple ###
project(cap_lru_simple CXX)
add_executable(${PROJECT_NAME} lru_simple.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE cappuccino)

### tlru_simple ###
project(cap_tlru_simple CXX)
add_executable(${PROJECT_NAME} tlru_simple.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE cappuccino)

### utlru_simple ###
project(cap_utlru_simple CXX)
add_executable(${PROJECT_NAME} utlru_simple.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE cappuccino)

### bench ###
project(cap_bench CXX)
add_executable(${PROJECT_NAME} bench.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE cappuccino pthread)

### mru_simple ###
project(cap_mru_simple CXX)
add_executable(${PROJECT_NAME} mru_simple.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE cappuccino)

### rr_simple ###
project(cap_rr_simple CXX)
add_executable(${PROJECT_NAME} rr_simple.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE cappuccino)

### utmap_simple ###
project(cap_utmap_simple CXX)
add_executable(${PROJECT_NAME} ut_map_simple.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE cappuccino)

### utset_simple ###
project(cap_utset_simple CXX)
add_executable(${PROJECT_NAME} ut_set_simple.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE cappuccino)
