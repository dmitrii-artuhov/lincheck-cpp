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

enum StrategyType { RR, RND, TLA };

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
  switch (opts.typ) {
    case RR: {
      log() << "round-robin\n";
      return std::make_unique<RoundRobinStrategy<TargetObj>>(opts.threads, l);
    }
    case RND: {
      log() << "random\n";
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
    default:
      assert(false && "unexpected typ");
  }
}

// Keeps pointer to strategy to pass reference to base scheduler.
// TODO: refactor.
struct StrategySchedulerWrapper : StrategyScheduler {
  StrategySchedulerWrapper(std::unique_ptr<Strategy> strategy,
                           ModelChecker &checker, size_t max_tasks,
                           size_t max_rounds, size_t threads_count)
      : strategy(std::move(strategy)),
        StrategyScheduler(*strategy.get(), checker, max_tasks, max_rounds,
                          threads_count){};

 private:
  std::unique_ptr<Strategy> strategy;
};

template <typename TargetObj>
std::unique_ptr<Scheduler> MakeScheduler(ModelChecker &checker, Opts &opts,
                                         TaskBuilderList l,
                                         std::vector<std::string> args) {
  log() << "strategy = ";
  switch (opts.typ) {
    case RR:
    case RND: {
      auto strategy = MakeStrategy<TargetObj>(opts, l, std::move(args));
      auto scheduler = std::make_unique<StrategySchedulerWrapper>(
          std::move(strategy), checker, opts.tasks, opts.rounds, opts.threads);
      return scheduler;
    }
    case TLA: {
      log() << "tla\n";
      auto scheduler = std::make_unique<TLAScheduler<TargetObj>>(
          opts.tasks, opts.rounds, opts.threads, l, checker);
      return scheduler;
    }
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

  using lchecker_t =
      LinearizabilityCheckerRecursive<typename Spec::linear_spec_t,
                                      typename Spec::linear_spec_hash_t,
                                      typename Spec::linear_spec_equals_t>;
  lchecker_t checker{Spec::linear_spec_t::GetMethods(),
                     typename Spec::linear_spec_t{}};

  auto scheduler = MakeScheduler<typename Spec::target_obj_t>(checker, opts, &l,
                                                              std::move(args));
  log() << "\n\n";

  auto result = scheduler->Run();
  if (result.has_value()) {
    std::cout << "non linearized:\n";
    pretty_print::pretty_print(result.value().second, std::cout, opts.threads);
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
