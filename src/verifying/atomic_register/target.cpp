#include "spec.h"

namespace target {

void Register::Reconstruct() { x = 0; }

Register::Register() {}

Register::Register(const Register& oth) { x.store(oth.x.load()); }

TARGET_METHOD(void, Register, add, ()) { x.fetch_add(1); }

TARGET_METHOD(int, Register, get, ()) { return x.load(); }

}  // namespace target
