cmake_minimum_required(VERSION 3.0.2)
project(cappuccino CXX)

set(LIBRARY_OUTPUT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/${CMAKE_BUILD_TYPE}/lib)

set(CAPPUCCINO_SYSTEM_LIBRARY_DEPENDENCIES pthread)

set(SOURCE_FILES
    inc/cappuccino/Cappuccino.h src/Cappuccino.cpp
    inc/cappuccino/LruCache.h inc/cappuccino/LruCache.tcc
    inc/cappuccino/LruCacheUniformTtl.h inc/cappuccino/LruCacheUniformTtl.tcc
)

add_library(${PROJECT_NAME} STATIC ${SOURCE_FILES})
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/inc)

target_link_libraries(${PROJECT_NAME} PRIVATE
    ${CAPPUCCINO_SYSTEM_LIBRARY_DEPENDENCIES}
)

if(UNIX)
    target_compile_options(${PROJECT_NAME} PRIVATE
        -Wall
        -Wextra
        -Weffc++
        -Werror
        -Wpedantic
        -pedantic-errors
        -MD
    )
endif()

include(examples/CMakeLists.txt)

