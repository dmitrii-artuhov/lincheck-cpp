#include "spec.h"

namespace target {

Queue::Queue() {}

void Queue::Reconstruct() {
  head.store(0);
  std::fill(a, a + N, 0);
}

generator int gen_int() { return rand() % 10 + 1; }

TARGET_METHOD(void, Queue, Push, (int v)) {
  int pos = head.fetch_add(1);
  a[pos] = v;
}

TARGET_METHOD(int, Queue, Pop, ()) {
  int last = head.load();
  for (int i = 0; i < last; ++i) {
    int e = a[i].load();
    if (e != 0 && a[i].compare_exchange_weak(e, 0)) {
      return e;
    }
  }
  return 0;
}

}  // namespace target
