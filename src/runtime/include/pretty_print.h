#pragma once
#include <algorithm>
#include <cassert>
#include <iostream>
#include <string>
#include <vector>

#include "scheduler.h"

namespace pretty_print {

using std::cout;
using std::string;
using std::to_string;

struct FitPrinter {
  int rest;
  FitPrinter(int rest) : rest(rest) {}

  void Out(const std::string& msg) {
    rest -= msg.size();
    cout << msg;
  }
};

void pretty_print(const std::vector<std::variant<Invoke, Response>>& result) {
  auto get_thread_num = [](const std::variant<Invoke, Response>& v) {
    // Crutch.
    if (v.index() == 0) {
      return get<0>(v).thread_id;
    }
    return get<1>(v).thread_id;
  };

  int cell_width = 20;  // Up it if necessary. Enough for now.
  int threads_num = 0;
  for (const auto& i : result) {
    threads_num = std::max(threads_num, get_thread_num(i) + 1);
  }

  /*
      *--------------------------------------*
      |        T0        |        T1         |
      *--------------------------------------*
      | Push(2)          |                   |
      | Ok(2)            |                   |
      |                  |  Pop()            |
      |                  |  Ok(5)            |
      *--------------------------------------*
  */
  auto print_separator = [threads_num, cell_width]() {
    cout << "*";
    for (int i = 0; i < threads_num * cell_width + threads_num - 1; ++i) {
      cout << "-";
    }
    cout << "*\n";
  };

  auto print_spaces = [](int count) {
    for (int i = 0; i < count; ++i) {
      cout << " ";
    }
  };

  print_separator();
  // Header.
  cout << "|";
  for (int i = 0; i < threads_num; ++i) {
    int rest = cell_width - 1 /*T*/ - to_string(i).size();
    print_spaces(rest / 2);
    cout << "T" << i;
    print_spaces(rest - rest / 2);
    cout << "|";
  }
  cout << "\n";

  print_separator();

  auto print_empty_cell = [&]() {
    print_spaces(cell_width);
    cout << "|";
  };

  // Rows.
  for (const auto& i : result) {
    int num = get_thread_num(i);
    cout << "|";
    for (int j = 0; j < num; ++j) {
      print_empty_cell();
    }

    FitPrinter fp{cell_width};
    fp.Out(" ");
    if (i.index() == 0) {
      auto inv = get<0>(i);
      auto& task = inv.GetTask();
      fp.Out(task.GetName() + "(");
      auto& args = task.GetArgs();
      for (int i = 0; i < args.size(); ++i) {
        if (i > 0) {
          fp.Out(", ");
        }
        fp.Out(to_string(args[i]));
      }
      fp.Out(")");
    } else {
      auto resp = get<1>(i);
      fp.Out("Ok(" + to_string(resp.GetTask().GetRetVal()) + ")");
    }
    assert(fp.rest > 0);
    print_spaces(fp.rest);
    cout << "|";

    for (int j = 0; j < threads_num - num - 1; ++j) {
      print_empty_cell();
    }
    cout << "\n";
  }

  print_separator();
}

};  // namespace pretty_print
