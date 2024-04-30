/**
 * ./verify.py build --src ./targets/mutex_queue.cpp
 * ./verify.py run --tasks 4 --switches 1 --rounds 100000 --strategy tla
*/
#include <atomic>
#include <cstring>

#include "../lib/mutex.h"
#include "../specs/queue.h"

const int N = 100;

struct Queue {
  void Push(std::shared_ptr<Token> token, int v);
  int Pop(std::shared_ptr<Token> token);

  void Reset() {
    mutex = Mutex{};
    tail = head = 0;
    std::fill(a, a + N, 0);
  }

  Mutex mutex{};
  int tail{}, head{};
  int a[N]{};
};

namespace ltest {}  // namespace ltest

auto generate_int() {
  return ltest::generators::make_single_arg(rand() % 10 + 1);
}

auto generate_args() {
  auto token = ltest::generators::gen_token();
  auto _int = generate_int();
  return std::tuple_cat(token, _int);
}

target_method(generate_args, void, Queue, Push, std::shared_ptr<Token> token,
              int v) {
  mutex.Lock(token);
  coro_yield();
  a[head++] = v;
  mutex.Unlock();
}

target_method(ltest::generators::gen_token, int, Queue, Pop,
              std::shared_ptr<Token> token) {
  mutex.Lock(token);
  int e = 0;
  if (head - tail > 0) {
    e = a[tail++];
  }
  mutex.Unlock();
  return e;
}

using QueueCls = spec::Queue<std::tuple<std::shared_ptr<Token>, int>, 1>;

using spec_t = ltest::Spec<Queue, QueueCls, spec::QueueHash<QueueCls>,
                           spec::QueueEquals<QueueCls>>;

LTEST_ENTRYPOINT(spec_t);
