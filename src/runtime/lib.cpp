#include "include/lib.h"

#include <cassert>
extern "C" {

CoroPromise *get_promise(std::coroutine_handle<CoroPromise> hdl) {
  return &hdl.promise();
}

void init_promise(CoroPromise *p) {
  p->child_hdl = nullptr;
  p->ret_val = 0;
  p->has_ret_val = 0;
}

decltype(CoroPromise::child_hdl) get_child_hdl(CoroPromise *p) {
  assert(p->child_hdl && "p has not child");
  return p->child_hdl;
}

bool has_child_hdl(CoroPromise *p) { return p->child_hdl != nullptr; }

void set_child_hdl(CoroPromise *p, int8_t *hdl) {
  p->child_hdl = std::coroutine_handle<CoroPromise>::from_address(hdl);
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
