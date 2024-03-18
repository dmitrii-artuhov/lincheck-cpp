cmake_minimum_required(VERSION 3.6)
project(lintest)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
find_package(LLVM REQUIRED CONFIG)

# Uncomment for debug.
# message("llvm location: ${LLVM_DIR}")

include(FetchContent)

FetchContent_Declare(
        fuzztest
        GIT_REPOSITORY https://github.com/google/fuzztest.git
        GIT_TAG "main"
        SYSTEM
)

FetchContent_MakeAvailable(fuzztest)

INCLUDE_DIRECTORIES("${GMOCK_DIR}/include")
INCLUDE_DIRECTORIES("${GTEST_DIR}/include")

set(CMAKE_CXX_FLAGS "-DCMAKE_BUILD_TYPE=RelWithDebug -DFUZZTEST_FUZZING_MODE=on ${CMAKE_CXX_FLAGS}")
add_compile_options(-fsanitize=address -DADDRESS_SANITIZER)
add_link_options(-fsanitize=address -DADDRESS_SANITIZER)

include(GoogleTest)

fuzztest_setup_fuzzing_flags()
enable_testing()

include(AddLLVM)
add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS})
link_directories(${LLVM_LIBRARY_DIRS})
add_subdirectory(codegen)
add_subdirectory(runtime)

add_subdirectory(test)
