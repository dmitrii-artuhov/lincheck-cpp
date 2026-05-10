#include <atomic>
#include <cassert>
#include <map>
#include <string>

#include "verifying/targets/wmm_litmus/litmus_common.h"

struct Exp29Test {
  std::atomic<int> x{0}, y{0};
  int r1 = -1, r2 = -1;

  non_atomic void A() {
    r1 = y.load(std::memory_order_relaxed);
    x.store(1, std::memory_order_relaxed);
    rassert(!(r1 == 1 && r2 == 1));
  }

  non_atomic void B() {
    r2 = x.load(std::memory_order_relaxed);
    y.store(1, std::memory_order_relaxed);
    rassert(!(r1 == 1 && r2 == 1));
  }
};

struct Exp29Spec {
  using method_t = std::function<ValueWrapper(Exp29Spec *, void *)>;
  static auto GetMethods() {
    method_t func = [](Exp29Spec *, void *) -> ValueWrapper { return void_v; };
    return std::map<std::string, method_t>{
        {"A", func},
        {"B", func},
    };
  }
};

using spec_t =
    ltest::Spec<Exp29Test, Exp29Spec, LinearWmmHash, LinearWmmEquals>;

LTEST_ENTRYPOINT(spec_t,
                 {{
                      method_invocation(std::tuple(), void, Exp29Test, A),
                  },
                  {
                      method_invocation(std::tuple(), void, Exp29Test, B),
                  }});
