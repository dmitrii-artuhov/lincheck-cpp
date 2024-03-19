#include <deque>
#include <iostream>

#include "../runtime/include/lincheck_recursive.h"
#include "../runtime/include/pretty_print.h"
#include "../runtime/include/round_robin_strategy.h"

namespace Register {

struct LinearRegister {
  int x = 0;
  int add() {
    ++x;
    return 0;
  }
  int get() { return x; }
};

struct LinearRegisterHash {
  size_t operator()(const LinearRegister &r) const { return r.x; }
};

struct LinearRegisterEquals {
  bool operator()(const LinearRegister &lhs, const LinearRegister &rhs) const {
    return lhs.x == rhs.x;
  }
};

using method_t =
    std::function<int(LinearRegister *l, const std::vector<int> &args)>;

auto getMethods() {
  method_t add_func = [](LinearRegister *l,
                         const std::vector<int> &args) -> int {
    assert(args.empty());
    return l->add();
  };

  method_t get_func = [](LinearRegister *l,
                         const std::vector<int> &args) -> int {
    assert(args.empty());
    return l->get();
  };

  return std::map<std::string, method_t>{
      {"add", add_func},
      {"get", get_func},
  };
}

};  // namespace Register

namespace Queue {

struct queue {
  std::deque<int> deq{};
  int Push(int v) {
    deq.push_back(v);
    return 0;
  }
  int Pop() {
    if (deq.empty()) return 0;
    int res = deq.front();
    deq.pop_front();
    return res;
  }
};

struct queueHash {
  size_t operator()(const queue &r) const {
    int res = 0;
    for (int elem : r.deq) {
      res += elem;
    }
    return res;
  }
};

struct queueEquals {
  bool operator()(const queue &lhs, const queue &rhs) const {
    return lhs.deq == rhs.deq;
  }
};

using method_t = std::function<int(queue *l, const std::vector<int> &args)>;

auto getMethods() {
  method_t push_func = [](queue *l, const std::vector<int> &args) -> int {
    assert(args.size() == 1);
    return l->Push(args[0]);
  };

  method_t pop_func = [](queue *l, const std::vector<int> &args) -> int {
    assert(args.empty());
    return l->Pop();
  };

  return std::map<std::string, method_t>{
      {"Push", push_func},
      {"Pop", pop_func},
  };
}

};  // namespace Queue

extern "C" void run(TaskBuilderList l, InitFuncList init_funcs) {
  assert(init_funcs != nullptr);
  assert(l != nullptr);
  auto strategy = RoundRobinStrategy{2, l, init_funcs};
#ifdef test_register
  using lchecker_t =
      LinearizabilityCheckerRecursive<Register::LinearRegister,
                                      Register::LinearRegisterHash,
                                      Register::LinearRegisterEquals>;

  lchecker_t checker{Register::getMethods(), Register::LinearRegister{}};
#else
  using lchecker_t =
      LinearizabilityCheckerRecursive<Queue::queue, Queue::queueHash,
                                      Queue::queueEquals>;
  lchecker_t checker{Queue::getMethods(), Queue::queue{}};
#endif

  auto scheduler = Scheduler{strategy, checker, 7, 1};
  auto result = scheduler.Run();
  if (!result.has_value()) {
    std::cout << "success!" << std::endl;
  } else {
    std::cout << "non linarized:" << std::endl;
    pretty_print::pretty_print(result.value().second);
  }
}

extern "C" void print(int x) { std::cout << "printed: " << x << std::endl; }
