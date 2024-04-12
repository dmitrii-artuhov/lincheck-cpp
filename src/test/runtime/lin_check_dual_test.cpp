#include <gtest/gtest.h>
#include <queue>

#include "include/lincheck.h"
#include "include/lincheck_dual.h"
#include "include/scheduler.h"
#include "stackfulltask_mock.h"

// TODO: поправить эту очередь, понять где и что копируется из-за чего в первой версии 0 в a при другом порядке запрсов
template<class T>
struct CoroutineQueue {
  struct SendPromise {
    SendPromise(T elem, CoroutineQueue<T> &queue): elem(elem), queue(queue) {}

    bool await_ready() { return false; }

    void await_suspend(std::coroutine_handle<> h) {
      if (!queue.receivers.empty()) {
        ReceivePromise receiver = queue.receivers.back();
        queue.receivers.pop();
        //   receiver.elem = new T();
        *receiver.elem = elem;
        receiver.receiver();
        h();
      } else {
        sender = h;
        queue.senders.push(*this);
      }
    }

    int await_resume() {
      return 0;
    }

    std::coroutine_handle<> sender;
    T elem;
    CoroutineQueue &queue;
  };

  struct ReceivePromise {
    explicit ReceivePromise(CoroutineQueue<T> &queue) : queue(queue) {

    }
    bool await_ready() { return false; }

    void await_suspend(std::coroutine_handle<> h) {
      elem = new T();
      if (!queue.senders.empty()) {
        SendPromise send_req = queue.senders.back();
        queue.senders.pop();
         *elem = send_req.elem;
        send_req.sender();
        h();
      } else {
        receiver = h;
//        elem = new T();
        queue.receivers.push(*this);
      }
    }

    T await_resume() {
      return *elem;
    }

    std::coroutine_handle<> receiver;
    T* elem;
    CoroutineQueue &queue;
  };

  CoroutineQueue(): receivers(std::queue<ReceivePromise>()), senders(std::queue<SendPromise>()) {}

  SendPromise Send(T elem) {
    return SendPromise(elem, *this);
  }

  ReceivePromise Receive() {
    return ReceivePromise(*this);
  }

  int ReceiversCount() {
    return receivers.size();
  }

  std::queue<ReceivePromise> receivers;
  std::queue<SendPromise> senders;
};

namespace LinearizabilityDualCheckerTest {
using ::testing::AnyNumber;
using ::testing::Return;

TEST(LinearizabilityDualCheckerQueueTest, SmallLinearizableHistory) {
  std::function<std::shared_ptr<BlockingMethodWrapper>(CoroutineQueue<int>*,
                                      const std::vector<int>& args)> send =
      [](CoroutineQueue<int>* c, [[maybe_unused]] const std::vector<int>& args) -> std::shared_ptr<BlockingMethodWrapper> {
        assert(args.size() == 1);
        return std::shared_ptr<BlockingMethodWrapper>(new BlockingMethodWrapperParam<CoroutineQueue<int>::SendPromise>(c->Send(args[0])));
      };
  std::function<std::shared_ptr<BlockingMethodWrapper>(CoroutineQueue<int>*,
                                                       const std::vector<int>& args)> receive =
          [](CoroutineQueue<int>* c, [[maybe_unused]] const std::vector<int>& args) -> std::shared_ptr<BlockingMethodWrapper> {
    assert(args.empty());
    return std::shared_ptr<BlockingMethodWrapper>(new BlockingMethodWrapperParam<CoroutineQueue<int>::ReceivePromise>(c->Receive()));
  };

  CoroutineQueue<int> q;
  LinearizabilityDualChecker<CoroutineQueue<int>> checker(
      LinearizabilityDualChecker<CoroutineQueue<int>>::MethodMap {
          {"send", send},
          {"receive", receive},
      },
      q);

  auto first_task = CreateMockStackfulTask("send", 0, std::vector<int>{3});
  auto second_task = CreateMockStackfulTask("receive", 3, std::vector<int>{});

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
  std::function<std::shared_ptr<BlockingMethodWrapper>(CoroutineQueue<int>*,
                                                       const std::vector<int>& args)> send =
          [](CoroutineQueue<int>* c, [[maybe_unused]] const std::vector<int>& args) -> std::shared_ptr<BlockingMethodWrapper> {
    assert(args.size() == 1);
    return std::shared_ptr<BlockingMethodWrapper>(new BlockingMethodWrapperParam<CoroutineQueue<int>::SendPromise>(c->Send(args[0])));
  };
  std::function<std::shared_ptr<BlockingMethodWrapper>(CoroutineQueue<int>*,
                                                       const std::vector<int>& args)> receive =
          [](CoroutineQueue<int>* c, [[maybe_unused]] const std::vector<int>& args) -> std::shared_ptr<BlockingMethodWrapper> {
    assert(args.empty());
    return std::shared_ptr<BlockingMethodWrapper>(new BlockingMethodWrapperParam<CoroutineQueue<int>::ReceivePromise>(c->Receive()));
  };

  CoroutineQueue<int> q;
  LinearizabilityDualChecker<CoroutineQueue<int>> checker(
      LinearizabilityDualChecker<CoroutineQueue<int>>::MethodMap {
          {"send", send},
          {"receive", receive},
      },
      q);

  auto first_task = CreateMockStackfulTask("send", 0, std::vector<int>{3});
  auto second_task = CreateMockStackfulTask("receive", 2, std::vector<int>{});

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
  std::function<std::shared_ptr<BlockingMethodWrapper>(CoroutineQueue<int>*,
                                                       const std::vector<int>& args)> send =
          [](CoroutineQueue<int>* c, [[maybe_unused]] const std::vector<int>& args) -> std::shared_ptr<BlockingMethodWrapper> {
    assert(args.size() == 1);
    return std::shared_ptr<BlockingMethodWrapper>(new BlockingMethodWrapperParam<CoroutineQueue<int>::SendPromise>(c->Send(args[0])));
  };
  std::function<std::shared_ptr<BlockingMethodWrapper>(CoroutineQueue<int>*,
                                                       const std::vector<int>& args)> receive =
          [](CoroutineQueue<int>* c, [[maybe_unused]] const std::vector<int>& args) -> std::shared_ptr<BlockingMethodWrapper> {
    assert(args.empty());
    return std::shared_ptr<BlockingMethodWrapper>(new BlockingMethodWrapperParam<CoroutineQueue<int>::ReceivePromise>(c->Receive()));
  };

  CoroutineQueue<int> q;
  LinearizabilityDualChecker<CoroutineQueue<int>> checker(
      LinearizabilityDualChecker<CoroutineQueue<int>>::MethodMap {
          {"send", send},
          {"receive", receive},
      },
      q);

  auto first_task = CreateMockStackfulTask("send", 0, std::vector<int>{3});
  auto second_task = CreateMockStackfulTask("receive", 3, std::vector<int>{});
  auto third_task = CreateMockStackfulTask("receive", 3, std::vector<int>{});

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

TEST(LinearizabilityDualCheckerQueueTest, SmallLinearizableHistoryWithNonBlocking) {
  std::function<std::shared_ptr<BlockingMethodWrapper>(CoroutineQueue<int>*,
                                                       const std::vector<int>& args)> send =
          [](CoroutineQueue<int>* c, [[maybe_unused]] const std::vector<int>& args) -> std::shared_ptr<BlockingMethodWrapper> {
    assert(args.size() == 1);
    return std::shared_ptr<BlockingMethodWrapper>(new BlockingMethodWrapperParam<CoroutineQueue<int>::SendPromise>(c->Send(args[0])));
  };
  std::function<std::shared_ptr<BlockingMethodWrapper>(CoroutineQueue<int>*,
                                                       const std::vector<int>& args)> receive =
          [](CoroutineQueue<int>* c, [[maybe_unused]] const std::vector<int>& args) -> std::shared_ptr<BlockingMethodWrapper> {
    assert(args.empty());
    return std::shared_ptr<BlockingMethodWrapper>(new BlockingMethodWrapperParam<CoroutineQueue<int>::ReceivePromise>(c->Receive()));
  };

  std::function<int(CoroutineQueue<int>*,
                    const std::vector<int>& args)> size =
          [](CoroutineQueue<int>* c, [[maybe_unused]] const std::vector<int>& args) -> int {
    assert(args.empty());
    return c->ReceiversCount();
  };

  CoroutineQueue<int> q;
  LinearizabilityDualChecker<CoroutineQueue<int>> checker(
      LinearizabilityDualChecker<CoroutineQueue<int>>::MethodMap {
          {"send", send},
          {"receive", receive},
          {"size", size}
      },
      q);

  auto first_task = CreateMockStackfulTask("receive", 3, std::vector<int>{});
  auto second_task = CreateMockStackfulTask("send", 0, std::vector<int>{3});
  auto third_task = CreateMockStackfulTask("size", 1, std::vector<int>{});

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

};  // namespace LinearizabilityCheckerTest