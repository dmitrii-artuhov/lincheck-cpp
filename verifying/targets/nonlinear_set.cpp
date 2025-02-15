#include <memory>
#include <atomic>

#include "../specs/set.h"

const int N = 100;

struct SlotsSet {
    SlotsSet() {
        Reset();
    }

    non_atomic bool Insert(int value) {
        assert(value != 0); // zero should never be added
        
        size_t hash = value % N;
        for (size_t i = 0; i < N; ++i) {
            size_t idx = (hash + i) % N;
            int current = slots[idx].load();
            if (current == value) break;
            if (current == 0) {
                if (slots[idx].compare_exchange_strong(current, value)) {
                    return true;
                }

                if (slots[idx].load() == value) break;
            }
        }
        return false;
    }
    
    non_atomic bool Erase(int value) {
        assert(value != 0);

        size_t hash = value % N;
        for (size_t i = 0; i < N; ++i) {
            size_t idx = (hash + i) % N;
            int current = slots[idx].load();
            if (current == value) {
                if (slots[idx].compare_exchange_strong(current, 0)) {
                    return true;
                }
            }
            if (current == 0) {
                break;
            }
        }
        return false;
    }

    void Reset() {
        for (size_t i = 0; i < N; ++i) {
            slots[i].store(0);
        }
    }
    
private:
    std::atomic<int> slots[N];
};

// Arguments generator.
auto generateInt(size_t unused_param) {
    // very small range of values, because to find nonlinearizable
    // scenario we need 4 operations with the same argument
    return ltest::generators::makeSingleArg(rand() % 3 + 1);
}

// Targets.
target_method(generateInt, int, SlotsSet, Insert, int);

target_method(generateInt, int, SlotsSet, Erase, int);

// Specify target structure and it's sequential specification.
using spec_t =
    ltest::Spec<SlotsSet, spec::Set<>, spec::SetHash<>, spec::SetEquals<>>;

LTEST_ENTRYPOINT(spec_t);