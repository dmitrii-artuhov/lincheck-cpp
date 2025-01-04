#include "verifying.h"

#include <gflags/gflags.h>

#include <algorithm>
#include <stdexcept>

namespace ltest {

template <>
std::string toString<int>(const int &a) {
  return std::to_string(a);
}

template <>
std::string toString<std::shared_ptr<Token>>(
    const std::shared_ptr<Token> &token) {
  return "token";
}

std::string toLower(std::string str) {
  std::transform(str.begin(), str.end(), str.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return str;
}

std::vector<std::string> split(const std::string &s, char delim) {
  std::vector<std::string> res{""};
  for (char c : s) {
    if (c == delim) {
      res.push_back("");
    } else {
      res.back() += c;
    }
  }
  return res;
}

constexpr const char *GetLiteral(StrategyType t) {
  switch (t) {
    case RR:
      return "rr";
    case RND:
      return "random";
    case TLA:
      return "tla";
    case PCT:
      return "pct";
  }
}

StrategyType FromLiteral(std::string &&a) {
  if (a == GetLiteral(StrategyType::PCT)) {
    return StrategyType::PCT;
  } else if (a == GetLiteral(StrategyType::RND)) {
    return StrategyType::RND;
  } else if (a == GetLiteral(StrategyType::RR)) {
    return StrategyType::RR;
  } else if (a == GetLiteral(StrategyType::TLA)) {
    return StrategyType::TLA;
  } else {
    throw std::invalid_argument(a);
  }
}

DEFINE_int32(threads, 2, "Number of threads");
DEFINE_int32(tasks, 15, "Number of tasks");
DEFINE_int32(switches, 100000000, "Number of switches");
DEFINE_int32(rounds, 5, "Number of rounds");
DEFINE_int32(minimization_runs, 10, "Number of runs during round minimization");
DEFINE_bool(verbose, false, "Verbosity");
DEFINE_string(strategy, GetLiteral(StrategyType::RR), "Strategy");
DEFINE_string(weights, "", "comma-separated list of weights for threads");

// Extracts required opts, returns the rest of args.
Opts parse_opts() {
  auto opts = Opts();
  opts.threads = FLAGS_threads;
  opts.tasks = FLAGS_tasks;
  opts.switches = FLAGS_switches;
  opts.rounds = FLAGS_rounds;
  opts.minimization_runs = FLAGS_minimization_runs;
  opts.verbose = FLAGS_verbose;
  opts.typ = FromLiteral(std::move(FLAGS_strategy));
  std::vector<int> thread_weights;
  if (FLAGS_weights != "") {
    auto splited = split(FLAGS_weights, ',');
    thread_weights.reserve(splited.size());
    for (auto &s : splited) {
      thread_weights.push_back(std::stoi(s));
    }
  }
  opts.thread_weights = std::move(thread_weights);
  return opts;
}

}  // namespace ltest
