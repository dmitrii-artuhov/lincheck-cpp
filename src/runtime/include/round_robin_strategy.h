#pragma once

#include <random>
#include <utility>

#include "pick_strategy.h"
#include "scheduler.h"

template <typename TargetObj>
struct RoundRobinStrategy : PickStrategy<TargetObj> {
  explicit RoundRobinStrategy(size_t threads_count,
                              std::vector<TasksBuilder> constructors)
      : next_task{0},
        PickStrategy<TargetObj>{threads_count, std::move(constructors)} {}

  size_t Pick() override {
    auto &threads = PickStrategy<TargetObj>::threads;
    for (size_t attempt = 0; attempt < threads.size(); ++attempt) {
      auto cur = (next_task++) % threads.size();
      if ((!threads[cur].empty() &&
           std::holds_alternative<Task>(threads[cur].back()) &&
           std::get<Task>(threads[cur].back())->IsSuspended()) ||
          (!threads[cur].empty() &&
           std::holds_alternative<DualTask>(threads[cur].back()) &&
           std::get<DualTask>(threads[cur].back())->IsRequestFinished())) {
        continue;
      }
      return cur;
    }
    assert(false && "deadlock");
  }

  size_t next_task;
};
