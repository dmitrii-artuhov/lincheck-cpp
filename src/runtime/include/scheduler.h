#pragma once
#include <optional>
#include <variant>

#include "lib.h"

// ModelChecker is the general checker interface which is implemented by
// different checkers, each of which checks its own consistency model
struct ModelChecker {
  virtual bool Check(
      const std::vector<std::variant<Invoke, Response>>&
          history) = 0;
};

// SchedulerClass is the general strategy interface which decides which task
// will be the next one it can be implemented by different strategies, such as:
// randomized/tla/fair
struct SchedulerClass {
  // Returns the next tasks, and also the flag which tells is the task new
  virtual std::pair<StackfulTask&, bool> Next() = 0;
};

// Scheduler generates different sequential histories(using SchedulerClass) and
// then checks them with the ModelChecker
struct Scheduler {
  // max_switches represents the maximal count of switches. After this count
  // scheduler will end execution of the Run function
  Scheduler(SchedulerClass& sched_class, ModelChecker& checker,
            size_t max_tasks);

  // Run returns full unliniarizable history if such a history is found. Full
  // history is a history with all events, where each element in the vector is a
  // Resume operation on the corresponding task
  // TODO: question: should I use pointers instead of std::reference_wrapper?
  std::optional<std::vector<std::reference_wrapper<StackfulTask>>> Run();

 private:
  // Full history of the current execution in the Run function
  std::vector<std::reference_wrapper<StackfulTask>> full_history;
  // History of invoke and response events which is required for the checker
  std::vector<std::variant<Invoke, Response>>
      sequential_history;

  SchedulerClass& sched_class;

  ModelChecker& checker;

  size_t max_tasks;
};
