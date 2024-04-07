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

// ------------------------ STACKFUL TASK ------------------------

StackfulTask::StackfulTask(Task task) : entrypoint(task) {
  stack = std::vector<Task>{task};
}

StackfulTask::StackfulTask() : entrypoint(nullptr) {}

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
  for (int i = static_cast<int>(stack.size()) - 1; i > -1; i--) {
    stack[i].ClearChild();
  }
}

void StackfulTask::StartFromTheBeginning(void *state) {
  entrypoint.StartFromTheBeginning(state);
  for (int i = static_cast<int>(stack.size()) - 1; i > -1; i--) {
    stack[i].ClearChild();
  }
  stack = {entrypoint};
}

Invoke::Invoke(const StackfulTask &task, int thread_id)
    : task(task), thread_id(thread_id) {}

Response::Response(const StackfulTask &task, int result, int thread_id)
    : task(task), result(result), thread_id(thread_id) {}
