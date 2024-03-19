#include <atomic>

#include "macro.h"

struct AtomicRegister {
  std::atomic<int> x{};

  void add() { x.fetch_add(1); }
  int get() { return x.load(); }
};

AtomicRegister r{};

void ini zero_register() { r.x.store(0); }

extern "C" {

na void add() { r.add(); }

na int get() { return r.get(); }
}
