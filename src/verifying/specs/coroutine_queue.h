#include "../../runtime/include/lincheck_dual.h"
#include <functional>
#include <map>
namespace spec {

template <typename PushArgTuple = std::tuple<int>, std::size_t ValueIndex = 0>
struct CoroutineQueue {
  struct SendPromise;
  struct ReceivePromise;

  CoroutineQueue()
      : receivers(std::queue<ReceivePromise>()),
        senders(std::queue<SendPromise>()) {}

  SendPromise Send(int elem) { return SendPromise(elem, *this); }

  ReceivePromise Receive() { return ReceivePromise(*this); }

  size_t ReceiversCount() { return receivers.size(); }

  struct ReceivePromise {
    explicit ReceivePromise(CoroutineQueue& queue) : queue(queue) {
      elem = std::make_shared<std::optional<int>>(std::optional<int>(std::nullopt));
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

    std::optional<int> await_resume() { return *elem; }

    std::coroutine_handle<> receiver;
    std::shared_ptr<std::optional<int>> elem;
    CoroutineQueue& queue;
  };

  struct SendPromise {
    SendPromise(int elem, CoroutineQueue& queue) : elem(elem), queue(queue) {}

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
    int elem;
    CoroutineQueue& queue;
  };

  static auto GetMethods() {
    LinearizabilityDualChecker<CoroutineQueue<>>::BlockingMethodFactory receive = [](CoroutineQueue<>* c,
       [[maybe_unused]] void* args) -> std::shared_ptr<BlockingMethod> {
      return std::shared_ptr<BlockingMethod>(
          new BlockingMethodWrapper<CoroutineQueue<>::ReceivePromise>(
              c->Receive()));
    };

    return LinearizabilityDualChecker<CoroutineQueue<>>::MethodMap{
        {"Receive", receive},
    };
  }

private:
  std::queue<ReceivePromise> receivers;
  std::queue<SendPromise> senders;
};

};