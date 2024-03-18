
#include <cassert>
#include <optional>

#include "../../runtime/include/lib.h"

Task find_task(TaskBuilderList l) {
  std::optional<Task> task;
  std::vector<int> args;
  for (auto task_builder : *l) {
    auto cur_task = task_builder(&args);
    if (cur_task.GetName() == "test") {
      assert(args.empty() && "test must be task without args");
      task = cur_task;
      break;
    }
  }
  assert(task.has_value() && "task `test` is not found");
  return task.value();
}
