#pragma once
#include <queue>
#include <random>
#include <utility>

#include "scheduler.h"

// Allows a random thread to work (uniformly).
// Randoms new task.
template <typename TargetObj>
struct UniformStrategy : Strategy {
  explicit UniformStrategy(size_t threads_count, TaskBuilderList constructors)
      : threads_count{threads_count}, constructors{constructors}, threads{} {
    assert(threads_count > 0);
    assert(constructors->size() > 0);
    std::random_device dev;
    rng = std::mt19937(dev());
    cons_distribution =
        std::uniform_int_distribution<std::mt19937::result_type>(
            0, constructors->size() - 1);
    thread_distribution =
        std::uniform_int_distribution<std::mt19937::result_type>(
            0, threads_count - 1);
    // Create queues.
    for (size_t i = 0; i < threads_count; ++i) {
      threads.emplace_back();
    }
  }

  std::tuple<StackfulTask&, bool, int> Next() override {
    size_t current_task = thread_distribution(rng);

    if (threads[current_task].empty() ||
        threads[current_task].back().IsReturned()) {
      auto constructor = constructors->at(cons_distribution(rng));
      threads[current_task].emplace(Task{&state, constructor});
      return {threads[current_task].back(), true, current_task};
    }

    return {threads[current_task].back(), false, current_task};
  }

  void StartNextRound() override {
    for (auto& thread : threads) {
      thread = std::queue<StackfulTask>();
    }
    state.Reconstruct();
  }

 private:
  TargetObj state{};
  size_t threads_count;
  TaskBuilderList constructors;
  std::vector<std::queue<StackfulTask>> threads;
  std::uniform_int_distribution<std::mt19937::result_type> cons_distribution;
  std::uniform_int_distribution<std::mt19937::result_type> thread_distribution;
  std::mt19937 rng;
};
