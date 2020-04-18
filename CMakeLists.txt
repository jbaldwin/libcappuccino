cmake_minimum_required(VERSION 2.8)
project(cappuccino CXX)

set(CAPPUCCINO_SOURCE_FILES
    inc/cappuccino/Cappuccino.h
    inc/cappuccino/CappuccinoLock.h
    inc/cappuccino/FifoCache.h inc/cappuccino/FifoCache.tcc
    inc/cappuccino/LfuCache.h inc/cappuccino/LfuCache.tcc
    inc/cappuccino/LfudaCache.h inc/cappuccino/LfudaCache.tcc
    inc/cappuccino/LruCache.h inc/cappuccino/LruCache.tcc
    inc/cappuccino/MruCache.h inc/cappuccino/MruCache.tcc
    inc/cappuccino/RrCache.h inc/cappuccino/RrCache.tcc
    inc/cappuccino/SyncImplEnum.h
    inc/cappuccino/TlruCache.h inc/cappuccino/TlruCache.tcc
    inc/cappuccino/UtlruCache.h inc/cappuccino/UtlruCache.tcc
)

add_library(${PROJECT_NAME} STATIC ${CAPPUCCINO_SOURCE_FILES})
set_target_properties(${PROJECT_NAME} PROPERTIES LINKER_LANGUAGE CXX)
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/inc)

if(${CMAKE_CXX_COMPILER_ID} MATCHES "GNU")
    target_compile_options(
        ${PROJECT_NAME} PRIVATE
        -Wno-unknown-pragmas
    )
endif()

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

add_subdirectory(examples)
enable_testing()
add_subdirectory(test)
