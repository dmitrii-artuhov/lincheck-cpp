#include <iostream>

#define na __attribute((__annotate__(("nonatomic"))))
#define gen __attribute((__annotate__("generator")))

namespace generators {

int v = 42;

int gen next_int() { return v++; }

};  // namespace generators

extern "C" na void test(int x, int y, int z) {
  std::cout << x << std::endl;
  std::cout << y << std::endl;
  std::cout << z << std::endl;
}
