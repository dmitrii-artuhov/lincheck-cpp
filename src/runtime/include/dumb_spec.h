
#include <cassert>
#include <functional>
#include <map>
#include <string>

#include "verfying.h"

namespace target {

// Target object.
struct Register {
  void add();
  int get();

  void Reconstruct();

  Register(const Register &oth);
  Register();

  int x{};
};

}  // namespace target

namespace spec {

struct LinearRegister;

using method_t =
    std::function<int(LinearRegister *l, const std::vector<int> &args)>;

struct LinearRegister {
  int x = 0;
  int add() {
    ++x;
    return 0;
  }
  int get() { return x; }

  static auto GetMethods() {
    method_t add_func = [](LinearRegister *l,
                           const std::vector<int> &args) -> int {
      assert(args.empty());
      return l->add();
    };

    method_t get_func = [](LinearRegister *l,
                           const std::vector<int> &args) -> int {
      assert(args.empty());
      return l->get();
    };

    return std::map<std::string, method_t>{
        {"add", add_func},
        {"get", get_func},
    };
  }
};

struct LinearRegisterHash {
  size_t operator()(const LinearRegister &r) const { return r.x; }
};

struct LinearRegisterEquals {
  bool operator()(const LinearRegister &lhs, const LinearRegister &rhs) const {
    return lhs.x == rhs.x;
  }
};

}  // namespace spec

struct VerifyingSpec {
  using target_t = target::Register;
  using spec_t = spec::LinearRegister;

  // TODO: auto determine if std::hash is provided.
  using hash_t = spec::LinearRegisterHash;
  using equals_t = spec::LinearRegisterEquals;
};
