cmake_minimum_required(VERSION 3.6)

set(HEADER_FILES
        include/lib.h
        include/lincheck.h
        include/lincheck_recursive.h
        include/logger.h
        include/pct_strategy.h
        include/pretty_print.h
        include/round_robin_strategy.h
        include/scheduler.h
        include/uniform_strategy.h
        include/verifying.h
)

add_library(runtime lib.cpp lin_check.cpp logger.cpp scheduler.cpp verifying.cpp ${HEADER_FILES})
target_include_directories(runtime PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
