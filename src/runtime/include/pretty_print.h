#pragma once
#include <algorithm>
#include <cassert>
#include <iostream>
#include <string>
#include <variant>
#include <vector>

#include "lib.h"
#include "lincheck.h"

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
  void PrettyPrint(const std::vector<HistoryEvent>& result, Out_t& out) {
    auto get_thread_num = [](const HistoryEvent& v) {
      // Crutch.
      if (v.index() == 0) {
        return get<0>(v).thread_id;
      } else if (v.index() == 1) {
        return get<1>(v).thread_id;
      } else if (v.index() == 2) {
        return get<2>(v).thread_id;
      } else if (v.index() == 3) {
        return get<3>(v).thread_id;
      } else if (v.index() == 4) {
        return get<4>(v).thread_id;
      } else if (v.index() == 5) {
        return get<5>(v).thread_id;
      }
      throw std::invalid_argument("unknown event in get num");
      return 0;
    };

    int cell_width = 50;  // Up it if necessary. Enough for now.

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
      if (std::holds_alternative<Invoke>(i)) {
        // TODO: вынести этот код
        auto inv = std::get<Invoke>(i);
        auto& task = inv.GetTask();
        fp.Out(std::string{task->GetName()});
        fp.Out("(");
        const auto& args = task->GetStrArgs();
        for (size_t i = 0; i < args.size(); ++i) {
          if (i > 0) {
            fp.Out(", ");
          }
          fp.Out(args[i]);
        }
        fp.Out(")");
      } else if (std::holds_alternative<Response>(i)) {
        auto resp = std::get<Response>(i);
        fp.Out("<-- " + to_string(resp.GetTask()->GetRetVal()));
      } else if (std::holds_alternative<RequestInvoke>(i)) {
        auto inv = std::get<RequestInvoke>(i);
        const DualTask& task = inv.GetTask();
        fp.Out(std::string{task->GetName()});
        fp.Out("Dual (");
        const auto& args = task->GetStrArgs();
        for (size_t i = 0; i < args.size(); ++i) {
          if (i > 0) {
            fp.Out(", ");
          }
          fp.Out(args[i]);
        }
        fp.Out(")");
      } else if (std::holds_alternative<RequestResponse>(i)) {
        //        auto resp = std::get<RequestResponse>(i);
        fp.Out("<-- Request ended");
      } else if (std::holds_alternative<FollowUpInvoke>(i)) {
        fp.Out("FollowUpInvoke");
      } else if (std::holds_alternative<FollowUpResponse>(i)) {
        auto resp = std::get<FollowUpResponse>(i);
        fp.Out("<-- followUpResponse" + to_string(resp.GetTask()->GetRetVal()));
      } else {
        assert(false);
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
  //  template <typename Out_t>
  //  void PrettyPrint(
  //            const std::vector<std::pair<int, std::reference_wrapper<Task>>>&
  //            result,
  //      Out_t& out) {
  //    int cell_width = 20;  // Up it if necessary. Enough for now.
  //
  //    auto print_separator = [&out, this, cell_width]() {
  //      out << "*";
  //      for (int i = 0; i < threads_num; ++i) {
  //        for (int j = 0; j < cell_width; ++j) {
  //          out << "-";
  //        }
  //        out << "*";
  //      }
  //      out << "\n";
  //    };
  //    auto print_spaces = [&out](int count) {
  //      for (int i = 0; i < count; ++i) {
  //        out << " ";
  //      }
  //    };
  //
  //    int spaces = 7;
  //    print_spaces(spaces);
  //    print_separator();
  //
  //    // Header.
  //    print_spaces(spaces);
  //    out << "|";
  //    for (size_t i = 0; i < threads_num; ++i) {
  //      int rest = cell_width - 1 /*T*/ - to_string(i).size();
  //      print_spaces(rest / 2);
  //      out << "T" << i;
  //      print_spaces(rest - rest / 2);
  //      out << "|";
  //    }
  //    out << "\n";
  //
  //    print_spaces(spaces);
  //    print_separator();
  //
  //    auto print_empty_cell = [&]() {
  //      print_spaces(cell_width);
  //      out << "|";
  //    };
  //
  ////     TODO: Fix
  //        std::map<Task*, int> index;
  //
  //        // Rows.
  //        for (const auto& i : result) {
  //          auto base = i.second.get().get();
  //          if (index.find(base) == index.end()) {
  //            int sz = index.size();
  //            index[base] = sz;
  //          }
  //          int length = std::to_string(index[base]).size();
  //          std::cout << index[base];
  //          assert(spaces - length >= 0);
  //          print_spaces(7 - length);
  //          int num = i.first;
  //          out << "|";
  //          for (int j = 0; j < num; ++j) {
  //            print_empty_cell();
  //          }
  //
  //          FitPrinter fp{out, cell_width};
  //          fp.Out(" ");
  //          fp.Out(std::string{i.second.get()->GetName()});
  //          fp.Out("(");
  //          const auto& args = i.second.get()->GetStrArgs();
  //          for (int i = 0; i < args.size(); ++i) {
  //            if (i > 0) {
  //              fp.Out(", ");
  //            }
  //            fp.Out(args[i]);
  //          }
  //          fp.Out(")");
  //
  //          assert(fp.rest > 0 && "increase cell_width in pretty printer");
  //          print_spaces(fp.rest);
  //          out << "|";
  //
  //          for (size_t j = 0; j < threads_num - num - 1; ++j) {
  //            print_empty_cell();
  //          }
  //          out << "\n";
  //        }
  //
  //    print_spaces(spaces);
  //    print_separator();
  //  }

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
