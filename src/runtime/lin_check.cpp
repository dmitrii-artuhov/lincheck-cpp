#include <cassert>
#include <map>

#include "include/lincheck.h"
#include "include/scheduler.h"

// get_inv_res_mapping returns map (invoke_index -> corresponding
// response_index)
std::map<size_t, size_t> get_inv_res_mapping(
    const std::vector<std::variant<Invoke, Response>>& history) {
  std::map<size_t, size_t> inv_res;            // inv -> corresponding response
  std::map<const StackfulTask*, size_t> uids;  // uid -> res

  for (size_t i = 0; i < history.size(); ++i) {
    auto el = history[i];
    if (el.index() == 0) {
      const Invoke& inv = std::get<0>(el);
      uids[&inv.GetTask()] = i;
    } else {
      const Response& res = std::get<1>(el);
      assert(uids.find(&res.GetTask()) != uids.end());
      auto inv_id = uids[&res.GetTask()];
      inv_res[inv_id] = i;
    }
  }

  return inv_res;
}

// fix_history deletes invokes that don't have corresponding responses,
// this is allowed by the definition of the linearizability
std::vector<std::variant<Invoke, Response>> fix_history(
    const std::vector<std::variant<Invoke, Response>>& history) {
  auto indexes = get_inv_res_mapping(history);
  // Usually histories are small and erase invalidate an iterator, so it's
  // easier to create new history
  std::vector<std::variant<Invoke, Response>> new_history;

  for (size_t i = 0; i < history.size(); i++) {
    if (history[i].index() == 1 || indexes.contains(i)) {
      new_history.push_back(history[i]);
    }
  }

  return new_history;
}
