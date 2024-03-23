#include <deque>
#include <iostream>
#include <memory>

#include "include/lincheck_recursive.h"
#include "include/logger.h"
#include "include/pretty_print.h"
#include "include/round_robin_strategy.h"
#include "include/uniform_strategy.h"

#ifndef CLI_BUILD
// Debugging purposes.
// Will be replace by build script.
#include "include/dumb_spec.h"
#endif

enum StrategyType { RR, UNIFORM };

const std::string kRR = "rr";
const std::string kUniform = "uniform";

// ./run <THREADS> <STRATEGY> <TASKS> <ROUNDS> <VERBOSE>

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
  if (strategy_name == kRR) {
    typ = RR;
  } else if (strategy_name == kUniform) {
    typ = UNIFORM;
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
  fill_ctx(&l);

  using spec = VerifyingSpec;
  log() << "strategy = ";

  std::unique_ptr<Strategy> strategy;
  switch (typ) {
    case RR:
      log() << "round-robin";
      strategy =
          std::make_unique<RoundRobinStrategy<spec::target_t>>(threads, &l);
      break;
    case UNIFORM:
      log() << "uniform";
      strategy = std::make_unique<UniformStrategy<spec::target_t>>(threads, &l);
      break;
  }
  log() << "\n\n";

  using lchecker_t = LinearizabilityCheckerRecursive<spec::spec_t, spec::hash_t,
                                                     spec::equals_t>;
  lchecker_t checker{spec::spec_t::GetMethods(), spec::spec_t{}};
  auto scheduler = Scheduler{*strategy.get(), checker, tasks, rounds};
  auto result = scheduler.Run();
  if (result.has_value()) {
    std::cout << "non linearized:\n";
    pretty_print::pretty_print(result.value().second, std::cout);
  } else {
    std::cout << "success!\n";
  }
}
