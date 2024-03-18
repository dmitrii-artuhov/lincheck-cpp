#include <atomic>

#include "macro.h"

extern "C" {

std::atomic_int x{};

na void add() { x.fetch_add(1); }

na int get() { return x.load(); }
}
