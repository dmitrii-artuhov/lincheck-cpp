#pragma once
#include <coroutine>
#include <cstdint>

extern "C" {

struct CoroPromise {
  int has_ret_val{};
  int ret_val{};
  std::coroutine_handle<CoroPromise> child_hdl{};
};

// C-style API to make LLVM calls easier.
CoroPromise *get_promise(std::coroutine_handle<CoroPromise> hdl);

void init_promise(CoroPromise *p);

void set_child_hdl(CoroPromise *p, int8_t *hdl);

bool has_child_hdl(CoroPromise *p);

decltype(CoroPromise::child_hdl) get_child_hdl(CoroPromise *p);

void set_ret_val(CoroPromise *p, int ret_val);

bool has_ret_val(CoroPromise *p);

int get_ret_val(CoroPromise *p);
}
