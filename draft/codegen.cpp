#include "lib.h"
#include <algorithm>
#include <iostream>
#include <map>
#include <vector>

const int THREADS = 2;
// Некоторое глобальное хранилище строчек исполнения для наглядности.
// Очень черновая имплементация, never mind.
std::map<int, std::string> lines[THREADS];
int current_thread = -1;
int current_time = 0;

void reinit() {
  current_time = 0;
  current_thread = -1;
  for (int i = 0; i < THREADS; ++i)
    lines[i].clear();
}

// Печатает табличку действий по потокам.
void report() {
  const int L = 25;
  for (int i = 0; i <THREADS * (L + 1) + 1; ++i)
    std::cout << "-";
  std::cout<<std::endl;

  for (int i = 0; i < current_time; ++i) {
    std::cout << "|";
    for (int j = 0; j < THREADS; ++j) {
      if (lines[j].find(i) != lines[j].end()) {
        int l = lines[j][i].length();
        std::cout << lines[j][i];
        for (int _ = 0; _  < L - l; ++_)
          std::cout << " ";
      } else {
        for (int _ = 0; _ < L; ++_)
          std::cout << " ";
      }
      std::cout << "|";
    }
    std::cout << std::endl;
  }

  for (int i = 0; i <THREADS * (L + 1) + 1; ++i)
      std::cout << "-";
  std::cout<<std::endl;
}

#define LINE(x) lines[current_thread][current_time++] = x

struct Adder {
  int val;

  Task<int> add() {
    LINE("int x = get();");
    auto get_task1 = get();
    int x = co_await get_task1;
    co_await std::suspend_always{};
    LINE("x++");
    x++;
    co_await std::suspend_always{};
    LINE("val = x");
    val = x;
    co_await std::suspend_always{};
    co_return 0;
  }

  Task<int> get() {
    LINE("return val;");
    co_return val;
  }
};

struct Env {
  Adder ad;
  Env() { ad = Adder{0}; }
};

// Некоторое исполнение, которое проверяется.
// Такие исполнения мы будем генерировать и обходить.
Task<int> func(Env &e) {
  LINE("e.ad.add();");
  auto addTask = e.ad.add();
  int rc = co_await addTask;

  auto getTask = e.ad.get();
  int result = co_await getTask;
  LINE("return e.ad.get();");
  co_return result;
}

int main(int argc, char *argv[]) {
  bool atomic_execution = argc > 1;
  using task_type = TaskWrapper<int>;

  for (int iter = 0; iter < 15; ++iter) {
    // Init report storage.
    reinit();

    auto env = Env();

    std::vector<task_type> tasks;

    // Create tasks.
    for (int i = 0; i < THREADS; ++i) {
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
      current_thread = abilities[nxt];
      tasks[current_thread].next(atomic_execution);
    }

    report();
    std::cout << "[Execution result]: (";
    for (int i = 0; i < THREADS; ++i) {
      if (i) std::cout << ", ";
      std::cout << tasks[i].get_result();
    }
    std::cout << ")" << std::endl;
  }
}
