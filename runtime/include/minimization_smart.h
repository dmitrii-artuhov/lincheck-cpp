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

struct SmartMinimizor : public RoundMinimizor {
  SmartMinimizor() = delete;
  explicit SmartMinimizor(int minimization_runs_, PrettyPrinter& pretty_printer_):
    minimization_runs(minimization_runs_),
    pretty_printer(pretty_printer_) {
    std::random_device dev;
    rng = std::mt19937(dev());
  }

  void Minimize(
    StrategyScheduler& sched,
    Scheduler::Histories& nonlinear_history
  ) const override {
    // reset
    total_tasks = sched.GetStrategy().GetTotalTasksCount();
    population.clear();
    population.insert(Solution(sched.GetStrategy(), nonlinear_history, total_tasks));

    // TODO: when the mutations count is 1, we could add a count which will increase,
    //       when it reaches the `max_nonsuccessful_runs` then we make a fast exit (cannot minimize anymore)
    for (int r = 0; r < minimization_runs; ++r) {
      // TODO: select with probability (allow to select worse parents as well)
      const Solution& p1 = *population.begin();
      const Solution& p2 = *(population.size() < 2 ? population.begin() : std::next(population.begin()));

      std::vector<Solution> offsprings = GenerateOffsprings(sched, p1, p2); // includes mutations
      for (auto& s : offsprings) {
        population.insert(s);
      }

      while (population.size() > max_population_size) {
        population.erase(std::prev(population.end()));
      }
    }

    // final answer
    assert(!population.empty()); // at least the 1st solution should be there
    const Solution& best_solution = *population.begin();
    
    // put tasks in a valid state according to the found best solution
    RemoveInvalidTasks(sched.GetStrategy(), best_solution.tasks);
    
    // replay the round with found nonlinearized interleaving, this put the round in correct final state and builds a `Histories` object
    auto replayed_result = sched.ReplayRound(StrategyScheduler::GetTasksOrdering(best_solution.nonlinear_history.first, {}));

    // override nonlinear history with the best solution
    assert(replayed_result.has_value());
    nonlinear_history = replayed_result.value();
  }

private:
  struct Solution {
    explicit Solution(
      const Strategy& strategy,
      const Scheduler::Histories& histories,
      int total_tasks
    ) {
      int total_threads = strategy.GetThreadsCount();

      // copy nonlinear history
      nonlinear_history = histories;

      // save valid task ids per thread
      const auto& threads = strategy.GetTasks();
      for (int i = 0; i < threads.size(); ++i) {
        for (int j = 0; j < threads[i].size(); ++j) {
          const auto& task = threads[i][j].get();

          if (!task->IsRemoved()) {
            valid_tasks++;
            tasks[i].insert(task->GetId());
          }
        }
      }

      // cache the fitness value of the solution
      float tasks_fitness = 1.0 - (valid_tasks * 1.0) / (total_tasks * 1.0); // the less tasks left, the closer tasks fitness is to 1.0
      float threads_fitness = eps + 1.0 - (tasks.size() * 1.0) / (total_threads * 1.0); // the less threads left, the closer threads fitness is to 1.0

      assert(tasks_fitness >= 0.0 && tasks_fitness <= 1.0);
      assert(threads_fitness >= 0.0 && threads_fitness <= 1.0);

      fitness = tasks_fitness * threads_fitness; // in [0.0, 1.0]: the bigger, the better
    }

    float GetFitness() const {
      return fitness;
    }


    int GetValidTasks() const {
      return valid_tasks;
    }

    std::unordered_map<int, std::unordered_set<int>> tasks; // ThreadId -> { ValidTaskId1, ValidTaskId2, ... }
    Scheduler::Histories nonlinear_history;
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

  void DropRandomTask(std::unordered_map<int, std::unordered_set<int>>& threads) const {
    if (threads.empty()) return;

    // pick a thread from which to drop task
    int thread_index = std::uniform_int_distribution<int>(0, threads.size() - 1)(rng);
    auto it = std::next(threads.begin(), thread_index);
    auto& tasks = it->second;

    if (
      tasks.empty() ||
      tasks.size() == 1 && threads.size() == 2 // removing task from selected thread will result in single thread left
    ) return;

    // remove task with position `task_index` from picked thread
    int task_index = std::uniform_int_distribution<int>(0, tasks.size() - 1)(rng);
    auto task_it = std::next(tasks.begin(), task_index);
    tasks.erase(task_it);
  }

  std::vector<Solution> GenerateOffsprings(
    StrategyScheduler& sched,
    const Solution& p1,
    const Solution& p2
  ) const {
    assert(attempts > 0);
    
    const Strategy& strategy = sched.GetStrategy();
    std::vector <Solution> result;

    for (int offspring = 1; offspring <= offsprings_per_generation; ++offspring) {
      int left_attempts = attempts;
      while (left_attempts--) {
        // cross product
        auto new_threads = CrossProduct(strategy, &p1, &p2);

        // mutations
        int applied_mutations = 0;
        for (int m = 1; m <= mutations_count; ++m) {
          // TODO: should we allow cross-product only even when `mutation_count > 1`?
          // when a single mutation left, we want sometimes to only have cross product without mutating
          if (mutations_count > 1 || dist(rng) < 0.95) {
            applied_mutations++;
            DropRandomTask(new_threads);
          }
        }

        // check for nonlinearizability
        // 1. mark only valid tasks in round as non-removed
        RemoveInvalidTasks(strategy, new_threads);

        // 2. explore round in order to find non-linearizable history
        auto histories = sched.ExploreRound(exploration_runs);

        // 3. new offspring successfully generated
        if (histories.has_value()) {
          Solution offspring(strategy, histories.value(), total_tasks);
          result.push_back(offspring);
          break;
        }
      }
    }

    if (result.size() * 2 < offsprings_per_generation && mutations_count > 1) {
      // update the mutations count
      mutations_count--;
    }

    return result;
  }

  // Marks tasks as removed if they do not appear in `valid_threads`.
  void RemoveInvalidTasks(
    const Strategy& strategy,
    const std::unordered_map<int, std::unordered_set<int>>& valid_threads
  ) const {
    const auto& tasks = strategy.GetTasks();
    for (int thread_id = 0; thread_id < tasks.size(); ++thread_id) {
      const auto& thread = tasks[thread_id];
      bool thread_exists = valid_threads.contains(thread_id);

      for (int i = 0; i < thread.size(); ++i) {
        if (thread_exists && valid_threads.at(thread_id).contains(thread[i]->GetId())) {
          thread[i]->SetRemoved(false);
        }
        else {
          thread[i]->SetRemoved(true);
        }
      }
    }
  }

  std::unordered_map<int, std::unordered_set<int>> CrossProduct(
    const Strategy& strategy,
    const Solution* p1,
    const Solution* p2
  ) const {
    // p1 has smaller number of threads
    if (p1->tasks.size() >= p2->tasks.size()) {
      std::swap(p1, p2);
    }

    // probability of copying thread tasks from p1
    const float p = 0.5; // TODO: could be weighted according to the number of tasks in thread
    std::unordered_map<int, std::unordered_set<int>> new_threads;

    for (auto& [thread_id, task_ids] : p1->tasks) {
      if (p2->tasks.contains(thread_id) && dist(rng) >= p) {
        new_threads[thread_id] = p2->tasks.at(thread_id);
      }
      else {
        new_threads[thread_id] = task_ids;
      }
    }

    return new_threads;
  }

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