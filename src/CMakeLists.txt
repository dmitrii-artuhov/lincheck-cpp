cmake_minimum_required(VERSION 3.6)
project(lintest)

set(CMAKE_CXX_STANDARD 20)
find_package(LLVM REQUIRED CONFIG)

# need this crunch for github ci https://github.com/actions/runner-images/issues/8659
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

# Uncomment for debug.
# message("llvm location: ${LLVM_DIR}")

#set(CMAKE_CXX_FLAGS "-DCMAKE_BUILD_TYPE=RelWithDebug -DFUZZTEST_FUZZING_MODE=on ${CMAKE_CXX_FLAGS}")
#add_compile_options(-fsanitize=address -DADDRESS_SANITIZER)
add_compile_options(-g)
#add_compile_options(-I/opt/homebrew/opt/protobuf@26/include)
#add_link_options(-fsanitize=address -DADDRESS_SANITIZER)
add_link_options(-g)

add_subdirectory(third_party)

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
add_subdirectory(verifying)
