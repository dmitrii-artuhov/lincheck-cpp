#include "include/scheduler.h"

StackfulTaskInvoke::StackfulTaskInvoke(const StackfulTask& task) : task(task) {}

StackfulTaskResponse::StackfulTaskResponse(const StackfulTask& task, int result)
    : task(task), result(result) {}

Scheduler::Scheduler(SchedulerClass& sched_class, ModelChecker& checker,
                     size_t max_switches)
    : full_history({}),
      sequential_history({}),
      sched_class(sched_class),
      checker(checker),
      max_tasks(max_switches) {}

std::optional<std::vector<std::reference_wrapper<StackfulTask>>>
Scheduler::Run() {
  for (size_t finished_tasks = 0; finished_tasks < max_tasks;) {
    auto [next_task, is_new] = sched_class.Next();

    // fill the sequential history
    if (is_new) {
      sequential_history.emplace_back(StackfulTaskInvoke(next_task));
    }
    full_history.emplace_back(next_task);

    next_task.Resume();
    if (next_task.IsReturned()) {
      finished_tasks++;

      auto result = next_task.GetRetVal();
      sequential_history.emplace_back(StackfulTaskResponse(next_task, result));

      if (!checker.Check(sequential_history)) {
        return full_history;
      }
    }
  }

  return std::nullopt;
}
