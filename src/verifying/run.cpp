#include <deque>
#include <iostream>
#include <memory>

#include "../runtime/include/lincheck_recursive.h"
#include "../runtime/include/logger.h"
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

enum StrategyType { RR, RND };

// ./run <THREADS> <STRATEGY> <TASKS> <ROUNDS> <VERBOSE>
// SPEC = queue | register
// STRATEGY = rr | rnd

std::string toLower(std::string str) {
  std::transform(str.begin(), str.end(), str.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return str;
}

void extract_args(int argc, char *argv[], size_t &threads, StrategyType &typ,
                  size_t &tasks, size_t &rounds, bool &verbose) {
  if (argc < 6) {
    throw std::invalid_argument("all arguments should be specified");
  }
  threads = std::stoul(argv[1]);  // Throws if can't transform.
  std::string strategy_name = argv[2];
  strategy_name = toLower(std::move(strategy_name));
  if (strategy_name == "rr") {
    typ = RR;
  } else if (strategy_name == "rnd") {
    typ = RND;
  } else {
    throw std::invalid_argument("unsupported strategy");
  }
  tasks = std::stoul(argv[3]);
  rounds = std::stoul(argv[4]);
  verbose = std::stoi(argv[5]) == 1;
}

int main(int argc, char *argv[]) {
  size_t threads{};
  size_t tasks{};
  size_t rounds{};
  StrategyType typ{};
  bool verbose{};
  extract_args(argc, argv, threads, typ, tasks, rounds, verbose);

  logger_init(verbose);
  log() << "threads  = " << threads << "\n";
  log() << "tasks    = " << tasks << "\n";
  log() << "rounds   = " << rounds << "\n";

  std::vector<TaskBuilder> l;
  std::vector<init_func_t> init_funcs;
  fill_ctx(&l, &init_funcs);
  if (init_funcs.empty()) {
    std::cout << "WARNING: not found any init funcs, multi-round testing could "
                 "be incorrect\n\n";
  }

  log() << "strategy = ";
  std::unique_ptr<Strategy> strategy;
  switch (typ) {
    case RR:
      log() << "round-robin";
      strategy = std::make_unique<RoundRobinStrategy>(threads, &l, &init_funcs);
      break;
    case RND:
      log() << "random";
      strategy = std::make_unique<RoundRobinStrategy>(threads, &l, &init_funcs);
      break;
  }
  log() << "\n\n";

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
  auto scheduler = Scheduler{*strategy.get(), checker, tasks, rounds};
  auto result = scheduler.Run();
  if (result.has_value()) {
    std::cout << "non linearized:\n";
    pretty_print::pretty_print(result.value().second, std::cout);
  } else {
    std::cout << "success!\n";
  }
}
