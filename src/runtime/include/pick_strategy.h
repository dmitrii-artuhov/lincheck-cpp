
#pragma once
#include <queue>

#include "scheduler.h"

template <typename TargetObj>
struct PickStrategy : Strategy {
  virtual size_t Pick() = 0;

  explicit PickStrategy(size_t threads_count, TaskBuilderList constructors)
      : next_task(0),
        threads_count(threads_count),
        constructors(constructors),
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
    auto current_task = Pick();

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

    // Reinitial target as we start from the beginning.
    state = initial_state;
  }

 protected:
  TargetObj initial_state{};
  TargetObj state{};
  size_t next_task = 0;
  size_t threads_count;
  TaskBuilderList constructors;
  // RoundRobinStrategy struct is the owner of all tasks, and all
  // references can't be invalidated before the end of the round,
  // so we have to contains all tasks in queues(queue doesn't invalidate the
  // references)
  std::vector<std::queue<StackfulTask>> threads;
  std::uniform_int_distribution<std::mt19937::result_type> distribution;
  std::mt19937 rng;
};
