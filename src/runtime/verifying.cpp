#include "include/verifying.h"

#include <algorithm>

std::string toLower(std::string str) {
  std::transform(str.begin(), str.end(), str.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return str;
}

const std::string kRR = "rr";
const std::string kUniform = "uniform";

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
