#include <cassert>

#include "../../../runtime/include/generators.h"
#include "../../../runtime/include/verifying_macro.h"

non_atomic void some_method(int i) {
  std::cout << "some_method(" << i << ")" << std::endl;
}

struct Test {
  void method();
};

target_method(ltest::generators::genEmpty, void, Test, method) {
  for (int i = 0; i < 3; ++i) {
    some_method(i);
  }
}

namespace ltest {
std::vector<TaskBuilder> task_builders;
GeneratedArgs gen_args = GeneratedArgs{};
}  // namespace ltest

int main() {
  Test tst{};
  auto cons = ltest::task_builders[0];
  auto task = StackfulTask{cons, &tst};
  int cnt = 0;
  while (!task.IsReturned()) {
    ++cnt;
    task.Resume();
  }
  task.StartFromTheBeginning(&tst);
  while (!task.IsReturned()) {
    --cnt;
    task.Resume();
  }
  // Ensure that if we restart the task,
  // the number of steps are needed to terminate it remains unchanged.
  assert(cnt == 0);
  return 0;
}
