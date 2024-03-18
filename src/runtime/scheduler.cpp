#include "include/scheduler.h"

Scheduler::Scheduler(Strategy& sched_class, ModelChecker& checker,
                     size_t max_tasks, size_t max_rounds)
    : strategy(sched_class),
      checker(checker),
      max_tasks(max_tasks),
      max_rounds(max_rounds) {}

Scheduler::round_result_t Scheduler::runRound() {
  // History of invoke and response events which is required for the checker
  std::vector<std::variant<Invoke, Response>> sequential_history;
  // Full history of the current execution in the Run function
  std::vector<std::reference_wrapper<StackfulTask>> full_history;

  for (size_t finished_tasks = 0; finished_tasks < max_tasks;) {
    auto [next_task, is_new, thread_id] = strategy.Next();

    // fill the sequential history
    if (is_new) {
      sequential_history.emplace_back(Invoke(next_task, thread_id));
    }
    full_history.emplace_back(next_task);

    next_task.Resume();
    if (next_task.IsReturned()) {
      finished_tasks++;

      auto result = next_task.GetRetVal();
      sequential_history.emplace_back(Response(next_task, result, thread_id));
    }
  }

  if (!checker.Check(sequential_history)) {
    return std::make_pair(full_history, sequential_history);
  }

  return std::nullopt;
}

Scheduler::round_result_t Scheduler::Run() {
  for (size_t i = 0; i < max_rounds; ++i) {
    auto seq_history = runRound();
    if (seq_history.has_value()) {
      return seq_history;
    }
    strategy.StartNextRound();
  }

  return std::nullopt;
}
