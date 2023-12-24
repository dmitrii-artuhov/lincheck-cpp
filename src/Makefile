
basic.bc: ./test/codegen/basic.cpp
	@ clang++ -c -emit-llvm ./test/codegen/basic.cpp

# Run codegen and dump output to res.ll.
run_codegen: basic.bc
	@ make -C build > /dev/null
	@ opt --load-pass-plugin ./build/codegen/CoroGenPass.so \
	-passes="coro_gen" basic.bc > res.bc
	@ llvm-dis res.bc -o res.ll
