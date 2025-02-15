#include <numeric>
#include <fstream>

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
                                     size_t exploration_runs,
                                     size_t minimization_runs,
                                     size_t benchmark_runs)
    : strategy(sched_class),
      checker(checker),
      pretty_printer(pretty_printer),
      max_tasks(max_tasks),
      max_rounds(max_rounds),
      exploration_runs(exploration_runs),
      minimization_runs(minimization_runs),
      benchmark_runs(benchmark_runs)
       {}

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

      next_task->Resume();
      if (next_task->IsReturned()) {
        tasks_to_run--;

        auto result = next_task->GetRetVal();
        sequential_history.emplace_back(Response(next_task, result, thread_id));
      }
    }

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
  std::unordered_set<int> finished_tasks;
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
    if (next_task->IsReturned()) {
      // we could witness the task that still has resumes (`resumes_count[next_task_id] > 0`),
      // but is already returned, so we just skip it, because its `Response` was already added
      if (!finished_tasks.contains(next_task_id)) {
        std::cout << "Task " << next_task_id << " is already returned, but not in the finished_tasks set" << std::endl;
        std::cout << "Resumes: " << resumes_count[next_task_id] << std::endl;
        std::cout << "Started: " << started_tasks.contains(next_task_id) << std::endl;
        for (auto tid : tasks_ordering) {
          std::cout << tid << ", ";
        }
        std::cout << std::endl;
      }
      assert(finished_tasks.contains(next_task_id));
      continue;
    }

    if (is_new) {
      sequential_history.emplace_back(Invoke(next_task, thread_id));
    }
    full_history.emplace_back(next_task);

    resumes_count[next_task_id]--;

    // if this is the last time this task appears in `tasks_ordering`, then complete it fully.
    if (resumes_count[next_task_id] == 0) {
      next_task->Terminate();
    }
    else {
      next_task->Resume();
    }

    if (next_task->IsReturned()) {
      auto result = next_task->GetRetVal();
      sequential_history.emplace_back(Response(next_task, result, thread_id));
      finished_tasks.insert(next_task_id);
    }
  }

  // pretty_printer.PrettyPrint(sequential_history, log());
  assert(started_tasks.size() == finished_tasks.size());

  if (!checker.Check(sequential_history)) {
    return std::make_pair(full_history, sequential_history);
  }

  return std::nullopt;
}

Strategy& StrategyScheduler::GetStrategy() const {
  return strategy;
}

void StrategyScheduler::Minimize(
  Scheduler::BothHistories& nonlinear_history,
  const RoundMinimizor& minimizor
) {
  minimizor.Minimize(*this, nonlinear_history);
}

Scheduler::Result StrategyScheduler::Run() {
  if (benchmark_runs > 0) {
    Timer timer;

    for (size_t i = 0; i < benchmark_runs; ++i) {
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
          ResetRoundForBenchmark();
          timer.Start();
          Minimize(hist_copy, SameInterleavingMinimizor());
          data.sequential_minimization_ms = t = timer.Stop();
          data.sequential_minimization_result = strategy.GetValidTasksCount();
          log() << "Elapsed time: " << t << " ms, tasks: " << strategy.GetValidTasksCount() << "/" << total_tasks << "\n";
          pretty_printer.PrettyPrint(hist_copy.second, log());

          // Exploration minimization
          log() << "Minimizing: Exploration minimization (runs: " << minimization_runs << ")\n";
          hist_copy = histories.value();
          ResetRoundForBenchmark();
          timer.Start();
          Minimize(hist_copy, StrategyExplorationMinimizor(minimization_runs));
          data.exploration_minimization_ms = t = timer.Stop();
          data.exploration_minimization_result = strategy.GetValidTasksCount();
          log() << "Elapsed time: " << t << " ms, tasks: " << strategy.GetValidTasksCount() << "/" << total_tasks << "\n";
          pretty_printer.PrettyPrint(hist_copy.second, log());

          // Greedy minimization
          log() << "Minimizing: Greedy minimization (runs: " << minimization_runs << ")\n";
          hist_copy = histories.value();
          ResetRoundForBenchmark();
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
          ResetRoundForBenchmark();
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
          ResetRoundForBenchmark();
          timer.Start();
          Minimize(hist_copy, SmartMinimizor(exploration_runs, minimization_runs, pretty_printer));
          data.smart_minimization_ms = t = timer.Stop();
          data.smart_minimization_result = strategy.GetValidTasksCount();
          log() << "Elapsed time: " << t << " ms, tasks: " << strategy.GetValidTasksCount() << "/" << total_tasks << "\n";
          pretty_printer.PrettyPrint(hist_copy.second, log());

          // Combined 3 minimization
          log() << "Minimizing: Combined 3 minimization (runs: " << minimization_runs << ")\n";
          hist_copy = histories.value();
          ResetRoundForBenchmark();
          timer.Start();
          Minimize(hist_copy, SameInterleavingMinimizor());
          Minimize(hist_copy, StrategyExplorationMinimizor(minimization_runs));
          Minimize(hist_copy, SmartMinimizor(exploration_runs, minimization_runs, pretty_printer));
          data.combined_3_minimization_ms = t = timer.Stop();
          data.combined_3_minimization_result = strategy.GetValidTasksCount();
          log() << "Elapsed time: " << t << " ms, tasks: " << strategy.GetValidTasksCount() << "/" << total_tasks << "\n";
          pretty_printer.PrettyPrint(hist_copy.second, log());

          benchmarks.push_back(data);
          break;
        }
        log() << "===============================================\n\n";
        log().flush();
        strategy.StartNextRound();
      }
      strategy.StartNextRound();
    }

    // Dump to file the results of the benchmark
    std::string filename = "benchmarks-" + std::to_string(rand()) + ".txt";
    std::ofstream file(filename);

    // Total tasks
    float sum = 0.0;
    int size = benchmarks.size();

    file << "Total ";
    for (const auto& data : benchmarks) {
      sum += data.total_tasks;
      file << data.total_tasks << " ";
    }
    file << ": " << sum / size;
    file << "\n";

    // Sequential minimization
    file << "Sequential\n";
    sum = 0;
    for (const auto& data : benchmarks) {
      sum += data.sequential_minimization_ms;
      file << data.sequential_minimization_ms << " ";
    }
    file << ": " << sum / size;
    file << "\n";
    sum = 0;
    for (const auto& data : benchmarks) {
      sum += data.sequential_minimization_result;
      file << data.sequential_minimization_result << " ";
    }
    file << ": " << sum / size;
    file << "\n";

    // Exploration minimization
    file << "Exploration\n";
    sum = 0;
    for (const auto& data : benchmarks) {
      sum += data.exploration_minimization_ms;
      file << data.exploration_minimization_ms << " ";
    }
    file << ": " << sum / size;
    file << "\n";
    sum = 0;
    for (const auto& data : benchmarks) {
      sum += data.exploration_minimization_result;
      file << data.exploration_minimization_result << " ";
    }
    file << ": " << sum / size;
    file << "\n";

    // Greedy minimization
    file << "Greedy\n";
    sum = 0;
    for (const auto& data : benchmarks) {
      sum += data.greedy_minimization_ms;
      file << data.greedy_minimization_ms << " ";
    }
    file << ": " << sum / size;
    file << "\n";
    sum = 0;
    for (const auto& data : benchmarks) {
      sum += data.greedy_minimization_result;
      file << data.greedy_minimization_result << " ";
    }
    file << ": " << sum / size;
    file << "\n";

    // Double greedy minimization
    file << "Double greedy\n";
    sum = 0;
    for (const auto& data : benchmarks) {
      sum += data.double_greedy_minimization_ms;
      file << data.double_greedy_minimization_ms << " ";
    }
    file << ": " << sum / size;
    file << "\n";
    sum = 0;
    for (const auto& data : benchmarks) {
      sum += data.double_greedy_minimization_result;
      file << data.double_greedy_minimization_result << " ";
    }
    file << ": " << sum / size;
    file << "\n";

    // Smart minimization
    file << "Smart\n";
    sum = 0;
    for (const auto& data : benchmarks) {
      sum += data.smart_minimization_ms;
      file << data.smart_minimization_ms << " ";
    }
    file << ": " << sum / size;
    file << "\n";
    sum = 0;
    for (const auto& data : benchmarks) {
      sum += data.smart_minimization_result;
      file << data.smart_minimization_result << " ";
    }
    file << ": " << sum / size;
    file << "\n";

    // Combined 3 minimization
    file << "Combined 3\n";
    sum = 0;
    for (const auto& data : benchmarks) {
      sum += data.combined_3_minimization_ms;
      file << data.combined_3_minimization_ms << " ";
    }
    file << ": " << sum / size;
    file << "\n";
    sum = 0;
    for (const auto& data : benchmarks) {
      sum += data.combined_3_minimization_result;
      file << data.combined_3_minimization_result << " ";
    }
    file << ": " << sum / size;
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
        log() << "Minimized to:\n";
        pretty_printer.PrettyPrint(sequential_history, log());

        log() << "Minimizing with rescheduling (exploration runs: " << exploration_runs << ")...\n";
        Minimize(histories.value(), StrategyExplorationMinimizor(exploration_runs));
        log() << "Minimized to:\n";
        pretty_printer.PrettyPrint(sequential_history, log());

        log() << "Minimizing with smart minimizor (exploration runs: "
              << exploration_runs << ", minimization runs: "
              << minimization_runs << ")...\n";
        Minimize(histories.value(), SmartMinimizor(exploration_runs, minimization_runs, pretty_printer));

        return histories;
      }
      log() << "===============================================\n\n";
      log().flush();
      strategy.StartNextRound();
    }
  }

  return std::nullopt;
}