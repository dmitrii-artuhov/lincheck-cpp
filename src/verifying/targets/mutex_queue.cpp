#include <atomic>

#include "../specs/queue.h"

const int N = 10'000;

struct Mutex {
  std::atomic<bool> flg{};

  void lock() {
    while (true) {
      bool busy = flg.load();
      if (busy) {
        continue;
      }
      if (flg.compare_exchange_weak(busy, true)) {
        break;
      }
    }
  }

  void unlock() {
    bool tru = true;
    flg.compare_exchange_weak(tru, false);
  }
};

struct Queue {
  Queue() {}
  void Reconstruct() {
    mutex.flg.store(false);
    tail = 0;
    head = 0;
    std::fill(a, a + N, 0);
  }

  void Push(int v);
  int Pop();

  Mutex mutex{};
  int tail{}, head{};
  int a[N]{};
};

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

using spec_t = Spec<Queue, spec::Queue, spec::QueueHash, spec::QueueEquals>;

LTEST_ENTRYPOINT(spec_t);
