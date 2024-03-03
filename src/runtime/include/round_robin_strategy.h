#pragma once

#include <queue>
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
  // RoundRobinStrategy struct is the owner of all tasks, and all
  // references can't be invalidated before the end of the round,
  // so we have to contains all tasks in queues(queue doesn't invalidate the
  // references)
  std::vector<std::queue<StackfulTask>> threads;
  std::vector<bool> is_new;
  std::uniform_int_distribution<std::mt19937::result_type> distribution;
  std::mt19937 rng;
};
