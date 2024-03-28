#include <atomic>
#include <cstring>
#include <iostream>

#include "../specs/queue.h"

const int N = 100;

struct Queue {
  Queue() {}
  void Reconstruct() {
    head.store(0);
    std::fill(a, a + N, 0);
  }

  Queue &operator=(const Queue &oth) {
    head.store(oth.head.load());
    std::memcpy(a, oth.a, N);
    return *this;
  }

  Queue(const Queue &oth) {
    head.store(oth.head.load());
    std::memcpy(a, oth.a, N);
  }

  void Push(int v);
  int Pop();

  std::atomic<int> a[N];
  std::atomic<int> head{};
};

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

using spec_t =
    ltest::Spec<Queue, spec::Queue, spec::QueueHash, spec::QueueEquals>;

LTEST_ENTRYPOINT(spec_t);
