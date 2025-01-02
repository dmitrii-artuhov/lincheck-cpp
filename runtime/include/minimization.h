#include "scheduler.h"
#include "lib.h"

struct RoundMinimizor {
    virtual Scheduler::Result onSingleTaskRemoved(StrategyScheduler* sched, const Scheduler::Histories& nonlinear_history, const Task& task) const = 0;

    virtual Scheduler::Result onTwoTasksRemoved(StrategyScheduler* sched, const Scheduler::Histories& nonlinear_history, const Task& task_i, const Task& task_j) const = 0;
};

struct InterleavingMinimizor : public RoundMinimizor {
    Scheduler::Result onSingleTaskRemoved(
        StrategyScheduler* sched,
        const Scheduler::Histories& nonlinear_history,
        const Task& task
    ) const override {
        std::vector<int> new_ordering = sched->getTasksOrdering(nonlinear_history.first, { task->GetId() });
        return sched->replayRound(new_ordering);
    }

    Scheduler::Result onTwoTasksRemoved(
        StrategyScheduler* sched,
        const Scheduler::Histories& nonlinear_history,
        const Task& task_i,
        const Task& task_j
    ) const override {
        std::vector<int> new_ordering = sched->getTasksOrdering(nonlinear_history.first, { task_i->GetId(), task_j->GetId() });
        return sched->replayRound(new_ordering);
    }
};

struct StrategyMinimizor : public RoundMinimizor {
    StrategyMinimizor() = delete;
    explicit StrategyMinimizor(int runs_): runs(runs_) {}

    Scheduler::Result onSingleTaskRemoved(StrategyScheduler* sched, const Scheduler::Histories& nonlinear_history, const Task& task) const override {
        task->SetRemoved(true);
        Scheduler::Result new_histories = sched->exploreRound(runs);

        if (!new_histories.has_value()) {
        task->SetRemoved(false);
        }

        return new_histories;
    }

    Scheduler::Result onTwoTasksRemoved(StrategyScheduler* sched, const Scheduler::Histories& nonlinear_history, const Task& task_i, const Task& task_j) const override {
        task_i->SetRemoved(true);
        task_j->SetRemoved(true);
        Scheduler::Result new_histories = sched->exploreRound(runs);

        if (!new_histories.has_value()) {
        task_i->SetRemoved(false);
        task_j->SetRemoved(false);
        }

        return new_histories;
    }

private:
    int runs;
};