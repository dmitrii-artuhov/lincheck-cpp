cmake_minimum_required(VERSION 3.6)

enable_testing()

include(GoogleTest)

#include(FetchContent)
#FetchContent_Declare(googletest
#        GIT_REPOSITORY https://github.com/google/googletest.git
#        GIT_TAG        v1.14.0
#        GIT_SHALLOW    TRUE
#        GIT_PROGRESS   TRUE
#        FIND_PACKAGE_ARGS NAMES GTest gtest
#)
#FetchContent_MakeAvailable(googletest)
#

INCLUDE_DIRECTORIES("${GMOCK_DIR}/include")

add_library(mocks stackfulltask_mock.h)
target_link_libraries(
        mocks
        PRIVATE
        runtime
        GTest::gmock_main
        GTest::gmock
)

add_executable(
        lin_check_test
        lin_check_test.cpp
)

target_compile_options(lin_check_test PRIVATE -Wall -Wextra -Werror -pedantic-errors)

target_link_libraries(
        lin_check_test
        PRIVATE
        runtime
        mocks
)

link_fuzztest(lin_check_test)
gtest_discover_tests(lin_check_test)
