/**
 * ./verify.py build --src ./targets/race_register.cpp
 * ./verify.py run
 */
#include "../../runtime/include/verifying.h"
#include "../specs/register.h"

struct Register {
  void add() { ++x; }

  int get() { return x; }

  void Reset() { x = 0; }

  int x{};
};

using spec_t =
    ltest::Spec<Register, spec::LinearRegister, spec::LinearRegisterHash,
                spec::LinearRegisterEquals>;

LTEST_ENTRYPOINT(spec_t);

target_method(ltest::generators::genEmpty, void, Register, add);

target_method(ltest::generators::genEmpty, int, Register, get);