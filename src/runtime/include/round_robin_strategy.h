#pragma once

#include <random>
#include <utility>

#include "pick_strategy.h"
#include "scheduler.h"

template <typename TargetObj>
struct RoundRobinStrategy : PickStrategy<TargetObj> {
  explicit RoundRobinStrategy(size_t threads_count,
                              TaskBuilderList constructors)
      : next_task{0}, PickStrategy<TargetObj>{threads_count, constructors} {}

  size_t Pick() override {
    return (next_task++) % PickStrategy<TargetObj>::threads_count;
  }

  size_t next_task;
};
