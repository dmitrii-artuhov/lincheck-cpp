#pragma once
#include "lib.h"

// Scheduler class decides which task will be the next
struct SchedulerClass {
  // Returns the next tasks, and also the flag which tells is the task new
  virtual std::pair<StackfulTask&, bool> Next();
};
