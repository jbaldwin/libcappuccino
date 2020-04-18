cmake_minimum_required(VERSION 2.8)
project(libcappuccino_tests)

set(LIBCAPPUCCINO_TEST_SOURCE_FILES
    FifoTest.hpp
)

add_executable(${PROJECT_NAME} main.cpp ${LIBCAPPUCCINO_TEST_SOURCE_FILES})
target_link_libraries(${PROJECT_NAME} PRIVATE cappuccino)

add_test(NAME LiftCappuccinoTest COMMAND ${PROJECT_NAME})
