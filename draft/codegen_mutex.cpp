#include "lib.h"
#include <iostream>
#include <mutex>
#include <vector>

struct Adder {
  int val{};
  std::mutex mu{};

  Task<int> add() {
    std::lock_guard<std::mutex> lock(mu);
    auto get_task1 = get();
    int x = co_await get_task1;
    co_await std::suspend_always{};
    x++;
    co_await std::suspend_always{};
    val = x;
    co_await std::suspend_always{};
    std::cout << "add returns" << std::endl;
    co_return 0;
  }

  Task<int> get() { co_return val; }
};

struct Env {
  Adder ad{};
};

Task<int> func(Env &e) {
  std::cout << "func begins" << std::endl;
  auto addTask = e.ad.add();
  int rc = co_await addTask;

  auto getTask = e.ad.get();
  int result = co_await getTask;
  co_return result;
}

int main() {
  using task_type = TaskWrapper<int>;

  for (int iter = 0; iter < 15; ++iter) {
    auto env = Env{};

    std::vector<task_type> tasks;

    // THREADS.
    for (int i = 0; i < 2; ++i) {
      tasks.emplace_back(std::move(task_type{func(env)}));
    }

    while (true) {
      std::vector<int> abilities;
      for (int i = 0; i < tasks.size(); ++i) {
        if (tasks[i].has_next()) {
          abilities.push_back(i);
        }
      }

      // Может быть все завершились?
      if (abilities.empty())
        break;

      // Иначем зовем next() на рандомной таске.
      auto nxt = rand() % abilities.size();
      std::cout << "call " << nxt << std::endl;
      tasks[abilities[nxt]].next();
    }

    std::cout << "[Execution result]: (" << tasks[0].get_result() << ", "
              << tasks[1].get_result() << ")" << std::endl;
  }
}
