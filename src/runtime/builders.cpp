#include "include/verifying_macro.h"

namespace ltest {
void SetGenArgs(std::shared_ptr<void> ptr, std::vector<std::string> str_args) {
  gen_args.args = ptr;
  gen_args.str_args = str_args;
}

void SetArgsInited(bool value) { gen_args.inited = value; }

}  // namespace ltest
