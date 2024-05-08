#include <iostream>
#include <string>

#include "../../../runtime/include/verifying.h"
#include "../../../runtime/include/lib.h"

void log(const std::string &msg) { std::cout << msg << std::endl; }

non_atomic int foo(int n) {
  log("foo(" + std::to_string(n) + ")");
  int mult = 0;
  CoroYield();
  for (int i = 0; i < n; ++i) {
    CoroYield();
    for (int j = i; j < n; ++j) {
      CoroYield();
      for (int k = 0; k < j; k += 2) {
        mult += i * j;
        CoroYield();
      }
    }
  }
  CoroYield();
  return mult;
}

non_atomic int bar(int n) {
  log("bar(" + std::to_string(n) + ")");
  int i = 0;
  int s = 0;
  while (i < n) {
    int f0 = 0;
    CoroYield();
    int f1 = 1;
    CoroYield();
    int j = 0;
    CoroYield();
    while (j < i) {
      int nxt = f0 + f1;
      CoroYield();
      f0 = f1;
      CoroYield();
      f1 = nxt;
      CoroYield();
      ++j;
    }
    s += f1;
    CoroYield();
    ++i;
  }
  return s;
}

non_atomic void f(int n) {
  if (n == 0) {
    log("done");
    return;
  }
  if (n % 2 == 0) {
    CoroYield();
    f(n / 2);
    int foo_res = foo(n);
    log("foo res: " + std::to_string(foo_res));
  }
  if (n & 1) {
    CoroYield();
    int bar_res = bar(n);
    log("bar res: " + std::to_string(bar_res));
  }
  f(n - 1);
}

non_atomic attr(ltesttarget_test) void test() { f(5); }
