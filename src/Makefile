
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
