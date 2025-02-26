#include <cassert>
#include <deque>
#include <functional>
#include <map>

#include "../../runtime/include/verifying.h"

namespace spec {

template <typename PushArgTuple = std::tuple<int>, std::size_t ValueIndex = 0>
struct Stack {
  std::deque<int> deq{};
  int Push(int v) {
    deq.push_back(v);
    return 0;
  }
  int Pop() {
    if (deq.empty()) return 0;
    int res = deq.back();
    deq.pop_back();
    return res;
  }

  using method_t = std::function<int(Stack *l, void *args)>;
  static auto GetMethods() {
    method_t push_func = [](Stack *l, void *args) -> int {
      auto real_args = reinterpret_cast<PushArgTuple *>(args);
      return l->Push(std::get<ValueIndex>(*real_args));
    };

    method_t pop_func = [](Stack *l, void *args) -> int { return l->Pop(); };

    return std::map<std::string, method_t>{
        {"Push", push_func},
        {"Pop", pop_func},
    };
  }
};

template <typename StackCls = Stack<>>
struct StackHash {
  size_t operator()(const StackCls &r) const {
    int res = 0;
    for (int elem : r.deq) {
      res += elem;
    }
    return res;
  }  // namespace spec
};

template <typename StackCls = Stack<>>
struct StackEquals {
  template <typename PushArgTuple, int ValueIndex>
  bool operator()(const StackCls &lhs, const StackCls &rhs) const {
    return lhs.deq == rhs.deq;
  }
};

}  // namespace spec
