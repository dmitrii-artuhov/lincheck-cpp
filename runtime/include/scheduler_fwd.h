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
    friend class GreedyRoundMinimizor;
    friend class SameInterleavingMinimizor;
    friend class StrategyExplorationMinimizor;

    virtual Result runRound() = 0;

    virtual Result exploreRound(int runs) = 0;

    virtual Result replayRound(const std::vector<int>& tasks_ordering) = 0;

    inline static std::vector<int> getTasksOrdering(
        const FullHistory& full_history,
        std::unordered_set<int> exclude_task_ids
    ) {
        std::vector <int> tasks_ordering;
  
        for (auto& task : full_history) {
            if (exclude_task_ids.contains(task.get()->GetId())) continue;
            tasks_ordering.emplace_back(task.get()->GetId());
        }

        return tasks_ordering;
    }

    virtual void minimize(
        Histories& nonlinear_history,
        const RoundMinimizor& minimizor
    ) = 0;
};