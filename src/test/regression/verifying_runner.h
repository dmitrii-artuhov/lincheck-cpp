#pragma once
#include <algorithm>

#include "../../runtime/include/verifying_macro.h"

template <typename T>
void run() {
  // Just executes each task in lexigraphical order.
  auto conss = std::move(ltest::task_builders);
  std::vector<StackfulTask> tasks;
  T state{};
  for (auto cons : conss) {
    tasks.push_back(StackfulTask{cons(&state)});
  }
  std::sort(tasks.begin(), tasks.end(), [](const auto &t1, const auto &t2) {
    return t1.GetName() < t2.GetName();
  });
  for (auto &task : tasks) {
    while (!task.IsReturned()) {
      task.Resume();
    }
    std::cout << "Returned: " << task.GetRetVal() << std::endl;
  }
}

#define TEST_ENTRYPOINT(obj)                 \
  namespace ltest {                          \
  std::vector<task_builder_t> task_builders; \
  GeneratedArgs gen_args = GeneratedArgs{};  \
  }                                          \
  int main(int argc, char *argv[]) {         \
    run<obj>();                              \
    return 0;                                \
  }\
