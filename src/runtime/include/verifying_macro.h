// Keeps as separated file because use in regression tests.
#pragma once
#include <cassert>
#include <iostream>
#include <vector>

#include "lib.h"

extern "C" void coro_yield() noexcept;

namespace ltest {

extern std::vector<task_builder_t> task_builders;

// Need to registrate target function in macro.
struct task_builder_pusher {
  task_builder_pusher(task_builder_t builder) {
    task_builders.push_back(builder);
  }
};

}  // namespace ltest

// Adds an attribute.
#define attr(attr) __attribute((__annotate__(#attr)))

// Concatenates attributes.
#define concat_attr(a, b) attr(a##b)

// Tell that the function need to be converted to the coroutine.
#define non_atomic attr(ltest_nonatomic)

// Tell that the function need to be converted to the coroutine,
// but it's the user responsibility to place coro_yield() calls.
#define non_atomic_manual attr(ltest_nonatomic_manual)

namespace ltest {

template <typename T>
std::string to_string(const T &a);

template <typename Func, typename... Args>
non_atomic_manual int call_coroutine(Func &&f, void *this_ptr,
                                     Args &&...args) noexcept {
  handle hdl = f(this_ptr, std::forward<Args>(args)...);
  auto tsk = StackfulTask{hdl};
  while (!tsk.IsReturned()) {
    tsk.Resume();
    coro_yield();
  }
  return tsk.GetRetVal();
}

template <typename Func, typename Tuple, size_t... Indices>
non_atomic_manual int call_coroutine_with_tuple_helper(
    Func &&func, void *this_ptr, Tuple &&tuple,
    std::index_sequence<Indices...>) noexcept {
  return call_coroutine(std::forward<Func>(func), this_ptr,
                        std::get<Indices>(std::forward<Tuple>(tuple))...);
}

template <typename Func, typename Tuple>
non_atomic_manual int call_coroutine_with_tuple(Func &&func, void *this_ptr,
                                                Tuple &&tuple) noexcept {
  constexpr size_t tuple_size =
      std::tuple_size<std::remove_reference_t<Tuple>>::value;
  return call_coroutine_with_tuple_helper(
      std::forward<Func>(func), this_ptr, std::forward<Tuple>(tuple),
      std::make_index_sequence<tuple_size>{});
}

template <typename tuple_t, size_t... index>
auto to_string_list_helper(const tuple_t &t,
                           std::index_sequence<index...>) noexcept {
  return std::vector<std::string>{ltest::to_string(std::get<index>(t))...};
}

template <typename tuple_t>
auto to_string_list(const tuple_t &t) noexcept {
  typedef typename std::remove_reference<decltype(t)>::type tuple_type;
  constexpr auto s = std::tuple_size<tuple_type>::value;
  if constexpr (s == 0) {
    return std::vector<std::string>{};
  }
  return to_string_list_helper<tuple_type>(t, std::make_index_sequence<s>{});
}

struct GeneratedArgs {
  std::shared_ptr<void> args{};
  std::vector<std::string> str_args{};
  bool inited{};
};

void SetGenArgs(std::shared_ptr<void> ptr, std::vector<std::string> str_args);

void SetArgsInited(bool value);

extern GeneratedArgs gen_args;

}  // namespace ltest

#define declare_task_name(symbol) const char *symbol##_task_name = #symbol

// Declares the coroutine. The body will be generated in pass.
// It is need, because we need to call mangled coroutine function on our cpp
// level.
#define declare_coro(symbol, ...) \
  extern "C" handle symbol##_coro(void *__VA_OPT__(, ) __VA_ARGS__)

// Declares the struct method for which a coroutine will be generated.
#define declare_target_method(ret, cls, symbol, ...) \
  concat_attr(ltesttarget_, symbol) non_atomic ret cls::symbol(__VA_ARGS__)

// Declares the cpp level launcher.
// It is need, because we need to generate arguments on cpp level.
// This launcher must be a coroutine itself.
#define declare_cpp_launcher(generator, symbol)                              \
  non_atomic_manual extern "C" int symbol##_cpp_launcher(                    \
      void *this_ptr) noexcept {                                             \
    auto generated_args = generator();                                       \
    ltest::SetGenArgs(std::shared_ptr<void>(new std::tuple(generated_args)), \
                      ltest::to_string_list(generated_args));                \
    ltest::SetArgsInited(true);                                              \
    coro_yield();                                                            \
    return ltest::call_coroutine_with_tuple(symbol##_coro, this_ptr,         \
                                            generated_args);                 \
  }                                                                          \
                                                                             \
  extern "C" handle symbol##_cpp_launcher_coro(void *this_ptr);

// Allows to clone the task, starting from the beginning
// and provide the same arguments.
#define declare_cloner_launcher(generator, symbol)                   \
  non_atomic_manual extern "C" int symbol##_clone_launcher(          \
      void *this_ptr, void *raw_args) noexcept {                     \
    assert(raw_args && "raw_args pointer is nullptr");               \
    auto generated_args_ptr =                                        \
        reinterpret_cast<decltype(generator()) *>(raw_args);         \
    return ltest::call_coroutine_with_tuple(symbol##_coro, this_ptr, \
                                            *generated_args_ptr);    \
  }                                                                  \
  extern "C" handle symbol##_clone_launcher_coro(void *this_ptr,     \
                                                 void *raw_args);

// Declares the cpp level builder. Returns a task.
#define declare_cpp_builder(symbol)                                       \
  Task symbol##_cpp_builder(void *this_ptr) {                             \
    ltest::SetArgsInited(false);                                          \
    auto hdl = symbol##_cpp_launcher_coro(this_ptr);                      \
    auto task = Task{hdl, &symbol##_clone_launcher_coro};                 \
    /* Waiting until builder generate arguments. */                       \
    while (!ltest::gen_args.inited) {                                     \
      task.Resume();                                                      \
    }                                                                     \
    task.SetMeta(std::make_shared<Task::Meta>(                            \
        std::string{symbol##_task_name}, std::move(ltest::gen_args.args), \
        std::move(ltest::gen_args.str_args)));                            \
    return task;                                                          \
  }

// Public entrypoint.
#define target_method(generator, ret, cls, symbol, ...)                   \
  declare_task_name(symbol);                                              \
  declare_coro(symbol, __VA_ARGS__);                                      \
  declare_cpp_launcher(generator, symbol);                                \
  declare_cloner_launcher(generator, symbol);                             \
  declare_cpp_builder(symbol);                                            \
  namespace ltest {                                                       \
  ltest::task_builder_pusher symbol##__task__push{&symbol##_cpp_builder}; \
  }                                                                       \
  declare_target_method(ret, cls, symbol, __VA_ARGS__)