#include <cassert>
#include <deque>
#include <functional>
#include <map>

#include "../../runtime/include/verifying.h"

namespace spec {

struct Queue;

using method_t = std::function<int(Queue *l, const std::vector<int> &args)>;

const int size = 2;

struct Queue {
  std::deque<int> deq{};
  int Push(int v) {
    if (deq.size() >= size) {
      return 0;
    }
    deq.push_back(v);
    return 1;
  }
  int Pop() {
    if (deq.empty()) return 0;
    int res = deq.front();
    deq.pop_front();
    return res;
  }
  static auto GetMethods() {
    method_t push_func = [](Queue *l, const std::vector<int> &args) -> int {
      assert(args.size() == 1);
      return l->Push(args[0]);
    };

    method_t pop_func = [](Queue *l, const std::vector<int> &args) -> int {
      assert(args.empty());
      return l->Pop();
    };

    return std::map<std::string, method_t>{
        {"Push", push_func},
        {"Pop", pop_func},
    };
  }
};

struct QueueHash {
  size_t operator()(const Queue &r) const {
    int res = 0;
    for (int elem : r.deq) {
      res += elem;
    }
    return res;
  }
};

struct QueueEquals {
  bool operator()(const Queue &lhs, const Queue &rhs) const {
    return lhs.deq == rhs.deq;
  }
};

}  // namespace spec
