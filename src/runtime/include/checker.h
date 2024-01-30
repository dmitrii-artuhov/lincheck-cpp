#pragma once
#include "scheduler.h"
#include "lib.h"
#include <variant>

// Scheduler class decides which task will be the next
struct ModelChecker {
  virtual bool Check(const std::vector<std::variant<StackfulTaskInvoke, StackfulTaskResponse>>& history);
};
