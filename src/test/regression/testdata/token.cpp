#include <cassert>
#include <iostream>

#include "../../../runtime/include/generators.h"
#include "../../../runtime/include/verifying_macro.h"

void set_parked(std::shared_ptr<Token> token, bool val) { token->parked = val; }

struct Test {
  bool parked_set{};

  void TestParked(std::shared_ptr<Token> token);
};

target_method(ltest::generators::gen_token, void, Test, TestParked,
              std::shared_ptr<Token> token) {
  set_parked(token, true);
  parked_set = true;
  coro_yield();

  set_parked(token, false);
  parked_set = false;
  coro_yield();
}

namespace ltest {
std::vector<task_builder_t> task_builders;
GeneratedArgs gen_args = GeneratedArgs{};
}  // namespace ltest

int main() {
  Test tst{};
  auto cons = ltest::task_builders[0];
  auto task = StackfulTask{cons, &tst};
  while (!tst.parked_set) {
    task.Resume();
  }
  assert(task.IsParked());
  while (tst.parked_set) {
    task.Resume();
  }
  assert(!task.IsParked());
  task.Terminate();
  std::cout << "success!" << std::endl;
  return 0;
}
