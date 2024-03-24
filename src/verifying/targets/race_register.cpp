#include "../../runtime/include/verifying.h"
#include "../specs/register.h"

struct Register {
  void add();
  int get();

  void Reconstruct() { x = 0; }
  Register(const Register& oth) { x = oth.x; }
  Register(){};

  int x{};
};

TARGET_METHOD(void, Register, add, ()) { ++x; }

TARGET_METHOD(int, Register, get, ()) { return x; }

using spec_t = Spec<Register, spec::LinearRegister, spec::LinearRegisterHash,
                    spec::LinearRegisterEquals>;

LTEST_ENTRYPOINT(spec_t);
