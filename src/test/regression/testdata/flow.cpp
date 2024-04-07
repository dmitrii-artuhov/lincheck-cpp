#include <iostream>
#include <string>

#include "../../../runtime/include/verifying.h"
#include "../../../runtime/include/lib.h"

void log(const std::string &msg) { std::cout << msg << std::endl; }

non_atomic int foo(int n) {
  log("foo(" + std::to_string(n) + ")");
  int mult = 0;
  coro_yield();
  for (int i = 0; i < n; ++i) {
    coro_yield();
    for (int j = i; j < n; ++j) {
      coro_yield();
      for (int k = 0; k < j; k += 2) {
        mult += i * j;
        coro_yield();
      }
    }
  }
  coro_yield();
  return mult;
}

non_atomic int bar(int n) {
  log("bar(" + std::to_string(n) + ")");
  int i = 0;
  int s = 0;
  while (i < n) {
    int f0 = 0;
    coro_yield();
    int f1 = 1;
    coro_yield();
    int j = 0;
    coro_yield();
    while (j < i) {
      int nxt = f0 + f1;
      coro_yield();
      f0 = f1;
      coro_yield();
      f1 = nxt;
      coro_yield();
      ++j;
    }
    s += f1;
    coro_yield();
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
    coro_yield();
    f(n / 2);
    int foo_res = foo(n);
    log("foo res: " + std::to_string(foo_res));
  }
  if (n & 1) {
    coro_yield();
    int bar_res = bar(n);
    log("bar res: " + std::to_string(bar_res));
  }
  f(n - 1);
}

non_atomic attr(ltesttarget_test) void test() { f(5); }
