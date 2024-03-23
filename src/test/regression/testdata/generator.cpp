#include <iostream>

#include "../../../runtime/include/verifying.h"

namespace generators {

int v = 42;

generator int next_int() { return v++; }

};  // namespace generators

struct State {
  void test(int, int, int);
};

TARGET_METHOD(void, State, test, (int x, int y, int z)) {
  std::cout << x << std::endl;
  std::cout << y << std::endl;
  std::cout << z << std::endl;
}
