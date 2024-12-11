#include "include/scheduler.h"

#include "include/logger.h"
#include "include/pretty_print.h"

StrategyScheduler::StrategyScheduler(Strategy &sched_class,
                                     ModelChecker &checker,
                                     PrettyPrinter &pretty_printer,
                                     size_t max_tasks, size_t max_rounds)
    : strategy(sched_class),
      checker(checker),
      pretty_printer(pretty_printer),
      max_tasks(max_tasks),
      max_rounds(max_rounds) {}

Scheduler::Result StrategyScheduler::runRound() {
  // History of invoke and response events which is required for the checker
  std::vector<std::variant<Invoke, Response>> sequential_history;
  // Full history of the current execution in the Run function
  std::vector<std::reference_wrapper<Task>> full_history;

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

StrategyScheduler::Result StrategyScheduler::replayRound(const std::vector<int>& tasks_ordering) {
  strategy.ResetCurrentRound();

  // History of invoke and response events which is required for the checker
  FullHistory full_history;
  SeqHistory sequential_history;
  // TODO: `IsRunning` field might be added to `Task` instead
  std::unordered_set<int> started_tasks;

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

    log() << "Resume task: id=" << next_task_id << ", " << next_task->GetName() << ", thread-id=" << thread_id << "\n";

    next_task->Resume();
    if (next_task->IsReturned()) {
      auto result = next_task->GetRetVal();
      sequential_history.emplace_back(Response(next_task, result, thread_id));
    }
  }

  log() << "Replayed round: tasks ordering = ";
  for (size_t i = 0; i < tasks_ordering.size(); ++i) {
    int task_id = tasks_ordering[i];
    log() << task_id;
    if (i != tasks_ordering.size() - 1) log() << ",";
    log() << " "; 
  }
  log() << "\n";

  pretty_printer.PrettyPrint(sequential_history, log());

  if (!checker.Check(sequential_history)) {
    return std::make_pair(full_history, sequential_history);
  }

  return std::nullopt;
}

std::vector <int> StrategyScheduler::getTasksOrdering(const FullHistory& full_history) const {
  std::vector <int> tasks_ordering;
  tasks_ordering.reserve(full_history.size());
  
  for (auto& task : full_history) {
    tasks_ordering.emplace_back(task.get()->GetId());
  }

  return tasks_ordering;
}

Scheduler::Result StrategyScheduler::Run() {
  for (size_t i = 0; i < max_rounds; ++i) {
    log() << "run round: " << i << "\n";
    auto histories = runRound();

    if (histories.has_value()) {
      log() << "found failing sequential history:\n";
      auto& [full_history, sequential_history] = histories.value();

      // Print the sequential history for debugging
      for (auto history_point : sequential_history) {
        if (std::holds_alternative<Invoke>(history_point)) {
          Invoke& inv = std::get<Invoke>(history_point);
          log() << "i(" << inv.thread_id << ", '" << inv.GetTask()->GetName()  << "'), ";
        }
        else {
          Response& response = std::get<Response>(history_point);
          log() << "r(" << response.thread_id << ", '" << response.GetTask()->GetName() << "'): " << response.result << ", ";
        }
      }
      log() << "\n";

      // TODO: add minimization here
      log() << "Replaying round for test\n";
      std::vector <int> tasks_ordering = getTasksOrdering(full_history);
      auto res = replayRound(tasks_ordering);

      return histories;
    }
    log() << "===============================================\n\n";
    log().flush();
    strategy.StartNextRound();
  }

  return std::nullopt;
}

// Scheduler::Result StrategyScheduler::minimizeHistory(Result nonlinear_history) {
//   if (!nonlinear_history.has_value()) return nonlinear_history;

//   auto& full_history = nonlinear_history.value().first;
//   auto& sequential_history = nonlinear_history.value().second;

//   log() << "Minimizing history:\n";
//   pretty_printer.PrettyPrint(sequential_history, log());
  
//   int task_idx_rm = 0; // task index in the sequential_history which we try to remove

//   while (task_idx_rm < sequential_history.size()) {
//     while (std::holds_alternative<Response>(sequential_history[task_idx_rm])) {
//       task_idx_rm++;
//     }

//     const Task& removed_task = std::get<Invoke>(sequential_history[task_idx_rm]).GetTask();
//     log() << "Try remove task: (" << task_idx_rm << ") " << removed_task->GetName() << "\n";
    
//     // create a new `std::vector<int> threads_order`, in which `removed_task` will be excluded
    
//   }
// }
