#include <cassert>
#include <map>

#include "include/lincheck.h"
#include "include/scheduler.h"

std::map<size_t, size_t> get_inv_res_mapping(
    const std::vector<std::variant<StackfulTaskInvoke, StackfulTaskResponse>>&
        history) {
  std::map<size_t, size_t> inv_res;            // inv -> corresponding response
  std::map<const StackfulTask*, size_t> uids;  // uid -> res

  for (size_t i = 0; i < history.size(); ++i) {
    auto el = history[i];
    if (el.index() == 0) {
      const StackfulTaskInvoke& inv = std::get<0>(el);
      uids[&inv.task] = i;
    } else {
      const StackfulTaskResponse& res = std::get<1>(el);
      assert(uids.find(&res.task) != uids.end());
      auto inv_id = uids[&res.task];
      inv_res[inv_id] = i;
    }
  }

  return inv_res;
}