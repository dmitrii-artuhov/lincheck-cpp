#pragma once

#include "scheduler_fwd.h"
#include "lib.h"

struct RoundMinimizor {
    virtual Scheduler::Result onSingleTaskRemoved(SchedulerWithReplay* sched, const Scheduler::Histories& nonlinear_history, const Task& task) const = 0;

    virtual Scheduler::Result onTwoTasksRemoved(SchedulerWithReplay* sched, const Scheduler::Histories& nonlinear_history, const Task& task_i, const Task& task_j) const = 0;
};

struct InterleavingMinimizor : public RoundMinimizor {
    Scheduler::Result onSingleTaskRemoved(
        SchedulerWithReplay* sched,
        const Scheduler::Histories& nonlinear_history,
        const Task& task
    ) const override;

    Scheduler::Result onTwoTasksRemoved(
        SchedulerWithReplay* sched,
        const Scheduler::Histories& nonlinear_history,
        const Task& task_i,
        const Task& task_j
    ) const override;
};

struct StrategyMinimizor : public RoundMinimizor {
    StrategyMinimizor() = delete;
    explicit StrategyMinimizor(int runs_): runs(runs_) {}

    Scheduler::Result onSingleTaskRemoved(
        SchedulerWithReplay* sched,
        const Scheduler::Histories& nonlinear_history,
        const Task& task
    ) const override;

    Scheduler::Result onTwoTasksRemoved(
        SchedulerWithReplay* sched,
        const Scheduler::Histories& nonlinear_history,
        const Task& task_i,
        const Task& task_j
    ) const override;

private:
    int runs;
};