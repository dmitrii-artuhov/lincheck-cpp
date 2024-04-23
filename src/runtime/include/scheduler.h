#pragma once
#include <limits>
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

struct Scheduler {
  using full_history_t = std::vector<std::reference_wrapper<StackfulTask>>;
  using seq_history_t = std::vector<std::variant<Invoke, Response>>;
  using result_t = std::optional<std::pair<full_history_t, seq_history_t>>;

  virtual result_t Run() = 0;

  virtual ~Scheduler() = default;
};

// StrategyScheduler generates different sequential histories(using Strategy)
// and then checks them with the ModelChecker
struct StrategyScheduler : Scheduler {
  // max_switches represents the maximal count of switches. After this count
  // scheduler will end execution of the Run function
  StrategyScheduler(Strategy& sched_class, ModelChecker& checker,
                    PrettyPrinter& pretty_printer, size_t max_tasks,
                    size_t max_rounds);

  // Run returns full unliniarizable history if such a history is found. Full
  // history is a history with all events, where each element in the vector is a
  // Resume operation on the corresponding task
  result_t Run() override;

 private:
  result_t runRound();

  Strategy& strategy;

  ModelChecker& checker;

  PrettyPrinter& pretty_printer;

  size_t max_tasks;

  size_t max_rounds;
};

// TLAScheduler generates all executions satisfying some conditions.
template <typename TargetObj>
struct TLAScheduler : Scheduler {
  TLAScheduler(size_t max_tasks, size_t max_rounds, size_t threads_count,
               size_t max_switches, std::vector<task_builder_t> constructors,
               ModelChecker& checker, PrettyPrinter& pretty_printer)
      : max_tasks{max_tasks},
        max_rounds{max_rounds},
        max_switches{max_switches},
        constructors{std::move(constructors)},
        checker{checker},
        pretty_printer{pretty_printer} {
    for (int i = 0; i < threads_count; ++i) {
      threads.emplace_back();
    }
  };

  result_t Run() override {
    auto [_, res] = run(0, 0);
    return res;
  }

 private:
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
    task_builder_t builder{};
  };

  // Replays all actions from step_begin to the step_end.
  void replay(size_t step_begin, size_t step_end) {
    // std::cout << "replay" << std::endl;
    // In histories we store references, so there's no need to update it.
    state = frames[step_begin].state;
    for (size_t step = step_begin; step < step_end; ++step) {
      auto& frame = frames[step];
      auto position = frame.position;
      assert(position);
      if (frame.builder != nullptr) {
        // It was a new task.
        // So restart it from the beginning with the same args.
        auto entrypoint = position->GetEntrypoint();
        entrypoint.StartFromTheBeginning(&state);
        *position = StackfulTask{entrypoint};
      } else {
        // It was a not new task, hence, we recreated in early.
      }
      position->Resume();
    }
  }

  // Resumes choosed task.
  // If task is finished and finished tasks == max_tasks, stops.
  std::tuple<bool, result_t> resume_task(frame_t& frame, size_t step,
                                         size_t thread_id, size_t switches,
                                         thread_t& thread,
                                         std::optional<Task> raw_task) {
    size_t previous_thread_id = thread_id_history.empty()
                                    ? std::numeric_limits<size_t>::max()
                                    : thread_id_history.back();
    bool is_new = raw_task.has_value();
    size_t nxt_switches = switches;
    if (is_new) {
      thread.emplace_back(StackfulTask{raw_task.value()});
    } else {
      if (thread_id != previous_thread_id) {
        ++nxt_switches;
      }
      if (nxt_switches > max_switches) {
        // The limit of switches is achieved.
        // So, do not resume task.
        return {false, {}};
      }
    }
    auto& task = thread.back();
    frame.position = &task;

    full_history.push_back({thread_id, task});
    thread_id_history.push_back(thread_id);
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
      auto [is_over, res] = run(step + 1, nxt_switches);
      if (is_over || res.has_value()) {
        return {is_over, res};
      }
    } else {
      log() << "run round: " << finished_rounds << "\n";
      pretty_printer.pretty_print(sequential_history, log());
      log() << "===============================================\n\n";
      // Stop, check if the the generated history is linearizable.
      ++finished_rounds;
      if (!checker.Check(sequential_history)) {
        return {false, std::make_pair(full_history_t{}, sequential_history)};
      }
      if (finished_rounds == max_rounds) {
        // It was the last round.
        return {true, {}};
      }
    }

    thread_id_history.pop_back();
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

  std::tuple<bool, result_t> run(size_t step, size_t switches) {
    // Push frame to the stack.
    frames.emplace_back(frame_t{state});
    auto& frame = frames.back();

    // Pick next task.
    for (size_t thread_id = 0; thread_id < threads.size(); ++thread_id) {
      auto& thread = threads[thread_id];
      if (!thread.empty() && !thread.back().IsReturned()) {
        // Task exists.
        frame.builder = nullptr;
        auto [is_over, res] =
            resume_task(frame, step, thread_id, switches, thread, {});
        if (is_over || res.has_value()) {
          return {is_over, res};
        }
        continue;
      }

      // Choose constructor to create task.
      for (auto cons : constructors) {
        frame.builder = cons;
        auto task = cons(&state);
        auto [is_over, res] =
            resume_task(frame, step, thread_id, switches, thread, task);
        if (is_over || res.has_value()) {
          return {is_over, res};
        }
      }
    }

    frames.pop_back();
    return {false, {}};
  }

  PrettyPrinter& pretty_printer;
  size_t max_tasks;
  size_t max_rounds;
  size_t max_switches;
  std::vector<task_builder_t> constructors;
  ModelChecker& checker;

  // Running state.
  size_t finished_tasks{};
  size_t finished_rounds{};
  TargetObj state{};
  std::vector<std::variant<Invoke, Response>> sequential_history;
  std::vector<std::pair<int, std::reference_wrapper<StackfulTask>>>
      full_history;
  std::vector<size_t> thread_id_history;
  StableVector<thread_t> threads;
  StableVector<frame_t> frames;
};
