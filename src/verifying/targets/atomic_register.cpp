#include <atomic>

#include "../../runtime/include/verifying.h"
#include "../specs/register.h"

struct Register {
  void add();
  int get();

  void Reconstruct() { x.store(0); }
  Register(const Register& oth) { x.store(oth.x.load()); }
  Register(){};

  std::atomic<int> x{};
};

TARGET_METHOD(void, Register, add, ()) { x.fetch_add(1); }

TARGET_METHOD(int, Register, get, ()) { return x.load(); }

using spec_t = Spec<Register, spec::LinearRegister, spec::LinearRegisterHash,
                    spec::LinearRegisterEquals>;

LTEST_ENTRYPOINT(spec_t);
