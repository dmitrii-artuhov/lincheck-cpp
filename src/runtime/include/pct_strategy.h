#pragma once

#include <queue>
#include <random>
#include <utility>

#include "scheduler.h"

// https://www.microsoft.com/en-us/research/wp-content/uploads/2016/02/asplos277-pct.pdf
// TODO: logger
// TODO: documentation about K
template <typename TargetObj>
struct PctStrategy : Strategy {
  explicit PctStrategy(size_t threads_count,
                              TaskBuilderList constructors,
                       size_t start_k)
      : threads_count(threads_count),
        current_depth(1),
        current_schedule_length(0),
        constructors(constructors),
        threads(),
        is_new(threads_count, false) {
    std::random_device dev;
    rng = std::mt19937(dev());
    constructors_distribution =
        std::uniform_int_distribution<std::mt19937::result_type>(
            0, constructors->size() - 1);

    PrepareForDepth(current_depth, start_k);

    // Create queues.
    for (size_t i = 0; i < threads_count; ++i) {
      threads.emplace_back();
      auto constructor = constructors->at(constructors_distribution(rng));
      threads[i].emplace(Task{&state, constructor});
      is_new[i] = true;
    }
  }

  // If there aren't any non returned tasks and the amount of finished tasks
  // is equal to the max_tasks the finished task will be returned
  std::tuple<StackfulTask&, bool, int> Next() override {
    size_t max = std::numeric_limits<size_t>::min();
    size_t index_of_max = 0;
    // Have to ignore waiting threads, so can't do it faster than O(n)
    for (size_t i = 0; i < threads.size(); ++i) {
      // Ignore waiting threads
      if (threads[i].back().IsBusy()) {
        continue;
      }

      if (max <= priorities[i]) {
        max = priorities[i];
        index_of_max = i;
      }
    }

    // Check whether the priority change is required
    current_schedule_length++;
    for (size_t i = 0; i < priority_change_points.size(); ++i) {
      if (current_schedule_length == priority_change_points[i]) {
        priorities[index_of_max] = current_depth - i;
      }
    }

    if (threads[index_of_max].back().IsReturned()) {
      auto constructor = constructors->at(constructors_distribution(rng));
      threads[index_of_max].emplace(Task{&state, constructor});
      return {threads[index_of_max].back(), true, index_of_max};
    }

    bool old_is_new = is_new[index_of_max];
    is_new[index_of_max] = false;
    return {threads[index_of_max].back(), old_is_new, index_of_max};
  }

  void StartNextRound() override {
    std::cout << "depth: " << current_depth << std::endl;
    // Reconstruct target as we start from the beginning.
    state.Reconstruct();
    current_depth++;
    // TODO: Это точно стоит делать так?
    k_statistics.push_back(current_schedule_length);
    current_schedule_length = 0;

    // current_depth have been increased
    PrepareForDepth(current_depth, std::reduce(k_statistics.begin(), k_statistics.end()) / k_statistics.size() + 20);

    for (size_t i = 0; i < threads.size(); ++i) {
      threads[i] = std::queue<StackfulTask>();
      auto constructor = constructors->at(constructors_distribution(rng));
      threads[i].emplace(Task{&state, constructor});
      is_new[i] = true;
    }
  }

 private:
  void PrepareForDepth(size_t depth, size_t k) {
    // Generates priorities
    priorities = std::vector<size_t>(threads_count);
    for (size_t i = 0; i < priorities.size(); ++i) {
      priorities[i] = current_depth + i;
    }
    std::shuffle(priorities.begin(), priorities.end(), rng);

    // Generates priority_change_points
    auto k_distribution =
        std::uniform_int_distribution<std::mt19937::result_type>(
            1, k);
    priority_change_points = std::vector<size_t>(depth-1);
    for (size_t i = 0; i < depth-1; ++i) {
      priority_change_points[i] = k_distribution(rng);
    }
  }

  TargetObj state{};
  TaskBuilderList constructors;
  std::vector<size_t> k_statistics;
  size_t threads_count;
  size_t current_depth;
  size_t current_schedule_length;
  std::vector<size_t> priorities;
  std::vector<size_t> priority_change_points;
  std::vector<bool> is_new;
  // RoundRobinStrategy struct is the owner of all tasks, and all
  // references can't be invalidated before the end of the round,
  // so we have to contains all tasks in queues(queue doesn't invalidate the
  // references)
  std::vector<std::queue<StackfulTask>> threads;
  std::uniform_int_distribution<std::mt19937::result_type> constructors_distribution;
  std::mt19937 rng;
};
