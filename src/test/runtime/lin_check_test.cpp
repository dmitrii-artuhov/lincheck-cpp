#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "include/lincheck.h"
#include "include/scheduler.h"

struct Counter {
  int count = 0;
};

template <>
struct std::hash<Counter> {
  std::size_t operator()(const Counter& c) const noexcept {
    return std::hash<int>{}(c.count);
  }
};

template <>
struct std::equal_to<Counter> {
  constexpr bool operator()(const Counter& lhs, const Counter& rhs) const {
    return lhs.count == rhs.count;
  }
};

namespace LinearizabilityCheckerTest {
class MockStackfulTask : public StackfulTask {
 public:
  MOCK_METHOD(void, Resume, (), (override));
  MOCK_METHOD(bool, IsReturned, (), (override));
  MOCK_METHOD(int, GetRetVal, (), (const, override));
  MOCK_METHOD(const std::string&, GetName, (), (const, override));
};

using ::testing::AnyNumber;
using ::testing::Return;
using ::testing::ReturnRef;

TEST(LinearizabilityCheckerCounterTest, SmallLinearizableHistory) {
  std::function<int(Counter*)> fetch_and_add = [](Counter* c) {
    c->count += 1;
    return c->count - 1;
  };
  std::function<int(Counter*)> get = [](Counter* c) { return c->count; };
  Counter c{};

  LinearizabilityChecker<Counter> checker(
      std::map<MethodName, std::function<int(Counter*)>>{
          {"faa", fetch_and_add},
          {"get", get},
      },
      c);

  MockStackfulTask first_task;
  std::string first_task_name("faa");
  EXPECT_CALL(first_task, GetRetVal())
      .Times(AnyNumber())
      .WillRepeatedly(Return(3));
  EXPECT_CALL(first_task, GetName())
      .Times(AnyNumber())
      .WillRepeatedly(ReturnRef(first_task_name));

  MockStackfulTask second_task;
  std::string second_task_name("get");
  EXPECT_CALL(second_task, GetRetVal())
      .Times(AnyNumber())
      .WillRepeatedly(Return(3));
  EXPECT_CALL(second_task, GetName())
      .Times(AnyNumber())
      .WillRepeatedly(ReturnRef(second_task_name));

  MockStackfulTask third_task;
  std::string third_task_name("faa");
  EXPECT_CALL(third_task, GetRetVal())
      .Times(AnyNumber())
      .WillRepeatedly(Return(2));
  EXPECT_CALL(third_task, GetName())
      .Times(AnyNumber())
      .WillRepeatedly(ReturnRef(third_task_name));

  MockStackfulTask fourth_task;
  std::string fourth_task_name("faa");
  EXPECT_CALL(fourth_task, GetRetVal())
      .Times(AnyNumber())
      .WillRepeatedly(Return(1));
  EXPECT_CALL(fourth_task, GetName())
      .Times(AnyNumber())
      .WillRepeatedly(ReturnRef(fourth_task_name));

  MockStackfulTask fifth_task;
  std::string fifth_task_name("faa");
  EXPECT_CALL(fifth_task, GetRetVal())
      .Times(AnyNumber())
      .WillRepeatedly(Return(0));
  EXPECT_CALL(fifth_task, GetName())
      .Times(AnyNumber())
      .WillRepeatedly(ReturnRef(fifth_task_name));

  std::vector<std::variant<StackfulTaskInvoke, StackfulTaskResponse>> history{};
  history.emplace_back(StackfulTaskInvoke(first_task));
  history.emplace_back(StackfulTaskInvoke(second_task));
  history.emplace_back(StackfulTaskInvoke(third_task));
  history.emplace_back(StackfulTaskInvoke(fourth_task));
  history.emplace_back(StackfulTaskInvoke(fifth_task));
  history.emplace_back(StackfulTaskResponse(fifth_task, 0));
  history.emplace_back(StackfulTaskResponse(fourth_task, 1));
  history.emplace_back(StackfulTaskResponse(third_task, 2));
  history.emplace_back(StackfulTaskResponse(second_task, 3));
  history.emplace_back(StackfulTaskResponse(first_task, 3));

  EXPECT_EQ(checker.Check(history), true);
}

TEST(LinearizabilityCheckerCounterTest, SmallUnlinearizableHistory) {
  std::function<int(Counter*)> fetch_and_add = [](Counter* c) {
    c->count += 1;
    return c->count - 1;
  };
  std::function<int(Counter*)> get = [](Counter* c) { return c->count; };
  Counter c{};

  LinearizabilityChecker<Counter> checker(
      std::map<MethodName, std::function<int(Counter*)>>{
          {"faa", fetch_and_add},
          {"get", get},
      },
      c);

  MockStackfulTask first_task;
  std::string first_task_name("faa");
  EXPECT_CALL(first_task, GetRetVal())
      .Times(AnyNumber())
      .WillRepeatedly(Return(2));
  EXPECT_CALL(first_task, GetName())
      .Times(AnyNumber())
      .WillRepeatedly(ReturnRef(first_task_name));

  MockStackfulTask second_task;
  std::string second_task_name("get");
  EXPECT_CALL(second_task, GetRetVal())
      .Times(AnyNumber())
      .WillRepeatedly(Return(3));
  EXPECT_CALL(second_task, GetName())
      .Times(AnyNumber())
      .WillRepeatedly(ReturnRef(second_task_name));

  MockStackfulTask third_task;
  std::string third_task_name("faa");
  EXPECT_CALL(third_task, GetRetVal())
      .Times(AnyNumber())
      .WillRepeatedly(Return(100));
  EXPECT_CALL(third_task, GetName())
      .Times(AnyNumber())
      .WillRepeatedly(ReturnRef(third_task_name));

  MockStackfulTask fourth_task;
  std::string fourth_task_name("faa");
  EXPECT_CALL(fourth_task, GetRetVal())
      .Times(AnyNumber())
      .WillRepeatedly(Return(1));
  EXPECT_CALL(fourth_task, GetName())
      .Times(AnyNumber())
      .WillRepeatedly(ReturnRef(fourth_task_name));

  MockStackfulTask fifth_task;
  std::string fifth_task_name("faa");
  EXPECT_CALL(fifth_task, GetRetVal())
      .Times(AnyNumber())
      .WillRepeatedly(Return(0));
  EXPECT_CALL(fifth_task, GetName())
      .Times(AnyNumber())
      .WillRepeatedly(ReturnRef(fifth_task_name));

  std::vector<std::variant<StackfulTaskInvoke, StackfulTaskResponse>> history{};
  history.emplace_back(StackfulTaskInvoke(first_task));
  history.emplace_back(StackfulTaskInvoke(second_task));
  history.emplace_back(StackfulTaskInvoke(third_task));
  history.emplace_back(StackfulTaskInvoke(fourth_task));
  history.emplace_back(StackfulTaskInvoke(fifth_task));
  history.emplace_back(StackfulTaskResponse(fifth_task, 0));
  history.emplace_back(StackfulTaskResponse(fourth_task, 1));
  history.emplace_back(StackfulTaskResponse(third_task, 100));
  history.emplace_back(StackfulTaskResponse(second_task, 3));
  history.emplace_back(StackfulTaskResponse(first_task, 3));

  EXPECT_EQ(checker.Check(history), false);
}

};  // namespace LinearizabilityCheckerTest