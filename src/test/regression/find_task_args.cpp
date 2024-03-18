#include <cassert>
#include <optional>

#include "../../runtime/include/lib.h"

Task find_task(TaskBuilderList l) {
  std::optional<Task> task;
  std::vector<int> args;
  for (auto task_builder : *l) {
    auto cur_task = task_builder(&args);
    if (cur_task.GetName() == "test") {
      assert(args.size() == 3);
      assert(args[0] == 42);
      assert(args[1] == 43);
      assert(args[2] == 44);
      task = cur_task;
      break;
    }
  }
  assert(task.has_value() && "task `test` is not found");
  return task.value();
}
