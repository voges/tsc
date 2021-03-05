#==============================================================================
# Generic Configuration
#==============================================================================

cmake_minimum_required(VERSION 3.16 FATAL_ERROR)
project("tsc")
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
set(CMAKE_CXX_STANDARD 17)

#==============================================================================
# Build Types
#==============================================================================

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
    message(STATUS "Using fall-back build type: ${CMAKE_BUILD_TYPE}")
endif()

if(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
    #
elseif(${CMAKE_BUILD_TYPE} STREQUAL "Release")
    #
else()
    message(FATAL_ERROR "Build type not supported: ${CMAKE_BUILD_TYPE}")
endif()

#==============================================================================
# Compile Options
#==============================================================================

if(${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU" OR ${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
    add_compile_options(-Wall -Wextra -pedantic -Werror)
elseif(${CMAKE_CXX_COMPILER_ID} MATCHES "MSVC")
    add_compile_options(/W4 /WX)
else()
    message(FATAL_ERROR "C++ compiler not supported: ${CMAKE_CXX_COMPILER_ID}")
endif()

#==============================================================================
# CMake Options
#==============================================================================

option(BUILD_TESTS "Build tests" OFF)

message(STATUS "")
message(STATUS "******** Options ********")
message(STATUS "  BUILD_TESTS: ${BUILD_TESTS}")
message(STATUS "")

if(${BUILD_TESTS})
    include(GoogleTest)
endif()

#==============================================================================
# Executable & Library Paths
#==============================================================================

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

#==============================================================================
# Include Paths
#==============================================================================

include_directories(${CMAKE_SOURCE_DIR}/src)

#==============================================================================
# Subdirectories
#==============================================================================

add_subdirectory(src)
if(${BUILD_TESTS})
    add_subdirectory(test)
endif()

#==============================================================================
# Summary
#==============================================================================

include(Summary)
print_summary()
