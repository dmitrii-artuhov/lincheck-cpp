
#include <cassert>
#include <optional>

#include "../../runtime/include/lib.h"

Task find_task(TaskBuilderList l, InitFuncList init_funcs) {
  assert(init_funcs->empty());
  std::optional<Task> task;
  for (auto task_builder : *l) {
    auto cur_task = Task{nullptr, task_builder};
    if (cur_task.GetName() == "test") {
      assert(cur_task.GetArgs().empty() && "test must be task without args");
      task = cur_task;
      break;
    }
  }
  assert(task.has_value() && "task `test` is not found");
  return task.value();
}
