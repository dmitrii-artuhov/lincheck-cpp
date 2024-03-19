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

// ./run <THREADS> <STRATEGY> <TASKS> <ROUNDS>
// SPEC = queue | register
// STRATEGY = rr

void extract_args(int argc, char *argv[], size_t &threads, size_t &tasks,
                  size_t &rounds) {
  if (argc > 1) {
    threads = std::stoul(argv[1]);  // Can throw.
  }
  if (argc > 2) {
    std::string strategy_name = argv[2];
    if (strategy_name != "rr") {
      throw std::invalid_argument("only rr is supported as sched now");
    }
  }
  if (argc > 3) {
    tasks = std::stoul(argv[3]);
  }
  if (argc > 4) {
    rounds = std::stoul(argv[4]);
  }
}

int main(int argc, char *argv[]) {
  size_t threads = 2;
  size_t tasks = 10;
  size_t rounds = 5;
  extract_args(argc, argv, threads, tasks, rounds);

  std::vector<TaskBuilder> l;
  std::vector<init_func_t> init_funcs;
  fill_ctx(&l, &init_funcs);
  if (init_funcs.empty()) {
    std::cout << "WARNING: not found any init funcs, multi-round testing could "
                 "be incorrect\n\n";
  }
  auto strategy = RoundRobinStrategy{threads, &l, &init_funcs};

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
  auto scheduler = Scheduler{strategy, checker, tasks, rounds};
  auto result = scheduler.Run();
  if (result.has_value()) {
    std::cout << "non linearized:\n";
    pretty_print::pretty_print(result.value().second);
  } else {
    std::cout << "success!\n";
  }
}
