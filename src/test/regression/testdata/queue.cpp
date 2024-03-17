#include <atomic>
#include <iostream>

#define na __attribute((__annotate__(("nonatomic"))))

std::atomic<int> queue_array[1000]{};
std::atomic<int> tail{}, head{};

int nxt{};

extern "C" {

void na Push(int v) {
  std::cout << "Push: " << v << std::endl;
  int pos = head.fetch_add(1);
  queue_array[pos].store(v);
}

int na Pop() {
  int last = head.load();
  for (int i = 0; i < last; ++i) {
    int elem = queue_array[i].load();
    if (elem != 0 && queue_array[i].compare_exchange_weak(elem, 0)) {
      return elem;
    }
  }
  return 0;
}

void na test() {
  for (int i = 0; i < 5; ++i) {
    Push(i + 1);
  }
  for (int i = 0; i < 5; ++i) {
    int p = Pop();
    std::cout << "Got: " << p << std::endl;
  }
}
}