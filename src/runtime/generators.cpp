#include "include/generators.h"

namespace ltest {

namespace generators {

// Generates empty arguments.
std::tuple<> genEmpty() { return std::tuple<>(); }

// Generates runtime token.
std::tuple<std::shared_ptr<Token>> genToken() {
  auto token = std::make_shared<Token>();
  GetCurrentTask()->SetToken(token);
  return {token};
}

}  // namespace generators

}  // namespace ltest
