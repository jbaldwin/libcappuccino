cmake_minimum_required(VERSION 2.8)
project(libcappuccino_test)

include(../Setup.cmake)
include(${MAKEDIR}/SetupLibraryTests.cmake)

set(SOURCE_FILES_LIB_CAPPUCCINO_TEST
        CatchMain.cpp
        UnitTestTlruCache.cpp
)

enable_testing()
add_executable(${PROJECT_NAME} ${SOURCE_FILES_LIB_CAPPUCCINO_TEST})
add_test(LibCappuccinoTest ${EXECUTABLE_OUTPUT_PATH}${PROJECT_NAME})

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../inc)
target_link_libraries(${PROJECT_NAME} PRIVATE pthread)
