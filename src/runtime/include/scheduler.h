#pragma once
#include <map>
#include <optional>
#include <variant>

#include "lib.h"

// ModelChecker is the general checker interface which is implemented by
// different checkers, each of which checks its own consistency model
struct ModelChecker {
  virtual bool Check(
      const std::vector<std::variant<Invoke, Response>>& history) = 0;
};

// Strategy is the general strategy interface which decides which task
// will be the next one it can be implemented by different strategies, such as:
// randomized/tla/fair
struct Strategy {
  // Returns the next tasks, and also the flag which tells is the task new
  virtual std::pair<StackfulTask&, bool> Next() = 0;

  // Strategy should stop all tasks that already have been started
  virtual void StartNextRound() = 0;
};

// Scheduler generates different sequential histories(using Strategy) and
// then checks them with the ModelChecker
struct Scheduler {
  // max_switches represents the maximal count of switches. After this count
  // scheduler will end execution of the Run function
  Scheduler(Strategy& sched_class, ModelChecker& checker, size_t max_tasks,
            size_t max_rounds);

  // Run returns full unliniarizable history if such a history is found. Full
  // history is a history with all events, where each element in the vector is a
  // Resume operation on the corresponding task
  // TODO: question: should I use pointers instead of std::reference_wrapper?
  std::optional<std::vector<std::reference_wrapper<StackfulTask>>> Run();

 private:
  std::optional<std::vector<std::reference_wrapper<StackfulTask>>> runRound();

  // Full history of the current execution in the Run function
  std::vector<std::reference_wrapper<StackfulTask>> full_history;

  Strategy& strategy;

  ModelChecker& checker;

  size_t max_tasks;

  size_t max_rounds;
};
