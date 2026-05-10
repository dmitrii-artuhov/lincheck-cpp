#pragma once

#include <cstdint>
#include <cstring>
#include <type_traits>

namespace ltest::wmm {

template <typename T>
uint64_t encode_to_u64(T x) {
  static_assert(std::is_trivially_copyable_v<T>);
  static_assert(sizeof(T) <= sizeof(uint64_t));
  uint64_t u = 0;
  std::memcpy(&u, &x, sizeof(T));
  return u;
}
template <typename T>
T decode_from_u64(uint64_t u) {
  static_assert(std::is_trivially_copyable_v<T>);
  T x;
  std::memcpy(&x, &u, sizeof(T));
  return x;
}

}  // namespace ltest::wmm