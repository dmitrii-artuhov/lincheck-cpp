#include <cstring>

#include "spec.h"

namespace target {

Queue::Queue() {}

void Queue::Reconstruct() {
  mutex.flg.store(false);
  tail = 0;
  head = 0;
  std::fill(a, a + N, 0);
}

generator int next_int() { return rand() % 5 + 1; }

TARGET_METHOD(void, Queue, Push, (int v)) {
  mutex.lock();
  a[head++] = v;
  mutex.unlock();
}

TARGET_METHOD(int, Queue, Pop, ()) {
  mutex.lock();
  int e = 0;
  if (head - tail > 0) {
    e = a[tail++];
  }
  mutex.unlock();
  return e;
}

}  // namespace target
