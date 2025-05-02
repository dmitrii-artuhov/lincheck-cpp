#include <atomic>
#include <iostream>
#include <tuple>

#include "runtime/include/verifying.h"
#include "runtime/include/verifying_macro.h"
#include "verifying/specs/register.h"

struct WmmTest {
  WmmTest() {}
  WmmTest(const WmmTest&) {}
  WmmTest& operator=(const WmmTest&) {
    return *this;
  }

  std::atomic<int> x{0}, y{0};

  non_atomic void A() {
    int r1 = y.load(std::memory_order_seq_cst);
    x.store(1, std::memory_order_seq_cst);
    std::string out = "r1 = " + std::to_string(r1);
    std::cout << out << std::endl;
  }

  non_atomic void B() {
    int r2 = x.load(std::memory_order_seq_cst);
    y.store(1, std::memory_order_seq_cst);
    std::string out = "r2 = " + std::to_string(r2);
    std::cout << out << std::endl;
  }
};

struct LinearWmmSpec {
  using method_t = std::function<int(LinearWmmSpec *l, void *)>;

  static auto GetMethods() {
    method_t A_func = [](LinearWmmSpec *l, void *) -> int {
      return 0;
    };

    method_t B_func = [](LinearWmmSpec *l, void *) -> int {
      return 0;
    };

    return std::map<std::string, method_t>{
      {"A", A_func},
      {"B", B_func},
    };
  }
};

struct LinearWmmHash {
  size_t operator()(const LinearWmmSpec &r) const { return 1; }
};

struct LinearWmmEquals {
  bool operator()(const LinearWmmSpec &lhs, const LinearWmmSpec &rhs) const {
    return true;
  }
};

using spec_t = ltest::Spec<WmmTest, LinearWmmSpec, LinearWmmHash, LinearWmmEquals>;

LTEST_ENTRYPOINT(
  spec_t,
  {
    {
      method_invocation(std::tuple(), void, WmmTest, A)
    },
    {
      method_invocation(std::tuple(), void, WmmTest, B)
    }
  }
);