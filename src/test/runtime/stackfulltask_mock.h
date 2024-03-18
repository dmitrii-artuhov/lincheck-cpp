#pragma once

#include <gmock/gmock.h>

#include "include/scheduler.h"

class MockStackfulTask : public StackfulTask {
 public:
  MOCK_METHOD(void, Resume, (), (override));
  MOCK_METHOD(bool, IsReturned, (), (override));
  MOCK_METHOD(int, GetRetVal, (), (const, override));
  MOCK_METHOD(std::string, GetName, (), (const, override));
};
