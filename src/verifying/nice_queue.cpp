#include <atomic>
#include <cstdlib>
#include <deque>

#include "macro.h"

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

int gen next_int() { return rand() % 5 + 1; }

struct Queue {
  Mutex mutex{};
  int tail{}, head{};
  int a[1000];

  na void Push(int v) {
    mutex.lock();
    a[head++] = v;
    mutex.unlock();
  }

  na int Pop() {
    mutex.lock();
    int e = 0;
    if (head - tail > 0) {
      e = a[tail++];
    }
    mutex.unlock();
    return e;
  }
};

Queue q{};

ini void clear_queue() {
  q.mutex.flg.store(0);
  q.tail = q.head = 0;
}

extern "C" {

na void Push(int v) { q.Push(v); }

na int Pop() { return q.Pop(); }
}
