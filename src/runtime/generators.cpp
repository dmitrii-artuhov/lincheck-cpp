#include "include/generators.h"

namespace ltest {

namespace generators {

// Generates empty arguments.
std::tuple<> empty_gen() { return std::tuple<>(); }

}  // namespace generators

}  // namespace ltest