#pragma once
#include <coroutine>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

extern "C" {
// Let's begin from C-style API to make LLVM calls easier.

// This structure must be equal to the clone in LLVM pass.
struct CoroPromise {
  using handle = std::coroutine_handle<CoroPromise>;

  int has_ret_val{};
  int ret_val{};
  handle child_hdl{};
};

struct CoroPromise;
using handle = std::coroutine_handle<CoroPromise>;

// Clones the existing task. Cloners are created with macro.
typedef handle (*task_cloner_t)(void* this_ptr, void* args_ptr);

// Task describes coroutine to run.
// Entrypoint receives list of task builders as input.
struct Task {
  // Constructs a non root task from handler.
  Task(handle hdl);

  // Мета информация о таске.
  // Не меняется в рантайме таски.
  // Хранятся только для корневых тасок.
  // args хранит поинтер на tuple из аргументов.
  // str_args - строковое представление аргументов.
  struct Meta {
    std::string name;
    std::shared_ptr<void> args;
    std::vector<std::string> str_args;
  };

  // Constructs a root task from handler, cloner.
  Task(handle hdl, task_cloner_t cloner);

  // Task(const Task&) = default;
  // Task(Task&&) = default;

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

  const std::string& GetName() const;
  void* GetArgs() const;
  const std::vector<std::string>& GetStrArgs() const;

  void SetMeta(std::shared_ptr<Meta> meta);

  void StartFromTheBeginning(void* state);

 private:
  std::shared_ptr<Meta> meta{};
  task_cloner_t cloner;
  handle hdl;
};

// StackfulTask is a Task wrapper which contains the stack inside, so resume
// method resumes the last subtask.
struct StackfulTask {
  explicit StackfulTask(Task task);

  const std::string& GetName() const;
  void* GetArgs() const;
  const std::vector<std::string>& GetStrArgs() const;
  Task GetEntrypoint() const;

  void StartFromTheBeginning(void* state);

  // Resume method resumes the last subtask.
  virtual void Resume();

  // Haven't the first task finished yet?
  virtual bool IsReturned();

  // Returns the value that was returned from the first task, have to be called
  // only when IsReturned is true
  // TODO: after a while int will be replaced with the trait
  [[nodiscard]] virtual int GetRetVal() const;

  virtual ~StackfulTask();

  StackfulTask& operator=(const StackfulTask& other) = default;

  // TODO: snapshot method might be useful.
 protected:
  // Need this constructor for tests
  StackfulTask();

 public:
  std::vector<Task> stack{};
  // Need option for tests, because I have to initialize Task field(
  Task entrypoint;
  int last_returned_value{};
};
}

// Creates task. Builders are created with macro.
typedef Task (*task_builder_t)(void* this_ptr);

struct Response {
  Response(const StackfulTask& task, int result, int thread_id);

  [[nodiscard]] const StackfulTask& GetTask() const;

  int result;
  int thread_id;

 private:
  std::reference_wrapper<const StackfulTask> task;
};

struct Invoke {
  explicit Invoke(const StackfulTask& task, int thread_id);

  [[nodiscard]] const StackfulTask& GetTask() const;

  int thread_id;

 private:
  std::reference_wrapper<const StackfulTask> task;
};
