#include "scheduler.h"
#include "minimization.h"


Scheduler::Result InterleavingMinimizor::onSingleTaskRemoved(
    SchedulerWithReplay* sched,
    const Scheduler::Histories& nonlinear_history,
    const Task& task
) const {
    std::vector<int> new_ordering = sched->getTasksOrdering(nonlinear_history.first, { task->GetId() });
    return sched->replayRound(new_ordering);
}

Scheduler::Result InterleavingMinimizor::onTwoTasksRemoved(
    SchedulerWithReplay* sched,
    const Scheduler::Histories& nonlinear_history,
    const Task& task_i,
    const Task& task_j
) const {
    std::vector<int> new_ordering = sched->getTasksOrdering(nonlinear_history.first, { task_i->GetId(), task_j->GetId() });
    return sched->replayRound(new_ordering);
}


Scheduler::Result StrategyMinimizor::onSingleTaskRemoved(
    SchedulerWithReplay* sched,
    const Scheduler::Histories& nonlinear_history,
    const Task& task
) const {
    task->SetRemoved(true);
    Scheduler::Result new_histories = sched->exploreRound(runs);

    if (!new_histories.has_value()) {
    task->SetRemoved(false);
    }

    return new_histories;
}

Scheduler::Result StrategyMinimizor::onTwoTasksRemoved(
    SchedulerWithReplay* sched,
    const Scheduler::Histories& nonlinear_history,
    const Task& task_i,
    const Task& task_j
) const {
    task_i->SetRemoved(true);
    task_j->SetRemoved(true);
    Scheduler::Result new_histories = sched->exploreRound(runs);

    if (!new_histories.has_value()) {
    task_i->SetRemoved(false);
    task_j->SetRemoved(false);
    }

    return new_histories;
}