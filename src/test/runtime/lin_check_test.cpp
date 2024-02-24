#include <fuzztest/fuzztest.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "include/lincheck.h"
#include "include/lincheck_recursive.h"
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
using ::testing::ReturnRefOfCopy;

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

std::vector<std::unique_ptr<MockStackfulTask>> create_mocks(const std::vector<bool> &b_history) {
  std::vector<std::unique_ptr<MockStackfulTask>> mocks;
  mocks.reserve(b_history.size());
  size_t adds = 0;

  for (auto v : b_history) {
    if (v) {
      auto* add_task = new MockStackfulTask();
      mocks.push_back(std::unique_ptr<MockStackfulTask>(add_task));

      EXPECT_CALL(*add_task, GetRetVal())
          .Times(AnyNumber())
          .WillRepeatedly(Return(adds));
      EXPECT_CALL(*add_task, GetName())
          .Times(AnyNumber())
          .WillRepeatedly(ReturnRefOfCopy(std::string("faa")));

      adds++;
    } else {
      auto* get_task = new MockStackfulTask();
      mocks.push_back(std::unique_ptr<MockStackfulTask>(get_task));

      EXPECT_CALL(*get_task, GetRetVal())
          .Times(AnyNumber())
          .WillRepeatedly(Return(adds));
      EXPECT_CALL(*get_task, GetName())
          .Times(AnyNumber())
          .WillRepeatedly(ReturnRefOfCopy(std::string("get")));
    }
  }

  return mocks;
}

std::vector<std::variant<StackfulTaskInvoke, StackfulTaskResponse>> create_history(const std::vector<std::unique_ptr<MockStackfulTask>>& mocks) {
  std::vector<std::variant<StackfulTaskInvoke, StackfulTaskResponse>> history;
  history.reserve(2 * mocks.size());

  for (auto& m: mocks) {
    history.emplace_back(StackfulTaskInvoke(*m));
    history.emplace_back(StackfulTaskResponse(*m, m->GetRetVal()));
  }

  std::random_device rd;
  std::mt19937 g(rd());
  std::shuffle(history.begin(), history.end(), g);

  // Fix the order between invokes and responses
  std::map<const StackfulTask*, size_t> responses_indexes;

  for (size_t i = 0; i < history.size(); ++i) {
    auto& event = history[i];
    if (event.index() == 0) {
       if(responses_indexes.find(&std::get<StackfulTaskInvoke>(event).GetTask()) != responses_indexes.end()) {
          size_t index = responses_indexes[&std::get<StackfulTaskInvoke>(event).GetTask()];
          std::swap(history[index], history[i]);
       }
    } else {
      responses_indexes[&std::get<StackfulTaskResponse>(event).GetTask()] = i;
    }
  }

  return history;
}

std::string draw_history(const std::vector<std::variant<StackfulTaskInvoke, StackfulTaskResponse>>& history) {
  std::map<const StackfulTask*, size_t> numeration;
  size_t i = 0;
  for (auto& event: history) {
    if (event.index() == 1) {
      continue;
    }

    StackfulTaskInvoke invoke = std::get<StackfulTaskInvoke>(event);
    numeration[&invoke.GetTask()] = i;
    ++i;
  }

  std::stringstream history_string;

  for (auto& event: history) {
    if (event.index() == 0) {
      StackfulTaskInvoke invoke = std::get<StackfulTaskInvoke>(event);
      history_string << "[" << numeration[&invoke.GetTask()] << " inv: " << invoke.GetTask().GetName() << "]\n";
    } else {
      StackfulTaskResponse response = std::get<StackfulTaskResponse>(event);
      history_string << "[" << numeration[&response.GetTask()] << " res: " << response.GetTask().GetName() << " returned: " << response.GetTask().GetRetVal() <<  "]\n";
    }
  }

  return history_string.str();
}

void CheckersAreTheSame(const std::vector<bool> &b_history) {
  std::function<int(Counter*)> fetch_and_add = [](Counter* c) {
    c->count += 1;
    return c->count - 1;
  };
  std::function<int(Counter*)> get = [](Counter* c) { return c->count; };
  Counter c{};

  LinearizabilityChecker<Counter> fast(
      std::map<MethodName, std::function<int(Counter*)>>{
          {"faa", fetch_and_add},
          {"get", get},
      },
      c);

  LinearizabilityCheckerRecursive<Counter> slow(
      std::map<MethodName, std::function<int(Counter*)>>{
          {"faa", fetch_and_add},
          {"get", get},
      },
      c);

  auto mocks = create_mocks(b_history);
  auto history = create_history(mocks);
  EXPECT_EQ(fast.Check(history), slow.Check(history)) << draw_history(history);
}

FUZZ_TEST(LinearizabilityCheckerCounterTest, CheckersAreTheSame);

};  // namespace LinearizabilityCheckerTest