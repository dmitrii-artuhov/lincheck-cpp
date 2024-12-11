
#pragma once
#include <queue>
#include <random>

#include "scheduler.h"

template <typename TargetObj>
struct PickStrategy : Strategy {
  virtual size_t Pick() = 0;

  explicit PickStrategy(size_t threads_count,
                        std::vector<TaskBuilder> constructors)
      : next_task(0),
        threads_count(threads_count),
        constructors(std::move(constructors)),
        threads() {
    std::random_device dev;
    rng = std::mt19937(dev());
    distribution = std::uniform_int_distribution<std::mt19937::result_type>(
        0, this->constructors.size() - 1);

    // Create queues.
    for (size_t i = 0; i < threads_count; ++i) {
      threads.emplace_back();
    }
  }

  // If there aren't any non returned tasks and the amount of finished tasks
  // is equal to the max_tasks the finished task will be returned
  std::tuple<Task&, bool, int> Next() override {
    // TODO: maybe `current_thread`?
    auto current_task = Pick();

    // it's the first task if the queue is empty
    if (threads[current_task].empty() ||
        threads[current_task].back()->IsReturned()) {
      // a task has finished or the queue is empty, so we add a new task
      auto constructor = constructors.at(distribution(rng));
      threads[current_task].emplace_back(
          constructor.Build(&state, current_task, next_task_id++));
      return {threads[current_task].back(), true, current_task};
    }

    return {threads[current_task].back(), false, current_task};
  }

  // TODO: same iplementation for pct
  std::optional<std::tuple<Task&, int>> GetTask(int task_id) override {
    // TODO: can this be optimized?
    int thread_id = 0;
    for (auto& thread : threads) {
      size_t tasks = thread.size();

      for (size_t i = 0; i < tasks; ++i) {
        Task& task = thread[i];
        if (task->GetId() == task_id) {
          std::tuple<Task&, int> result = { task, thread_id };
          return result;
          // return std::make_tuple(task, thread_id);
        }
      }

      thread_id++;
    }
    return std::nullopt;
  }

  void StartNextRound() override {
    TerminateTasks();
    for (auto& thread : threads) {
      // We don't have to keep references alive
      while (thread.size() > 0) {
        thread.pop_back();
      }
    }

    // Reinitial target as we start from the beginning.
    state.Reset();
  }

  // TODO: this method is identical for pick_strategy and for pct_strategy
  void ResetCurrentRound() override {
    TerminateTasks();
    state.Reset();
    for (auto& thread : threads) {
      size_t tasks_in_thread = thread.size();
      for (size_t i = 0; i < tasks_in_thread; ++i) {
        thread[i] = thread[i]->Restart(&state);
      }
    }
  }

  ~PickStrategy() { TerminateTasks(); }

 protected:
  // Terminates all running tasks.
  // We do it in a dangerous way: in random order.
  // Actually, we assume obstruction free here.
  // TODO: for non obstruction-free we need to take into account dependencies.
  void TerminateTasks() {
    for (size_t i = 0; i < threads.size(); ++i) {
      if (!threads[i].empty()) {
        threads[i].back()->Terminate();
      }
    }
  }

  TargetObj state{};
  size_t next_task = 0;
  size_t threads_count;
  std::vector<TaskBuilder> constructors;
  // RoundRobinStrategy struct is the owner of all tasks, and all
  // references can't be invalidated before the end of the round,
  // so we have to contains all tasks in queues(queue doesn't invalidate the
  // references)
  std::vector<StableVector<Task>> threads;
  std::uniform_int_distribution<std::mt19937::result_type> distribution;
  std::mt19937 rng;
};
