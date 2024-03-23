#include <atomic>
#include <iostream>

#include "../../../runtime/include/verifying.h"

struct Queue {
  std::atomic<int> queue_array[1000]{};
  std::atomic<int> tail{}, head{};

  void Log(const std::string &msg) { std::cout << msg << std::endl; }

  non_atomic void Push(int v) {
    Log("Push: " + std::to_string(v));
    int pos = head.fetch_add(1);
    queue_array[pos].store(v);
  }

  non_atomic int Pop() {
    int last = head.load();
    for (int i = 0; i < last; ++i) {
      int elem = queue_array[i].load();
      if (elem != 0 && queue_array[i].compare_exchange_weak(elem, 0)) {
        return elem;
      }
    }
    return 0;
  }
};

struct State {
  void test();
};

TARGET_METHOD(void, State, test, ()) {
  Queue q{};
  for (int i = 0; i < 5; ++i) {
    q.Push(i + 1);
  }
  for (int i = 0; i < 5; ++i) {
    int p = q.Pop();
    std::cout << "Got: " << p << std::endl;
  }
}
