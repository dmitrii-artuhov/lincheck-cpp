#include <iostream>

#include "macro.h"

namespace generators {

int v = 42;

int gen next_int() { return v++; }

};  // namespace generators

extern "C" na void test(int x, int y, int z) {
  std::cout << x << std::endl;
  std::cout << y << std::endl;
  std::cout << z << std::endl;
}
