#pragma once
#include <coroutine>
#include <cstdint>

extern "C" {

struct CoroPromise {
  int has_ret_val{};
  int ret_val{};
  int child_return{};
  int has_child{};
  std::coroutine_handle<CoroPromise> child_hdl{};
};

// C-style API to make LLVM calls easier.
void init_promise(CoroPromise *p);

void set_child_ret(CoroPromise *p, int ret);

int get_child_ret(CoroPromise *p);

void set_child_hdl(CoroPromise *p, int8_t *hdl);

bool has_child_hdl(CoroPromise *p);

decltype(CoroPromise::child_hdl) get_child_hdl(CoroPromise *p);

void set_ret_val(CoroPromise *p, int ret_val);

bool has_ret_val(CoroPromise *p);

int get_ret_val(CoroPromise *p);
}
