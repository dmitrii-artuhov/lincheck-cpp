cmake_minimum_required(VERSION 3.6)

include(FetchContent)
#FetchContent_Declare(
#        googletest
#        URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
#)
#
#set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
#FetchContent_MakeAvailable(googletest)

FetchContent_Declare(
        fuzztest
        GIT_REPOSITORY https://github.com/google/fuzztest.git
        GIT_TAG "main"
)

FetchContent_MakeAvailable(fuzztest)

fuzztest_setup_fuzzing_flags()
enable_testing()
include(GoogleTest)

# I don't know why, but fuzzing isn't working if i have GTest::gmock_main and fuzzing in the same target
# so I've split this
add_library(proxy_lib lin_check_test.cpp)
target_link_libraries(
        proxy_lib
        PRIVATE
        runtime
        GTest::gmock_main
)


add_executable(
        lin_check_test
        lin_check_test.cpp
)

target_link_libraries(
        lin_check_test
        PRIVATE
        runtime
        proxy_lib
)

link_fuzztest(lin_check_test)
link_fuzztest(proxy_lib)
gtest_discover_tests(lin_check_test)
