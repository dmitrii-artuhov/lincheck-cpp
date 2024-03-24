#pragma once
#include <memory>

#include "lib.h"
#include "lincheck_recursive.h"
#include "logger.h"
#include "pretty_print.h"
#include "round_robin_strategy.h"
#include "scheduler.h"
#include "uniform_strategy.h"
#include "pct_strategy.h"

// Public macros.
#define non_atomic attr(ltest_nonatomic)
#define generator attr(ltest_gen)
#define ini attr(ltest_initfunc)
#define TARGET_METHOD(ret, cls, symbol, args) \
  concat_attr(ltesttarget_, symbol) non_atomic ret cls::symbol args

enum StrategyType { RR, UNIFORM, PCT };

template <class TargetObj, class LinearSpec,
          class LinearSpecHash = std::hash<LinearSpec>,
          class LinearSpecEquals = std::equal_to<LinearSpec>>
struct Spec {
  using target_obj_t = TargetObj;
  using linear_spec_t = LinearSpec;
  using linear_spec_hash_t = LinearSpecHash;
  using linear_spec_equals_t = LinearSpecEquals;
};

void extract_args(int argc, char *argv[], size_t &threads, StrategyType &typ,
                  size_t &tasks, size_t &rounds, bool &verbose);

template <class Spec>
struct Entrypoint {
  void run(int argc, char *argv[]) {
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

    log() << "strategy = ";

    std::unique_ptr<Strategy> strategy;
    switch (typ) {
      case RR:
        log() << "round-robin";
        strategy =
            std::make_unique<RoundRobinStrategy<typename Spec::target_obj_t>>(
                threads, &l);
        break;
      case UNIFORM:
        log() << "uniform";
        strategy =
            std::make_unique<UniformStrategy<typename Spec::target_obj_t>>(
                threads, &l);
        break;
      case PCT:
        log() << "pct";
        // TODO: k
        strategy =
            std::make_unique<PctStrategy<typename Spec::target_obj_t>>(
                threads, &l, 100);
        break;
    }
    log() << "\n\n";

    using lchecker_t =
        LinearizabilityCheckerRecursive<typename Spec::linear_spec_t,
                                        typename Spec::linear_spec_hash_t,
                                        typename Spec::linear_spec_equals_t>;
    lchecker_t checker{Spec::linear_spec_t::GetMethods(),
                       typename Spec::linear_spec_t{}};
    auto scheduler = Scheduler{*strategy.get(), checker, tasks, rounds};
    auto result = scheduler.Run();
    if (result.has_value()) {
      std::cout << "non linearized:\n";
      pretty_print::pretty_print(result.value().second, std::cout);
    } else {
      std::cout << "success!\n";
    }
  }
};

#define LTEST_ENTRYPOINT(spec_obj_t)          \
  int main(int argc, char *argv[]) {          \
    Entrypoint<spec_obj_t>{}.run(argc, argv); \
    return 0;                                 \
  }\
