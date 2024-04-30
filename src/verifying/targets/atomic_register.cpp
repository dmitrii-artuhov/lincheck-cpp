/**
 * ./verify.py build --src ./targets/atomic_register.cpp
 * ./verify.py run --tasks 3 --strategy tla --rounds 100000
 */
#include <atomic>

#include "../../runtime/include/verifying.h"
#include "../specs/register.h"

struct Register {
  void add();
  int get();

  void Reset() { x.store(0); }

  std::atomic<int> x{};
};

target_method(ltest::generators::empty_gen, void, Register, add) {
  x.fetch_add(1);
}

target_method(ltest::generators::empty_gen, int, Register, get) {
  return x.load();
}

using spec_t =
    ltest::Spec<Register, spec::LinearRegister, spec::LinearRegisterHash,
                spec::LinearRegisterEquals>;

LTEST_ENTRYPOINT(spec_t);
