#include <numeric>
#include <fstream>
#include <iostream>

#include "include/scheduler.h"
#include "include/logger.h"
#include "include/pretty_print.h"
#include "include/minimization.h"
#include "include/minimization_smart.h"
#include "include/timer.h"

StrategyScheduler::StrategyScheduler(Strategy &sched_class,
                                     ModelChecker &checker,
                                     PrettyPrinter &pretty_printer,
                                     size_t max_tasks,
                                     size_t max_rounds,
                                     size_t minimization_runs,
                                     size_t benchmark_rounds)
    : strategy(sched_class),
      checker(checker),
      pretty_printer(pretty_printer),
      max_tasks(max_tasks),
      max_rounds(max_rounds),
      minimization_runs(minimization_runs),
      benchmark_rounds(benchmark_rounds) {}

Scheduler::Result StrategyScheduler::RunRound() {
  // History of invoke and response events which is required for the checker
  SeqHistory sequential_history;
  // Full history of the current execution in the Run function
  FullHistory full_history;

  for (size_t finished_tasks = 0; finished_tasks < max_tasks;) {
    auto [next_task, is_new, thread_id] = strategy.Next();

    // fill the sequential history
    if (is_new) {
      sequential_history.emplace_back(Invoke(next_task, thread_id));
    }
    full_history.emplace_back(next_task);

    next_task->Resume();
    if (next_task->IsReturned()) {
      finished_tasks++;

      auto result = next_task->GetRetVal();
      sequential_history.emplace_back(Response(next_task, result, thread_id));
    }
  }

  pretty_printer.PrettyPrint(sequential_history, log());

  if (!checker.Check(sequential_history)) {
    return std::make_pair(full_history, sequential_history);
  }

  return std::nullopt;
}

// TODO: Sometimes this method hangs on call to Resume() method
//       same applies to the RunRound method which hangs sometimes, probably on the same call.
//       Should check what cauases that.
StrategyScheduler::Result StrategyScheduler::ExploreRound(int runs) {
  for (int i = 0; i < runs; ++i) {
    // log() << "Run " << i + 1 << "/" << runs << "\n";
    strategy.ResetCurrentRound();
    SeqHistory sequential_history;
    FullHistory full_history;

    for (int tasks_to_run = strategy.GetValidTasksCount(); tasks_to_run > 0;) {
      auto [next_task, is_new, thread_id] = strategy.NextSchedule();

      if (is_new) {
        sequential_history.emplace_back(Invoke(next_task, thread_id));
      }
      full_history.emplace_back(next_task);

      // log() << "Rusume task: is_new=" << is_new << ", thread=" << thread_id << ", task_id=" << next_task->GetId() << " (removed: " << next_task->IsRemoved() << ", returned: " << next_task->IsReturned() << ")" << "\n";
      next_task->Resume();
      if (next_task->IsReturned()) {
        // log() << "Returned task (" << next_task->IsReturned() << "): thread=" << thread_id << ", task_id=" << next_task->GetId() << "\n";
        tasks_to_run--;

        auto result = next_task->GetRetVal();
        // log() << "Returned value: " << result << "\n";
        sequential_history.emplace_back(Response(next_task, result, thread_id));
      }
    }

    // log() << "Checking round for linearizability...\n";
    if (!checker.Check(sequential_history)) {
      // log() << "New nonlinearized scenario:\n";
      // pretty_printer.PrettyPrint(sequential_history, log());
      return std::make_pair(full_history, sequential_history);
    }
  }

  return std::nullopt;
}

StrategyScheduler::Result StrategyScheduler::ReplayRound(const std::vector<int>& tasks_ordering) {
  strategy.ResetCurrentRound();

  // History of invoke and response events which is required for the checker
  FullHistory full_history;
  SeqHistory sequential_history;
  // TODO: `IsRunning` field might be added to `Task` instead
  std::unordered_set<int> started_tasks;
  std::unordered_map<int, int> resumes_count; // task id -> number of appearences in `tasks_ordering`

  for (int next_task_id : tasks_ordering) {
    resumes_count[next_task_id]++;
  }

  for (int next_task_id : tasks_ordering) {
    bool is_new = started_tasks.contains(next_task_id) ? false : started_tasks.insert(next_task_id).second;
    auto task_info = strategy.GetTask(next_task_id);

    if (!task_info.has_value()) {
      std::cerr << "No task with id " << next_task_id << " exists in round" << std::endl;
      throw std::runtime_error("Invalid task id");
    }

    auto [next_task, thread_id] = task_info.value();
    if (is_new) {
      sequential_history.emplace_back(Invoke(next_task, thread_id));
    }
    full_history.emplace_back(next_task);

    if (next_task->IsReturned()) continue;

    // if this is the last time this task appears in `tasks_ordering`, then complete it fully.
    if (resumes_count[next_task_id] == 0) {
      next_task->Terminate();
    }
    else {
      resumes_count[next_task_id]--;
      next_task->Resume();
    }

    if (next_task->IsReturned()) {
      auto result = next_task->GetRetVal();
      sequential_history.emplace_back(Response(next_task, result, thread_id));
    }
  }

  // pretty_printer.PrettyPrint(sequential_history, log());

  if (!checker.Check(sequential_history)) {
    return std::make_pair(full_history, sequential_history);
  }

  return std::nullopt;
}


const Strategy& StrategyScheduler::GetStrategy() const {
  return strategy;
}

std::vector<int> StrategyScheduler::GetTasksOrdering(
  const FullHistory& full_history,
  const std::unordered_set<int> exclude_task_ids
) {
  std::vector <int> tasks_ordering;
  
  for (auto& task : full_history) {
    if (exclude_task_ids.contains(task.get()->GetId())) continue;
    tasks_ordering.emplace_back(task.get()->GetId());
  }

  return tasks_ordering;
}

void StrategyScheduler::Minimize(
  Scheduler::Histories& nonlinear_history,
  const RoundMinimizor& minimizor
) {
  minimizor.Minimize(*this, nonlinear_history);
}

void StrategyScheduler::ResetCurrentRound() {
  strategy.ResetCurrentRound();
  auto& threads = strategy.GetTasks();
  for (auto& thread : threads) {
    for (int i = 0; i < thread.size(); ++i) {
      thread[i]->SetRemoved(false);
    }
  }
}

Scheduler::Result StrategyScheduler::Run() {
  if (benchmark_rounds > 0) {
    Timer timer;

    for (size_t i = 0; i < benchmark_rounds; ++i) {
      while (true) {
        log() << "run round: " << i << "\n";
        auto histories = RunRound();

        if (histories.has_value()) {
          BenchmarkData data;

          auto& [full_history, sequential_history] = histories.value();
          int total_tasks = data.total_tasks = strategy.GetTotalTasksCount();
          int64_t t = 0;

          // Sequential minimization
          log() << "Minimizing: Sequential minimization\n";
          auto hist_copy = histories.value();
          ResetCurrentRound();
          timer.Start();
          Minimize(hist_copy, SameInterleavingMinimizor());
          data.sequential_minimization_ms = t = timer.Stop();
          data.sequential_minimization_result = strategy.GetValidTasksCount();
          log() << "Elapsed time: " << t << " ms, tasks: " << strategy.GetValidTasksCount() << "/" << total_tasks << "\n";
          pretty_printer.PrettyPrint(hist_copy.second, log());

          // Exploration minimization
          log() << "Minimizing: Exploration minimization (runs: " << minimization_runs << ")\n";
          hist_copy = histories.value();
          ResetCurrentRound();
          timer.Start();
          Minimize(hist_copy, StrategyExplorationMinimizor(minimization_runs));
          data.exploration_minimization_ms = t = timer.Stop();
          data.exploration_minimization_result = strategy.GetValidTasksCount();
          log() << "Elapsed time: " << t << " ms, tasks: " << strategy.GetValidTasksCount() << "/" << total_tasks << "\n";
          pretty_printer.PrettyPrint(hist_copy.second, log());

          // Greedy minimization
          log() << "Minimizing: Greedy minimization (runs: " << minimization_runs << ")\n";
          hist_copy = histories.value();
          ResetCurrentRound();
          timer.Start();
          Minimize(hist_copy, SameInterleavingMinimizor());
          Minimize(hist_copy, StrategyExplorationMinimizor(minimization_runs));
          data.greedy_minimization_ms = t = timer.Stop();
          data.greedy_minimization_result = strategy.GetValidTasksCount();
          log() << "Elapsed time: " << t << " ms, tasks: " << strategy.GetValidTasksCount() << "/" << total_tasks << "\n";
          pretty_printer.PrettyPrint(hist_copy.second, log());

          // Double greedy minimization
          log() << "Minimizing: Double greedy minimization (runs: " << minimization_runs << ")\n";
          hist_copy = histories.value();
          ResetCurrentRound();
          timer.Start();
          Minimize(hist_copy, SameInterleavingMinimizor());
          Minimize(hist_copy, StrategyExplorationMinimizor(minimization_runs));
          Minimize(hist_copy, StrategyExplorationMinimizor(minimization_runs));
          data.double_greedy_minimization_ms = t = timer.Stop();
          data.double_greedy_minimization_result = strategy.GetValidTasksCount();
          log() << "Elapsed time: " << t << " ms, tasks: " << strategy.GetValidTasksCount() << "/" << total_tasks << "\n";
          pretty_printer.PrettyPrint(hist_copy.second, log());

          // Smart minimization
          log() << "Minimizing: Smart minimization (runs: " << minimization_runs << ")\n";
          hist_copy = histories.value();
          ResetCurrentRound();
          timer.Start();
          Minimize(hist_copy, SmartMinimizor(minimization_runs, pretty_printer));
          data.smart_minimization_ms = t = timer.Stop();
          data.smart_minimization_result = strategy.GetValidTasksCount();
          log() << "Elapsed time: " << t << " ms, tasks: " << strategy.GetValidTasksCount() << "/" << total_tasks << "\n";
          pretty_printer.PrettyPrint(hist_copy.second, log());

          // Combined 3 minimization
          log() << "Minimizing: Combined 3 minimization (runs: " << minimization_runs << ")\n";
          hist_copy = histories.value();
          ResetCurrentRound();
          timer.Start();
          Minimize(hist_copy, SameInterleavingMinimizor());
          Minimize(hist_copy, StrategyExplorationMinimizor(minimization_runs));
          Minimize(hist_copy, SmartMinimizor(minimization_runs, pretty_printer));
          data.combined_3_minimization_ms = t = timer.Stop();
          data.combined_3_minimization_result = strategy.GetValidTasksCount();
          log() << "Elapsed time: " << t << " ms, tasks: " << strategy.GetValidTasksCount() << "/" << total_tasks << "\n";
          pretty_printer.PrettyPrint(hist_copy.second, log());

          benchmark_runs.push_back(data);
          break;
        }
        log() << "===============================================\n\n";
        log().flush();
        strategy.StartNextRound();
      }
    }

    // Dump to file the results of the benchmark
    std::ofstream file("benchmarks.txt");
    
    // Total tasks
    file << "Total ";
    for (const auto& data : benchmark_runs) {
      file << data.total_tasks << " ";
    }
    file << "\n";

    // Sequential minimization
    file << "Sequential\n";
    for (const auto& data : benchmark_runs) {
      file << data.sequential_minimization_ms << " ";
    }
    file << "\n";
    for (const auto& data : benchmark_runs) {
      file << data.sequential_minimization_result << " ";
    }
    file << "\n";

    // Exploration minimization
    file << "Exploration\n";
    for (const auto& data : benchmark_runs) {
      file << data.exploration_minimization_ms << " ";
    }
    file << "\n";
    for (const auto& data : benchmark_runs) {
      file << data.exploration_minimization_result << " ";
    }
    file << "\n";

    // Greedy minimization
    file << "Greedy\n";
    for (const auto& data : benchmark_runs) {
      file << data.greedy_minimization_ms << " ";
    }
    file << "\n";
    for (const auto& data : benchmark_runs) {
      file << data.greedy_minimization_result << " ";
    }
    file << "\n";

    // Double greedy minimization
    file << "Double greedy\n";
    for (const auto& data : benchmark_runs) {
      file << data.double_greedy_minimization_ms << " ";
    }
    file << "\n";
    for (const auto& data : benchmark_runs) {
      file << data.double_greedy_minimization_result << " ";
    }
    file << "\n";

    // Smart minimization
    file << "Smart\n";
    for (const auto& data : benchmark_runs) {
      file << data.smart_minimization_ms << " ";
    }
    file << "\n";
    for (const auto& data : benchmark_runs) {
      file << data.smart_minimization_result << " ";
    }
    file << "\n";

    // Combined 3 minimization
    file << "Combined 3\n";
    for (const auto& data : benchmark_runs) {
      file << data.combined_3_minimization_ms << " ";
    }
    file << "\n";
    for (const auto& data : benchmark_runs) {
      file << data.combined_3_minimization_result << " ";
    }
    file << "\n";
    file.close();
  }
  else {
    for (size_t i = 0; i < max_rounds; ++i) {
      log() << "run round: " << i << "\n";
      auto histories = RunRound();

      if (histories.has_value()) {
        auto& [full_history, sequential_history] = histories.value();

        log() << "Full nonlinear scenario: \n";
        pretty_printer.PrettyPrint(sequential_history, log());
        
        log() << "Minimizing same interleaving...\n";
        Minimize(histories.value(), SameInterleavingMinimizor());

        log() << "Minimizing with rescheduling (runs: " << minimization_runs << ")...\n";
        Minimize(histories.value(), StrategyExplorationMinimizor(minimization_runs));

        log() << "Minimizing with smart minimizor (run: " << minimization_runs << ")...\n";
        Minimize(histories.value(), SmartMinimizor(minimization_runs, pretty_printer));

        return histories;
      }
      log() << "===============================================\n\n";
      log().flush();
      strategy.StartNextRound();
    }
  }

  return std::nullopt;
}