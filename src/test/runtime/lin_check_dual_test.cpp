#include <gtest/gtest.h>

#include <queue>

#include "include/lincheck.h"
#include "include/lincheck_dual.h"
#include "include/scheduler.h"
#include "stackfulltask_mock.h"

template <class T>
struct CoroutineQueue {
  struct SendPromise;
  struct ReceivePromise;

  CoroutineQueue()
      : receivers(std::queue<ReceivePromise>()),
        senders(std::queue<SendPromise>()) {}

  SendPromise Send(T elem) { return SendPromise(elem, *this); }

  ReceivePromise Receive() { return ReceivePromise(*this); }

  size_t ReceiversCount() { return receivers.size(); }

  struct ReceivePromise {
    explicit ReceivePromise(CoroutineQueue<T>& queue) : queue(queue) {
      elem = std::make_shared<std::optional<T>>(std::optional<T>(std::nullopt));
    }
    bool await_ready() { return false; }

    void await_suspend(std::coroutine_handle<> h) {
      if (!queue.senders.empty()) {
        SendPromise send_req = queue.senders.back();
        queue.senders.pop();
        *elem = send_req.elem;
        send_req.sender();
        h();
      } else {
        receiver = h;
        queue.receivers.push(*this);
      }
    }

    std::optional<T> await_resume() { return *elem; }

    std::coroutine_handle<> receiver;
    std::shared_ptr<std::optional<T>> elem;
    CoroutineQueue& queue;
  };

  struct SendPromise {
    SendPromise(T elem, CoroutineQueue<T>& queue) : elem(elem), queue(queue) {}

    bool await_ready() { return false; }

    void await_suspend(std::coroutine_handle<> h) {
      if (!queue.receivers.empty()) {
        ReceivePromise receiver = queue.receivers.back();
        queue.receivers.pop();
        *(receiver.elem) = elem;
        receiver.receiver();
        h();
      } else {
        sender = h;
        queue.senders.push(*this);
      }
    }

    int await_resume() { return 0; }

    std::coroutine_handle<> sender;
    T elem;
    CoroutineQueue& queue;
  };

 private:
  std::queue<ReceivePromise> receivers;
  std::queue<SendPromise> senders;
};

namespace LinearizabilityDualCheckerTest {
using ::testing::AnyNumber;
using ::testing::Return;

std::function<std::shared_ptr<BlockingMethod>(CoroutineQueue<int>*, void* args)>
    send = [](CoroutineQueue<int>* c,
              [[maybe_unused]] void* args) -> std::shared_ptr<BlockingMethod> {
  auto real_args = reinterpret_cast<std::tuple<int>*>(args);
  return std::shared_ptr<BlockingMethod>(
      new BlockingMethodWrapper<CoroutineQueue<int>::SendPromise>(
          c->Send(std::get<0>(*real_args))));
};
std::function<std::shared_ptr<BlockingMethod>(CoroutineQueue<int>*, void* args)>
    receive =
        [](CoroutineQueue<int>* c,
           [[maybe_unused]] void* args) -> std::shared_ptr<BlockingMethod> {
  return std::shared_ptr<BlockingMethod>(
      new BlockingMethodWrapper<CoroutineQueue<int>::ReceivePromise>(
          c->Receive()));
};

std::function<int(CoroutineQueue<int>*, void* args)> size =
    [](CoroutineQueue<int>* c, [[maybe_unused]] void* args) -> int {
  return c->ReceiversCount();
};

TEST(LinearizabilityDualCheckerQueueTest, SmallLinearizableHistory) {
  CoroutineQueue<int> q;
  LinearizabilityDualChecker<CoroutineQueue<int>> checker(
      LinearizabilityDualChecker<CoroutineQueue<int>>::MethodMap{
          {"send", send},
          {"receive", receive},
      },
      q);

  auto first_task_args = std::make_unique<std::tuple<int>>(std::tuple<int>{3});
  auto first_task = CreateMockStackfulTask(
      "send", 0, reinterpret_cast<void*>(first_task_arg.get()));

  auto second_task_args = std::make_unique<std::tuple<>>(std::tuple<>{});
  auto second_task = CreateMockStackfulTask(
      "receive", 3, reinterpret_cast<void*>(second_task_args.get()));

  std::vector<HistoryEvent> history{};
  history.emplace_back(RequestInvoke(*first_task, 0));
  history.emplace_back(RequestInvoke(*second_task, 1));
  history.emplace_back(RequestResponse(*first_task, 0, 0));
  history.emplace_back(RequestResponse(*second_task, 0, 1));
  history.emplace_back(FollowUpInvoke(*second_task, 1));
  history.emplace_back(FollowUpInvoke(*first_task, 0));
  history.emplace_back(FollowUpResponse(*first_task, 0, 0));
  history.emplace_back(FollowUpResponse(*second_task, 3, 1));

  EXPECT_EQ(checker.Check(history), true);
}

TEST(LinearizabilityDualCheckerQueueTest, SmallUnlinearizableHistory) {
  CoroutineQueue<int> q;
  LinearizabilityDualChecker<CoroutineQueue<int>> checker(
      LinearizabilityDualChecker<CoroutineQueue<int>>::MethodMap{
          {"send", send},
          {"receive", receive},
      },
      q);

  auto first_task_args = std::make_unique<std::tuple<int>>(std::tuple<int>{3});
  auto first_task = CreateMockStackfulTask(
      "send", 0, reinterpret_cast<void*>(first_task_args.get()));

  auto second_task_args = std::make_unique<std::tuple<>>(std::tuple<>{});
  auto second_task = CreateMockStackfulTask(
      "receive", 2, reinterpret_cast<void*>(second_task_args.get()));

  std::vector<HistoryEvent> history{};
  history.emplace_back(RequestInvoke(*first_task, 0));
  history.emplace_back(RequestInvoke(*second_task, 1));
  history.emplace_back(RequestResponse(*first_task, 0, 0));
  history.emplace_back(RequestResponse(*second_task, 0, 1));
  history.emplace_back(FollowUpInvoke(*second_task, 1));
  history.emplace_back(FollowUpInvoke(*first_task, 0));
  history.emplace_back(FollowUpResponse(*first_task, 0, 0));
  history.emplace_back(FollowUpResponse(*second_task, 2, 1));

  EXPECT_EQ(checker.Check(history), false);
}

TEST(LinearizabilityDualCheckerQueueTest, SmallUnlinearizableHistoryBadSend) {
  CoroutineQueue<int> q;
  LinearizabilityDualChecker<CoroutineQueue<int>> checker(
      LinearizabilityDualChecker<CoroutineQueue<int>>::MethodMap{
          {"send", send},
          {"receive", receive},
      },
      q);

  auto first_task_args = std::make_unique<std::tuple<int>>(std::tuple<int>{3});
  auto first_task = CreateMockStackfulTask(
      "send", 0, reinterpret_cast<void*>(first_task_args.get()));

  auto second_task_args = std::make_unique<std::tuple<>>(std::tuple<>{});
  auto second_task = CreateMockStackfulTask(
      "receive", 3, reinterpret_cast<void*>(second_task_args.get()));

  auto third_task_args = std::make_unique<std::tuple<>>(std::tuple<>{});
  auto third_task = CreateMockStackfulTask(
      "receive", 3, reinterpret_cast<void*>(third_task_args.get()));

  std::vector<HistoryEvent> history{};
  history.emplace_back(RequestInvoke(*first_task, 0));
  history.emplace_back(RequestInvoke(*second_task, 1));
  history.emplace_back(RequestResponse(*first_task, 0, 0));
  history.emplace_back(RequestResponse(*second_task, 0, 1));
  history.emplace_back(FollowUpInvoke(*second_task, 1));
  history.emplace_back(FollowUpInvoke(*first_task, 0));
  history.emplace_back(FollowUpResponse(*first_task, 0, 0));
  history.emplace_back(FollowUpResponse(*second_task, 2, 1));
  history.emplace_back(RequestInvoke(*third_task, 0));
  history.emplace_back(RequestResponse(*third_task, 0, 0));
  history.emplace_back(FollowUpInvoke(*third_task, 1));
  history.emplace_back(FollowUpResponse(*third_task, 3, 1));

  EXPECT_EQ(checker.Check(history), false);
}

TEST(LinearizabilityDualCheckerQueueTest,
     SmallLinearizableHistoryWithNonBlocking) {
  CoroutineQueue<int> q;
  LinearizabilityDualChecker<CoroutineQueue<int>> checker(
      LinearizabilityDualChecker<CoroutineQueue<int>>::MethodMap{
          {"send", send}, {"receive", receive}, {"size", size}},
      q);

  auto first_task_args = std::make_unique<std::tuple<>>(std::tuple<>{});
  auto first_task = CreateMockStackfulTask(
      "receive", 3, reinterpret_cast<void*>(first_task_args.get()));

  auto second_task_args = std::make_unique<std::tuple<int>>(std::tuple<int>{3});
  auto second_task = CreateMockStackfulTask(
      "send", 0, reinterpret_cast<void*>(second_task_args.get()));

  auto third_task_args = std::make_unique<std::tuple<>>(std::tuple<>{});
  auto third_task = CreateMockStackfulTask(
      "size", 1, reinterpret_cast<void*>(third_task_args.get()));

  std::vector<HistoryEvent> history{};
  history.emplace_back(Invoke(*third_task, 4));
  history.emplace_back(RequestInvoke(*first_task, 0));
  history.emplace_back(RequestInvoke(*second_task, 1));
  history.emplace_back(RequestResponse(*first_task, 0, 0));
  history.emplace_back(RequestResponse(*second_task, 0, 1));
  history.emplace_back(FollowUpInvoke(*second_task, 1));
  history.emplace_back(FollowUpInvoke(*first_task, 0));
  history.emplace_back(FollowUpResponse(*first_task, 3, 0));
  history.emplace_back(FollowUpResponse(*second_task, 0, 1));
  history.emplace_back(Response(*third_task, 1, 4));

  EXPECT_EQ(checker.Check(history), true);
}

};  // namespace LinearizabilityDualCheckerTest