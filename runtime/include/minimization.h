#include "scheduler.h"
#include "lib.h"

struct RoundMinimizor {
  // Minimizes number of tasks in the nonlinearized history; modifies argument `nonlinear_history`.
  virtual void minimize(
    StrategyScheduler& sched,
    Scheduler::Histories& nonlinear_history
  ) const = 0;
};

struct GreedyRoundMinimizor : public RoundMinimizor {
  void minimize(
    StrategyScheduler& sched,
    Scheduler::Histories& nonlinear_history
  ) const override {
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
      auto new_histories = onSingleTaskRemoved(sched, nonlinear_history, task.get());

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
        auto new_histories = onTwoTasksRemoved(sched, nonlinear_history, task_i.get(), task_j.get());

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
    sched.replayRound(StrategyScheduler::getTasksOrdering(nonlinear_history.first, {}));
  };

  virtual Scheduler::Result onSingleTaskRemoved(
    StrategyScheduler& sched,
    const Scheduler::Histories& nonlinear_history,
    const Task& task
  ) const = 0;

  virtual Scheduler::Result onTwoTasksRemoved(
    StrategyScheduler& sched,
    const Scheduler::Histories& nonlinear_history,
    const Task& task_i,
    const Task& task_j
  ) const = 0;
};

struct InterleavingMinimizor : public GreedyRoundMinimizor {
  Scheduler::Result onSingleTaskRemoved(
    StrategyScheduler& sched,
    const Scheduler::Histories& nonlinear_history,
    const Task& task
  ) const override {
    std::vector<int> new_ordering = StrategyScheduler::getTasksOrdering(nonlinear_history.first, { task->GetId() });
    return sched.replayRound(new_ordering);
  }

  Scheduler::Result onTwoTasksRemoved(
    StrategyScheduler& sched,
    const Scheduler::Histories& nonlinear_history,
    const Task& task_i,
    const Task& task_j
  ) const override {
    std::vector<int> new_ordering = StrategyScheduler::getTasksOrdering(nonlinear_history.first, { task_i->GetId(), task_j->GetId() });
    return sched.replayRound(new_ordering);
  }
};

struct StrategyMinimizor : public GreedyRoundMinimizor {
  StrategyMinimizor() = delete;
  explicit StrategyMinimizor(int runs_): runs(runs_) {}

  Scheduler::Result onSingleTaskRemoved(
    StrategyScheduler& sched,
    const Scheduler::Histories& nonlinear_history,
    const Task& task
  ) const override {
    task->SetRemoved(true);
    Scheduler::Result new_histories = sched.exploreRound(runs);

    if (!new_histories.has_value()) {
      task->SetRemoved(false);
    }

    return new_histories;
  }

  Scheduler::Result onTwoTasksRemoved(
    StrategyScheduler& sched,
    const Scheduler::Histories& nonlinear_history,
    const Task& task_i,
    const Task& task_j
  ) const override {
    task_i->SetRemoved(true);
    task_j->SetRemoved(true);
    Scheduler::Result new_histories = sched.exploreRound(runs);

    if (!new_histories.has_value()) {
      task_i->SetRemoved(false);
      task_j->SetRemoved(false);
    }

    return new_histories;
  }

private:
  int runs;
};