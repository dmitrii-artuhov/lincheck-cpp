
basic.bc: basic.cpp
	clang++ -O3 -c -emit-llvm basic.cpp

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

# Собрать пример:
# SAMPLE=<пример из verifying> FLAG=<флаг для создания спеки> make build
# SAMPLE=register.cpp FLAG=test_register  make build
.PHONY: build
build:
	clang++ -O3 -std=c++2a -c -emit-llvm ./verifying/$(SAMPLE) -o bytecode.bc
	opt --load-pass-plugin ./build/codegen/CoroGenPass.so \
	-passes="coro_gen" bytecode.bc -o res.bc
	llvm-dis res.bc -o res.ll
	clang++ -D$(FLAG) -O0 -g -std=c++2a res.ll ./verifying/run.cpp ./runtime/lib.cpp ./runtime/scheduler.cpp ./runtime/round_robin_strategy.cpp ./runtime/lin_check.cpp -o ./verifying/run
