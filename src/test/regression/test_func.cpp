#include <iostream>
#include <vector>

#include "../../runtime/include/lib.h"

extern "C" {

int var{};
void tick() { ++var; }

// This function runs task by handler until it and all children are terminated.
void test_func(int8_t *hdl_addr) {
  auto root_hdl = std::coroutine_handle<CoroPromise>::from_address(hdl_addr);
  // Keep stack that contains launched functions.
  std::vector<decltype(root_hdl)> stack{root_hdl};

  while (stack.size()) {
    auto hdl = stack.back();
    auto &promise = hdl.promise();
    if (has_ret_val(&promise)) {
      auto ret_val = get_ret_val(&promise);
      std::cout << "returned " << ret_val << std::endl;
      auto ret = get_ret_val(&promise);
      stack.pop_back();
      if (!stack.empty()) {
        set_child_hdl(&stack.back().promise(), nullptr);
      }
    } else {
      hdl.resume();
      std::cout << var << std::endl;
      if (has_child_hdl(&promise)) {
        // Coroutine has gave birth to a child.
        stack.push_back(get_child_hdl(&promise));
      }
    }
  }
}
}