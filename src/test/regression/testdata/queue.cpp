#include <atomic>
#include <iostream>

#include "../../../runtime/include/lib.h"
#include "../../../runtime/include/verifying.h"

struct Queue {
  std::atomic<int> queue_array[1000]{};
  std::atomic<int> tail{}, head{};

  void Log(const std::string &msg) { std::cout << msg << std::endl; }

  non_atomic void Push(int v) {
    Log("Push: " + std::to_string(v));
    CoroYield();
    int pos = head.fetch_add(1);
    CoroYield();
    queue_array[pos].store(v);
    CoroYield();
  }

  non_atomic int Pop() {
    int last = head.load();
    CoroYield();
    for (int i = 0; i < last; ++i) {
      int elem = queue_array[i].load();
      CoroYield();
      if (elem != 0 && queue_array[i].compare_exchange_weak(elem, 0)) {
        CoroYield();
        return elem;
      }
    }
    return 0;
  }
};

non_atomic attr(ltesttarget_test) void test() {
  Queue q{};
  for (int i = 0; i < 5; ++i) {
    CoroYield();
    q.Push(i + 1);
    CoroYield();
  }
  for (int i = 0; i < 5; ++i) {
    int p = q.Pop();
    CoroYield();
    std::cout << "Got: " << p << std::endl;
    CoroYield();
  }
}
