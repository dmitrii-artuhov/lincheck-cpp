#pragma once
#include <memory>

#include "lib.h"
#include "lincheck_recursive.h"
#include "logger.h"
#include "pretty_print.h"
#include "random_strategy.h"
#include "round_robin_strategy.h"
#include "scheduler.h"

// Public macros.
#define non_atomic attr(ltest_nonatomic)
#define generator attr(ltest_gen)
#define ini attr(ltest_initfunc)
#define TARGET_METHOD(ret, cls, symbol, args) \
  concat_attr(ltesttarget_, symbol) non_atomic ret cls::symbol args

namespace ltest {

enum StrategyType { RR, RND };

template <class TargetObj, class LinearSpec,
          class LinearSpecHash = std::hash<LinearSpec>,
          class LinearSpecEquals = std::equal_to<LinearSpec>>
struct Spec {
  using target_obj_t = TargetObj;
  using linear_spec_t = LinearSpec;
  using linear_spec_hash_t = LinearSpecHash;
  using linear_spec_equals_t = LinearSpecEquals;
};

struct Opts {
  size_t threads;
  size_t tasks;
  size_t rounds;
  bool verbose;
  StrategyType typ;
};

std::vector<std::string> parse_opts(std::vector<std::string> args, Opts &opts);

std::vector<std::string> split(const std::string &s, char delim);

template <typename TargetObj>
std::unique_ptr<Strategy> MakeStrategy(Opts &opts, TaskBuilderList l,
                                       std::vector<std::string> args) {
  log() << "strategy = ";
  switch (opts.typ) {
    case RR:
      log() << "round-robin";
      return std::make_unique<RoundRobinStrategy<TargetObj>>(opts.threads, l);
    case RND:
      log() << "random";
      std::vector<int> weights;
      if (args.empty()) {
        weights.assign(opts.threads, 1);
      } else {
        auto splited = split(args[0], ',');
        if (splited.size() != opts.threads) {
          throw std::invalid_argument{
              "number of threads not equal to number of weights"};
        }
        for (const auto &s : splited) {
          weights.push_back(std::stoul(s));
        }
      }
      return std::make_unique<RandomStrategy<TargetObj>>(opts.threads, l,
                                                         weights);
  }
}

template <class Spec>
void Run(int argc, char *argv[]) {
  std::vector<std::string> args;
  for (size_t i = 1; i < argc; ++i) {
    args.push_back(std::string{argv[i]});
  }
  Opts opts;
  args = parse_opts(std::move(args), opts);

  logger_init(opts.verbose);
  log() << "threads  = " << opts.threads << "\n";
  log() << "tasks    = " << opts.tasks << "\n";
  log() << "rounds   = " << opts.rounds << "\n";

  std::vector<TaskBuilder> l;
  fill_ctx(&l);

  auto strategy =
      MakeStrategy<typename Spec::target_obj_t>(opts, &l, std::move(args));

  log() << "\n\n";

  using lchecker_t =
      LinearizabilityCheckerRecursive<typename Spec::linear_spec_t,
                                      typename Spec::linear_spec_hash_t,
                                      typename Spec::linear_spec_equals_t>;
  lchecker_t checker{Spec::linear_spec_t::GetMethods(),
                     typename Spec::linear_spec_t{}};
  auto scheduler = Scheduler{*strategy.get(), checker, opts.tasks, opts.rounds};
  auto result = scheduler.Run();
  if (result.has_value()) {
    std::cout << "non linearized:\n";
    pretty_print::pretty_print(result.value().second, std::cout);
  } else {
    std::cout << "success!\n";
  }
}

}  // namespace ltest

#define LTEST_ENTRYPOINT(spec_obj_t)    \
  int main(int argc, char *argv[]) {    \
    ltest::Run<spec_obj_t>(argc, argv); \
    return 0;                           \
  }\
