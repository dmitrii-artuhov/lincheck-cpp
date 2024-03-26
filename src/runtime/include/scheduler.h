#pragma once
#include <map>
#include <optional>
#include <variant>

#include "lib.h"
#include "logger.h"
#include "pretty_print.h"
#include "stable_vector.h"

// ModelChecker is the general checker interface which is implemented by
// different checkers, each of which checks its own consistency model
struct ModelChecker {
  virtual bool Check(
      const std::vector<std::variant<Invoke, Response>>& history) = 0;
};

// Strategy is the general strategy interface which decides which task
// will be the next one it can be implemented by different strategies, such as:
// randomized/tla/fair
struct Strategy {
  // Returns the next tasks,
  // the flag which tells is the task new, and the thread number.
  virtual std::tuple<StackfulTask&, bool, int> Next() = 0;

  // Strategy should stop all tasks that already have been started
  virtual void StartNextRound() = 0;

  virtual ~Strategy() = default;
};

// Scheduler generates different sequential histories(using Strategy) and
// then checks them with the ModelChecker
struct Scheduler {
  // max_switches represents the maximal count of switches. After this count
  // scheduler will end execution of the Run function
  Scheduler(Strategy& sched_class, ModelChecker& checker, size_t max_tasks,
            size_t max_rounds);

  // Run returns full unliniarizable history if such a history is found. Full
  // history is a history with all events, where each element in the vector is a
  // Resume operation on the corresponding task
  std::optional<std::vector<std::reference_wrapper<StackfulTask>>> Run();

 private:
  std::optional<std::vector<std::reference_wrapper<StackfulTask>>> runRound();

  Strategy& strategy;

  ModelChecker& checker;

  size_t max_tasks;

  size_t max_rounds;
};

// TLAScheduler generates all executions satisfying some conditions.
template <typename TargetObj>
struct TLAScheduler : IScheduler {
  TLAScheduler(int max_tasks, int max_rounds, int threads_count,
               TaskBuilderList constructors, ModelChecker& checker)
      : max_tasks{max_tasks},
        max_rounds{max_rounds},
        threads_count{threads_count},
        constructors{constructors},
        checker{checker} {
    for (int i = 0; i < threads_count; ++i) {
      threads.emplace_back();
    }
  };

  using thread_t = StableVector<StackfulTask>;

  // TLAScheduler enumerates all possible executions with finished max_tasks.
  // In fact, it enumerates tables (c = continue, f = finished):
  //         *---------*---------*--------*
  //         |   T1    |   T2    |   T3   |
  //         *---------*---------*--------*
  // state0  | task_i  |         |        |
  // state1  |    c    |         |        |
  // state2  |         | task_j  |        |
  // ...     |         |    c    |        |
  //         |    f    |         |        |
  //                      .....
  // frame_t struct describes one row of this table.
  struct frame_t {
    frame_t(const TargetObj& state) : state{state} {}
    // Current state.
    TargetObj state;
    // Pointer to the position in task thread.
    StackfulTask* position{};
    // Builder is non nullptr if the task was created at this step.
    TaskBuilder builder{};
  };

  // Replays all actions from step_begin to the step_end.
  void replay(int step_begin, int step_end) {
    // In histories we store references, so there's no need to update it.
    state = frames[step_begin].state;
    for (int step = step_begin; step < step_end; ++step) {
      auto& frame = frames[step];
      auto position = frame.position;
      assert(position);
      if (frame.builder != nullptr) {
        // It was a new task.
        // So recreate it and replace.
        *position = StackfulTask{Task{&state, frame.builder}};
      } else {
        // It was a not new task, hence, we recreated in early.
      }
      position->Resume();
    }
  }

  // Resumes choosed task.
  // If task is finished and finished tasks == max_tasks, stops.
  std::tuple<bool, result_t> resume_task(frame_t& frame, int step,
                                         size_t thread_id, thread_t& thread,
                                         std::optional<Task> raw_task) {
    bool is_new = raw_task.has_value();
    if (is_new) {
      thread.emplace_back(StackfulTask{raw_task.value()});
    }
    auto& task = thread.back();
    frame.position = &task;

    full_history.push_back({thread_id, task});
    if (is_new) {
      sequential_history.emplace_back(Invoke(task, thread_id));
    }

    task.Resume();
    bool is_finished = task.IsReturned();
    if (is_finished) {
      finished_tasks++;
      auto result = task.GetRetVal();
      sequential_history.emplace_back(Response(task, result, thread_id));
    }

    bool stop = finished_tasks == max_tasks;
    if (!stop) {
      // Run recursive step.
      auto [is_over, res] = run(step + 1);
      if (is_over || res.has_value()) {
        return {is_over, res};
      }
    } else {
      log() << "run round: " << finished_rounds << "\n";
      pretty_print::pretty_print(sequential_history, log(), threads_count);
      log() << "===============================================\n\n";
      // Stop, check if the the generated history is linearizable.
      ++finished_rounds;
      if (!checker.Check(sequential_history)) {
        return {false, std::make_pair(full_history, sequential_history)};
      }
      if (finished_rounds == max_rounds) {
        // It was the last round.
        return {true, {}};
      }
    }

    full_history.pop_back();
    if (is_finished) {
      --finished_tasks;
      // resp.
      sequential_history.pop_back();
    }
    if (is_new) {
      // inv.
      sequential_history.pop_back();
      thread.pop_back();
    }

    // As we can't return to the past in coroutine, we need to replay all tasks
    // from the beginning.
    replay(0, step);
    return {false, {}};
  }

  std::tuple<bool, result_t> run(int step) {
    // Push frame to the stack.
    frames.emplace_back(frame_t{state});
    auto& frame = frames.back();

    // Pick next task.
    for (size_t thread_id = 0; thread_id < threads.size(); ++thread_id) {
      auto& thread = threads[thread_id];
      if (!thread.empty() && !thread.back().IsReturned()) {
        // Task exists.
        frame.builder = nullptr;
        auto [is_over, res] = resume_task(frame, step, thread_id, thread, {});
        if (is_over || res.has_value()) {
          return {is_over, res};
        }
        continue;
      }

      // Choose constructor to create task.
      for (auto cons : *constructors) {
        frame.builder = cons;
        auto task = Task{&state, cons};
        auto [is_over, res] = resume_task(frame, step, thread_id, thread, task);
        if (is_over || res.has_value()) {
          return {is_over, res};
        }
      }
    }

    frames.pop_back();
    return {false, {}};
  }

  result_t Run() override {
    auto [_, res] = run(0);
    return res;
  }

  int threads_count;
  int max_tasks;
  int max_rounds;
  TaskBuilderList constructors;
  ModelChecker& checker;

  // Running state.
  int finished_tasks{};
  int finished_rounds{};
  TargetObj state{};
  std::vector<std::variant<Invoke, Response>> sequential_history;
  std::vector<std::pair<int, std::reference_wrapper<StackfulTask>>>
      full_history;
  StableVector<thread_t> threads;
  StableVector<frame_t> frames;
};
