#==============================================================================
# Generic
#==============================================================================

# We require at least CMake 3.1
cmake_minimum_required(VERSION 3.1)
message(STATUS "CMake generator: ${CMAKE_GENERATOR}")

# C and C++ code shall be compliant to C(++)11
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 11)

# This is tsc
project("tsc")


#==============================================================================
# Compiler
#==============================================================================

message(STATUS "CMAKE_CXX_COMPILER_ID=${CMAKE_CXX_COMPILER_ID}")
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wextra")
elseif("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wextra")
elseif("${CMAKE_CXX_COMPILER_ID}" MATCHES "Intel")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wextra")
else()
    message(FATAL_ERROR "C++ compiler currently not supported by CMake build")
endif()


#==============================================================================
# Build type
#==============================================================================

# Set default build type
if(NOT CMAKE_BUILD_TYPE)
    message(STATUS "Build type not specified")
    set(CMAKE_BUILD_TYPE Debug)
    message(STATUS "Using fall-back build type '${CMAKE_BUILD_TYPE}'")
endif()

# Log the compiler flags for each build type
message(STATUS "C++ compiler flags: ${CMAKE_CXX_FLAGS}")
message(STATUS "Additional C++ compiler flags for build type 'Debug': ${CMAKE_CXX_FLAGS_DEBUG}")
message(STATUS "Additional C++ compiler flags for build type 'Release': ${CMAKE_CXX_FLAGS_RELEASE}")
message(STATUS "Additional C++ compiler flags for build type 'RelWithDebInfo': ${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
message(STATUS "Additional C++ compiler flags for build type 'MinSizeRel': ${CMAKE_CXX_FLAGS_MINSIZEREL}")
message(STATUS "Selected build type: ${CMAKE_BUILD_TYPE}")


#==============================================================================
# Subdirectories
#==============================================================================

add_subdirectory(source/tsc ${CMAKE_CURRENT_BINARY_DIR}/tsc)
