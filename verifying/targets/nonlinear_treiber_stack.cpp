#include <atomic>

#include "../specs/stack.h"

const size_t capacity = 100;

struct TreiberStack {
    TreiberStack(): nodes(capacity), head(-1), free_list(0) {
        Reset();
    }

    non_atomic void Push(int value) {
        int node_index;
        do {
            node_index = free_list.load();
            if (node_index == -1) return;  // Stack is full (no free nodes)
        } while (!free_list.compare_exchange_strong(node_index, nodes[node_index].next.load()));

        nodes[node_index].value = value;

        int old_head;
        int time = 0;
        do {
            old_head = head.load();
            nodes[node_index].next.store(old_head);
            time++;
        } while (
            time < 4 /* MISTAKE: early quit */ &&
            !head.compare_exchange_strong(old_head, node_index)
        );
    }

    non_atomic int Pop() {
        int node_index;
        do {
            node_index = head.load();
            if (node_index == -1) return 0;  // Stack is empty
        } while (!head.compare_exchange_strong(node_index, nodes[node_index].next.load()));

        int value = nodes[node_index].value;

        int old_free;
        do {
            old_free = free_list.load();
            nodes[node_index].next.store(old_free);
        } while (!free_list.compare_exchange_strong(old_free, node_index));

        return value;
    }

    void Reset() {
        // Reset free list (each node points to the next)
        for (size_t i = 0; i < nodes.size() - 1; ++i) {
            nodes[i].next.store(i + 1);
        }
        nodes[nodes.size() - 1].next.store(-1);

        // Reset stack head and free list pointer
        head.store(-1);
        free_list.store(0);
    }

private:
    struct Node {
        int value;
        std::atomic<int> next;
    };

    std::vector<Node> nodes;
    std::atomic<int> head;
    std::atomic<int> free_list;
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
