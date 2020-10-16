cmake_minimum_required(VERSION 2.8)
project(libcappuccino_tests)

set(LIBCAPPUCCINO_TEST_SOURCE_FILES
    FifoTest.hpp
    LfuTest.hpp
    LfudaTest.hpp
    LruTest.hpp
    MruTest.hpp
    RrTest.hpp
    UtMapTest.hpp
    UtSetTest.hpp
    TlruTest.hpp
    UtlruTest.hpp
)

add_executable(${PROJECT_NAME} main.cpp ${LIBCAPPUCCINO_TEST_SOURCE_FILES})
target_link_libraries(${PROJECT_NAME} PRIVATE cappuccino pthread)

if(CAPPUCCINO_CODE_COVERAGE)
    target_compile_options(${PROJECT_NAME} PRIVATE --coverage)
    target_link_libraries(${PROJECT_NAME} PRIVATE gcov)
endif()

add_test(NAME LiftCappuccinoTest COMMAND ${PROJECT_NAME})
