#include <gtest/gtest.h>
#include "include/scheduler.h"

struct Counter {
  int count = 0;
};

template<>
struct std::hash<Counter>
{
  std::size_t operator()(const Counter& c) const noexcept
  {
    return std::hash<int>{}(c.count);
  }
};

template <>
struct std::equal_to<Counter> {
  constexpr bool operator()(const Counter& lhs, Counter& rhs) const
  {
    return lhs.count == rhs.count;
  }
};

// Demonstrate some basic assertions.
TEST(LinearizabilityCheckerCounterTest, SmallLinearizableHistory) {
  std::function<int(Counter*)> fetch_and_add = [](Counter* c) {
    c->count += 1;
    return c->count - 1;
  };
  std::function<int(Counter*)> get = [](Counter* c) {
    return c->count;
  };
  std::function<Counter*(Counter*)> copy = [](Counter* c) {
    // TODO: destructor with free
    auto* p = new Counter;
    p->count = c->count;
    return p;
  };
  Counter c{};

  LinearizabilityChecker<Counter*> checker(std::map<MethodName, std::function<int(Counter*)>>{
    {"faa", fetch_and_add},
    {"get", get},
  }, copy, &c);

  // TODO: generate StackfulTask mock
  std::vector<std::variant<StackfulTaskInvoke, StackfulTaskResponse>> history{};
  history.emplace_back(StackfulTaskInvoke(StackfulTask(3, 1, "faa")));
  history.emplace_back(StackfulTaskInvoke(StackfulTask(3, 2, "get")));
  history.emplace_back(StackfulTaskInvoke(StackfulTask(2, 3, "faa")));
  history.emplace_back(StackfulTaskInvoke(StackfulTask(1, 4, "faa")));
  history.emplace_back(StackfulTaskInvoke(StackfulTask(0, 5, "faa")));
  history.emplace_back(StackfulTaskResponse(StackfulTask(0, 5, "faa"), 0));
  history.emplace_back(StackfulTaskResponse(StackfulTask(0, 4, "faa"), 1));
  history.emplace_back(StackfulTaskResponse(StackfulTask(0, 3, "faa"), 2));
  history.emplace_back(StackfulTaskResponse(StackfulTask(0, 2, "get"), 3));
  history.emplace_back(StackfulTaskResponse(StackfulTask(0, 1, "faa"), 3));

  EXPECT_EQ(checker.Check(history), true);
}
