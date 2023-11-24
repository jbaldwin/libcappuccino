cmake_minimum_required(VERSION 2.8)
project(libcappuccino_tests)

set(LIBCAPPUCCINO_TEST_SOURCE_FILES
    catch.cpp
    test_fifo_cache.cpp
    test_lfu_cache.cpp
    test_lfuda_cache.cpp
    test_lru_cache.cpp
    test_mru_cache.cpp
    test_rr_cache.cpp
    test_tlru_cache.cpp
    test_ut_map.cpp
    test_ut_set.cpp
    test_utlru_cache.cpp
)

add_executable(${PROJECT_NAME} main.cpp ${LIBCAPPUCCINO_TEST_SOURCE_FILES})
target_link_libraries(${PROJECT_NAME} PRIVATE cappuccino pthread)

if(CAPPUCCINO_CODE_COVERAGE)
    target_compile_options(${PROJECT_NAME} PRIVATE --coverage)
    target_link_libraries(${PROJECT_NAME} PRIVATE gcov)
endif()

add_test(NAME LiftCappuccinoTest COMMAND ${PROJECT_NAME})
