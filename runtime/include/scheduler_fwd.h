#pragma once
#include <vector>
#include <unordered_set>
#include <optional>
#include <variant>

#include "lincheck.h"

struct RoundMinimizor;

struct Scheduler {
    using FullHistory = std::vector<std::reference_wrapper<Task>>;
    using SeqHistory = std::vector<std::variant<Invoke, Response>>;
    using Histories = std::pair<FullHistory, SeqHistory>;
    using Result = std::optional<Histories>;

    virtual Result Run() = 0;

    virtual ~Scheduler() = default;
};

struct SchedulerWithReplay : Scheduler {
protected:
    friend class InterleavingMinimizor;
    friend class StrategyMinimizor;

    virtual Result runRound() = 0;

    virtual Result exploreRound(int max_runs) = 0;

    virtual Result replayRound(const std::vector<int>& tasks_ordering) = 0;

    virtual std::vector<int> getTasksOrdering(
        const FullHistory& full_history,
        std::unordered_set<int> exclude_task_ids
    ) const = 0;

    virtual void minimize(
        Histories& nonlinear_history,
        const RoundMinimizor& minimizor
    ) = 0;
};