#include <coroutine>
#include <cstdint>
#include <optional>

extern "C" {

struct CoroPromise {
  std::optional<int> ret_val{};
  int child_return{};
  std::coroutine_handle<CoroPromise> child_hdl{};
};

void set_child_hdl(CoroPromise *p, int8_t *hdl) {
  p->child_hdl = std::coroutine_handle<CoroPromise>::from_address(hdl);
}

void set_ret_val(CoroPromise *p, int ret_val) { p->ret_val = ret_val; }

int get_ret_val(CoroPromise *p) { return p->ret_val.value(); }
}
