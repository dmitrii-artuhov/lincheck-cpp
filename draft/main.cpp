#include "lib.h"
#include <iostream>
#include <optional>

Task<int> getA() {
  std::cout << "getA begins" << std::endl;
  co_return 2;
}

Task<int> sum() {
  int res = 0;
  for (int i = 0; i < 5; ++i) {
    std::cout << "iter " << i << std::endl;
    auto cur_task = getA();

    int val = co_await cur_task;
    res += val;
  }

  std::cout << "result is ready, return" << std::endl;
  co_return res;
}

int main() {
  Task<int> sum_task = sum();
  execute(sum_task);
  return 0;
}
