#include <atomic>

#include "../specs/stack.h"

struct TreiberStack {
    struct Node {
        int val;
        std::atomic<Node*> next;
    };

    std::atomic<Node*> top{nullptr};

    non_atomic void Push(int v) {
        while (true) {
            Node* curTop = top.load();
            Node* newNode = new Node{v, curTop};
            if (top.compare_exchange_weak(curTop, newNode)) {
                break;
            }
        }
    }

    non_atomic int Pop() {
        while (true) {
            Node* curTop = top.load();
            if (curTop == nullptr) return 0;
            // if (top.compare_exchange_strong(curTop, curTop->next)) {
            //     return curTop->val;
            // }
            top.compare_exchange_strong(curTop, curTop->next);
            if (curTop == nullptr) return 0;
            return curTop->val;
        }
    }

    void Reset() {
        top.store(nullptr);
    }
};



// Arguments generator.
auto generateInt(size_t thread_num) {
    return ltest::generators::makeSingleArg(rand() % 10 + 1);
}
  
// Targets.
target_method(generateInt, void, TreiberStack, Push, int);

target_method(ltest::generators::genEmpty, int, TreiberStack, Pop);

// Specify target structure and it's sequential specification.
using spec_t =
    ltest::Spec<TreiberStack, spec::Stack<>, spec::StackHash<>, spec::StackEquals<>>;

LTEST_ENTRYPOINT(spec_t);
