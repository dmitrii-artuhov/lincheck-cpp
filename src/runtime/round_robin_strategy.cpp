#include "include/round_robin_strategy.h"

RoundRobinStrategy::RoundRobinStrategy(size_t threads_count,
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

  // Create queues
  for (size_t i = 0; i < threads_count; ++i) {
    threads.emplace_back();
  }
}

// If there aren't any non returned tasks and the amount of finished tasks
// is equal to the max_tasks the finished task will be returned
std::tuple<StackfulTask&, bool, int> RoundRobinStrategy::Next() {
  size_t current_task = next_task;
  // update the next pointer
  next_task = (++next_task) % threads_count;

  // it's the first task if the queue is empty
  if (threads[current_task].empty() ||
      threads[current_task].back().IsReturned()) {
    // a task has finished or the queue is empty, so we add a new task
    std::vector<int> args;
    auto constructor = constructors->at(distribution(rng));
    auto task = constructor(&args);
    auto stackfulTask = StackfulTask{task};
    stackfulTask.SetArgs(std::move(args));

    threads[current_task].emplace(stackfulTask);
    return {threads[current_task].back(), true, current_task};
  }

  return {threads[current_task].back(), false, current_task};
}

// Have to stop all current tasks and spawn new tasks
// StartNextRound invalidates all references from Next
void RoundRobinStrategy::StartNextRound() {
  for (auto& thread : threads) {
    auto constructor = constructors->at(distribution(rng));
    // We don't have to keep references alive
    thread = std::queue<StackfulTask>();
  }

  // Run init funcs.
  for (const auto& fun : *init_funcs) {
    fun();
  }
}
