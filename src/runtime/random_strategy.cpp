#include "include/random_strategy.h"

#include <cassert>

RandomStrategy::RandomStrategy(size_t threads_count,
                               TaskBuilderList constructors,
                               InitFuncList init_funcs)
    : threads_count{threads_count},
      constructors{constructors},
      init_funcs{init_funcs},
      threads{} {
  assert(threads_count > 0);
  assert(constructors->size() > 0);
  std::random_device dev;
  rng = std::mt19937(dev());
  cons_distribution = std::uniform_int_distribution<std::mt19937::result_type>(
      0, constructors->size() - 1);
  thread_distribution =
      std::uniform_int_distribution<std::mt19937::result_type>(
          0, threads_count - 1);
}

std::tuple<StackfulTask&, bool, int> RandomStrategy::Next() {
  size_t current_task = thread_distribution(rng);

  if (threads[current_task].empty() ||
      threads[current_task].back().IsReturned()) {
    std::vector<int> args;
    auto constructor = constructors->at(cons_distribution(rng));
    auto task = constructor(&args);
    auto stackfulTask = StackfulTask{task};
    stackfulTask.SetArgs(std::move(args));

    threads[current_task].emplace(stackfulTask);
    return {threads[current_task].back(), true, current_task};
  }

  return {threads[current_task].back(), false, current_task};
}

void RandomStrategy::StartNextRound() {
  for (auto& thread : threads) {
    thread = std::queue<StackfulTask>();
  }

  // Run init funcs.
  // TODO: this code can be extracted to parent.
  for (const auto& fun : *init_funcs) {
    fun();
  }
}
