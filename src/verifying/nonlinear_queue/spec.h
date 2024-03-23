#include <atomic>

#include "../specs/queue.h"

namespace target {

const int N = 10'000;

struct Queue {
  Queue();
  void Push(int v);
  int Pop();
  void Reconstruct();

  std::atomic<int> a[N];
  std::atomic<int> head{};
};

};  // namespace target

struct VerifyingSpec {
  using target_t = target::Queue;
  using spec_t = spec::Queue;

  // TODO: auto determine if std::hash is provided.
  using hash_t = spec::QueueHash;
  using equals_t = spec::QueueEquals;
};
