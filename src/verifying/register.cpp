#include "macro.h"

struct Register {
  int x{};
  void add() { ++x; }
  int get() { return x; }
};

Register r{};

extern "C" {

void ini create_register() { r = Register{}; }

na void add() { r.add(); }

na int get() { return r.get(); }
}
