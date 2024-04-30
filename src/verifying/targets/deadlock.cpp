/**
 * ./verify.py build --src ./targets/deadlock.cpp
 * ./verify.py run -v --tasks 5 --strategy rr
 * ./verify.py run -v --tasks 5 --strategy random
 *
 * It important to limit switches.
 * ./verify.py run -v --tasks 2 --strategy tla --rounds 100000 --switches 4
 */
#include <atomic>
#include <cstring>

#include "../lib/mutex.h"

// Test is implementation and the specification at the same time.
struct Test {
  Test() {}

  void Lock(std::shared_ptr<Token>, int);

  void Reset() {
    mu1 = Mutex{};
    mu2 = Mutex{};
  }

  Mutex mu1, mu2{};

  using method_t = std::function<int(Test *t, void *args)>;

  static auto GetMethods() {
    method_t lock_func = [](Test *l, void *args) -> int {
      // `void` return type is always return 0 equivalent.
      return 0;
    };

    return std::map<std::string, method_t>{
        {"Lock", lock_func},
    };
  }
};

auto generateInt() { return ltest::generators::makeSingleArg(rand() % 10 + 1); }

auto generateArgs() {
  auto token = ltest::generators::genToken();
  auto _int = generateInt();
  return std::tuple_cat(token, _int);
}

// Lock(odd) in parallel with Lock(even) causes deadlock.
target_method(generateArgs, void, Test, Lock, std::shared_ptr<Token> token,
              int v) {
  if (v % 2 == 0) {
    mu1.Lock(token);
    mu2.Lock(token);
  } else {
    mu2.Lock(token);
    mu1.Lock(token);
  }
  mu1.Unlock();
  mu2.Unlock();
}

using spec_t = ltest::Spec<Test, Test>;

LTEST_ENTRYPOINT(spec_t);
