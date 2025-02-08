#include "minimization.h"


// greedy
void GreedyRoundMinimizor::Minimize(
StrategyScheduler& sched,
Scheduler::BothHistories& nonlinear_history
) const {
  std::vector<std::reference_wrapper<const Task>> tasks;

  for (const HistoryEvent& event : nonlinear_history.second) {
    if (std::holds_alternative<Invoke>(event)) {
      tasks.push_back(std::get<Invoke>(event).GetTask());
    }
  }

  // remove single task
  for (auto& task : tasks) {
    if (task.get()->IsRemoved()) continue;

    // log() << "Try to remove task with id: " << task.get()->GetId() << "\n";
    auto new_histories = OnTasksRemoved(sched, nonlinear_history, { task.get() });

    if (new_histories.has_value()) {
      nonlinear_history.first.swap(new_histories.value().first);
      nonlinear_history.second.swap(new_histories.value().second);
      task.get()->SetRemoved(true);
    }
  }

  // remove two tasks (for operations with semantics of add/remove)
  for (size_t i = 0; i < tasks.size(); ++i) {
    auto& task_i = tasks[i];
    if (task_i.get()->IsRemoved()) continue;
    
    for (size_t j = i + 1; j < tasks.size(); ++j) {
      auto& task_j = tasks[j];
      if (task_j.get()->IsRemoved()) continue;
      
      // log() << "Try to remove tasks with ids: " << task_i.get()->GetId() << " and "
      //       << task_j.get()->GetId() << "\n";
      auto new_histories = OnTasksRemoved(sched, nonlinear_history, { task_i.get(), task_j.get() }); 

      if (new_histories.has_value()) {
        // sequential history (Invoke/Response events) must have even number of history events
        assert(new_histories.value().second.size() % 2 == 0);

        nonlinear_history.first.swap(new_histories.value().first);
        nonlinear_history.second.swap(new_histories.value().second);

        task_i.get()->SetRemoved(true);
        task_j.get()->SetRemoved(true);
        break; // tasks (i, j) were removed, so go to the next iteration of i
      }
    }
  }

  // replay minimized round one last time to put coroutines in `returned` state
  // (because multiple failed attempts to minimize new scenarios could leave tasks in invalid state)
  sched.ReplayRound(StrategyScheduler::GetTasksOrdering(nonlinear_history.first, {}));
}


// same interleaving
Scheduler::Result SameInterleavingMinimizor::OnTasksRemoved(
  StrategyScheduler& sched,
  const Scheduler::BothHistories& nonlinear_history,
  const std::vector<Task>& tasks
) const {
  std::unordered_set<int> task_ids;
  for (const auto& task : tasks) {
    task_ids.insert(task->GetId());
  }
  std::vector<int> new_ordering = StrategyScheduler::GetTasksOrdering(nonlinear_history.first, task_ids);
  return sched.ReplayRound(new_ordering);
}

// strategy exploration
StrategyExplorationMinimizor::StrategyExplorationMinimizor(int runs_): runs(runs_) {}

Scheduler::Result StrategyExplorationMinimizor::OnTasksRemoved(
  StrategyScheduler& sched,
  const Scheduler::BothHistories& nonlinear_history,
  const std::vector<Task>& tasks
) const {
  auto mark_tasks_as_removed = [&](bool is_removed) {
    for (const auto& task : tasks) {
      task->SetRemoved(is_removed);
    }
  };

  mark_tasks_as_removed(true);
  Scheduler::Result new_histories = sched.ExploreRound(runs);
  if (!new_histories.has_value()) {
    mark_tasks_as_removed(false);
  }

  return new_histories;
}