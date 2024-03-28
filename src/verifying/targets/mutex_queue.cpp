#include <atomic>
#include <cstring>

#include "../specs/queue.h"

const int N = 100;

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

  Queue(const Queue &oth) {
    // Reinit mutex unconditionally.
    mutex.flg.store(false);
    tail = oth.tail;
    head = oth.head;
    std::memcpy(a, oth.a, N);
  }
  Queue &operator=(const Queue &oth) {
    // Reinit mutex unconditionally.
    mutex.flg.store(false);
    tail = oth.tail;
    head = oth.head;
    std::memcpy(a, oth.a, N);
    return *this;
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

using spec_t =
    ltest::Spec<Queue, spec::Queue, spec::QueueHash, spec::QueueEquals>;

LTEST_ENTRYPOINT(spec_t);
