#include <memory>

#include "lib.h"

// Public macros.
#define non_atomic attr(ltest_nonatomic)
#define generator attr(ltest_gen)
#define ini attr(ltest_initfunc)
#define TARGET_METHOD(ret, cls, symbol, args) \
  concat_attr(ltesttarget_, symbol) non_atomic ret cls::symbol args
