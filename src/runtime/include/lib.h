#pragma once
#include <coroutine>
#include <cstdint>
#include <string>
#include <vector>

extern "C" {
// Let's begin from C-style API to make LLVM calls easier.

struct CoroPromise;
using handle = std::coroutine_handle<CoroPromise>;

// Task describes coroutine to run.
// Entrypoint receives list of task builders as input.
struct Task {
  Task(handle hdl);

  // Resumes task until next suspension point.
  // If task calls another coroutine during execution
  // then has_child() returns true after this call.
  //
  // Panics if has_ret_val() == true.
  void Resume();

  // Returns true if the task called another coroutine task.
  // Scheduler must check result of this function after each resume.
  bool HasChild();

  // Returns child of the task.
  //
  // Panics if has_child() == false.
  Task GetChild();

  // Must be called by scheduler after current
  // child task is terminated.
  void ClearChild();

  // Returns true if the task is terminated.
  bool IsReturned();

  // Get return value of the task.
  //
  // Panics if has_ret_val() == false.
  int GetRetVal();

  // Returns the task name.
  std::string GetName();

 private:
  handle hdl;
};

// Describes function that builds new task.
typedef Task (*TaskBuilder)();

// Contains task builders.
// Will be created during LLVM pass and
// passed to the runtime entrypoint.
using TaskBuilderList = std::vector<TaskBuilder> *;

// Runtime entrypoint.
// Call will be generated during LLVM pass.
void run(TaskBuilderList l);
}
