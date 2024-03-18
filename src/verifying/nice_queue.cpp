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

std::atomic<int> queue_array[1000];

int tail{}, head{};
Mutex mutex{};

extern "C" {

void na Push(int v) {
  mutex.lock();
  queue_array[head++] = v;
  mutex.unlock();
}

int na Pop() {
  mutex.lock();
  int res = 0;
  if (head - tail > 0) {
    res = queue_array[tail++];
  }
  mutex.unlock();
  return res;
}
}
