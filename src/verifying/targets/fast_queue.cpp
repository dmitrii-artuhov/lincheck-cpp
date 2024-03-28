#include <atomic>
#include <iostream>
#include <vector>

#include "../specs/bounded_queue.h"

template <typename T>
struct alignas(512) Node {
  std::atomic<size_t> generation;
  T val;

  Node() : generation{} {}

  Node(const std::atomic<T> &a) : generation(a.load()) {}

  Node(const Node &other) : generation(other.generation.load()) {}

  Node &operator=(const Node &other) {
    generation.store(other.generation.load());
    return *this;
  }
};

const int size = 2;

generator int next_int() { return rand() % 5 + 1; }

class MPMCBoundedQueue {
 public:
  explicit MPMCBoundedQueue() : max_size_{size - 1} {
    vec_.resize(size);
    for (size_t i = 0; i < size; ++i) {
      vec_[i].generation.store(i, std::memory_order_relaxed);
    }
  }

  MPMCBoundedQueue(const MPMCBoundedQueue &oth) : max_size_{oth.max_size_} {
    vec_ = oth.vec_;
    head_.store(oth.head_.load());
    tail_.store(oth.tail_.load());
  }

  MPMCBoundedQueue &operator=(const MPMCBoundedQueue &oth) {
    max_size_ = oth.max_size_;
    vec_ = oth.vec_;
    head_.store(oth.head_.load());
    tail_.store(oth.tail_.load());
    return *this;
  }

  int Push(int value);

  int Pop();

 private:
  size_t max_size_;
  std::vector<Node<int>> vec_;

  std::atomic<size_t> head_{};
  std::atomic<size_t> tail_{};
};

TARGET_METHOD(int, MPMCBoundedQueue, Push, (int value)) {
  while (true) {
    auto h = head_.load(/*std::memory_order_relaxed*/);
    auto hid = h & max_size_;
    auto gen = vec_[hid].generation.load(/*std::memory_order_relaxed*/);
    if (gen == h) {
      if (head_.compare_exchange_weak(h,
                                      h + 1 /*, std::memory_order_acquire*/)) {
        // I am owner of the element.
        vec_[hid].val = value;
        vec_[hid].generation.fetch_add(1 /*, std::memory_order_release*/);
        return true;
      }
    } else if (gen < h) {
      return false;
    }
  }
}

TARGET_METHOD(int, MPMCBoundedQueue, Pop, ()) {
  while (true) {
    auto t = tail_.load(/*std::memory_order_relaxed*/);
    auto tid = t & max_size_;
    auto gen = vec_[tid].generation.load(/*std::memory_order_relaxed*/);
    if (gen == t + 1) {
      if (tail_.compare_exchange_weak(t,
                                      t + 1 /*, std::memory_order_acquire*/)) {
        int ret = std::move(vec_[tid].val);
        vec_[tid].generation.fetch_add(
            max_size_ /*, std::memory_order_release*/);
        return ret;
      }
    } else {
      if (gen < t + 1) {
        return 0;
      }
    }
  }
}

using spec_t = ltest::Spec<MPMCBoundedQueue, spec::Queue, spec::QueueHash,
                           spec::QueueEquals>;

LTEST_ENTRYPOINT(spec_t);
