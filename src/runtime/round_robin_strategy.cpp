#include "include/round_robin_strategy.h"

// max_tasks has to be greater than threads_count
RoundRobinStrategy::RoundRobinStrategy(size_t threads_count,
                                       TaskBuilderList constructors)
    : next_task(0),
      threads_count(threads_count),
      constructors(constructors),
      threads(),
      is_new(threads_count, true) {
  std::random_device dev;
  rng = std::mt19937(dev());
  distribution = std::uniform_int_distribution<std::mt19937::result_type>(
      0, constructors->size() - 1);

  // Create tasks
  for (size_t i = 0; i < threads_count; ++i) {
    auto method = constructors->at(distribution(rng));
    threads.emplace_back();
    threads[i].emplace(method());
  }
}

// If there aren't any non returned tasks and the amount of finished tasks
// is equal to the max_tasks the finished task will be returned
std::pair<StackfulTask&, bool> RoundRobinStrategy::Next() {
  size_t current_task = next_task;
  // update the next pointer
  next_task = (++next_task) % threads_count;
  bool old_new = is_new[current_task];
  is_new[current_task] = false;

  StackfulTask& next = threads[current_task].back();
  if (next.IsReturned()) {
    // task has finished, so we replace it with the new one
    auto constructor = constructors->at(distribution(rng));
    threads[current_task].emplace(constructor());

    return {threads[current_task].back(), true};
  }

  return {next, old_new};
}

// Have to stop all current tasks and spawn new tasks
// StartNextRound invalidates all references from Next
void RoundRobinStrategy::StartNextRound() {
  for (auto& thread : threads) {
    auto constructor = constructors->at(distribution(rng));
    // We don't have to keep references alive
    thread = std::queue<StackfulTask>();
    thread.emplace(constructor());
  }

  for (size_t i = 0; i < is_new.size(); ++i) {
    is_new[i] = true;
  }
}
