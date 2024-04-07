#include <atomic>
#include <iostream>

#include "../../../runtime/include/verifying.h"
#include "../../../runtime/include/lib.h"

struct Queue {
  std::atomic<int> queue_array[1000]{};
  std::atomic<int> tail{}, head{};

  void Log(const std::string &msg) { std::cout << msg << std::endl; }

  non_atomic void Push(int v) {
    Log("Push: " + std::to_string(v));\
    coro_yield();
    int pos = head.fetch_add(1);
    coro_yield();
    queue_array[pos].store(v);
    coro_yield();
  }

  non_atomic int Pop() {
    int last = head.load();
    coro_yield();
    for (int i = 0; i < last; ++i) {
      int elem = queue_array[i].load();
      coro_yield();
      if (elem != 0 && queue_array[i].compare_exchange_weak(elem, 0)) {
        coro_yield();
        return elem;
      }
    }
    return 0;
  }
};

non_atomic attr(ltesttarget_test) void test() {
  Queue q{};
  for (int i = 0; i < 5; ++i) {
    coro_yield();
    q.Push(i + 1);
    coro_yield();
  }
  for (int i = 0; i < 5; ++i) {
    int p = q.Pop();
    coro_yield();
    std::cout << "Got: " << p << std::endl;
    coro_yield();
  }
}
