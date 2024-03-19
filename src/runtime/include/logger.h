#pragma once
#include <iostream>

struct Logger {
  bool verbose{};

  template <typename T>
  Logger& operator<<(const T& val) {
    if (verbose) {
      std::cout << val;
    }
    return *this;
  }
};

void logger_init(bool verbose);

Logger& log();
