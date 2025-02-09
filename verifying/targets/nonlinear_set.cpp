#include <memory>

#include "../specs/set.h"

/**
 * A Linked List to be used with other concurrency mechanisms.
 * This data structure is NOT thread-safe
 * <p>
 * This set has three main operations:
 * <ul>
 * <li>add(x)      - Non thread-safe
 * <li>remove(x)   - Non thread-safe
 * <li>contains(x) - Non thread-safe
 * </ul><p>
 * and two helper operations: size() and clear()
 */
template<typename T> class LinkedListSet {

private:
    struct Node {
        T key;
        Node* next;
        // std::shared_ptr<Node> next;
    };

    Node* _head;
    Node* _tail;
    // std::shared_ptr<Node> _head;
    // std::shared_ptr<Node> _tail;

public:

    LinkedListSet() {
        _head = new Node();
        _tail = new Node();
        // _head = std::make_shared<Node>();
        // _tail = std::make_shared<Node>();
        _head->next = _tail;
    }

    ~LinkedListSet() {
        clear(); // delete all nodes except _head and _tail
        // delete _head;
        // delete _tail;
    }


    /**
     * Adds a key to the set if it is not already there
     * <p>
     * Progress Condition: non thread-safe
     *
     * @param key
     * @return Returns true if "key" was added to the set, and false if it was
     * already there
     */
    bool add(T key) {
        Node* newNode = new Node();
        //auto newNode = std::make_shared<Node>();
        newNode->key = key;
        Node* node = _head->next;
        Node* prev = _head;
        // auto node = _head->next;
        // auto prev = _head;
        while (node != _tail) {
            if (key == node->key) {
                // This key is already in the set, return false
                return false;
            }
            if (key < node->key) {
                // We found the right place to insert, break out of the while()
                break;
            }
            prev = node;
            node = node->next;
        }
        // Insert the new node
        newNode->next = node;
        prev->next = newNode;
        return true;
    }


    /**
     * Removes a key from the set if it is there
     * <p>
     * Progress Condition: non thread-safe
     *
     * @param key
     * @return Returns true if "key" is found and removed from the list, false
     * if key was not found in the set.
     */
    bool remove(T key) {
        Node* node = _head->next;
        Node* prev = _head;
        // auto node = _head->next;
        // auto prev = _head;
        while (node != _tail) {
            if (key == node->key) {
                // Found a matching key, unlink the node
                prev->next = node->next;
                // delete node;
                return true;
            }
            if (key < node->key) {
                // The key is not in the set, return false
                return false;
            }
            prev = node;
            node = node->next;
        }
        return false;
    }


    /**
     * Searches for a given key.
     * <p>
     * Progress Condition: non thread-safe
     *
     * @param key
     * @return
     */
    bool contains(T key) {
        Node* node = _head->next;
        // auto node = _head->next;
        while (node != _tail) {
            if (key == node->key) {
                // Found the key in the set
                return true;
            }
            if (key < node->key) {
                // The key is not in the set, return false
                return false;
            }
            node = node->next;
        }
        return false;
    }


    /*
    * Cleans all entries in the list (set) except the head and tail
    */
    void clear(void) {
        Node * node = _head->next;
        Node * prev = _head->next;
        // auto node = _head->next;
        // auto prev = _head->next;
        while (node != _tail) {
            prev = node;
            node = node->next;
            // delete prev;
        }
        _head->next = _tail;
    }


    long size(void) {
        Node* node = _head->next;
        // auto node = _head->next;
        long size = 0;
        while (node != _tail) {
            size++;
            node = node->next;
        }
        return size;
    }
};


struct NonlinearSet {
    LinkedListSet<int> set{};

    non_atomic int Insert(int v) {
        return set.add(v);
    }

    non_atomic int Erase(int v) {
        return set.remove(v);
    }

    void Reset() {
        set.clear();
    }
};


// Arguments generator.
auto generateInt(size_t unused_param) {
    return ltest::generators::makeSingleArg(rand() % 10 + 1);
  }
  
// Targets.
target_method(generateInt, int, NonlinearSet, Insert, int);

target_method(generateInt, int, NonlinearSet, Erase, int);

// Specify target structure and it's sequential specification.
using spec_t =
    ltest::Spec<NonlinearSet, spec::Set<>, spec::SetHash<>, spec::SetEquals<>>;

LTEST_ENTRYPOINT(spec_t);