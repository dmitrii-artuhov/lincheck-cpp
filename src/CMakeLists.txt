cmake_minimum_required(VERSION 3.6)
project(lintest)

set(CMAKE_CXX_STANDARD 20)
find_package(LLVM REQUIRED CONFIG)

# Uncomment for debug.
# message("llvm location: ${LLVM_DIR}")

include(AddLLVM)
add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS})
link_directories(${LLVM_LIBRARY_DIRS})
add_subdirectory(codegen)
add_subdirectory(runtime)

add_subdirectory(test)
