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

Handle get_child_hdl(CoroPromise *p) {
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

void set_suspension_points(CoroPromise *p, int32_t suspension_points) {
  assert(p != nullptr);
  // TODO: delete after debug
  //  std::cout << "set suspension_points " << *suspension_points << std::endl;
  p->suspension_points = suspension_points;
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

void CoroYield() noexcept {}
}

StackfulTask *current_task;
StackfulTask *GetCurrentTask() {
  assert(current_task != nullptr);
  return current_task;
}

void SetCurrentTask(StackfulTask *task) { current_task = task; }

void Token::Reset() { parked = false; }

// ------------------------------ TASK ----------------------------------------

Task::Task(Handle hdl)
    : hdl(hdl), suspension_points(hdl.promise().suspension_points) {}

Task::Task(Handle hdl, TaskCloner cloner)
    : hdl{hdl},
      suspension_points(hdl.promise().suspension_points),
      cloner{cloner} {}

Task::Task(Task &&oth) {
  hdl = oth.hdl;
  oth.hdl = nullptr;
  meta = oth.meta;
  cloner = oth.cloner;
}

Task &Task::operator=(Task &&oth) {
  std::swap(hdl, oth.hdl);
  std::swap(meta, oth.meta);
  std::swap(cloner, oth.cloner);
  return *this;
}

void Task::SetMeta(std::shared_ptr<Meta> meta) { this->meta = meta; }

void Task::Resume() {
  assert(!IsReturned() && "returned task can not be resumed");
  assert(!has_child_hdl(&hdl.promise()));
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

Task Task::StartFromTheBeginning(void *state) {
  assert(meta);
  auto hdl = cloner(state, meta->args.get());
  auto new_task = Task{hdl, cloner};
  new_task.SetMeta(meta);
  return new_task;
}

const std::string &Task::GetName() const {
  assert(meta);
  return meta->name;
}

size_t Task::GetSuspensionPoints() const { return suspension_points; }

void *Task::GetArgs() const {
  assert(meta);
  return meta->args.get();
}

const std::vector<std::string> &Task::GetStrArgs() const {
  assert(meta);
  return meta->str_args;
}

Task::~Task() {
  if (hdl != nullptr) {
    hdl.destroy();
    hdl = nullptr;
  }
}

// ------------------------ STACKFUL TASK ------------------------

StackfulTask::StackfulTask(TaskBuilder builder, void *this_state) {
  SetCurrentTask(this);
  // Builder could call generator that set token.
  spawned_tasks.emplace_back(builder(this_state));
  SetCurrentTask(nullptr);
  stack = {spawned_tasks[0]};
}

StackfulTask::StackfulTask() {}

StackfulTask::StackfulTask(Task raw_task) {
  spawned_tasks.emplace_back(std::move(raw_task));
  stack = {spawned_tasks[0]};
}

StackfulTask::StackfulTask(StackfulTask &&oth) {
  spawned_tasks = std::move(oth.spawned_tasks);
  stack = std::move(oth.stack);
  token = std::move(oth.token);
  last_returned_value = oth.last_returned_value;
}

void StackfulTask::Resume() {
  assert(!stack.empty());
  Task &stack_head = stack.back();
  stack_head.Resume();

  if (stack_head.HasChild()) {
    // new child was forked
    spawned_tasks.emplace_back(stack_head.GetChild());
    stack.push_back(spawned_tasks.back());
  } else if (stack_head.IsReturned()) {
    // stack_head returned
    last_returned_value = stack_head.GetRetVal();
    stack.pop_back();

    // if it wasn't the first task clean up children
    if (!stack.empty()) {
      stack.back().get().ClearChild();
    }
  }
}

const std::string &StackfulTask::GetName() const {
  return spawned_tasks[0].GetName();
}

void *StackfulTask::GetArgs() const { return spawned_tasks[0].GetArgs(); }

const std::vector<std::string> &StackfulTask::GetStrArgs() const {
  return spawned_tasks[0].GetStrArgs();
}

bool StackfulTask::IsReturned() { return stack.empty(); }

// TODO: implement this
bool StackfulTask::IsSuspended() const { return false; }
// TODO: implement this
bool StackfulTask::IsBlocking() const { return false; }

int StackfulTask::GetRetVal() const { return last_returned_value; }

size_t StackfulTask::GetSuspensionPoints() const {
  assert(!stack.empty());
  return stack[0].get().GetSuspensionPoints();
}

const StackfulTask &Invoke::GetTask() const { return this->task.get(); }

const StackfulTask &Response::GetTask() const { return this->task.get(); }

StackfulTask::~StackfulTask() {
  // The task must be returned if we want to restart it.
  // We can't just Terminate() it because it is the runtime responsibility to
  // decide, in which order the tasks should be terminated.
  if (!IsReturned()) {
    std::cout << "not returned" << std::endl;
  }
  assert(IsReturned());
}

void StackfulTask::StartFromTheBeginning(void *state) {
  // The task must be returned if we want to restart it.
  // We can't just Terminate() it because it is the runtime responsibility to
  // decide, in which order the tasks should be terminated.
  assert(IsReturned());
  // Remove all tasks except entrypoint.
  while (spawned_tasks.size() > 1) {
    spawned_tasks.pop_back();
  }
  if (token != nullptr) {
    token->Reset();
  }
  spawned_tasks[0] = spawned_tasks[0].StartFromTheBeginning(state);
  stack = {spawned_tasks[0]};
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
