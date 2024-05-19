cmake_minimum_required(VERSION 3.6)

set(HEADER_FILES
        include/generators.h
        include/lib.h
        include/lincheck_dual.h
        include/lincheck_recursive.h
        include/lincheck.h
        include/logger.h
        include/pct_strategy.h
        include/pick_strategy.h
        include/pretty_print.h
        include/round_robin_strategy.h
        include/scheduler.h
        include/stable_vector.h
        include/verifying_macro.h
        include/verifying.h
)

add_library(runtime generators.cpp lib.cpp lin_check.cpp logger.cpp pretty_printer.cpp scheduler.cpp verifying.cpp ${HEADER_FILES})
target_include_directories(runtime PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
