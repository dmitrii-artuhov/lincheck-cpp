#include <numeric>

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
  std::unordered_map<int, int> resumes_count; // TaskId -> Appearences in `tasks_ordering`

  for (int next_task_id : tasks_ordering) {
    resumes_count[next_task_id]++;
  }

  // log() << "\n\n\nReplaying round: ";
  // for (int task_id : tasks_ordering) log() << task_id << " ";
  // log() << "\n";

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

    // log() << "Resume task: id=" << next_task_id << ", " << next_task->GetName() << ", thread-id=" << thread_id << "\n";

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
      // log() << "Return from task: " << next_task_id << "\n";
      auto result = next_task->GetRetVal();
      sequential_history.emplace_back(Response(next_task, result, thread_id));
    }
  }

  //pretty_printer.PrettyPrint(sequential_history, log());

  if (!checker.Check(sequential_history)) {
    return std::make_pair(full_history, sequential_history);
  }

  return std::nullopt;
}

std::vector<int> StrategyScheduler::getTasksOrdering(const FullHistory& full_history, const std::unordered_set<int> exclude_task_ids) const {
  std::vector <int> tasks_ordering;
  
  for (auto& task : full_history) {
    if (exclude_task_ids.contains(task.get()->GetId())) continue;
    tasks_ordering.emplace_back(task.get()->GetId());
  }

  return tasks_ordering;
}

void StrategyScheduler::minimize(
  std::pair<Scheduler::FullHistory, Scheduler::SeqHistory>& nonlinear_history
) {
  std::vector<std::reference_wrapper<const Task>> tasks;

  for (const HistoryEvent& event : nonlinear_history.second) {
    if (std::holds_alternative<Invoke>(event)) {
      tasks.push_back(std::get<Invoke>(event).GetTask());
    }
  }

  // remove single task
  for (auto& task : tasks) {
    int task_id = task.get()->GetId();

    // log() << "Try to remove task with id: " << task_id << "\n";
    std::vector<int> new_ordering = getTasksOrdering(nonlinear_history.first, { task_id });
    auto new_histories = replayRound(new_ordering);

    if (new_histories.has_value()) {
      nonlinear_history.first.swap(new_histories.value().first);
      nonlinear_history.second.swap(new_histories.value().second);
      task.get()->SetRemoved(true);
    }
  }

  // remove two tasks (for operations with semantics of add/remove)
  for (auto& task_i : tasks) {
    if (task_i.get()->IsRemoved()) continue;
    
    for (auto& task_j : tasks) {
      if (task_j.get()->IsRemoved()) continue;

      int task_i_id = task_i.get()->GetId();
      int task_j_id = task_j.get()->GetId();
      
      if (task_i_id == task_j_id) continue;
      
      // log() << "Try to remove tasks with ids: " << task_i_id << " and " << task_j_id << "\n";
      std::vector<int> new_ordering = getTasksOrdering(nonlinear_history.first, { task_i_id, task_j_id });
      auto new_histories = replayRound(new_ordering);

      if (new_histories.has_value()) {
        // sequential history (Invoke/Response) must have even number of history events
        assert(new_histories.value().second.size() % 2 == 0);

        nonlinear_history.first.swap(new_histories.value().first);
        nonlinear_history.second.swap(new_histories.value().second);
        
        task_i.get()->SetRemoved(true);
        task_j.get()->SetRemoved(true);
      }
    }
  }

  // replay minimized round one last time to put coroutines in `returned` state
  // (because multiple failed attempts to minimize new scenarios could leave tasks in invalid state)
  replayRound(getTasksOrdering(nonlinear_history.first, {}));
}

Scheduler::Result StrategyScheduler::Run() {
  for (size_t i = 0; i < max_rounds; ++i) {
    log() << "run round: " << i << "\n";
    auto histories = runRound();

    if (histories.has_value()) {
      auto& [full_history, sequential_history] = histories.value();

      log() << "Full nonlinear scenario: \n";
      pretty_printer.PrettyPrint(sequential_history, log());
      
      log() << "Minimizing...\n";
      minimize(histories.value());

      return histories;
    }
    log() << "===============================================\n\n";
    log().flush();
    strategy.StartNextRound();
  }

  return std::nullopt;
}