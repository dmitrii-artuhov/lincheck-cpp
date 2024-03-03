#include "include/scheduler.h"

Invoke::Invoke(const StackfulTask& task) : task(task) {}

Response::Response(const StackfulTask& task, int result)
    : task(task), result(result) {}

Scheduler::Scheduler(Strategy& sched_class, ModelChecker& checker,
                     size_t max_tasks, size_t max_rounds)
    : strategy(sched_class),
      checker(checker),
      max_tasks(max_tasks),
      max_rounds(max_rounds) {}

std::optional<std::vector<std::reference_wrapper<StackfulTask>>>
Scheduler::runRound() {
  // History of invoke and response events which is required for the checker
  std::vector<std::variant<Invoke, Response>> sequential_history;
  // Full history of the current execution in the Run function
  std::vector<std::reference_wrapper<StackfulTask>> full_history;

  for (size_t finished_tasks = 0; finished_tasks < max_tasks;) {
    auto [next_task, is_new] = strategy.Next();

    // fill the sequential history
    if (is_new) {
      sequential_history.emplace_back(Invoke(next_task));
    }
    full_history.emplace_back(next_task);

    next_task.Resume();
    if (next_task.IsReturned()) {
      finished_tasks++;

      auto result = next_task.GetRetVal();
      sequential_history.emplace_back(Response(next_task, result));
    }
  }

  if (!checker.Check(sequential_history)) {
    return full_history;
  }

  return std::nullopt;
}

std::optional<std::vector<std::reference_wrapper<StackfulTask>>>
Scheduler::Run() {
  for (size_t i = 0; i < max_rounds; ++i) {
    auto seq_history = runRound();
    if (seq_history.has_value()) {
      return seq_history;
    }
    strategy.StartNextRound(max_tasks);
  }

  return std::nullopt;
}
