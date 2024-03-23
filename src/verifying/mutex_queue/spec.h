#include <atomic>

#include "../specs/queue.h"

namespace target {

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

const int N = 1000;

struct Queue {
  Queue();
  void Push(int v);
  int Pop();
  void Reconstruct();

  Mutex mutex{};
  int tail{}, head{};
  int a[N]{};
};

};  // namespace target

struct VerifyingSpec {
  using target_t = target::Queue;
  using spec_t = spec::Queue;

  // TODO: auto determine if std::hash is provided.
  using hash_t = spec::QueueHash;
  using equals_t = spec::QueueEquals;
};
