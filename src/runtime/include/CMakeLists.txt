cmake_minimum_required(VERSION 2.6)

add_library(lib scheduler.h lib.h scheduler_class.h)
set_target_properties(lib PROPERTIES LINKER_LANGUAGE CXX)
