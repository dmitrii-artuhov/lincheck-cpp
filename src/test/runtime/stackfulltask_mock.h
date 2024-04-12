#pragma once

#include <gmock/gmock.h>

#include <utility>

#include "include/scheduler.h"

class MockStackfulTask : public StackfulTask {
 public:
  MOCK_METHOD(void, Resume, (), (override));
  MOCK_METHOD(bool, IsReturned, (), (override));
  MOCK_METHOD(int, GetRetVal, (), (const, override));
  MOCK_METHOD(std::string, GetName, (), (const, override));
  MOCK_METHOD(std::vector<int>, GetArgs, (), (const, override));
};

std::unique_ptr<MockStackfulTask> CreateMockStackfulTask(std::string name, int ret_val, std::vector<int> args) {
  std::unique_ptr<MockStackfulTask> mock = std::make_unique<MockStackfulTask>();
  EXPECT_CALL(*mock, GetRetVal())
      .Times(testing::AnyNumber())
      .WillRepeatedly(testing::Return(ret_val));
  EXPECT_CALL(*mock, GetName())
      .Times(testing::AnyNumber())
      .WillRepeatedly(testing::Return(std::move(name)));
  EXPECT_CALL(*mock, GetArgs())
      .Times(testing::AnyNumber())
      .WillRepeatedly(testing::Return(std::move(args)));

  return mock;
}
