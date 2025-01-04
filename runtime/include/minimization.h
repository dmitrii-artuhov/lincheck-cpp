#pragma once

#include "scheduler_fwd.h"
#include "lib.h"

struct RoundMinimizor {
  // Minimizes number of tasks in the nonlinearized history; modifies argument `nonlinear_history`.
  virtual void Minimize(
    SchedulerWithReplay& sched,
    Scheduler::Histories& nonlinear_history
  ) const = 0;
};

struct GreedyRoundMinimizor : public RoundMinimizor {
  void Minimize(
    SchedulerWithReplay& sched,
    Scheduler::Histories& nonlinear_history
  ) const override;

  virtual Scheduler::Result OnSingleTaskRemoved(
    SchedulerWithReplay& sched,
    const Scheduler::Histories& nonlinear_history,
    const Task& task
  ) const = 0;

  virtual Scheduler::Result OnTwoTasksRemoved(
    SchedulerWithReplay& sched,
    const Scheduler::Histories& nonlinear_history,
    const Task& task_i,
    const Task& task_j
  ) const = 0;
};

struct SameInterleavingMinimizor : public GreedyRoundMinimizor {
  Scheduler::Result OnSingleTaskRemoved(
    SchedulerWithReplay& sched,
    const Scheduler::Histories& nonlinear_history,
    const Task& task
  ) const override;

  Scheduler::Result OnTwoTasksRemoved(
    SchedulerWithReplay& sched,
    const Scheduler::Histories& nonlinear_history,
    const Task& task_i,
    const Task& task_j
  ) const override;
};

struct StrategyExplorationMinimizor : public GreedyRoundMinimizor {
  StrategyExplorationMinimizor() = delete;
  explicit StrategyExplorationMinimizor(int runs_): runs(runs_) {}

  Scheduler::Result OnSingleTaskRemoved(
    SchedulerWithReplay& sched,
    const Scheduler::Histories& nonlinear_history,
    const Task& task
  ) const override;

  Scheduler::Result OnTwoTasksRemoved(
    SchedulerWithReplay& sched,
    const Scheduler::Histories& nonlinear_history,
    const Task& task_i,
    const Task& task_j
  ) const override;

private:
  int runs;
};