#include <iostream>

#include "include/lib.h"

extern "C" {

// Testing entrypoint.
//
// TaskBuilderList created by LLVM and passed here.
// Caller is responsible to clear l.
void run(TaskBuilderList l) {
  std::cout << "got list with " << l->size() << " tasks:" << std::endl;
  for (auto task_builder : *l) {
    auto task = task_builder();
    std::cout << task.GetName() << std::endl;
  }
  // TODO: linearizable checks.
}
}