#pragma once

#include <random>
#include <utility>

#include "scheduler.h"

struct RoundRobinStrategy : Strategy {
  explicit RoundRobinStrategy(size_t threads_count,
                              TaskBuilderList constructors);

  std::pair<StackfulTask&, bool> Next() override;

  void StartNextRound() override;

 private:
  size_t next_task = 0;
  size_t threads_count;
  TaskBuilderList constructors;
  std::vector<StackfulTask> threads;
  std::uniform_int_distribution<std::mt19937::result_type> distribution;
  std::mt19937 rng;
};
