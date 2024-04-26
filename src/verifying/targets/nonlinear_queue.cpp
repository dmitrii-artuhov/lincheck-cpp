#include <atomic>
#include <cstring>
#include <iostream>

#include "../specs/queue.h"

const int N = 100;

// Implementation.
struct Queue {
  Queue() {}

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
  int Pop(void);

  std::atomic<int> a[N];
  std::atomic<int> head{};
};

// Arguments generator.
auto generate_int() {
  return ltest::generators::make_single_arg(rand() % 10 + 1);
}

// Targets.
target_method(generate_int, void, Queue, Push, int v) {
  int pos = head.fetch_add(1);
  a[pos] = v;
}

target_method(ltest::generators::empty_gen, int, Queue, Pop) {
  int last = head.load();
  for (int i = 0; i < last; ++i) {
    int e = a[i].load();
    if (e != 0 && a[i].compare_exchange_strong(e, 0)) {
      return e;
    }
  }
  return 0;
}

// Specify target structure and it's sequential specification.
using spec_t =
    ltest::Spec<Queue, spec::Queue, spec::QueueHash, spec::QueueEquals>;

LTEST_ENTRYPOINT(spec_t);
