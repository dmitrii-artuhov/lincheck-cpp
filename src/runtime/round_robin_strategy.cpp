#include "include/round_robin_strategy.h"

RoundRobinStrategy::RoundRobinStrategy(size_t threads_count,
                                       TaskBuilderList constructors)
    : next_task(0),
      threads_count(threads_count),
      constructors(constructors),
      threads() {
  std::random_device dev;
  rng = std::mt19937(dev());
  distribution = std::uniform_int_distribution<std::mt19937::result_type>(
      0, constructors->size() - 1);

  // Create queues
  for (size_t i = 0; i < threads_count; ++i) {
    threads.emplace_back();
  }
}

// If there aren't any non returned tasks and the amount of finished tasks
// is equal to the max_tasks the finished task will be returned
std::pair<StackfulTask&, bool> RoundRobinStrategy::Next() {
  size_t current_task = next_task;
  // update the next pointer
  next_task = (++next_task) % threads_count;

  // it's the first task if the queue is empty
  if (threads[current_task].empty() ||
      threads[current_task].back().IsReturned()) {
    // a task has finished or the queue is empty, so we add a new task
    auto constructor = constructors->at(distribution(rng));
    threads[current_task].emplace(constructor());

    return {threads[current_task].back(), true};
  }

  return {threads[current_task].back(), false};
}

// Have to stop all current tasks and spawn new tasks
// StartNextRound invalidates all references from Next
void RoundRobinStrategy::StartNextRound() {
  for (auto& thread : threads) {
    auto constructor = constructors->at(distribution(rng));
    // We don't have to keep references alive
    thread = std::queue<StackfulTask>();
  }
}
