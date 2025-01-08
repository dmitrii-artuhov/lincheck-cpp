#pragma once

#include <vector>
#include <set>
#include <unordered_map>

#include "minimization.h"
#include "scheduler.h"
#include "lib.h"

struct SmartMinimizor : public RoundMinimizor {
  SmartMinimizor() = delete;
  explicit SmartMinimizor(int runs_): runs(runs_) {}

  void Minimize(
    StrategyScheduler& sched,
    Scheduler::Histories& nonlinear_history
  ) const override {
    // reset
    population.clear();
    population.insert(Solution(sched.GetStrategy(), nonlinear_history));

    for (int r = 0; r < runs; ++r) {
      const Solution& p1 = *population.begin();
      const Solution& p2 = *(population.size() < 2 ? population.begin() : std::next(population.begin()));

      std::vector<Solution> offsprings = GenerateOffsprings(p1, p2); // includes mutations
      for (auto& s : offsprings) {
        population.insert(s);
      }

      while (population.size() > max_population_size) {
        population.erase(std::prev(population.end()));
      }
    }

    // final answer
    nonlinear_history = GetBestSolution();
    
    // put tasks in round in valid state according to found best solution
    sched.ReplayRound(StrategyScheduler::GetTasksOrdering(nonlinear_history.first, {}));
  }

private:
  struct Solution {
    std::unordered_map<int, std::vector<int>> tasks; // ThreadId -> { ValidTaskId1, ValidTaskId2, ... }
    Scheduler::FullHistory nonlinear_history;
    // Fitness is a value in range [0.0, 1.0], the bigger it is, the better is the Solution.
    float fitness = 0.0;
    
    explicit Solution(const Strategy& strategy, const Scheduler::Histories& histories) {
      int total_threads = 0;
      int total_tasks = 0;
      int valid_tasks = 0;

      // copy nonlinear history
      nonlinear_history = histories.first;

      // save valid task ids per thread
      const auto& threads = strategy.GetTasks();
      for (int i = 0; i < threads.size(); ++i) {
        total_threads++;

        for (int j = 0; j < threads[i].size(); ++j) {
          total_tasks++;
          const auto& task = threads[i][j].get();

          if (!task->IsRemoved()) {
            valid_tasks++;
            tasks[i].emplace_back(task->GetId());
          }
        }
      }

      // cache the fitness value of the solution
      float tasks_fitness = 1.0 - (valid_tasks * 1.0) / (total_tasks * 1.0); // the less tasks left, the closer tasks fitness is to 1.0
      float threads_fitness = 1.0 - (tasks.size() * 1.0) / (total_threads * 1.0); // the less threads left, the closer threads fitness is to 1.0

      assert(tasks_fitness >= 0.0 && tasks_fitness <= 1.0);
      assert(threads_fitness >= 0.0 && threads_fitness <= 1.0);

      fitness = tasks_fitness * threads_fitness;
    }

    float GetFitness() const {
      return fitness;
    }
  };

  struct SolutionSorter {
    bool operator()(const Solution& a, const Solution& b) const {
      // the biggest fitness, goes first
      return a.GetFitness() > b.GetFitness();
    }
  };

  std::vector<Solution> GenerateOffsprings(const Solution& p1, const Solution& p2) const {
    assert(attempts > 0);

    std::vector <Solution> result;
    for (int offspring = 1; offspring <= offsprings_per_generation; ++offspring) {
      int left_attempts = attempts;
      while (left_attempts--) {
        // TODO: generation
        
        // cross product


        // mutations


        // check for nonlinearizability
        // if (success) {
        //   result.emplace_back(new_offspring);
        //   break;
        // }
      }
    }
    return result;
  }

  Scheduler::Histories GetBestSolution() const {

  }

  const int runs;
  // TODO: make this constructor params
  const int max_population_size = 100;
  const int offsprings_per_generation = 5;
  const int attempts = 10; // attemps to generate each offspring with nonlinear history
  mutable std::set<Solution, SolutionSorter> population;
};