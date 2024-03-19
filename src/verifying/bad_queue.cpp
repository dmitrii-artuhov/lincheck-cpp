
#include <atomic>
#include <cstdlib>

#include "macro.h"

int gen next_int() { return rand() % 5 + 1; }

const int N = 10'000;

struct Queue {
  std::atomic<int> a[N];
  std::atomic<int> head{};

  na void Push(int v) {
    int pos = head.fetch_add(1);
    a[pos] = v;
  }

  na int Pop() {
    int last = head.load();
    for (int i = 0; i < last; ++i) {
      int e = a[i].load();
      if (e != 0 && a[i].compare_exchange_weak(e, 0)) {
        return e;
      }
    }
    return 0;
  }
};

Queue q{};

ini void clear_queue() {
  for (int i = 0; i < q.head; ++i) {
    q.a[i].store(0);
  }
  q.head.store(0);
}

extern "C" {
na void Push(int v) { q.Push(v); }
na int Pop() { return q.Pop(); }
}
