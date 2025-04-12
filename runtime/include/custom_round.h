#pragma once

#include <vector>

#include "lib.h"

// TODO: make it just scenario and put in a startegy even when nothing custom is
// provided?
struct CustomRound {
  CustomRound(std::vector<std::vector<TaskBuilder>> threads_)
      : threads(threads_) {}

  std::vector<std::vector<TaskBuilder>> threads;
};