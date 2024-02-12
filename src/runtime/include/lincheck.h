#pragma once
#include <any>
#include <cassert>
#include <functional>
#include <map>
#include <stdexcept>
#include <unordered_set>

#include "lib.h"
#include "scheduler.h"

using MethodName = std::string;

template <class LinearSpecificationObject,
          class SpecificationObjectHash = std::hash<LinearSpecificationObject>,
          class SpecificationObjectEqual =
              std::equal_to<LinearSpecificationObject>>
struct LinearizabilityChecker : ModelChecker {
  LinearizabilityChecker() = delete;

  LinearizabilityChecker(
      std::map<MethodName, std::function<int(LinearSpecificationObject*)>>
          specification_methods,
      LinearSpecificationObject first_state);

  bool Check(
      const std::vector<std::variant<StackfulTaskInvoke, StackfulTaskResponse>>&
          history) override;

 private:
  std::map<MethodName, std::function<int(LinearSpecificationObject*)>>
      specification_methods;
  LinearSpecificationObject first_state;
};

std::map<size_t, size_t> get_inv_res_mapping(
    const std::vector<std::variant<StackfulTaskInvoke, StackfulTaskResponse>>&
        history);

template <class LinearSpecificationObject,
          class SpecificationObjectHash = std::hash<LinearSpecificationObject>>
struct PairHash {
  std::uint64_t operator()(
      const std::pair<std::vector<bool>, LinearSpecificationObject>& pair)
      const {
    std::uint64_t vector_hash = std::hash<std::vector<bool>>{}(pair.first);
    std::uint64_t object_hash = SpecificationObjectHash{}(pair.second);

    return vector_hash ^ object_hash;
  }
};

template <class LinearSpecificationObject,
          class SpecificationObjectEqual =
              std::equal_to<LinearSpecificationObject>>
struct PairEqual {
  constexpr bool operator()(
      const std::pair<std::vector<bool>, LinearSpecificationObject>& lhs,
      const std::pair<std::vector<bool>, LinearSpecificationObject>& rhs)
      const {
    return lhs.first == rhs.first &&
           SpecificationObjectEqual{}(lhs.second, rhs.second);
  }
};

template <class LinearSpecificationObject, class SpecificationObjectHash,
          class SpecificationObjectEqual>
LinearizabilityChecker<LinearSpecificationObject, SpecificationObjectHash,
                       SpecificationObjectEqual>::
    LinearizabilityChecker(
        std::map<MethodName, std::function<int(LinearSpecificationObject*)>>
            specification_methods,
        LinearSpecificationObject first_state)
    : specification_methods(specification_methods), first_state(first_state) {
  if (!std::is_copy_assignable_v<LinearSpecificationObject>) {
    // TODO: should do it in the compile time
    throw std::invalid_argument(
        "LinearSpecificationObject type have to be is_copy_assignable_v");
  }
}

// Implements the wgl linearizability checker,
// http://www.cs.ox.ac.uk/people/gavin.lowe/LinearizabiltyTesting/
// https://arxiv.org/pdf/1504.00204.pdf
template <class LinearSpecificationObject, class SpecificationObjectHash,
          class SpecificationObjectEqual>
bool LinearizabilityChecker<LinearSpecificationObject, SpecificationObjectHash,
                            SpecificationObjectEqual>::
    Check(const std::vector<
          std::variant<StackfulTaskInvoke, StackfulTaskResponse>>& history) {
  // head entry
  size_t current_section_start = 0;
  LinearSpecificationObject data_structure_state = first_state;
  // indexes of invokes
  std::vector<size_t> open_sections_stack;
  // TODO: Can replace it with stack of hashes and map: hash ->
  // LinearSpecificationObject contains previous states stack
  std::vector<LinearSpecificationObject> states_stack;
  std::map<size_t, size_t> inv_res = get_inv_res_mapping(history);
  std::vector<bool> linearized(history.size(), false);
  std::unordered_set<
      std::pair<std::vector<bool>, LinearSpecificationObject>,
      PairHash<LinearSpecificationObject, SpecificationObjectHash>,
      PairEqual<LinearSpecificationObject, SpecificationObjectEqual>>
      states_cache;

  while (open_sections_stack.size() != history.size() / 2) {
    // This event is already in the stack, don't need lift function with this
    // predicate
    if (linearized[current_section_start]) {
      current_section_start++;
    }

    // Current event is an invoke event
    if (history[current_section_start].index() == 0) {
      // invoke
      const StackfulTaskInvoke& inv =
          std::get<0>(history[current_section_start]);
      assert(specification_methods.find(inv.task.GetName()) !=
             specification_methods.end());
      auto method = specification_methods.find(inv.task.GetName())->second;
      // apply method
      bool was_checked = false;
      LinearSpecificationObject data_structure_state_copy =
          data_structure_state;
      int res = method(&data_structure_state_copy);

      if (res == inv.task.GetRetVal()) {
        // We can append this event to a linearization
        linearized[current_section_start] = true;
        assert(inv_res.find(current_section_start) != inv_res.end());
        linearized[inv_res[current_section_start]] = true;

        was_checked =
            states_cache.find({linearized, data_structure_state_copy}) !=
            states_cache.end();
        if (!was_checked) {
          states_cache.insert({linearized, data_structure_state_copy});
        } else {
          // already checked equal state, don't want to Check it again
          linearized[current_section_start] = false;
          assert(inv_res.find(current_section_start) != inv_res.end());
          linearized[inv_res[current_section_start]] = false;
        }
      }

      // haven't seen this state previously, so continue procedure with this new
      // state
      if (res == inv.task.GetRetVal() && !was_checked) {
        // open section
        open_sections_stack.push_back(current_section_start);
        states_stack.push_back(data_structure_state);
        // update the structure state
        data_structure_state = data_structure_state_copy;
        // Now we have to try to add previously skipped entries
        current_section_start = 0;
      } else {
        // already seen this state (linearization part and state) or not
        // linearizable, anyway should go to the next one can't append this
        // entry now, have to go to the next one
        current_section_start++;
      }
    } else {
      // A response event
      // If we see a response, then next operations are not minimal operations
      if (open_sections_stack.empty()) {
        return false;
      }

      // update the data structure state
      data_structure_state = states_stack.back();
      states_stack.pop_back();

      size_t last_inv = open_sections_stack.back();
      current_section_start = last_inv;
      linearized[last_inv] = false;
      linearized[inv_res[last_inv]] = false;

      open_sections_stack.pop_back();
      // last time we started with the previous one
      // now want to start with the next
      current_section_start++;
    }
  }

  return true;
}