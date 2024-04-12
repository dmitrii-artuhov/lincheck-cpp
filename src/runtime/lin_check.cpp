#include <cassert>
#include <map>
#include <set>

#include "include/lincheck.h"
#include "include/lincheck_dual.h"
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

// get_inv_res_mapping returns map (invoke_index -> corresponding
// response_index)
std::map<size_t, size_t> get_inv_res_mapping_full(
    const std::vector<HistoryEvent>& history) {
  std::map<size_t, size_t> inv_res;            // inv -> corresponding response
  std::map<const StackfulTask*, size_t> uids;  // uid -> res
  std::map<const StackfulTask*, size_t> follow_up_uids;
  std::map<const StackfulTask*, size_t> requests_uids;

  for (size_t i = 0; i < history.size(); ++i) {
    auto el = history[i];
    if (std::holds_alternative<Invoke>(history[i])) {
      const Invoke& inv = std::get<Invoke>(el);
      uids[&inv.GetTask()] = i;
    } else if (std::holds_alternative<RequestInvoke>(history[i])) {
      const RequestInvoke& inv = std::get<RequestInvoke>(el);
      requests_uids[&inv.GetTask()] = i;
    } else if (std::holds_alternative<FollowUpInvoke>(history[i])) {
      const FollowUpInvoke& inv = std::get<FollowUpInvoke>(el);
      follow_up_uids[&inv.GetTask()] = i;
    }  else if (std::holds_alternative<Response>(history[i])) {
      const Response& res = std::get<Response>(el);
      assert(uids.find(&res.GetTask()) != uids.end());
      auto inv_id = uids[&res.GetTask()];
      inv_res[inv_id] = i;
    } else if (std::holds_alternative<RequestResponse>(history[i])) {
      const RequestResponse& res = std::get<RequestResponse>(el);
      assert(requests_uids.find(&res.GetTask()) != requests_uids.end());
      auto inv_id = requests_uids[&res.GetTask()];
      inv_res[inv_id] = i;
    } else if (std::holds_alternative<FollowUpResponse>(history[i])) {
      const FollowUpResponse& res = std::get<FollowUpResponse>(el);
      assert(follow_up_uids.find(&res.GetTask()) != follow_up_uids.end());
      auto inv_id = follow_up_uids[&res.GetTask()];
      inv_res[inv_id] = i;
    } else {
      assert(false);
    }
  }

  return inv_res;
}

std::map<size_t, size_t> get_followup_request_mapping(const std::vector<HistoryEvent>& history) {
  std::map<const StackfulTask*, size_t> uids;
  std::map<size_t, size_t> inv_res;

  for (size_t i = 0; i < history.size(); ++i) {
    auto el = history[i];
    if (std::holds_alternative<RequestInvoke>(history[i])) {
      const RequestInvoke& inv = std::get<RequestInvoke>(el);
      uids[&inv.GetTask()] = i;
    } else if (std::holds_alternative<FollowUpResponse>(history[i])) {
      const FollowUpResponse& inv = std::get<FollowUpResponse>(el);
      inv_res[i] = uids[&inv.GetTask()];
    }
  }

  return inv_res;
}

// checks the following condition:
// if the blocking task made a request first, then the corresponding
// follow_up was the first follow_up
// history have to be sequential history!
bool is_fifo(const std::vector<std::variant<Invoke, Response>>& history) {
  // request_inv -> follow_up_res
  std::vector<std::pair<size_t, size_t>> answered_blocking_tasks;
  std::map<const StackfulTask*, size_t> starts_id;

  for (size_t i = 0; i < history.size(); ++i) {
    if (history[i].index() == 0) {
      auto inv = std::get<Invoke>(history[i]);
      auto& task = inv.GetTask();

      if (!task.IsBlocking()) {
        // Have no interest in non-blocking tasks
        continue;
      }

      if (!starts_id.contains(&task)) {
        // Start of the request section
        starts_id[&task] = i;
      }
      // Already have seen this task before, so
      // now we are considering the follow_up section start,
      // but we don't need a follow_up start
    } else {
      auto res = std::get<Response>(history[i]);
      auto& task = res.GetTask();

      // Have to duplicate this code, because of std::variant
      if (!task.IsBlocking()) {
        // Have no interest in non-blocking tasks
        continue;
      }

      // Can do it because the history is a sequential history
      if (starts_id[&task] != i-1) {
        // the previous event is not the start of the request!
        answered_blocking_tasks.emplace_back(starts_id[&task], i);
      }
    }
  }

  if (answered_blocking_tasks.empty()) {
    return true;
  }

  // Sort is required, add elements by follow_up end
  std::sort(answered_blocking_tasks.begin(), answered_blocking_tasks.end());
  size_t previous = answered_blocking_tasks[0].second;

  for (size_t i = 1; i < answered_blocking_tasks.size(); ++i) {
    if (previous > answered_blocking_tasks[i].second) {
      return false;
    }
    previous = answered_blocking_tasks[i].second;
  }

  return true;
}
