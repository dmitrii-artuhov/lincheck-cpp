#pragma once
#include <tuple>
#include <type_traits>

#include "lib.h"

namespace ltest {

namespace generators {

// Makes single argument from the value.
template <typename T>
auto make_single_arg(T&& arg) {
  using arg_type = typename std::remove_reference<T>::type;
  return std::tuple<arg_type>{std::forward<arg_type>(arg)};
}

std::tuple<> empty_gen();

std::tuple<std::shared_ptr<Token>> gen_token();

// TODO: concatenate generated tuples.
// sum_generator<arg1gen, arg2gen> or something else.

}  // namespace generators

}  // namespace ltest