#pragma once
#include <tuple>
#include <type_traits>

#include "lib.h"

namespace ltest {

namespace generators {

// Makes single argument from the value.
template <typename T>
auto makeSingleArg(T&& arg) {
  using arg_type = typename std::remove_reference<T>::type;
  return std::tuple<arg_type>{std::forward<arg_type>(arg)};
}

std::tuple<> genEmpty();

std::tuple<std::shared_ptr<Token>> genToken();

// TODO: concatenate generated tuples.
// sum_generator<arg1gen, arg2gen> or something else.

}  // namespace generators

}  // namespace ltest
