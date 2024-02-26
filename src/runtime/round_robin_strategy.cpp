#include "include/round_robin_strategy.h"

RoundRobinStrategy::RoundRobinStrategy(size_t threads_count, TaskBuilderList constructors) : next_task(0),
                                                                                             threads_count(threads_count),
                                                                                             constructors(constructors),
                                                                                             threads() {
  std::random_device dev;
  rng = std::mt19937(dev());
  distribution = std::uniform_int_distribution<std::mt19937::result_type>(0,threads_count-1);

  // Create tasks
  for (size_t i = 0; i < threads_count; ++i) {
    auto method = constructors->at(distribution(rng));
    threads.emplace_back(method());
  }
}

std::pair<StackfulTask&, bool> RoundRobinStrategy::Next() {
  size_t current_task = next_task;
  // update the next pointer
  next_task = (++next_task) % threads_count;

  StackfulTask& next = threads[current_task];
  if (next.IsReturned()) {
    // If next task have returned we have to replace it with new one
    auto constructor = constructors->at(distribution(rng));
    threads[current_task] = StackfulTask(constructor());

    return {threads[current_task], true};
  }

  return {next, false};
}

// Have to stop all current tasks and spawn new tasks
void RoundRobinStrategy::StartNextRound() {
  for (auto & thread : threads) {
    auto constructor = constructors->at(distribution(rng));
    thread = StackfulTask(constructor());
  }
}
