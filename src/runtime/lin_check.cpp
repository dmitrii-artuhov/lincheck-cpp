#include <cassert>
#include <map>
#include <set>

#include "include/lincheck.h"
#include "include/lincheck_dual.h"

// get_inv_res_mapping returns map (invoke_index -> corresponding
// response_index)
std::map<size_t, size_t> get_inv_res_mapping(
    const std::vector<HistoryEvent>& history) {
  std::map<size_t, size_t> inv_res;      // inv -> corresponding response
  std::map<Task, size_t> uids;  // uid -> res

  for (size_t i = 0; i < history.size(); ++i) {
    auto el = history[i];
    if (std::holds_alternative<Invoke>(el)) {
      const Invoke& inv = std::get<Invoke>(el);
      uids[inv.GetTask()] = i;
    } else if (std::holds_alternative<Response>(el)) {
      const Response& res = std::get<Response>(el);
//      std::cout << "size: " << history.size() << " i " << i << std::endl;
//      if (uids.find(res.GetTask()) == uids.end()) {
//        for (auto& h : history) {
//          if (std::holds_alternative<Invoke>(h)) {
//            const Invoke& inv = std::get<Invoke>(h);
//            std::cout << "inv " << inv.GetTask().get() << " "
//                      << inv.GetTask()->GetName() << std::endl;
//          } else {
//            const Response& res1 = std::get<Response>(h);
//            std::cout << "res " << res1.GetTask().get() << " "
//                      << res1.GetTask()->GetName() << std::endl;
//          }
//        }
//        std::cout << "aboba " << i << " " << res.GetTask().get() << std::endl;
//      }
      assert(uids.find(res.GetTask()) != uids.end());
      auto inv_id = uids[res.GetTask()];
      inv_res[inv_id] = i;
    } else {
      assert("undefined");
    }
  }

  return inv_res;
}

// get_inv_res_mapping returns map (invoke_index -> corresponding
// response_index)
std::map<size_t, size_t> get_inv_res_full_mapping(
    const std::vector<HistoryEvent>& history) {
  std::map<size_t, size_t> inv_res{};      // inv -> corresponding response
  std::map<Task, size_t> uids{};  // uid -> res
  std::map<DualTask, size_t> follow_up_uids{};
  std::map<DualTask, size_t> requests_uids{};

  for (size_t i = 0; i < history.size(); ++i) {
    auto event = history[i];
    if (std::holds_alternative<Invoke>(event)) {
      const Invoke& inv = std::get<Invoke>(event);
      uids[inv.GetTask()] = i;
    } else if (std::holds_alternative<RequestInvoke>(event)) {
      const RequestInvoke& inv = std::get<RequestInvoke>(event);
      requests_uids[inv.GetTask()] = i;
    } else if (std::holds_alternative<FollowUpInvoke>(event)) {
      const FollowUpInvoke& inv = std::get<FollowUpInvoke>(event);
      follow_up_uids[inv.GetTask()] = i;
    } else if (std::holds_alternative<Response>(event)) {
      const Response& res = std::get<Response>(event);
      assert(uids.find(res.GetTask()) != uids.end());
      auto inv_id = uids[res.GetTask()];
      inv_res[inv_id] = i;
    } else if (std::holds_alternative<RequestResponse>(event)) {
      const RequestResponse& res = std::get<RequestResponse>(event);
      assert(requests_uids.find(res.GetTask()) != requests_uids.end());
      auto inv_id = requests_uids[res.GetTask()];
      inv_res[inv_id] = i;
    } else if (std::holds_alternative<FollowUpResponse>(event)) {
      const FollowUpResponse& res = std::get<FollowUpResponse>(event);
      assert(follow_up_uids.find(res.GetTask()) != follow_up_uids.end());
      auto inv_id = follow_up_uids[res.GetTask()];
      inv_res[inv_id] = i;
    } else {
      assert(false);
    }
  }

  return inv_res;
}

std::map<size_t, size_t> get_followup_res_request_inv_mapping(
    const std::vector<HistoryEvent>& history) {
  std::map<DualTask, size_t> uids;
  std::map<size_t, size_t> inv_res;

  for (size_t i = 0; i < history.size(); ++i) {
    auto el = history[i];
    if (std::holds_alternative<RequestInvoke>(history[i])) {
      const RequestInvoke& inv = std::get<RequestInvoke>(el);
      uids[inv.GetTask()] = i;
    } else if (std::holds_alternative<FollowUpResponse>(history[i])) {
      const FollowUpResponse& inv = std::get<FollowUpResponse>(el);
      inv_res[i] = uids[inv.GetTask()];
    }
  }

  return inv_res;
}

Invoke::Invoke(const Task& task, int thread_id)
    : task(std::cref(task)), thread_id(thread_id) {}

Response::Response(const Task& task, int result, int thread_id)
    : task(std::cref(task)), result(result), thread_id(thread_id) {}

RequestInvoke::RequestInvoke(const DualTask& task, int thread_id)
    : thread_id(thread_id), task(std::cref(task)) {}

RequestResponse::RequestResponse(const DualTask &task, int thread_id)
    : thread_id(thread_id), task(std::cref(task)) {}

FollowUpInvoke::FollowUpInvoke(const DualTask& task, int thread_id)
    : thread_id(thread_id), task(std::cref(task)) {}

FollowUpResponse::FollowUpResponse(const DualTask& task, int thread_id)
    : thread_id(thread_id), task(std::cref(task)) {}

const Task& Invoke::GetTask() const { return this->task.get(); }

const Task& Response::GetTask() const { return this->task.get(); }

const DualTask& RequestInvoke::GetTask() const { return this->task.get(); }

const DualTask& RequestResponse::GetTask() const { return this->task.get(); }

const DualTask& FollowUpInvoke::GetTask() const { return this->task.get(); }

const DualTask& FollowUpResponse::GetTask() const { return this->task.get(); }


