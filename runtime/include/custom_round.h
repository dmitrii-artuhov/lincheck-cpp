#pragma once

#include <vector>
#include <iostream>

#include "lib.h"

namespace ltest {

// TODO: make it just scenario and put in a startegy even when nothing custom is provided?
struct CustomRound {
  CustomRound(std::vector<std::vector<TaskBuilder>> threads_): threads(threads_) {}

  std::vector<std::vector<TaskBuilder>> threads;
};

inline std::ostream& operator<<(std::ostream& os, const CustomRound& cs) {
  for (auto& v : cs.threads) {
    for (auto& x : v) {
      os << x.GetName() << ", ";
    }
    os << "\n";
  }
  return os;
}

} // namespace ltest