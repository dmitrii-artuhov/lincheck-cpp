#include <iostream>

#include "macro.h"

struct Register {
  int x{};
  void add() { ++x; }
  int get() { return x; }
};

Register r{};

extern "C" {

void ini create_register() {
  std::cout << "create_register" << std::endl;
  r = Register{};
}

void ini some_init_routine() { std::cout << "some_init_routine" << std::endl; }

na void add() { r.add(); }

na int get() { return r.get(); }
}
