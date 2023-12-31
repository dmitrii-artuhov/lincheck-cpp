include_directories(include)
add_llvm_pass_plugin(CoroGenPass coropass.cpp)
