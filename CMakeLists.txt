cmake_minimum_required(VERSION 3.1 FATAL_ERROR)

project("tsc")

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
    message(STATUS "Using fall-back build type '${CMAKE_BUILD_TYPE}'")
endif()

set(CMAKE_C_STANDARD 11)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wextra")

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

include_directories("${CMAKE_SOURCE_DIR}/src")
include_directories("${CMAKE_SOURCE_DIR}/third_party")

add_subdirectory(src)
add_subdirectory(third_party)
