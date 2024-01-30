#pragma once
#include "lib.h"
#include "checker.h"
#include "scheduler_class.h"
#include <variant>
#include <optional>

struct Scheduler {
  Scheduler(SchedulerClass sched_class, ModelChecker checker, size_t max_tasks);

  std::optional<std::vector<ActionHandle>> Run();

private:
  // Contains stacks snapshots
  std::vector<ActionHandle> full_history;
  // history for a linearizability checker
  std::vector<std::variant<StackfulTaskInvoke, StackfulTaskResponse>> sequential_history;

  SchedulerClass sched_class;

  ModelChecker checker;

  // The number of maximum terminated tasks(methods)
  size_t max_tasks;
};
