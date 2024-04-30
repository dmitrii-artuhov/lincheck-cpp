# basic.cpp playground.

FLAGS = -std=c++2a -O3 -fno-discard-value-names
CC = clang++
DEPS = ./runtime/verifying.cpp ./runtime/lib.cpp ./runtime/generators.cpp ./runtime/builders.cpp ./runtime/logger.cpp ./runtime/lin_check.cpp ./runtime/pretty_printer.cpp ./runtime/scheduler.cpp
LLVM_PASS = ./build/codegen/CoroGenPass.so
BINARY = basic

basic.bc: basic.cpp ./runtime/include/verifying.h
	$(CC) $(FLAGS) -c -emit-llvm basic.cpp

.PHONY: regression
regression:
	make -Cbuild
	pytest -v ./test/regression

# Pass runs in the llvm pipeline. All works, but for now pass running at very early stage, when
# the code is not optimized (registerPipelineStartEPCallback). TODO.
# So, this type of build can produce too many suspend points.
# For example, nonlinearized queue is hacked only after ~80k rounds with tla strategy.
build_basic:
	$(CC) $(FLAGS) -fpass-plugin=$(LLVM_PASS) basic.cpp $(DEPS) -o $(BINARY)

# For now, pass not supports debug information in coroutines. TODO.
# So, to build for debug we have to run pass at the beginning and then build with debug information.
debug_basic:
	$(CC) $(FLAGS) -emit-llvm -S -fno-discard-value-names -fpass-plugin=$(LLVM_PASS) basic.cpp -o basic.bc
	$(CC) -g -std=c++2a -fno-discard-value-names basic.bc $(DEPS) -o $(BINARY)

# Pass runs after main llvm pipeline. This can lead to incostitency, when `alloca` results
# are not saved to the coroutine frame.
# This can produce invalid build when there is an `alloca` in the optimized code and the result
# is used after suspension
# (for example, std::shared_ptr<Token> as an argument of the target method).
# This build produces more or less optimal number of suspend points,
# nonlinearized queue is hacked after ~2k rounds with tla strategy (4 tasks, 1 switch)
#                                     ~300 rounds with tla strategy (3 tasks, 1 switch).
build_basic_unsafe: basic.bc
	$(CC) $(FLAGS) -fpass-plugin=$(LLVM_PASS) basic.bc $(DEPS) -o $(BINARY)
