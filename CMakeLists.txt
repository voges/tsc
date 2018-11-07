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

message(STATUS "CMAKE_CXX_COMPILER_ID=${CMAKE_CXX_COMPILER_ID})")
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wextra")
elseif ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wextra")
elseif ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Intel")
     set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
     set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wextra")
else ()
    message(FATAL_ERROR "C++ compiler currently not supported by CMake build")
endif ()


#==============================================================================
# Build type
#==============================================================================

# Set default build type
if (NOT CMAKE_BUILD_TYPE)
    message(WARNING "Unsupported build type (CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE})")
    set(CMAKE_BUILD_TYPE Debug)
    message(WARNING "Using default build type '${CMAKE_BUILD_TYPE}'")
endif ()

# Log the compiler flags for each build type
message(STATUS "C++ compiler flags: ${CMAKE_CXX_FLAGS}")
message(STATUS "Additional C++ compiler flags for build type 'Debug': ${CMAKE_CXX_FLAGS_DEBUG}")
message(STATUS "Additional C++ compiler flags for build type 'Release': ${CMAKE_CXX_FLAGS_RELEASE}")
message(STATUS "Additional C++ compiler flags for build type 'RelWithDebInfo': ${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
message(STATUS "Additional C++ compiler flags for build type 'MinSizeRel': ${CMAKE_CXX_FLAGS_MINSIZEREL}")
message(STATUS "Selected build type: ${CMAKE_BUILD_TYPE}")


#==============================================================================
# tsc
#==============================================================================

# Set the paths
set(tsc "tsc")
set(tsc_source_dir ${CMAKE_SOURCE_DIR}/source)
set(tsc_header_dir ${CMAKE_SOURCE_DIR}/source)
set(tsc_include_dir ${CMAKE_SOURCE_DIR}/source)

# Get all source and header files
file(GLOB_RECURSE tsc_source_files ${tsc_source_dir}/*.c)
file(GLOB_RECURSE tsc_header_files ${tsc_header_dir}/*.h)

# Group the source and header files
source_group("Source Files" FILES ${tsc_source_files})
source_group("Header Files" FILES ${tsc_source_files})

# Set up the target
add_executable(${tsc} ${tsc_source_files} ${tsc_header_files})
target_include_directories(${tsc} PRIVATE ${tsc_include_dir})
target_link_libraries(${tsc} z)
