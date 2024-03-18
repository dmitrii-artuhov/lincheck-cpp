#include "macro.h"
extern "C" {

int x{};

// Bad.
na void add() { ++x; }

na int get() { return x; }
}
