#include <cassert>
#include <iostream>
#include <optional>
#include <vector>

#include "../../runtime/include/lib.h"

Task find_task(TaskBuilderList);

int var{};

extern "C" void tick() { ++var; }

int main() {
  std::vector<TaskBuilder> task_builders;
  fill_ctx(&task_builders);
  auto task = find_task(&task_builders);
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
