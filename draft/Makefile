CC := clang++

FLAGS := -std=c++2a

BIN_DIR := bin

all: build_main

build_main: $(BIN_DIR)/main

$(BIN_DIR)/main: main.cpp lib.h
	mkdir -p bin
	$(CC) $(FLAGS) main.cpp -o $(BIN_DIR)/main

run_main: build_main
	./bin/main

# codegen mock
build_codegen: $(BIN_DIR)/codegen

$(BIN_DIR)/codegen: codegen.cpp lib.h
	mkdir -p bin
	$(CC) $(FLAGS) codegen.cpp -o $(BIN_DIR)/codegen

run_codegen: build_codegen
	./bin/codegen $(ARGS)
