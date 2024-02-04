#pragma once
#include "lib.h"
#include "scheduler_class.h"
#include <variant>
#include <optional>
#include <functional>
#include <map>


// Scheduler class decides which task will be the next
struct ModelChecker {
  virtual bool Check(const std::vector<std::variant<StackfulTaskInvoke, StackfulTaskResponse>>& history) = 0;
};

using MethodName = std::string;

// TODO: formatter
// TODO: как задекларировать, что нужен copy constructor? Не нашел std::copyable или чего-то похожего
// если будет понятно - то можно будет удалить аргумент ```std::function<Object(Object)> copy```
template<
  class LinearSpecificationObject,
  class SpecificationObjectHash = std::hash<LinearSpecificationObject>,
  class SpecificationObjectEqual = std::equal_to<LinearSpecificationObject>
> struct LinearizabilityChecker : ModelChecker {
  LinearizabilityChecker() = delete;

  LinearizabilityChecker(std::map<MethodName, std::function<int(LinearSpecificationObject)>> specification_methods,
                         std::function<LinearSpecificationObject(LinearSpecificationObject)> copy, LinearSpecificationObject first_state);

  bool Check(const std::vector<std::variant<StackfulTaskInvoke, StackfulTaskResponse>>& history) override;

private:
  std::map<MethodName, std::function<int(LinearSpecificationObject)>> specification_methods;
  std::function<LinearSpecificationObject(LinearSpecificationObject)> copy;
  LinearSpecificationObject first_state;
};


struct Scheduler {
  Scheduler(SchedulerClass sched_class, ModelChecker& checker, size_t max_tasks);

  std::optional<std::vector<ActionHandle>> Run();

private:
  // Contains stacks snapshots
  std::vector<ActionHandle> full_history;
  // history for a linearizability checker
  std::vector<std::variant<StackfulTaskInvoke, StackfulTaskResponse>> sequential_history;

  SchedulerClass sched_class;

  ModelChecker& checker;

  // The number of maximum terminated tasks(methods)
  size_t max_tasks;
};
