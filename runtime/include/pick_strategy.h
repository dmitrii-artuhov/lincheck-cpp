
#pragma once
#include <algorithm>
#include <random>

#include "scheduler.h"

template <typename TargetObj, StrategyVerifier Verifier>
struct PickStrategy : public BaseStrategyWithThreads<TargetObj, Verifier> {
  virtual size_t Pick() = 0;

  virtual size_t PickSchedule() = 0;

  explicit PickStrategy(size_t threads_count,
                        std::vector<TaskBuilder> constructors)
      : BaseStrategyWithThreads<TargetObj, Verifier>(threads_count,
                                                     std::move(constructors)),
        next_task(0) {}

  // If there aren't any non returned tasks and the amount of finished tasks
  // is equal to the max_tasks the finished task will be returned
  TaskWithMetaData Next() override {
    auto& threads = this->threads;
    auto current_thread = Pick();
    debug(stderr, "Picked thread: %zu\n", current_thread);

    // it's the first task if the queue is empty
    if (threads[current_thread].empty() ||
        threads[current_thread].back()->IsReturned()) {
      // a task has finished or the queue is empty, so we add a new task
      std::shuffle(this->constructors.begin(), this->constructors.end(),
                   this->rng);
      size_t verified_constructor = -1;
      for (size_t i = 0; i < this->constructors.size(); ++i) {
        TaskBuilder constructor = this->constructors.at(i);
        CreatedTaskMetaData next_task = {constructor.GetName(), true,
                                         current_thread};
        if (this->sched_checker.Verify(next_task)) {
          verified_constructor = i;
          break;
        }
      }
      if (verified_constructor == -1) {
        assert(false && "Oops, possible deadlock or incorrect verifier\n");
      }
      threads[current_thread].emplace_back(
          this->constructors[verified_constructor].Build(
              this->state.get(), current_thread, this->new_task_id++));
      TaskWithMetaData task{threads[current_thread].back(), true,
                            current_thread};
      return task;
    }

    return {threads[current_thread].back(), false, current_thread};
  }

  TaskWithMetaData NextSchedule() override {
    auto& round_schedule = this->round_schedule;
    size_t current_thread = PickSchedule();
    int next_task_index = this->GetNextTaskInThread(current_thread);
    bool is_new = round_schedule[current_thread] != next_task_index;

    round_schedule[current_thread] = next_task_index;
    return TaskWithMetaData{this->threads[current_thread][next_task_index],
                            is_new, current_thread};
  }

  // TODO: same implementation for pct
  std::optional<std::tuple<Task&, int>> GetTask(int task_id) override {
    // TODO: can this be optimized?
    int thread_id = 0;
    for (auto& thread : this->threads) {
      size_t tasks = thread.size();

      for (size_t i = 0; i < tasks; ++i) {
        Task& task = thread[i];
        if (task->GetId() == task_id) {
          std::tuple<Task&, int> result = {task, thread_id};
          return result;
          // return std::make_tuple(task, thread_id);
        }
      }

      thread_id++;
    }
    return std::nullopt;
  }

  ~PickStrategy() { this->TerminateTasks(); }

 protected:
  size_t next_task = 0;
};
