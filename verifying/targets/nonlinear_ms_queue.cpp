/**
 * Michael-Scott lock-free queue implementation.
 * Taken from: https://web.archive.org/web/20190905090735/http://blog.shealevy.com/2015/04/23/use-after-free-bug-in-maged-m-michael-and-michael-l-scotts-non-blocking-concurrent-queue-algorithm/
 */

// #include <condition_variable>
// #include <thread>
// #include <mutex>
#include <atomic>

#include "../specs/queue.h"

// Am I the slow thread?
// thread_local auto is_slow_thread = bool{false};

// Is the slow thread waiting yet?
// auto slow_thread_waiting = bool{false};

// Has the node been freed yet?
// auto node_freed = bool{false};

// Used for inter-thread signalling
// std::condition_variable cond{};
// Mutex for cond
// std::mutex cond_mutex{};

// Implementation of the Michael-Scott algorithm
// template <typename T>
// class queue_t {
//     struct node_t;
//     // Explicitly aligned due to https://gcc.gnu.org/bugzilla/show_bug.cgi?id=65147: gcc should automatically align std::atomic<pointer_t> on 16-byte boundary but doesn't (until 5.1)
//     struct alignas(16) pointer_t {
//         node_t* ptr;
//         unsigned int count;
//         // A zero-initialized pointer_t
//         // I'm pretty sure we don't actually need to initialize count to 0 here given how these are used, but it can't hurt.
//         pointer_t() noexcept : ptr{nullptr}, count{0} {}
//         // A pointer_t pointing to a specific node
//         pointer_t(node_t* ptr) : ptr{ptr}, count{0} {}
//         // A pointer_t pointing to a specific node with a specific count
//         pointer_t(node_t* ptr, unsigned int count) : ptr{ptr}, count{count} {}
//         // bitwise-compare two pointer_ts
//         bool operator==(const pointer_t& other) const {
//             return ptr == other.ptr && count == other.count;
//         }
//     };
//     struct node_t {
//         T value;
//         // We're going to do atomic ops on next
//         std::atomic<pointer_t> next;
//         // A dummy node, next is initialized with a zero-initialized ptr
//         node_t() : next{pointer_t{}} {}
//         // A node filled with a given value, next is initialized with a zero-initialized ptr
//         node_t(T value) : value(value), next{pointer_t{}} {}
//     };

//     // We're going to do atomic ops on Head
//     std::atomic<pointer_t> Head;
//     // We're going to do atomic ops on Tail
//     std::atomic<pointer_t> Tail;

//     public:
//     queue_t() : Head{new node_t{}}, Tail{Head.load().ptr} {}
     

//     void enqueue(T value) {
//         // Node is initialized in ctor, so three lines in one
//         auto node = new node_t{value}; // E1, E2, E3
//         decltype(Tail.load()) tail;
//         while (true) { // E4
//             tail = Tail.load(); // E5
//             // If we're the slow thread, we wait until the node we just loaded is freed.
//             // if (is_slow_thread) {
//             //     {
//             //         std::lock_guard<std::mutex> lock{cond_mutex};
//             //         slow_thread_waiting = true;
//             //     }
//             //     // Let the main thread know we're waiting
//             //     cond.notify_one();
//             //     auto lock = std::unique_lock<std::mutex>{cond_mutex};
//             //     // Wait until the main thread tells us the node is freed.
//             //     cond.wait(lock, []{ return node_freed; });
//             // }
//             // Use-after-free here in slow thread!
//             auto next = tail.ptr->next.load(); // E6
//             if (tail == Tail.load()) { // E7
//                 if (!next.ptr) { // E8
//                     if (tail.ptr->next.compare_exchange_weak(next, pointer_t{node, next.count + 1})) { // E9
//                         break; // E10
//                     } // E11
//                 } else { // E12
//                     Tail.compare_exchange_weak(tail, pointer_t{next.ptr, tail.count + 1}); // E13
//                 } // E14
//             } // E15
//         } // E16

//         Tail.compare_exchange_weak(tail, pointer_t{node, tail.count + 1}); // E17
//     }

//     bool dequeue(T* pvalue) {
//         decltype(Head.load()) head;
//         while (true) { // D1
//             head = Head.load(); // D2
//             auto tail = Tail.load(); // D3
//             auto next = head.ptr->next.load(); // D4
//             if (head == Head.load()) { // D5
//                 if (head.ptr == tail.ptr) { // D6
//                     if (!next.ptr) { // D7
//                         return false; // D8
//                     } // D9
//                     Tail.compare_exchange_weak(tail, pointer_t{next.ptr, tail.count + 1}); // D10
//                 } else { // D11
//                     *pvalue = next.ptr->value; // D12
//                     if (Head.compare_exchange_weak(head, pointer_t{next.ptr, head.count + 1})) { // D13
//                         break; // D14
//                     } // D15
//                 } // D16
//             } // D17
//         } // D18
//         delete head.ptr; // D19
//         return true; // D20
//     }
// };



// template<typename T>
// class HazardPointers {

// private:
//     static const int      HP_MAX_THREADS = 128;
//     static const int      HP_MAX_HPS = 4;     // This is named 'K' in the HP paper
//     static const int      CLPAD = 128/sizeof(std::atomic<T*>);
//     static const int      HP_THRESHOLD_R = 0; // This is named 'R' in the HP paper
//     static const int      MAX_RETIRED = HP_MAX_THREADS*HP_MAX_HPS; // Maximum number of retired objects per thread

//     const int             maxHPs;
//     const int             maxThreads;

//     std::atomic<T*>*      hp[HP_MAX_THREADS];
//     // It's not nice that we have a lot of empty vectors, but we need padding to avoid false sharing
//     std::vector<T*>       retiredList[HP_MAX_THREADS*CLPAD];

// public:
//     HazardPointers(int maxHPs=HP_MAX_HPS, int maxThreads=HP_MAX_THREADS) : maxHPs{maxHPs}, maxThreads{maxThreads} {
//         for (int ithread = 0; ithread < HP_MAX_THREADS; ithread++) {
//             hp[ithread] = new std::atomic<T*>[CLPAD*2]; // We allocate four cache lines to allow for many hps and without false sharing
//             for (int ihp = 0; ihp < HP_MAX_HPS; ihp++) {
//                 hp[ithread][ihp].store(nullptr, std::memory_order_relaxed);
//             }
//         }
//     }

//     ~HazardPointers() {
//         for (int ithread = 0; ithread < HP_MAX_THREADS; ithread++) {
//             delete[] hp[ithread];
//             // Clear the current retired nodes
//             for (unsigned iret = 0; iret < retiredList[ithread*CLPAD].size(); iret++) {
//                 delete retiredList[ithread*CLPAD][iret];
//             }
//         }
//     }


//     /**
//      * Progress Condition: wait-free bounded (by maxHPs)
//      */
//     void clear(const int tid) {
//         for (int ihp = 0; ihp < maxHPs; ihp++) {
//             hp[tid][ihp].store(nullptr, std::memory_order_release);
//         }
//     }


//     /**
//      * Progress Condition: wait-free population oblivious
//      */
//     void clearOne(int ihp, const int tid) {
//         hp[tid][ihp].store(nullptr, std::memory_order_release);
//     }


//     /**
//      * Progress Condition: lock-free
//      */
//     T* protect(int index, const std::atomic<T*>& atom, const int tid) {
//         T* n = nullptr;
//         T* ret;
// 		while ((ret = atom.load()) != n) {
// 			hp[tid][index].store(ret);
// 			n = ret;
// 		}
// 		return ret;
//     }

	
//     /**
//      * This returns the same value that is passed as ptr, which is sometimes useful
//      * Progress Condition: wait-free population oblivious
//      */
//     T* protectPtr(int index, T* ptr, const int tid) {
//         hp[tid][index].store(ptr);
//         return ptr;
//     }



//     /**
//      * This returns the same value that is passed as ptr, which is sometimes useful
//      * Progress Condition: wait-free population oblivious
//      */
//     T* protectRelease(int index, T* ptr, const int tid) {
//         hp[tid][index].store(ptr, std::memory_order_release);
//         return ptr;
//     }


//     /**
//      * Progress Condition: wait-free bounded (by the number of threads squared)
//      */
//     void retire(T* ptr, const int tid) {
//         retiredList[tid*CLPAD].push_back(ptr);
//         if (retiredList[tid*CLPAD].size() < HP_THRESHOLD_R) return;
//         for (unsigned iret = 0; iret < retiredList[tid*CLPAD].size();) {
//             auto obj = retiredList[tid*CLPAD][iret];
//             bool canDelete = true;
//             for (int tid = 0; tid < maxThreads && canDelete; tid++) {
//                 for (int ihp = maxHPs-1; ihp >= 0; ihp--) {
//                     if (hp[tid][ihp].load() == obj) {
//                         canDelete = false;
//                         break;
//                     }
//                 }
//             }
//             if (canDelete) {
//                 retiredList[tid*CLPAD].erase(retiredList[tid*CLPAD].begin() + iret);
//                 delete obj;
//                 continue;
//             }
//             iret++;
//         }
//     }
// };



// /**
//  * <h1> Michael-Scott Queue </h1>
//  *
//  * enqueue algorithm: MS enqueue
//  * dequeue algorithm: MS dequeue
//  * Consistency: Linearizable
//  * enqueue() progress: lock-free
//  * dequeue() progress: lock-free
//  * Memory Reclamation: Hazard Pointers (lock-free)
//  *
//  *
//  * Maged Michael and Michael Scott's Queue with Hazard Pointers
//  * <p>
//  * Lock-Free Linked List as described in Maged Michael and Michael Scott's paper:
//  * {@link http://www.cs.rochester.edu/~scott/papers/1996_PODC_queues.pdf}
//  * <a href="http://www.cs.rochester.edu/~scott/papers/1996_PODC_queues.pdf">
//  * Simple, Fast, and Practical Non-Blocking and Blocking Concurrent Queue Algorithms</a>
//  * <p>
//  * The paper on Hazard Pointers is named "Hazard Pointers: Safe Memory
//  * Reclamation for Lock-Free objects" and it is available here:
//  * http://web.cecs.pdx.edu/~walpole/class/cs510/papers/11.pdf
//  *
//  */
// template<typename T>
// class MichaelScottQueue {

// private:
//     struct Node {
//         T* item;
//         std::atomic<Node*> next;

//         Node(T* userItem) : item{userItem}, next{nullptr} { }

//         bool casNext(Node *cmp, Node *val) {
//             return next.compare_exchange_strong(cmp, val);
//         }
//     };

//     bool casTail(Node *cmp, Node *val) {
// 		return tail.compare_exchange_strong(cmp, val);
// 	}

//     bool casHead(Node *cmp, Node *val) {
//         return head.compare_exchange_strong(cmp, val);
//     }

//     // Pointers to head and tail of the list
//     alignas(128) std::atomic<Node*> head;
//     alignas(128) std::atomic<Node*> tail;

//     static const int MAX_THREADS = 128;
//     const int maxThreads;

//     // We need two hazard pointers for dequeue()
//     HazardPointers<Node> hp {2, maxThreads};
//     const int kHpTail = 0;
//     const int kHpHead = 0;
//     const int kHpNext = 1;

// public:
//     MichaelScottQueue(int maxThreads=MAX_THREADS) : maxThreads{maxThreads} {
//         Node* sentinelNode = new Node(nullptr);
//         head.store(sentinelNode, std::memory_order_relaxed);
//         tail.store(sentinelNode, std::memory_order_relaxed);
//     }


//     ~MichaelScottQueue() {
//         while (dequeue(0) != nullptr); // Drain the queue
//         delete head.load();            // Delete the last node
//     }

//     std::string className() { return "MichaelScottQueue"; }

//     non_atomic void enqueue(T* item, const int tid) {
//         if (item == nullptr) throw std::invalid_argument("item can not be nullptr");
//         Node* newNode = new Node(item);
//         while (true) {
//             Node* ltail = hp.protectPtr(kHpTail, tail, tid);
//             if (ltail == tail.load()) {
//                 Node* lnext = ltail->next.load();
//                 if (lnext == nullptr) {
//                     // It seems this is the last node, so add the newNode here
//                     // and try to move the tail to the newNode
//                     if (ltail->casNext(nullptr, newNode)) {
//                         casTail(ltail, newNode);
//                         hp.clear(tid);
//                         return;
//                     }
//                 } else { 
//                     casTail(ltail, lnext);
//                 }
//             }
//         }
//     }

//     non_atomic T* dequeue(const int tid) {
//         Node* node = hp.protect(kHpHead, head, tid);
//         while (node != tail.load()) {
//             Node* lnext = hp.protect(kHpNext, node->next, tid);
//             if (casHead(node, lnext)) {
//                 T* item = lnext->item;  // Another thread may clean up lnext after we do hp.clear()
//                 hp.clear(tid);
//                 hp.retire(node, tid);
//                 return item;
//             }
//             node = hp.protect(kHpHead, head, tid);
//         }
//         hp.clear(tid);
//         return nullptr;                  // Queue is empty
//     }
// };


struct MSQueue {
    // memory leaks, but we don't care
    struct Node {
        int val;
        std::atomic<Node*> next;
    };

    Node dummy{0, nullptr};
    std::atomic<Node*> head{&dummy};
    std::atomic<Node*> tail{&dummy};

    non_atomic void Push(int val) {
        Node* newNode = new Node{val, nullptr};

        while (true) {
            Node* currentTail = tail.load();
            Node* tailNext = currentTail->next.load();

            if (currentTail == tail.load()) {
                if (tailNext != nullptr) {
                    tail.compare_exchange_strong(currentTail, tailNext);
                }
                else {
                    // if (currentTail->next.compare_exchange_strong(tailNext, newNode)) {
                    //     tail.compare_exchange_strong(currentTail, newNode);
                    //     return;
                    // }
                    currentTail->next.compare_exchange_strong(tailNext, newNode);
                    if (tail.load() != nullptr) {
                        tail.compare_exchange_strong(currentTail, newNode);
                        return;
                    }
                }
            }
        }
    }

    non_atomic int Pop() {
        while (true) {
            Node* first = head.load();
            Node* last = tail.load();
            // Node* next = first->next.load();
            Node* next = nullptr;
            if (first != nullptr) next = first->next.load();

            if (first == head.load()) {
                if (first == last) {
                    if (next == nullptr) {
                        return 0;
                    }
                    tail.compare_exchange_strong(last, next);
                }
                else {
                    // int result = next->val;
                    int result = 0;
                    if (next != nullptr)
                        result = next->val;

                    if (head.compare_exchange_strong(first, next)) {
                        if (result != 0) {
                            next->val = 0;
                        }
                        return result;
                    }
                }
            }
        }
    }

    void Reset() {
        dummy.val = 0;
        dummy.next = nullptr;

        head.store(&dummy);
        tail.store(&dummy);
    }
};


// Arguments generator.
auto generateInt(size_t thread_num) {
    return ltest::generators::makeSingleArg(rand() % 10 + 1);
}
  
// Targets.
target_method(generateInt, void, MSQueue, Push, int);

target_method(ltest::generators::genEmpty, int, MSQueue, Pop);

// Specify target structure and it's sequential specification.
using spec_t =
    ltest::Spec<MSQueue, spec::Queue<>, spec::QueueHash<>, spec::QueueEquals<>>;

LTEST_ENTRYPOINT(spec_t);