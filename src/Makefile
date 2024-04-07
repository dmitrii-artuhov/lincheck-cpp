
basic.bc: basic.cpp ./runtime/include/verifying.h
	clang++ -std=c++2a -O3 -c -emit-llvm basic.cpp

# Run codegen and dump output to res.ll.
run_codegen: basic.bc
	@ make -C build > /dev/null
	@ opt --load-pass-plugin ./build/codegen/CoroGenPass.so \
	-passes="coro_gen" basic.bc -o res.bc
	@ llvm-dis res.bc -o res.ll

.PHONY: regression
regression:
	make -Cbuild
	pytest -v ./test/regression

DEPS=./runtime/verifying.cpp ./runtime/lib.cpp ./runtime/generators.cpp ./runtime/builders.cpp ./runtime/logger.cpp ./runtime/lin_check.cpp

build_basic: basic.bc
	clang++ -emit-llvm -S -O3 -std=c++2a -fno-discard-value-names basic.cpp -o res.ll
	clang++ -O3 -std=c++2a  -fpass-plugin=./build/codegen/CoroGenPass.so  -fno-discard-value-names res.ll $(DEPS)
