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
  int r1 = -1, r2 = -1;

  // Example 1
  non_atomic void Exp1_A() {
    r1 = y.load(std::memory_order_seq_cst);
    x.store(1, std::memory_order_seq_cst);
    std::string out = "r1 = " + std::to_string(r1) + "\n";
    std::cout << out;

    assert(!(r1 == 1 && r2 == 1));
  }

  non_atomic void Exp1_B() {
    r2 = x.load(std::memory_order_seq_cst);
    y.store(1, std::memory_order_seq_cst);
    std::string out = "r2 = " + std::to_string(r2) + "\n";
    std::cout << out;
    
    assert(!(r1 == 1 && r2 == 1));
  }

  // Example 2
  non_atomic void Exp2_A() {
    r1 = 1;
    x.store(2, std::memory_order_seq_cst);
  }

  non_atomic void Exp2_B() {
    if (x.load(std::memory_order_seq_cst) == 2) {
      assert(r1 == 1);
    }
    std::string out = "r1 = " + std::to_string(r1) + "\n";
    std::cout << out;
  }

  // Example 3
  non_atomic void Exp3_A() {
    y.store(20, std::memory_order_seq_cst);
    x.store(10, std::memory_order_seq_cst);
  }

  non_atomic void Exp3_B() {
    if (x.load(std::memory_order_seq_cst) == 10) {
      assert(y.load(std::memory_order_seq_cst) == 20);
      y.store(10, std::memory_order_seq_cst);
    }
  }

  non_atomic void Exp3_C() {
    if (y.load(std::memory_order_seq_cst) == 10) {
      assert(x.load(std::memory_order_seq_cst) == 10);
    }
  }
};

struct LinearWmmSpec {
  using method_t = std::function<int(LinearWmmSpec *l, void *)>;

  static auto GetMethods() {
    method_t func = [](LinearWmmSpec *l, void *) -> int { return 0; };

    return std::map<std::string, method_t>{
      {"Exp1_A", func},
      {"Exp1_B", func},

      {"Exp2_A", func},
      {"Exp2_B", func},

      {"Exp3_A", func},
      {"Exp3_B", func},
      {"Exp3_C", func},
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

using spec_t =
    ltest::Spec<WmmTest, LinearWmmSpec, LinearWmmHash, LinearWmmEquals>;

LTEST_ENTRYPOINT(spec_t, 
  // {
  //   {
  //     method_invocation(std::tuple(), void, WmmTest, Exp1_A)
  //   },
  //   {
  //     method_invocation(std::tuple(), void, WmmTest, Exp1_B)
  //   }
  // },
  // {
  //   {
  //     method_invocation(std::tuple(), void, WmmTest, Exp2_A)
  //   },
  //   {
  //     method_invocation(std::tuple(), void, WmmTest, Exp2_B)
  //   }
  // },
  {
    {
      method_invocation(std::tuple(), void, WmmTest, Exp3_A)
    },
    {
      method_invocation(std::tuple(), void, WmmTest, Exp3_B)
    },
    {
      method_invocation(std::tuple(), void, WmmTest, Exp3_C)
    }
  }
);