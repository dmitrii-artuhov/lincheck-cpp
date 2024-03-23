#pragma once

#include <queue>
#include <random>
#include <utility>

#include "scheduler.h"

template <typename TargetObj>
struct RoundRobinStrategy : Strategy {
  explicit RoundRobinStrategy(size_t threads_count,
                              TaskBuilderList constructors,
                              InitFuncList init_funcs)
      : next_task(0),
        threads_count(threads_count),
        constructors(constructors),
        init_funcs(init_funcs),
        threads() {
    std::random_device dev;
    rng = std::mt19937(dev());
    distribution = std::uniform_int_distribution<std::mt19937::result_type>(
        0, constructors->size() - 1);

    // Create queues.
    for (size_t i = 0; i < threads_count; ++i) {
      threads.emplace_back();
    }
  }

  // If there aren't any non returned tasks and the amount of finished tasks
  // is equal to the max_tasks the finished task will be returned
  std::tuple<StackfulTask&, bool, int> Next() override {
    size_t current_task = next_task;
    // update the next pointer
    next_task = (++next_task) % threads_count;

    // it's the first task if the queue is empty
    if (threads[current_task].empty() ||
        threads[current_task].back().IsReturned()) {
      // a task has finished or the queue is empty, so we add a new task
      auto constructor = constructors->at(distribution(rng));
      threads[current_task].emplace(Task{&state, constructor});
      return {threads[current_task].back(), true, current_task};
    }

    return {threads[current_task].back(), false, current_task};
  }

  void StartNextRound() override {
    for (auto& thread : threads) {
      auto constructor = constructors->at(distribution(rng));
      // We don't have to keep references alive
      thread = std::queue<StackfulTask>();
    }

    // Run init funcs.
    for (const auto& fun : *init_funcs) {
      fun();
    }

    // Reconstruct target as we start from the beginning.
    state.Reconstruct();
  }

 private:
  TargetObj state{};
  size_t next_task = 0;
  size_t threads_count;
  TaskBuilderList constructors;
  InitFuncList init_funcs;
  // RoundRobinStrategy struct is the owner of all tasks, and all
  // references can't be invalidated before the end of the round,
  // so we have to contains all tasks in queues(queue doesn't invalidate the
  // references)
  std::vector<std::queue<StackfulTask>> threads;
  std::uniform_int_distribution<std::mt19937::result_type> distribution;
  std::mt19937 rng;
};
