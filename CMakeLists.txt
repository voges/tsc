cmake_minimum_required(VERSION 3.16)
project("tsc")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

if(MSVC)
    # warning level 4 and all warnings as errors
    add_compile_options(/W4 /WX)
else()
    # lots of warnings and all warnings as errors
    add_compile_options(-Wall -Wextra -pedantic -Werror)
endif()

include_directories("${CMAKE_SOURCE_DIR}/src")

option(BUILD_TESTS "Build tests" ON)
if(${BUILD_TESTS})
    include(GoogleTest)
    add_subdirectory(test)
endif()

add_subdirectory(src)

