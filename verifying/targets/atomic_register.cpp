/**
 * ./verify.py build --src ./targets/atomic_register.cpp
 * ./verify.py run --tasks 3 --strategy tla --rounds 100000
 */
#include <atomic>

#include "../../runtime/include/verifying.h"
#include "../specs/register.h"

struct Register {
  non_atomic void add() { x.fetch_add(1); }
  non_atomic int get() { return x.load(); }

  void Reset() { x.store(0); }

  std::atomic<int> x{};
};

using spec_t =
    ltest::Spec<Register, spec::LinearRegister, spec::LinearRegisterHash,
                spec::LinearRegisterEquals>;

LTEST_ENTRYPOINT(spec_t);

target_method(ltest::generators::genEmpty, void, Register, add);

target_method(ltest::generators::genEmpty, int, Register, get);