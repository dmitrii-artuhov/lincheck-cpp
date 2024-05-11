// Keeps as separated file because use in regression tests.
#pragma once
#include <cassert>
#include <iostream>
#include <vector>

#include "lib.h"

extern "C" void CoroYield() noexcept;

namespace ltest {

extern std::vector<TaskBuilder> task_builders;

// Need to registrate target function in macro.
struct TaskBuilderPusher {
  TaskBuilderPusher(TaskBuilder builder) { task_builders.push_back(builder); }
};

}  // namespace ltest

// Adds an attribute.
#define attr(attr) __attribute((__annotate__(#attr)))

// Concatenates attributes.
#define concat_attr(a, b) attr(a##b)

// Tell that the function need to be converted to the coroutine.
#define non_atomic attr(ltest_nonatomic)

// Tell that the function need to be converted to the coroutine,
// but it's the user responsibility to place CoroYield() calls.
#define non_atomic_manual attr(ltest_nonatomic_manual)

// Have to generate coroutines to multiple await_suspended, to avoid same coro
// names add the class name to the coro
#define dual_name attr(ltest_dual)

namespace ltest {

template <typename T>
std::string toString(const T &a);

template <typename Func, typename... Args>
non_atomic_manual int callCoroutine(Func &&f, void *this_ptr,
                                    Args &&...args) noexcept {
  Handle hdl = f(this_ptr, std::forward<Args>(args)...);
  auto tsk = StackfulTask{Task{hdl}};
  while (!tsk.IsReturned()) {
    tsk.Resume();
    CoroYield();
  }
  return tsk.GetRetVal();
}

template <typename Func, typename Tuple, size_t... Indices>
non_atomic_manual int callCoroutineWithTupleHelper(
    Func &&func, void *this_ptr, Tuple &&tuple,
    std::index_sequence<Indices...>) noexcept {
  return callCoroutine(std::forward<Func>(func), this_ptr,
                       std::get<Indices>(std::forward<Tuple>(tuple))...);
}

template <typename Func, typename Tuple>
non_atomic_manual int callCoroutineWithTuple(Func &&func, void *this_ptr,
                                             Tuple &&tuple) noexcept {
  constexpr size_t tuple_size =
      std::tuple_size<std::remove_reference_t<Tuple>>::value;
  return callCoroutineWithTupleHelper(std::forward<Func>(func), this_ptr,
                                      std::forward<Tuple>(tuple),
                                      std::make_index_sequence<tuple_size>{});
}

template <typename tuple_t, size_t... index>
auto toStringListHelper(const tuple_t &t,
                        std::index_sequence<index...>) noexcept {
  return std::vector<std::string>{ltest::toString(std::get<index>(t))...};
}

template <typename tuple_t>
auto toStringList(const tuple_t &t) noexcept {
  typedef typename std::remove_reference<decltype(t)>::type tuple_type;
  constexpr auto s = std::tuple_size<tuple_type>::value;
  if constexpr (s == 0) {
    return std::vector<std::string>{};
  }
  return toStringListHelper<tuple_type>(t, std::make_index_sequence<s>{});
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
  extern "C" Handle symbol##_coro(void *__VA_OPT__(, ) __VA_ARGS__)

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
                      ltest::toStringList(generated_args));                  \
    ltest::SetArgsInited(true);                                              \
    CoroYield();                                                             \
    return ltest::callCoroutineWithTuple(symbol##_coro, this_ptr,            \
                                         generated_args);                    \
  }                                                                          \
                                                                             \
  extern "C" Handle symbol##_cpp_launcher_coro(void *this_ptr);

template <typename Function, typename Tuple, size_t... I>
auto call(Function f, Tuple t, std::index_sequence<I...>) {
  return f(std::get<I>(t)...);
}

// Declares the cpp level launcher.
// It is need, because we need to generate arguments on cpp level.
// This launcher must be a coroutine itself.
#define declare_cpp_launcher_for_dual(generator, awaitable, class_name,      \
                                      symbol)                                \
  non_atomic_manual extern "C" int symbol##_cpp_launcher(                    \
      void *this_ptr) noexcept {                                             \
    auto generated_args = generator();                                       \
    ltest::SetGenArgs(std::shared_ptr<void>(new std::tuple(generated_args)), \
                      ltest::toStringList(generated_args));                  \
    ltest::SetArgsInited(true);                                              \
    CoroYield();                                                             \
    static constexpr auto size =                                             \
        std::tuple_size<decltype(generated_args)>::value;                    \
    auto awaitable_object =                                                  \
        call(std::bind(&class_name::symbol, (class_name *)this_ptr),         \
             generated_args, std::make_index_sequence<size>{});              \
    auto callback_coroutine = std::coroutine_handle<>(/* TODO */);           \
    return ltest::callCoroutine(awaitable##_await_suspended_coro,            \
                                (void *)(&awaitable_object),                 \
                                callback_coroutine);                         \
  }                                                                          \
                                                                             \
  extern "C" Handle symbol##_cpp_launcher_coro(void *this_ptr);

// Allows to clone the task, starting from the beginning
// and provide the same arguments.
#define declare_cloner_launcher(generator, symbol)                \
  non_atomic_manual extern "C" int symbol##_clone_launcher(       \
      void *this_ptr, void *raw_args) noexcept {                  \
    assert(raw_args && "raw_args pointer is nullptr");            \
    auto generated_args_ptr =                                     \
        reinterpret_cast<decltype(generator()) *>(raw_args);      \
    return ltest::callCoroutineWithTuple(symbol##_coro, this_ptr, \
                                         *generated_args_ptr);    \
  }                                                               \
  extern "C" Handle symbol##_clone_launcher_coro(void *this_ptr,  \
                                                 void *raw_args);

// Allows to clone the task, starting from the beginning
// and provide the same arguments.
#define declare_cloner_launcher_dual(generator, awaitable, class_name, symbol) \
  non_atomic_manual extern "C" int symbol##_clone_launcher(                    \
      void *this_ptr, void *raw_args) noexcept {                               \
    assert(raw_args && "raw_args pointer is nullptr");                         \
    auto generated_args_ptr =                                                  \
        reinterpret_cast<decltype(generator()) *>(raw_args);                   \
    static constexpr auto size = std::tuple_size<                              \
        std::remove_reference_t<decltype(*generated_args_ptr)>>::value;        \
    /* TODO: allocate awaitable_object on heap */                              \
    auto awaitable_object =                                                    \
        call(std::bind(&class_name::symbol, (class_name *)this_ptr),           \
             *generated_args_ptr, std::make_index_sequence<size>{});           \
    auto callback_coroutine = std::coroutine_handle<>(/* TODO */);             \
    return ltest::callCoroutine(awaitable##_await_suspended_coro,              \
                                (void *)(&awaitable_object),                   \
                                callback_coroutine);                           \
  }                                                                            \
  extern "C" Handle symbol##_clone_launcher_coro(void *this_ptr,               \
                                                 void *raw_args);

// Declares the cpp level builder. Returns a task.
#define declare_cpp_builder(symbol)                                         \
  Task symbol##_cpp_builder(void *this_ptr) {                               \
    ltest::SetArgsInited(false);                                            \
    auto hdl = symbol##_cpp_launcher_coro(this_ptr);                        \
    auto task = Task{hdl, &symbol##_clone_launcher_coro};                   \
    /* Waiting until builder generate arguments. */                         \
    while (!ltest::gen_args.inited) {                                       \
      task.Resume();                                                        \
    }                                                                       \
    /*TODO: тут нужно выставлять про блокинг */ \
    task.SetMeta(std::make_shared<Task::Meta>(                              \
        std::string{symbol##_task_name}, std::move(ltest::gen_args.args),   \
        std::move(ltest::gen_args.str_args)));                              \
    return task;                                                            \
  }

// Public entrypoint.
#define target_method(generator, ret, cls, symbol, ...)                 \
  declare_task_name(symbol);                                            \
  declare_coro(symbol, __VA_ARGS__);                                    \
  declare_cpp_launcher(generator, symbol);                              \
  declare_cloner_launcher(generator, symbol);                           \
  declare_cpp_builder(symbol);                                          \
  namespace ltest {                                                     \
  ltest::TaskBuilderPusher symbol##__task__push{&symbol##_cpp_builder}; \
  }                                                                     \
  declare_target_method(ret, cls, symbol, __VA_ARGS__)

// awaitable_method(void, recive_awaiter) {code}

// Symbol is the name of the method that will return the awaitable
#define awaitable_method(ret, data_structure_class, promise_class,     \
                         coroutine_handle_arg)                         \
  /*declare_task_name(await_suspended);*/                              \
  declare_coro(promise_class##_await_suspended, coroutine_handle_arg); \
  declare_target_method(ret, data_structure_class::promise_class,      \
                        promise_class##_await_suspended, coroutine_handle_arg)

// target_method_dual(generator_int, ret)

// Коуртина для symbol не нужна, нужна только для awaitable
#define target_method_dual(generator, ret, promise_type, cls, symbol, ...) \
  declare_task_name(symbol);                                               \
  declare_cpp_launcher_for_dual(generator, promise_type, cls, symbol);     \
  declare_cloner_launcher_dual(generator, promise_type, cls, symbol);      \
  declare_cpp_builder(symbol);                                             \
  namespace ltest {                                                        \
  ltest::TaskBuilderPusher symbol##__task__push{&symbol##_cpp_builder};    \
  }                                                                        \
  ret cls::symbol(__VA_ARGS__)