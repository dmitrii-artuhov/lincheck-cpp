#include <iostream>
#include <string>

#include "../../../runtime/include/generators.h"
#include "../../../runtime/include/verifying_macro.h"

struct Fatty {
  int a = 42;
  int b = 28;
  int c = 30;
  std::string str = "abacaba";
  double x = 1.5;
  float y = 36.6;
};

namespace ltest {

template <>
std::string toString<Fatty>(const Fatty &f) {
  return "Fatty";
}

template <>
std::string toString<std::pair<int, int>>(const std::pair<int, int> &p) {
  return "{" + toString(p.first) + ", " + toString(p.second) + "}";
}

template <>
std::string toString<std::string>(const std::string &a) {
  return a;
}

};  // namespace ltest

struct Test {
  void non_args();

  int two_sum(int, int);

  int fatty_sum(Fatty f);

  void print(std::string a, std::string b, std::string c);

  int sum_of_pairs(std::pair<int, int> a, std::pair<int, int> b);
};

// Generators.
auto two_sum_generator() { return std::tuple<int, int>{42, 43}; }

auto fatty_generator() { return ltest::generators::makeSingleArg(Fatty{}); }

auto fatty_ptr_generator() {
  auto fatty = new Fatty();
  return ltest::generators::makeSingleArg(fatty);
}

auto three_strings_generator() {
  return std::tuple<std::string, std::string, std::string>{"hello, ", "world",
                                                           "!"};
}

auto two_pairs_generator() {
  using ipair = std::pair<int, int>;
  return std::tuple<ipair, ipair>{{1, 2}, {3, 4}};
}

// Implementation.
target_method(fatty_generator, int, Test, fatty_sum, Fatty f) {
  std::cout << "fatty_sum(" << f.a << ", " << f.b << ", " << f.c << ", "
            << f.str << ", " << f.x << ", " << f.y << ")" << std::endl;
  return f.str.size() + f.a + f.b + f.c + static_cast<int>(f.x) +
         static_cast<int>(f.y);
}

target_method(ltest::generators::genEmpty, void, Test, non_args) {
  std::cout << "No args!" << std::endl;
}

target_method(three_strings_generator, void, Test, print, std::string a,
              std::string b, std::string c) {
  std::cout << a << " " << b << " " << c << std::endl;
}

target_method(two_pairs_generator, int, Test, sum_of_pairs,
              std::pair<int, int> a, std::pair<int, int> b) {
  return a.first + a.second + b.first + b.second;
}

target_method(two_sum_generator, int, Test, two_sum, int a, int b) {
  return a + b;
}

namespace ltest {
std::vector<TaskBuilder> task_builders;
GeneratedArgs gen_args = GeneratedArgs{};
}  // namespace ltest

int main() {
  auto conss = std::move(ltest::task_builders);
  Test tst{};
  for (auto cons : conss) {
    auto task = StackfulTask{cons, &tst};
    while (!task.IsReturned()) {
      task.Resume();
    }
    std::cout << "Returned: " << task.GetRetVal() << std::endl;
  }
}
