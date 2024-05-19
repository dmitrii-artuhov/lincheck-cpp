# basic.cpp playground.

FLAGS = -std=c++2a -O3 -fno-discard-value-names
CC = clang++
DEPS = ./runtime/lib.cpp ./runtime/generators.cpp ./runtime/verifying.cpp  ./runtime/logger.cpp ./runtime/lin_check.cpp ./runtime/pretty_printer.cpp ./runtime/scheduler.cpp
LLVM_PASS = ./build/codegen/CoroGenPass.so
YIELD_INSERTER_PASS = ./build/codegen/YieldPass.so
BINARY = basic

.PHONY: regression
regression:
	make -Cbuild
	pytest -v ./test/regression

.PHONY: build_basic
build_basic:
	$(CC) $(FLAGS) -fpass-plugin=$(YIELD_INSERTER_PASS) basic.cpp $(DEPS) -o $(BINARY)

.PHONY: build_basic_optimized
build_basic_optimized:
	$(CC) $(FLAGS) -c -emit-llvm basic.cpp
	$(CC) $(FLAGS) -fpass-plugin=$(YIELD_INSERTER_PASS) basic.bc $(DEPS) -o $(BINARY)
