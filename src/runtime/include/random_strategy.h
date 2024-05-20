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
  explicit RandomStrategy(size_t threads_count,
                          std::vector<TasksBuilder> constructors,
                          std::vector<int> weights)
      : PickStrategy<TargetObj>{threads_count, std::move(constructors)},
        weights{std::move(weights)} {}

  size_t Pick() override {
    pick_weights.clear();
    auto &threads = PickStrategy<TargetObj>::threads;
    for (size_t i = 0; i < threads.size(); ++i) {
      if ((!threads[i].empty() &&
           std::holds_alternative<Task>(threads[i].back()) &&
           std::get<Task>(threads[i].back())->IsSuspended()) ||
          (!threads[i].empty() &&
           std::holds_alternative<DualTask>(threads[i].back()) &&
           std::get<DualTask>(threads[i].back())->IsRequestFinished())) {
        continue;
      }
      pick_weights.push_back(weights[i]);
    }
    assert(!pick_weights.empty() && "deadlock");
    auto thread_distribution =
        std::discrete_distribution<>(pick_weights.begin(), pick_weights.end());
    auto num = thread_distribution(PickStrategy<TargetObj>::rng);
    for (size_t i = 0; i < threads.size(); ++i) {
      if ((!threads[i].empty() &&
           std::holds_alternative<Task>(threads[i].back()) &&
           std::get<Task>(threads[i].back())->IsSuspended()) ||
          (!threads[i].empty() &&
           std::holds_alternative<DualTask>(threads[i].back()) &&
           std::get<DualTask>(threads[i].back())->IsRequestFinished())) {
        continue;
      }
      if (num == 0) {
        return i;
      }
      num--;
    }
    assert(false && "oops");
  }

 private:
  std::vector<int> weights;
  std::vector<int> pick_weights;
};
