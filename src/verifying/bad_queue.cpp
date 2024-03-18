
#include <atomic>
#include <cstdlib>

#include "macro.h"

int gen next_int() { return rand() % 5 + 1; }

const int N = 10'000;
std::atomic<int> queue_array[N]{};
std::atomic<int> head{};

extern "C" {

void na Push(int v) {
  int pos = head.fetch_add(1);
  queue_array[pos] = v;
}

int na Pop() {
  int last = head.load();
  for (int i = 0; i < last; ++i) {
    int e = queue_array[i];
    if (e == 0) {
      continue;
    }
    if (queue_array[i].compare_exchange_weak(e, 0)) {
      return e;
    }
  }
  return 0;
}
}
