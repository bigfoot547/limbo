cmake_minimum_required(VERSION 3.16 FATAL_ERROR)

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_CURRENT_BINARY_DIR)
    message(FATAL_ERROR "*** In-source builds are not supported.")
endif()

set(PROJECT_NAME limbo)
set(SERVER_TARGET_NAME ${PROJECT_NAME})

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
    message(STATUS "*** Defaulting to build type ${CMAKE_BUILD_TYPE}.")
endif()

set(VERSION_MAJOR 0)
set(VERSION_MINOR 1)
set(VERSION_PATCH 0)
set(VERSION_NAME ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH})

set(VERSION_RELEASE OFF)

if(NOT VERSION_RELEASE)
    set(VERSION_NAME ${VERSION_NAME}-dev)
endif()

if(CMAKE_BUILD_TYPE STREQUAL Debug)
    set(VERSION_NAME ${VERSION_NAME}-debug)
endif()

project(${PROJECT_NAME})
message(STATUS "*** Will build version ${VERSION_NAME}")

add_subdirectory(src)

add_executable(${SERVER_TARGET_NAME} ${${PROJECT_NAME}_SOURCES})
configure_file(include/build_config.h.in include/build_config.h)
target_include_directories(${SERVER_TARGET_NAME} PRIVATE include ${CMAKE_CURRENT_BINARY_DIR}/include)

find_package(Threads REQUIRED)
if(NOT CMAKE_USE_PTHREADS_INIT)
    message(FATAL_ERROR "*** This program must use a pthread-compatible threads library. Sorry.")
endif()

include(CheckSymbolExists)
check_symbol_exists(epoll_create "sys/epoll.h" HAS_EPOLL_CREATE)

if(HAS_EPOLL_CREATE)
    target_compile_definitions(${SERVER_TARGET_NAME} PRIVATE SOCKET_ENGINE_EPOLL)
    message(STATUS "*** Using socket engine: epoll")
else()
    # TODO: search for kqueue or /dev/poll or fall back to poll(2)
    message(FATAL_ERROR "*** This program only works with epoll currently.")
endif()

target_compile_options(${SERVER_TARGET_NAME} PRIVATE -Wall -Wextra -pedantic)

target_link_libraries(${SERVER_TARGET_NAME} PRIVATE Threads::Threads)

option(ENABLE_ASAN "Enables address sanitizer (gcc only probably)" OFF)
if(ENABLE_ASAN)
    target_compile_options(${SERVER_TARGET_NAME} PRIVATE -fsanitize=address)
    target_link_options(${SERVER_TARGET_NAME} PRIVATE -fsanitize=address)
    message(STATUS "*** Compiling with address sanitizer - performance will be impacted!")
endif()


if(CMAKE_BUILD_TYPE STREQUAL Debug)
    target_compile_options(${SERVER_TARGET_NAME} PRIVATE -O0 -g)
else()
    target_compile_options(${SERVER_TARGET_NAME} PRIVATE -O3)
endif()
