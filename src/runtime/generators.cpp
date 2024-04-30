#include "include/generators.h"

namespace ltest {

namespace generators {

// Generates empty arguments.
std::tuple<> empty_gen() { return std::tuple<>(); }

// Generates runtime token.
std::tuple<std::shared_ptr<Token>> gen_token() {
  auto token = std::make_shared<Token>();
  GetCurrentTask()->SetToken(token);
  return {token};
}

}  // namespace generators

}  // namespace ltest
