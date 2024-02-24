#pragma once
#include <coroutine>
#include <cstdint>
#include <optional>
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

// StackfulTask is a Task wrapper which contains the stack inside, so resume
// method resumes the last subtask.
struct StackfulTask {
  explicit StackfulTask(Task task);

  // Resume method resumes the last subtask.
  virtual void Resume();

  // Haven't the first task finished yet?
  virtual bool IsReturned();

  // Returns the value that was returned from the first task, have to be called
  // only when IsReturned is true
  // TODO: after a while int will be replaced with the trait
  [[nodiscard]] virtual int GetRetVal() const;

  [[nodiscard]] virtual const std::string &GetName() const;

  virtual ~StackfulTask() = default;

  StackfulTask& operator=(const StackfulTask& other)
  = default;

  // TODO: snapshot method might be useful.
 protected:
  // Need this constructor for tests
  StackfulTask() = default;

 private:
  std::vector<Task> stack;
  // Need option for tests, because I have to initialize Task field(
  std::optional<Task> entrypoint;
  int last_returned_value{};
};
}

struct StackfulTaskResponse {
  StackfulTaskResponse(const StackfulTask &task, int result);

  const StackfulTask& GetTask() const;

  int result;

 private:
  std::reference_wrapper<const StackfulTask> task;
};

//void swap(StackfulTaskResponse& lhs, StackfulTaskResponse& rhs);

struct StackfulTaskInvoke {
  explicit StackfulTaskInvoke(const StackfulTask &task);

  const StackfulTask& GetTask() const;

 private:
  std::reference_wrapper<const StackfulTask> task;
};

//void swap(StackfulTaskInvoke& lhs, StackfulTaskInvoke& rhs);
