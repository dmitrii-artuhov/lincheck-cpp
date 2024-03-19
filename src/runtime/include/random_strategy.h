#pragma once
#include <queue>
#include <random>
#include <utility>

#include "scheduler.h"

// Allows a random thread to work.
// Randoms new task.
struct RandomStrategy : Strategy {
  explicit RandomStrategy(size_t threads_count, TaskBuilderList constructors,
                          InitFuncList init_funcs);

  std::tuple<StackfulTask&, bool, int> Next() override;

  void StartNextRound();

 private:
  size_t threads_count;
  TaskBuilderList constructors;
  InitFuncList init_funcs;
  std::vector<std::queue<StackfulTask>> threads;
  std::uniform_int_distribution<std::mt19937::result_type> cons_distribution;
  std::uniform_int_distribution<std::mt19937::result_type> thread_distribution;
  std::mt19937 rng;
};
