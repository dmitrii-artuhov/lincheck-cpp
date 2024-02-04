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
  // Scheduler must Check result of this function after each resume.
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
  std::string GetName() const;

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

// Task wrapper with more user-friendly interface
struct StackfulTask {
  explicit StackfulTask(Task task);
  // TODO: заменить это на мок, сейчас я хочу потестить линчек, не хочу сча разбираться с gmock
  StackfulTask(int ret_val, int uid, std::string name);

  // Resumes the last child
  void Resume();

  bool IsReturned();

  int GetRetVal() const;

  std::string GetName() const;

  // TODO: google uuid
  int Uid() const;

// TODO: snapshot
 private:
  std::vector<Task> stack;
  Task entrypoint;
  int last_returned_value;
  // TODO: заменить это на мок, сейчас я хочу потестить линчек, не хочу сча разбираться с gmock
  std::string name;
  bool is_testing;
  int ret_value;
  int uid;
};
}

// TODO: potential cyclic dependency
struct ActionHandle {
  ActionHandle(StackfulTask& task);
  StackfulTask& task;
};

struct StackfulTaskResponse {
  StackfulTaskResponse(const StackfulTask &task, int result);
  const StackfulTask &task;
  int result;
};

struct StackfulTaskInvoke {
  StackfulTaskInvoke(const StackfulTask &task);
  const StackfulTask &task;
};
