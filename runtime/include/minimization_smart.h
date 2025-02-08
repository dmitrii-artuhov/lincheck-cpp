#pragma once

#include <vector>
#include <set>
#include <unordered_map>
#include <random>
#include <string>

#include "pretty_print.h"
#include "minimization.h"
#include "scheduler.h"
#include "lib.h"

/**
 * `SmartMinimizor` uses a genetic algorithm to minimize the number of tasks in the nonlinearized history.
 * 
 * It tries to generate new `Solution` objects which contain the nonlinearized histories
 * of the round with some tasks being removed. Each `Solution` object has a `fitness` value
 * in range `[0.0, 1.0]`, the closer fitness to 1.0, the smaller the nonlinearizable history is.
 * 
 * During execution this minimizor generates a population of `Solution` objects, then it selects
 * the best two solutions from the population and generates new offsprings by crossing and mutating
 * their set of removed tasks. The new offsprings are added to the population,
 * and sorted by their fitness values. The best among them are preserved for the next offsprings generation.
 * 
 * When the minimization budget is exhausted, the best solution is returned.  
 */
struct SmartMinimizor : public RoundMinimizor {
  SmartMinimizor() = delete;
  explicit SmartMinimizor(int minimization_runs_, PrettyPrinter& pretty_printer_);

  void Minimize(
    StrategyScheduler& sched,
    Scheduler::BothHistories& nonlinear_histories
  ) const override;

private:
  struct Solution {
    explicit Solution(
      const Strategy& strategy,
      const Scheduler::BothHistories& histories,
      int total_tasks
    );

    float GetFitness() const;
    int GetValidTasks() const;

    std::unordered_map<int, std::unordered_set<int>> tasks; // ThreadId -> { ValidTaskId1, ValidTaskId2, ... }
    Scheduler::BothHistories nonlinear_histories;
    // Fitness is a value in range [0.0, 1.0], the bigger it is, the better is the Solution.
  private:
    float eps = 0.0001;
    float fitness = 0.0;
    int valid_tasks = 0;
  };

  struct SolutionSorter {
    bool operator()(const Solution& a, const Solution& b) const {
      // the biggest fitness, goes first
      return a.GetFitness() > b.GetFitness();
    }
  };

  // Mutation basically removes a single random task from round. 
  void DropRandomTask(std::unordered_map<int, std::unordered_set<int>>& threads) const;

  // Generates offsprings by crossing and mutating the set of removed tasks in the parents `p1`, `p2`.
  std::vector<Solution> GenerateOffsprings(
    StrategyScheduler& sched,
    const Solution& p1,
    const Solution& p2
  ) const;

  // Marks tasks as removed if they do not appear in `valid_threads`.
  void RemoveInvalidTasks(
    const Strategy& strategy,
    const std::unordered_map<int, std::unordered_set<int>>& valid_threads
  ) const;

  /**
   * Crosses two parents `p1` and `p2` to generate a new set of threads with tasks.
   * If `p1` had the following set of threads:
   * 
   * `[p1:T1, p1:T2, p1:T3]`
   * 
   * and `p2` had:
   * 
   * `[p2:T1, p2:T2, p2:T3]`,
   * 
   * possible crossproduct could look like this: `[p1:T1, p2:T2, p1:T3]` (a mix of threads from both parents).
   * 
   * Note: resulting number of threads will be equal to the `min(p1.thread_count, p2.thread_count)`.
   */
  std::unordered_map<int, std::unordered_set<int>> CrossProduct(
    const Strategy& strategy,
    const Solution* p1,
    const Solution* p2
  ) const;

  const int minimization_runs;
  // TODO: make this constructor params
  const int max_population_size = 2;
  const int offsprings_per_generation = 5;
  const int attempts = 10; // attemps to generate each offspring with nonlinear history
  const int exploration_runs = 10; // TODO: for other minimizors, the `minimization_runs` relates to this. Should refactor this
  // std::vector<std::pair<std::unique_ptr<Mutation>, float /* probability of applying the mutation */>> mutations;
  mutable int total_tasks;
  mutable int mutations_count = 10;
  mutable std::multiset<Solution, SolutionSorter> population;
  mutable std::mt19937 rng;
  mutable std::uniform_real_distribution<double> dist{0.0, 1.0};
  PrettyPrinter& pretty_printer;
};