#pragma once

#include <gmock/gmock.h>

#include <utility>

#include "include/scheduler.h"

class MockTask : public TaskAbstract {
 public:
  MOCK_METHOD(std::shared_ptr<TaskAbstract>, Restart, (void*),
              (override));
  MOCK_METHOD(void, Resume, (), (override));
  MOCK_METHOD(bool, IsReturned, (), (const, override));
  MOCK_METHOD(int, GetRetVal, (), (const, override));
  MOCK_METHOD(std::string, GetName, (), (const, override));
  MOCK_METHOD(std::vector<std::string>, GetStrArgs, (), (const, override));
  MOCK_METHOD(void*, GetArgs, (), (const, override));
  MOCK_METHOD(bool, IsSuspended, (), (const, override));
  MOCK_METHOD(void, Terminate, (), (override));
  MOCK_METHOD(void, SetToken, (std::shared_ptr<Token>), (override));
};

class MockDualTask : public DualTaskAbstract {
public:
  MOCK_METHOD(std::shared_ptr<DualTaskAbstract>, Restart, (void*),
              (override));
  MOCK_METHOD(void, ResumeRequest, (), (override));
  MOCK_METHOD(bool, IsRequestFinished, (), (const, override));
  MOCK_METHOD(void, SetFollowUpTerminateCallback, (std::function<void()>), (override));
  MOCK_METHOD(bool, IsFollowUpFinished, (), (const, override));
  MOCK_METHOD(int, GetRetVal, (), (const, override));
  MOCK_METHOD(std::string, GetName, (), (const, override));
  MOCK_METHOD(std::vector<std::string>, GetStrArgs, (), (const, override));
  MOCK_METHOD(void*, GetArgs, (), (const, override));
  MOCK_METHOD(void, Terminate, (), (override));
};

std::shared_ptr<MockTask> CreateMockTask(std::string name, int ret_val,
                                         void* args) {
  std::shared_ptr<MockTask> mock = std::make_shared<MockTask>();
  EXPECT_CALL(*mock, GetRetVal())
      .Times(testing::AnyNumber())
      .WillRepeatedly(testing::Return(ret_val));
  EXPECT_CALL(*mock, GetName())
      .Times(testing::AnyNumber())
      .WillRepeatedly(testing::Return(name));
  EXPECT_CALL(*mock, GetArgs())
      .Times(testing::AnyNumber())
      .WillRepeatedly(testing::Return(args));

  return mock;
}

std::shared_ptr<MockDualTask> CreateMockDualTask(std::string name, int ret_val,
                                         void* args) {
  std::shared_ptr<MockDualTask> mock = std::make_shared<MockDualTask>();
  EXPECT_CALL(*mock, GetRetVal())
      .Times(testing::AnyNumber())
      .WillRepeatedly(testing::Return(ret_val));
  EXPECT_CALL(*mock, GetName())
      .Times(testing::AnyNumber())
      .WillRepeatedly(testing::Return(name));
  EXPECT_CALL(*mock, GetArgs())
      .Times(testing::AnyNumber())
      .WillRepeatedly(testing::Return(args));

  return mock;
}