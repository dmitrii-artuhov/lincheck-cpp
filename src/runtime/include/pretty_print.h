#pragma once
#include <algorithm>
#include <cassert>
#include <iostream>
#include <string>
#include <variant>
#include <vector>

#include "lib.h"

using std::string;
using std::to_string;

struct PrettyPrinter {
  PrettyPrinter(size_t threads_num);

  /*
      Prints like this:
      *------------------*-------------------*
      |        T0        |        T1         |
      *------------------*-------------------*
      | Push(2)          |                   |
      | <--- 0           |                   |
      |                  |  Pop()            |
      |                  |  <--- 5           |
      *------------------*-------------------*
       <---------------->
           cell_width
  */
  template <typename Out_t>
  void PrettyPrint(const std::vector<std::variant<Invoke, Response>>& result,
                   Out_t& out) {
    auto get_thread_num = [](const std::variant<Invoke, Response>& v) {
      // Crutch.
      if (v.index() == 0) {
        return get<0>(v).thread_id;
      }
      return get<1>(v).thread_id;
    };

    int cell_width = 20;  // Up it if necessary. Enough for now.

    auto print_separator = [&out, this, cell_width]() {
      out << "*";
      for (int i = 0; i < threads_num; ++i) {
        for (int j = 0; j < cell_width; ++j) {
          out << "-";
        }
        out << "*";
      }
      out << "\n";
    };

    auto print_spaces = [&out](int count) {
      for (int i = 0; i < count; ++i) {
        out << " ";
      }
    };

    print_separator();
    // Header.
    out << "|";
    for (size_t i = 0; i < threads_num; ++i) {
      int rest = cell_width - 1 /*T*/ - to_string(i).size();
      print_spaces(rest / 2);
      out << "T" << i;
      print_spaces(rest - rest / 2);
      out << "|";
    }
    out << "\n";

    print_separator();

    auto print_empty_cell = [&]() {
      print_spaces(cell_width);
      out << "|";
    };

    // Rows.
    for (const auto& i : result) {
      int num = get_thread_num(i);
      out << "|";
      for (int j = 0; j < num; ++j) {
        print_empty_cell();
      }

      FitPrinter fp{out, cell_width};
      fp.Out(" ");
      if (i.index() == 0) {
        auto inv = get<0>(i);
        auto& task = inv.GetTask();
        fp.Out(task.GetName() + "(");
        const auto& args = task.GetStrArgs();
        for (size_t i = 0; i < args.size(); ++i) {
          if (i > 0) {
            fp.Out(", ");
          }
          fp.Out(args[i]);
        }
        fp.Out(")");
      } else {
        auto resp = get<1>(i);
        fp.Out("<-- " + to_string(resp.GetTask().GetRetVal()));
      }
      assert(fp.rest > 0 && "increase cell_width in pretty printer");
      print_spaces(fp.rest);
      out << "|";

      for (size_t j = 0; j < threads_num - num - 1; ++j) {
        print_empty_cell();
      }
      out << "\n";
    }

    print_separator();
  }

  // Helps to debug full histories.
  template <typename Out_t>
  void PrettyPrint(
      const std::vector<std::pair<int, std::reference_wrapper<StackfulTask>>>&
          result,
      Out_t& out) {
    int cell_width = 20;  // Up it if necessary. Enough for now.

    auto print_separator = [&out, this, cell_width]() {
      out << "*";
      for (int i = 0; i < threads_num; ++i) {
        for (int j = 0; j < cell_width; ++j) {
          out << "-";
        }
        out << "*";
      }
      out << "\n";
    };
    auto print_spaces = [&out](int count) {
      for (int i = 0; i < count; ++i) {
        out << " ";
      }
    };

    print_separator();
    // Header.
    out << "|";
    for (size_t i = 0; i < threads_num; ++i) {
      int rest = cell_width - 1 /*T*/ - to_string(i).size();
      print_spaces(rest / 2);
      out << "T" << i;
      print_spaces(rest - rest / 2);
      out << "|";
    }
    out << "\n";

    print_separator();

    auto print_empty_cell = [&]() {
      print_spaces(cell_width);
      out << "|";
    };

    // Rows.
    for (const auto& i : result) {
      int num = i.first;
      out << "|";
      for (int j = 0; j < num; ++j) {
        print_empty_cell();
      }

      FitPrinter fp{out, cell_width};
      fp.Out(" ");
      fp.Out(i.second.get().GetName());
      assert(fp.rest > 0 && "increase cell_width in pretty printer");
      print_spaces(fp.rest);
      out << "|";

      for (size_t j = 0; j < threads_num - num - 1; ++j) {
        print_empty_cell();
      }
      out << "\n";
    }

    print_separator();
  }

 private:
  // Counts how much symbols is left after printing.
  template <typename Out_t>
  struct FitPrinter {
    Out_t& out;
    int rest;
    FitPrinter(Out_t& out, int rest) : out(out), rest(rest) {}

    void Out(const std::string& msg) {
      rest -= msg.size();
      out << msg;
    }
  };
  size_t threads_num;
};
