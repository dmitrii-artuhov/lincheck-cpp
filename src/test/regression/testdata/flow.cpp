#include <iostream>
#include <string>

#include "macro.h"

void log(const std::string &msg) { std::cout << msg << std::endl; }

int na foo(int n) {
  log("foo(" + std::to_string(n) + ")");
  int mult = 0;
  for (int i = 0; i < n; ++i) {
    for (int j = i; j < n; ++j) {
      for (int k = 0; k < j; k += 2) {
        mult += i * j;
      }
    }
  }
  return mult;
}

int na bar(int n) {
  log("bar(" + std::to_string(n) + ")");
  int i = 0;
  int s = 0;
  while (i < n) {
    int f0 = 0;
    int f1 = 1;
    int j = 0;
    while (j < i) {
      int nxt = f0 + f1;
      f0 = f1;
      f1 = nxt;
      ++j;
    }
    s += f1;
    ++i;
  }
  return s;
}

void na f(int n) {
  if (n == 0) {
    log("done");
    return;
  }
  if (n % 2 == 0) {
    f(n / 2);
    int foo_res = foo(n);
    log("foo res: " + std::to_string(foo_res));
  }
  if (n & 1) {
    int bar_res = bar(n);
    log("bar res: " + std::to_string(bar_res));
  }
  f(n - 1);
}

extern "C" void na test() { f(5); }
