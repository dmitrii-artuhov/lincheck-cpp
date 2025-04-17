#pragma once
#include <optional>
#include <unordered_set>
#include <variant>
#include <vector>

#include "lincheck.h"

struct Strategy;
struct RoundMinimizor;

struct Scheduler {
  using FullHistory = std::vector<std::reference_wrapper<Task>>;
  using SeqHistory = std::vector<std::variant<Invoke, Response>>;
  using BothHistories = std::pair<FullHistory, SeqHistory>;
  using Result = std::optional<BothHistories>;

  virtual Result Run() = 0;

  virtual int GetStartegyThreadsCount() const = 0;

  virtual ~Scheduler() = default;
};

struct SchedulerWithReplay : Scheduler {
 protected:
  friend class GreedyRoundMinimizor;
  friend class SameInterleavingMinimizor;
  friend class StrategyExplorationMinimizor;
  friend class SmartMinimizor;

  virtual Result GenerateAndRunRound() = 0;

  virtual Result ExploreRound(int runs, bool log_each_interleaving = false) = 0;

  virtual Result ReplayRound(const std::vector<int>& tasks_ordering) = 0;

  virtual Strategy& GetStrategy() const = 0;

  virtual void Minimize(BothHistories& nonlinear_history,
                        const RoundMinimizor& minimizor) = 0;
};