#include "spec.h"

namespace target {

void Register::Reconstruct() { x = 0; }

Register::Register() {}

Register::Register(const Register& oth) { x = oth.x; }

TARGET_METHOD(void, Register, add, ()) { ++x; }

TARGET_METHOD(int, Register, get, ()) { return x; }

}  // namespace target
