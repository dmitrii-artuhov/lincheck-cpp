#include <cassert>
#include <iostream>
#include <optional>
#include <vector>

#include "../../runtime/include/lib.h"

int var{};

extern "C" void tick() { ++var; }

extern "C" Handle test_coro();

int main() {
  // Keep stack that contains launched tasks.
  StableVector<Task> stack;
  stack.emplace_back(Task{test_coro()});
  while (stack.size()) {
    auto &current = stack.back();
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
        stack.emplace_back(current.GetChild());
      }
    }
  }
}
