cmake_minimum_required(VERSION 3.5)
project(libant)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_VERBOSE_MAKEFILE 1)
set(CMAKE_BUILD_TYPE "Debug")
set(ENABLE_DEBUG 1)  # 0 Relaese , 1 Debug, 2 RelWithDebInfo

set(SOURCE_FILES
        include/libant/log.h
        src/bencode.h
        src/bencode.cpp
        src/utils.cpp
        src/utils.hpp
        src/sha1.hpp
        src/multithread_queue.h
        src/network.h
        src/network.cpp
        src/libant.h
        src/libant.cpp
        src/libsrt.h
        src/libsrt.cpp
        src/logger.cpp
        src/logger.h
        src/channel_statistics.h
        src/channel_statistics.cpp)


set(SOURCE_FILES_SRT
        ${SOURCE_FILES}
        tests/stat.h
        tests/srt_test.cpp)

include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/thirdparty/srt/srtcore)  ## srt lib need to add include guard
include_directories(${CMAKE_SOURCE_DIR}/thirdparty/srt)
include_directories(${CMAKE_SOURCE_DIR}/thirdparty/srt/apps)
include_directories(${CMAKE_SOURCE_DIR}/cmake-build-debug/thirdparty/srt)

cmake_policy(SET CMP0003 NEW) ## Libraries linked via full path no longer produce linker search paths.

## todo: make libant to be compiled once for all applications
add_executable(srt_test ${SOURCE_FILES_SRT})
add_library(ant ${SOURCE_FILES})

target_compile_options(ant PUBLIC -O0 -g3 -Wall)
target_link_libraries(srt_test PUBLIC m pthread srt_static)

## add_definitions(-DPACKET_TRACER)
## add_definitions(-DANT_UNIT_TESTS)
## add_definitions(-DUSE_HOLEPUNCH) compilation error for this extension
## add_definitions(-DTEST_ANNOUNCE) ## send announce to myself

add_subdirectory(thirdparty/srt)

get_property(dirs DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY INCLUDE_DIRECTORIES)
foreach(dir ${dirs})
    message(STATUS "include dir='${dir}'")
endforeach()


