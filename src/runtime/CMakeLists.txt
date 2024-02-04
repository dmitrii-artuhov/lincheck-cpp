cmake_minimum_required(VERSION 2.6)

add_library(runtime lib.cpp run.cpp scheduler.cpp lin_check.cpp)
add_library(lincheck lin_check.cpp)
target_include_directories(runtime PUBLIC "${CMAKE_CURRENT_LIST_DIR}")
