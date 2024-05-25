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
  std::vector<HistoryEvent> sequential_history;
  // Full history of the current execution in the Run function
  std::vector<std::variant<Task, DualTask>> full_history;

  for (size_t finished_tasks = 0; finished_tasks < max_tasks;) {
    auto [next_task, is_new, thread_id] = strategy.Next();

    if (std::holds_alternative<Task>(next_task)) {
      auto task = std::get<Task>(next_task);
      if (is_new) {
        sequential_history.emplace_back(Invoke(task, thread_id));
      }

      full_history.emplace_back(task);
      task->Resume();
      if (task->IsReturned()) {
        finished_tasks++;

        auto result = task->GetRetVal();
        sequential_history.emplace_back(Response(task, result, thread_id));
      }
    } else {
      DualTask task = std::get<DualTask>(next_task);
      if (is_new) {
        sequential_history.emplace_back(RequestInvoke(task, thread_id));
        task->SetFollowUpTerminateCallback([&sequential_history, &finished_tasks, task,
                                            thread_id]() {
          // TODO: подумать над thread_id
          // TODO: нас разбудили раньше, чем закончился await_suspend
          // бывает, сразу закончим историю
          if (!task->IsRequestFinished()) {
            sequential_history.push_back(RequestResponse(task, thread_id));
            sequential_history.push_back(FollowUpInvoke(task, thread_id));
          }
          sequential_history.emplace_back(FollowUpResponse(task, thread_id));
          finished_tasks++;
        });
      }

      // Bug! Deadlock!
      if (task->IsRequestFinished()) {
        pretty_printer.PrettyPrint(sequential_history, log());
      }
      assert(!task->IsRequestFinished());
      full_history.emplace_back(task);
      task->ResumeRequest();

      // если разбудили раньше, то не нужно добавлять, тк добавилось в колбэке
      // Кто-то успеет раньше
      if (task->IsRequestFinished() && !task->IsFollowUpFinished()) {
        sequential_history.emplace_back(RequestResponse(task, thread_id));
        sequential_history.emplace_back(FollowUpInvoke(task, thread_id));
      }
    }
  }

  pretty_printer.PrettyPrint(sequential_history, log());

  if (!checker.Check(sequential_history)) {
    return std::make_pair(full_history, sequential_history);
  }

  return std::nullopt;
}

Scheduler::Result StrategyScheduler::Run() {
  for (size_t i = 0; i < max_rounds; ++i) {
    log() << "run round: " << i << "\n";
    auto seq_history = runRound();
    if (seq_history.has_value()) {
      return seq_history;
    }
    log() << "===============================================\n\n";
    log().flush();
    strategy.StartNextRound();
  }

  return std::nullopt;
}
