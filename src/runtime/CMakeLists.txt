cmake_minimum_required(VERSION 3.6)

set(HEADER_FILES
        include/lib.h
        include/lincheck.h
        include/scheduler.h
        include/lincheck_recursive.h
        include/round_robin_strategy.h
)

add_library(runtime lib.cpp run.cpp scheduler.cpp lin_check.cpp round_robin_strategy.cpp ${HEADER_FILES})
target_include_directories(runtime PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
