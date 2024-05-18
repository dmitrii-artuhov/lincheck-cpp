#include "include/generators.h"

namespace ltest {

namespace generators {

std::shared_ptr<Token> generated_token{};

// Generates empty arguments.
std::tuple<> genEmpty() { return std::tuple<>(); }

// Generates runtime token.
std::tuple<std::shared_ptr<Token>> genToken() {
  assert(!generated_token && "forgot to reset generated_token");
  generated_token = std::make_shared<Token>();
  return {generated_token};
}

}  // namespace generators

}  // namespace ltest
