#pragma once

#include <random>
#include <utility>

#include "pick_strategy.h"
#include "scheduler.h"

template <typename TargetObj>
struct RoundRobinStrategy : PickStrategy<TargetObj> {
  explicit RoundRobinStrategy(size_t threads_count,
                              std::vector<task_builder_t> constructors)
      : next_task{0},
        PickStrategy<TargetObj>{threads_count, std::move(constructors)} {}

  size_t Pick() override {
    return (next_task++) % PickStrategy<TargetObj>::threads_count;
  }

  size_t next_task;
};
