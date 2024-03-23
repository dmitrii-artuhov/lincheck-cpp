#include <atomic>

#include "../specs/register.h"

namespace target {

// Target object.
struct Register {
  void add();
  int get();

  void Reconstruct();

  Register(const Register &oth);
  Register();

  std::atomic<int> x{};
};

}  // namespace target

struct VerifyingSpec {
  using target_t = target::Register;
  using spec_t = spec::LinearRegister;

  // TODO: auto determine if std::hash is provided.
  using hash_t = spec::LinearRegisterHash;
  using equals_t = spec::LinearRegisterEquals;
};
