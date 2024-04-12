#pragma once

#include <functional>
#include <numeric>
#include <iostream>
#include "lincheck.h"

struct BlockingMethodWrapper {
  virtual void StartRequest() = 0;
  // Returns is followup finished successfully
  virtual bool IsFinished() = 0;
  virtual int GetResult() = 0;
  virtual ~BlockingMethodWrapper() = default;
};

// PromiseType have to be awaitable object
template<class PromiseType>
struct BlockingMethodWrapperParam : BlockingMethodWrapper {
  explicit BlockingMethodWrapperParam(PromiseType promise) : promise(promise) {}

  // один ответил на другой, в каком порядке класть ответы?
  void StartRequest() override {
    StartRequestCoroutine();
  }

  bool IsFinished() override {
    return result != std::nullopt;
  }

  int GetResult() override {
    assert(result != std::nullopt);
    return result.value();
  }

private:
  struct CoroutineResponse;

  // Need this coroutine
  CoroutineResponse StartRequestCoroutine() {
    result = co_await promise;
  }

  struct CoroutineResponse {
    struct promise_type {
      CoroutineResponse get_return_object() { return {}; }
      std::suspend_never initial_suspend() { return {}; }
      std::suspend_never final_suspend() noexcept { return {}; }
      void unhandled_exception() {}
    };
  };

  PromiseType promise;
  std::optional<int> result;
};

// TODO: санитайзер
// This is the simplest wgl version, it doesn't contain any optimizations, it's
// slow but useful for stress tests of other implementations
template <class LinearSpecificationObject,
          class SpecificationObjectHash = std::hash<LinearSpecificationObject>,
          class SpecificationObjectEqual =
              std::equal_to<LinearSpecificationObject>>
struct LinearizabilityDualChecker {
  using BlockingMethodFactory = std::function<std::shared_ptr<BlockingMethodWrapper>(LinearSpecificationObject*,
                                                                 const std::vector<int>& args)>;

  using NonBlockingMethod = std::function<int(LinearSpecificationObject*,
                                     const std::vector<int>& args)>;

  using Method = std::variant<NonBlockingMethod, BlockingMethodFactory>;
  using MethodMap = std::map<MethodName, Method>;

  LinearizabilityDualChecker() = delete;

  LinearizabilityDualChecker(MethodMap specification_methods,
                                  LinearSpecificationObject first_state);

  bool Check(const std::vector<HistoryEvent>& fixed_history);

private:

  // pair - event + event's index in the original history
  std::pair<LinearSpecificationObject, std::map<size_t, std::shared_ptr<BlockingMethodWrapper>>> ReproduceSeqHistory(std::vector<std::pair<HistoryEvent, size_t>> &seq_history);

  std::map<MethodName, Method> specification_methods;
  LinearSpecificationObject first_state;
};

template <class LinearSpecificationObject, class SpecificationObjectHash,
          class SpecificationObjectEqual>
LinearizabilityDualChecker<LinearSpecificationObject,
                                SpecificationObjectHash,
                                SpecificationObjectEqual>::
    LinearizabilityDualChecker(
        LinearizabilityDualChecker::MethodMap specification_methods,
        LinearSpecificationObject first_state)
    : specification_methods(specification_methods), first_state(first_state) {
  if (!std::is_copy_assignable_v<LinearSpecificationObject>) {
    // TODO: should do it in the compile time
    throw std::invalid_argument(
        "LinearSpecificationObject type have to be is_copy_assignable_v");
  }
}

// TODO: have to restore history each time, because coroutine handles copies as pointers
template <class LinearSpecificationObject, class SpecificationObjectHash,
          class SpecificationObjectEqual>
bool LinearizabilityDualChecker<LinearSpecificationObject,
                                     SpecificationObjectHash,
                                     SpecificationObjectEqual>::
    Check(const std::vector<HistoryEvent>& history) {
  // It's a crunch, but it's required because the semantics of this
  // implementation must be the same as the semantics of the non-recursive
  // implementation
  if (history.empty()) {
    return true;
  }

  // TODO: такая же мапа для блокирующих вызовов, индекс request invoke -> follow_up response
  std::map<size_t, size_t> followup_request = get_followup_request_mapping(history);
  std::map<size_t, size_t> inv_res = get_inv_res_mapping_full(history);
  std::map<size_t, std::shared_ptr<BlockingMethodWrapper>> dual_requests;
  std::vector<std::pair<HistoryEvent, size_t>> seq_history;

  std::function<bool(const std::vector<HistoryEvent>&,
                     std::vector<bool>&, LinearSpecificationObject)>
      recursive_step;

  recursive_step =
      [&](const std::vector<HistoryEvent>& history,
          std::vector<bool>& linearized,
          LinearSpecificationObject data_structure_state) -> bool {
    // the fixed_history is empty
    if (std::reduce(linearized.begin(), linearized.end(), true,
                    std::bit_and<>())) {
      return true;
    }

    // walk all minimal operations
    for (size_t i = 0; i < history.size(); ++i) {
      // we could think that fixed_history doesn't contain events that already
      // have been linearized
      if (linearized[i]) {
        continue;
      }

      // TODO: pattern matching
      // all next operations are not minimal
      if (history[i].index() == 1 || history[i].index() == 3 || history[i].index() == 5) {
        break;
      }

      // nonblocking method
      if (std::holds_alternative<Invoke>(history[i])) {
        Invoke minimal_op = std::get<Invoke>(history[i]);
        NonBlockingMethod method =
            std::get<NonBlockingMethod>(specification_methods.find(minimal_op.GetTask().GetName())->second);

        LinearSpecificationObject data_structure_state_copy =
            data_structure_state;
        // state is already have been copied, because it's the argument of the
        // lambda
        int res =
            method(&data_structure_state_copy, minimal_op.GetTask().GetArgs());

        // If invoke doesn't have a response we can't check the response
        if (inv_res.find(i) == inv_res.end()) {
          linearized[i] = true;
          seq_history.emplace_back(history[i], i);
          if (recursive_step(history, linearized, data_structure_state_copy)) {
            return true;
          }
          linearized[i] = false;
          seq_history.pop_back();
          std::tie(data_structure_state, dual_requests) = ReproduceSeqHistory(seq_history);
          continue;
        }

        // TODO: только блокирующие методы могут влиять на блокирубющиеся методы
        if (res == minimal_op.GetTask().GetRetVal()) {
          linearized[i] = true;
          seq_history.emplace_back(history[i], i);
          assert(inv_res.find(i) != inv_res.end());
          linearized[inv_res[i]] = true;
          seq_history.emplace_back(history[inv_res[i]], inv_res[i]);

          if (recursive_step(history, linearized, data_structure_state_copy)) {
            return true;
          } else {
            linearized[i] = false;
            seq_history.pop_back();
            linearized[inv_res[i]] = false;
            seq_history.pop_back();
            std::tie(data_structure_state, dual_requests) = ReproduceSeqHistory(seq_history);
          }
        }
      } else if (std::holds_alternative<RequestInvoke>(history[i])) {
        RequestInvoke minimal_op = std::get<RequestInvoke>(history[i]);
        // Request invoke
        BlockingMethodFactory mf = std::get<BlockingMethodFactory>(specification_methods.find(minimal_op.GetTask().GetName())->second);
        LinearSpecificationObject data_structure_state_copy =
            data_structure_state;
        std::shared_ptr<BlockingMethodWrapper> method = mf(&data_structure_state_copy, minimal_op.GetTask().GetArgs());
        method->StartRequest();
        dual_requests[i] = method;

        linearized[i] = true;
        seq_history.emplace_back(history[i], i);
        if (inv_res.find(i) != inv_res.end()) {
          linearized[inv_res[i]] = true;
          seq_history.emplace_back(history[inv_res[i]], inv_res[i]);
        }
        if (recursive_step(history, linearized, data_structure_state_copy)) {
          return true;
        }
        seq_history.pop_back();
        linearized[i] = false;
        if (inv_res.find(i) != inv_res.end()) {
          linearized[inv_res[i]] = false;
          seq_history.pop_back();
        }
        std::tie(data_structure_state, dual_requests) = ReproduceSeqHistory(seq_history);
      } else if (std::holds_alternative<FollowUpInvoke>(history[i])) {
        auto [a, b] = ReproduceSeqHistory(seq_history);
        dual_requests.clear();
        std::tie(data_structure_state, dual_requests) = ReproduceSeqHistory(seq_history);

        FollowUpInvoke minimal_op = std::get<FollowUpInvoke>(history[i]);
        size_t followup_response_index = inv_res[i];
        size_t request_index = followup_request[followup_response_index];
        assert(dual_requests.find(request_index) != dual_requests.end());
        std::shared_ptr<BlockingMethodWrapper> method = dual_requests[request_index];

        // If doesn't ready just keep execution
        if (method->IsFinished() && method->GetResult() == minimal_op.GetTask().GetRetVal()) {
          linearized[i] = true;
          // TODO: может не быть ответа, проверяем неполные истории
          seq_history.emplace_back(history[i], i);
          linearized[inv_res[i]] = true;
          seq_history.emplace_back(history[inv_res[i]], inv_res[i]);

          if (recursive_step(history, linearized, data_structure_state)) {
            return true;
          } else {
            // TODO: тут можно не откатывать состояние
            linearized[i] = false;
            seq_history.pop_back();
            linearized[inv_res[i]] = false;
            seq_history.pop_back();
            std::tie(data_structure_state, dual_requests) = ReproduceSeqHistory(seq_history);
          }
        }
      }
    }

    return false;
  };

  std::vector<bool> linearized(history.size(), false);
  return recursive_step(history, linearized, first_state);
}

template <class LinearSpecificationObject, class SpecificationObjectHash,
          class SpecificationObjectEqual>
std::pair<LinearSpecificationObject, std::map<size_t, std::shared_ptr<BlockingMethodWrapper>>> LinearizabilityDualChecker<LinearSpecificationObject,
                                SpecificationObjectHash,
                                SpecificationObjectEqual>::ReproduceSeqHistory(std::vector<std::pair<HistoryEvent, size_t>> &seq_history) {
  // Copy first state
  LinearSpecificationObject state = first_state;
  std::map<size_t, std::shared_ptr<BlockingMethodWrapper>> new_stack;

  for (auto event_pair : seq_history) {
    if (std::holds_alternative<Invoke>(event_pair.first)) {
      Invoke op = std::get<Invoke>(event_pair.first);

      NonBlockingMethod method =
          std::get<NonBlockingMethod>(specification_methods.find(op.GetTask().GetName())->second);

      method(&state, op.GetTask().GetArgs());
    } else if (std::holds_alternative<RequestInvoke>(event_pair.first)) {
      RequestInvoke op = std::get<RequestInvoke>(event_pair.first);

      BlockingMethodFactory mf = std::get<BlockingMethodFactory>(specification_methods.find(op.GetTask().GetName())->second);
      std::shared_ptr<BlockingMethodWrapper> method = mf(&state, op.GetTask().GetArgs());
      method->StartRequest();
      new_stack[event_pair.second] = method;
    }
  }
  if (new_stack.size() >= 1) {
    std::cout << new_stack[1]->IsFinished() << std::endl;
  }
  return {state, new_stack};
}
