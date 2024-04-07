#include <cassert>
#include <iostream>
#include <optional>
#include <vector>

#include "../../runtime/include/lib.h"

int var{};

extern "C" void tick() { ++var; }

extern "C" handle test_coro();

int main() {
  auto task = Task{test_coro()};
  // Keep stack that contains launched tasks.
  std::vector<Task> stack = {task};
  while (stack.size()) {
    auto current = stack.back();
    if (current.IsReturned()) {
#ifndef no_trace
      std::cout << "returned " << current.GetRetVal() << std::endl;
#endif
      stack.pop_back();
      if (!stack.empty()) {
        stack.back().ClearChild();
      }
    } else {
      current.Resume();
#ifndef no_trace
      std::cout << var << std::endl;
#endif
      if (current.HasChild()) {
        stack.push_back(current.GetChild());
      }
    }
  }
}
