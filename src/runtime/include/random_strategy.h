#pragma once
#include <random>
#include <utility>
#include <vector>

#include "pick_strategy.h"
#include "scheduler.h"

// Allows a random thread to work.
// Randoms new task.
template <typename TargetObj>
struct RandomStrategy : PickStrategy<TargetObj> {
  explicit RandomStrategy(size_t threads_count, TaskBuilderList constructors,
                          std::vector<int> weights)
      : PickStrategy<TargetObj>{threads_count, constructors} {
    auto distribution =
        std::uniform_int_distribution<std::mt19937::result_type>{
            0, threads_count - 1};
    thread_distribution =
        std::discrete_distribution<>(weights.begin(), weights.end());
  }

  size_t Pick() override {
    return thread_distribution(PickStrategy<TargetObj>::rng);
  }

 private:
  std::discrete_distribution<> thread_distribution;
};
