#include "include/lib.h"

#include <cassert>
extern "C" {

void init_promise(CoroPromise *p) {
  p->has_child = 0;
  p->child_hdl = nullptr;
  p->ret_val = 0;
  p->has_ret_val = 0;
  p->child_return = -1;
}

decltype(CoroPromise::child_hdl) get_child_hdl(CoroPromise *p) {
  assert(p->has_child && "p has not child");
  return p->child_hdl;
}

bool has_child_hdl(CoroPromise *p) { return p->has_child; }

void set_child_hdl(CoroPromise *p, int8_t *hdl) {
  p->has_child = 1;
  p->child_hdl = std::coroutine_handle<CoroPromise>::from_address(hdl);
}

void set_child_ret(CoroPromise *p, int ret) {
  p->child_return = ret;
  p->has_child = 0;
  p->child_hdl = nullptr;
  // TODO: memory leaks?
}

int get_child_ret(CoroPromise *p) {
  assert(p->has_child == 0 && "child has not returned yet");
  return p->child_return;
}

void set_ret_val(CoroPromise *p, int ret_val) {
  p->has_ret_val = 1;
  p->ret_val = ret_val;
}

bool has_ret_val(CoroPromise *p) { return p->has_ret_val; }

int get_ret_val(CoroPromise *p) {
  assert(p->has_ret_val && "promise has not ret val");
  return p->ret_val;
}
}
