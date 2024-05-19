#include <cassert>
#include <map>
#include <set>

#include "include/lincheck.h"
#include "include/lincheck_dual.h"
#include "include/scheduler.h"

// get_inv_res_mapping returns map (invoke_index -> corresponding
// response_index)
std::map<size_t, size_t> get_inv_res_mapping(
    const std::vector<std::variant<Invoke, Response>> &history) {
  std::map<size_t, size_t> inv_res;     // inv -> corresponding response
  std::map<const Task *, size_t> uids;  // uid -> res

  for (size_t i = 0; i < history.size(); ++i) {
    auto el = history[i];
    if (el.index() == 0) {
      const Invoke &inv = std::get<0>(el);
      uids[&inv.GetTask()] = i;
    } else {
      const Response &res = std::get<1>(el);
      assert(uids.find(&res.GetTask()) != uids.end());
      auto inv_id = uids[&res.GetTask()];
      inv_res[inv_id] = i;
    }
  }

  return inv_res;
}

<<<<<<< HEAD
// get_inv_res_mapping returns map (invoke_index -> corresponding
// response_index)
std::map<size_t, size_t> get_inv_res_full_mapping(
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
    } else if (std::holds_alternative<Response>(history[i])) {
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

std::map<size_t, size_t> get_followup_res_request_inv_mapping(
    const std::vector<HistoryEvent>& history) {
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
=======
Invoke::Invoke(const Task &task, int thread_id)
    : task(task), thread_id(thread_id) {}

Response::Response(const Task &task, int result, int thread_id)
    : task(task), result(result), thread_id(thread_id) {}

const Task &Invoke::GetTask() const { return this->task; }

const Task &Response::GetTask() const { return this->task; }
>>>>>>> rewrite-coroutines
