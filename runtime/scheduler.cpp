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

// TODO: refactor
// StrategyScheduler::Result StrategyScheduler::replayRound(FullHistory tasks_order) {
//   // History of invoke and response events which is required for the checker
//   SeqHistory sequential_history;

//   for (std::reference_wrapper<Task> next_task : tasks_order) {
    
//   }
// }

Scheduler::Result StrategyScheduler::Run() {
  for (size_t i = 0; i < max_rounds; ++i) {
    log() << "run round: " << i << "\n";
    auto seq_history = runRound();
    if (seq_history.has_value()) {

      log() << "found failing sequential history:\n";
      auto& seq = seq_history.value().second;
      for (auto history_point : seq) {
        if (std::holds_alternative<Invoke>(history_point)) {
          Invoke& inv = std::get<Invoke>(history_point);
          log() << "i(" << inv.thread_id << ", '" << inv.GetTask()->GetName()  << "'), ";
        }
        else {
          Response& resp = std::get<Response>(history_point);
          log() << "r(" << resp.thread_id << ", '" << resp.GetTask()->GetName() << "'): " << resp.result << ", ";
        }
      }
      log() << "\n";

      // TODO: add minimization here
      return seq_history;
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
