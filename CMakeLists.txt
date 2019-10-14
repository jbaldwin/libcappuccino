cmake_minimum_required(VERSION 3.0.2)
project(cappuccino CXX)

set(LIBRARY_OUTPUT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/${CMAKE_BUILD_TYPE}/lib)

set(CAPPUCCINO_SYSTEM_LIBRARY_DEPENDENCIES pthread)

set(SOURCE_FILES
    inc/cappuccino/Cappuccino.h src/Cappuccino.cpp
    inc/cappuccino/LockScopeGuard.h
    inc/cappuccino/LruCache.h inc/cappuccino/LruCache.tcc
    inc/cappuccino/MruCache.h inc/cappuccino/MruCache.tcc
    inc/cappuccino/RrCache.h inc/cappuccino/RrCache.tcc
    inc/cappuccino/SyncImplEnum.h
    inc/cappuccino/TlruCache.h inc/cappuccino/TlruCache.tcc
    inc/cappuccino/UtlruCache.h inc/cappuccino/UtlruCache.tcc
)

add_library(${PROJECT_NAME} STATIC ${SOURCE_FILES})
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)

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

if(${COMPILE_TESTS})
    include(examples/CMakeLists.txt)

    add_subdirectory(test)
endif()
