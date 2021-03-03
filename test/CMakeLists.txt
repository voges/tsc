project("test")
add_executable(test sam_test.cc main.cc)
target_link_libraries(test PRIVATE gtest_main)
