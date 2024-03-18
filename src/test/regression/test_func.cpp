#include <cassert>
#include <iostream>
#include <optional>
#include <vector>

#include "../../runtime/include/lib.h"

Task find_task(TaskBuilderList);

extern "C" {

int var{};
void tick() { ++var; }

// This function runs `test` task until it and all children are terminated.
void run(TaskBuilderList l) {
  auto task = find_task(l);
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
}