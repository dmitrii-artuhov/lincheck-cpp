#include "include/lib.h"

#include <cassert>
#include <iostream>
#include <vector>

extern "C" {

CoroPromise *get_promise(std::coroutine_handle<CoroPromise> hdl) {
  return &hdl.promise();
}

// We must keep promise in the stack (LLVM now doesn't support heap promises).
// So there is no `new_promise()` function,
// instead we allocate promise at the stack in codegen pass.
void init_promise(CoroPromise *p) {
  assert(p != nullptr);

  p->child_hdl = nullptr;
  p->ret_val = 0;
  p->has_ret_val = 0;
}

handle get_child_hdl(CoroPromise *p) {
  assert(p != nullptr);
  assert(p->child_hdl && "p has not child");

  return p->child_hdl;
}

bool has_child_hdl(CoroPromise *p) {
  assert(p != nullptr);

  return p->child_hdl != nullptr;
}

void set_child_hdl(CoroPromise *p, int8_t *hdl) {
  assert(p != nullptr);

  p->child_hdl = std::coroutine_handle<CoroPromise>::from_address(hdl);
}

void set_ret_val(CoroPromise *p, int ret_val) {
  assert(p != nullptr);

  p->has_ret_val = 1;
  p->ret_val = ret_val;
}

bool has_ret_val(CoroPromise *p) {
  assert(p != nullptr);

  return p->has_ret_val;
}

int get_ret_val(CoroPromise *p) {
  assert(p != nullptr);
  assert(p->has_ret_val && "promise has not ret val");

  return p->ret_val;
}

void coro_yield() noexcept {}
}

StackfulTask *current_task;
StackfulTask *GetCurrentTask() {
  assert(current_task != nullptr);
  return current_task;
}

void SetCurrentTask(StackfulTask *task) { current_task = task; }

void Token::Reset() { parked = false; }

// ------------------------------ TASK ----------------------------------------

Task::Task(handle hdl) : hdl(hdl) {}

Task::Task(handle hdl, task_cloner_t cloner) : hdl{hdl}, cloner{cloner} {}

void Task::SetMeta(std::shared_ptr<Meta> meta) { this->meta = meta; }

void Task::Resume() {
  assert(!IsReturned() && "returned task can not be resumed");
  hdl.resume();
}

bool Task::HasChild() { return has_child_hdl(&hdl.promise()); }

Task Task::GetChild() {
  assert(HasChild() && "get_child() can not be called on childless task");
  return Task(hdl.promise().child_hdl);
}

void Task::ClearChild() { set_child_hdl(&hdl.promise(), nullptr); }

bool Task::IsReturned() { return has_ret_val(&hdl.promise()); }

int Task::GetRetVal() { return get_ret_val(&hdl.promise()); }

void Task::StartFromTheBeginning(void *state) {
  assert(meta);
  hdl = cloner(state, meta->args.get());
}

const std::string &Task::GetName() const {
  assert(meta);
  return meta->name;
}

void *Task::GetArgs() const {
  assert(meta);
  return meta->args.get();
}

const std::vector<std::string> &Task::GetStrArgs() const {
  assert(meta);
  return meta->str_args;
}

void Task::Destroy() { hdl.destroy(); }

// ------------------------ STACKFUL TASK ------------------------

StackfulTask::StackfulTask(task_builder_t builder, void *this_state)
    : entrypoint{nullptr} {
  SetCurrentTask(this);
  // Builder could call generator that set token.
  auto task = builder(this_state);
  SetCurrentTask(nullptr);
  entrypoint = task;
  stack = std::vector<Task>{task};
}

StackfulTask::StackfulTask() : entrypoint(nullptr) {}

StackfulTask::StackfulTask(Task raw_task) : entrypoint(raw_task) {
  stack = std::vector<Task>{raw_task};
}

void StackfulTask::Resume() {
  assert(!stack.empty());
  Task &stack_head = stack.back();
  stack_head.Resume();

  if (stack_head.HasChild()) {
    // new child was forked
    stack.push_back(stack_head.GetChild());
  } else if (stack_head.IsReturned()) {
    // stack_head returned
    last_returned_value = stack_head.GetRetVal();

    to_destroy.push_back(stack.back());
    stack.pop_back();

    // if it wasn't the first task clean up children
    if (!stack.empty()) {
      stack.back().ClearChild();
    }
  }
}

const std::string &StackfulTask::GetName() const {
  return entrypoint.GetName();
}

void *StackfulTask::GetArgs() const { return entrypoint.GetArgs(); }

const std::vector<std::string> &StackfulTask::GetStrArgs() const {
  return entrypoint.GetStrArgs();
}

Task StackfulTask::GetEntrypoint() const { return entrypoint; }

bool StackfulTask::IsReturned() { return stack.empty(); }

int StackfulTask::GetRetVal() const { return last_returned_value; }

const StackfulTask &Invoke::GetTask() const { return this->task.get(); }

const StackfulTask &Response::GetTask() const { return this->task.get(); }

StackfulTask::~StackfulTask() {
  // The task must be returned if we want to restart it.
  // We can't just Terminate() it because it is the runtime responsibility to
  // decide, in which order the tasks should be terminated.
  assert(IsReturned());
  for (auto &task : to_destroy) {
    task.Destroy();
  }
}

void StackfulTask::StartFromTheBeginning(void *state) {
  // The task must be returned if we want to restart it.
  // We can't just Terminate() it because it is the runtime responsibility to
  // decide, in which order the tasks should be terminated.
  assert(IsReturned());
  for (auto &task : to_destroy) {
    task.Destroy();
  }
  to_destroy = {};
  if (token != nullptr) {
    token->Reset();
  }
  entrypoint.StartFromTheBeginning(state);
  stack = {entrypoint};
}

void StackfulTask::SetToken(std::shared_ptr<Token> token) {
  this->token = token;
}

bool StackfulTask::IsParked() { return token != nullptr && token->parked; }

void StackfulTask::Terminate() {
  int tries = 0;
  while (!IsReturned()) {
    ++tries;
    Resume();
    assert(tries < 10000000 &&
           "task is spinning too long, possible wrong task terminating order");
  }
}

Invoke::Invoke(const StackfulTask &task, int thread_id)
    : task(task), thread_id(thread_id) {}

Response::Response(const StackfulTask &task, int result, int thread_id)
    : task(task), result(result), thread_id(thread_id) {}
