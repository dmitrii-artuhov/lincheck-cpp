// Keeps as separated file because use in regression tests.
#pragma once
#include <cassert>
#include <iostream>
#include <vector>

#include "lib.h"

namespace ltest {

extern std::vector<TaskBuilder> task_builders;

}  // namespace ltest

// Adds an attribute.
#define attr(attr) __attribute((__annotate__(#attr)))

// Tell that the function need to be converted to the coroutine.
#define non_atomic attr(ltest_nonatomic)

namespace ltest {

template <typename T>
std::string toString(const T &a);

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

template <typename... Args>
auto toStringArgs(std::shared_ptr<void> args) {
  auto real_args = reinterpret_cast<std::tuple<Args...> *>(args.get());
  return toStringList(*real_args);
}

template <typename Ret, typename Target, typename... Args>
struct TargetMethod;

template <typename Target, typename... Args>
struct TargetMethod<int, Target, Args...> {
  using Method = std::function<int(Target *, Args...)>;
  TargetMethod(std::string_view method_name,
               std::function<std::tuple<Args...>(size_t)> gen, Method method) {
    auto builder =
        [gen = std::move(gen), method_name, method = std::move(method)](
            void *this_ptr, size_t thread_num) -> std::shared_ptr<CoroBase> {
      auto args = std::shared_ptr<void>(new std::tuple(gen(thread_num)));
      auto coro = Coro<Target, Args...>::New(
          method, this_ptr, args, &ltest::toStringArgs<Args...>, method_name);
      if (ltest::generators::generated_token) {
        coro->SetToken(ltest::generators::generated_token);
        ltest::generators::generated_token.reset();
      }
      return coro;
    };
    ltest::task_builders.push_back(builder);
  }
};

template <typename Ret, typename Target, typename... Args>
struct TargetMethodDual;

template <typename Target, typename... Args>
struct TargetMethodDual<int, Target, Args...> {
  using Method = std::function<int(Target *, Args...)>;
  TargetMethodDual(std::string_view method_name,
               std::function<std::tuple<Args...>(size_t)> gen, Method method) {
    auto builder =
        [gen = std::move(gen), method_name, method = std::move(method)](
            void *this_ptr, size_t thread_num) -> std::shared_ptr<CoroBase> {
      auto args = std::shared_ptr<void>(new std::tuple(gen(thread_num)));
      auto coro = Coro<Target, Args...>::New(
          method, this_ptr, args, &ltest::toStringArgs<Args...>, method_name);
      if (ltest::generators::generated_token) {
        coro->SetToken(ltest::generators::generated_token);
        ltest::generators::generated_token.reset();
      }
      return coro;
    };
    ltest::task_builders.push_back(builder);
  }
};

// Emulate that void f() returns 0.
template <typename Target, typename F, typename... Args>
struct Wrapper {
  F f;
  Wrapper(F f) : f(std::move(f)) {}
  int operator()(void *this_ptr, Args &&...args) {
    f(reinterpret_cast<Target *>(this_ptr), std::forward<Args>(args)...);
    return 0;
  }
};

template <typename Target, typename... Args>
struct TargetMethod<void, Target, Args...> {
  using Method = std::function<void(Target *, Args...)>;

  TargetMethod(std::string_view method_name,
               std::function<std::tuple<Args...>(size_t)> gen, Method method) {
    auto builder =
        [gen = std::move(gen), method_name, method = std::move(method)](
            void *this_ptr, size_t thread_num) -> std::shared_ptr<CoroBase> {
      auto wrapper = Wrapper<Target, decltype(method), Args...>{method};
      auto args = std::shared_ptr<void>(new std::tuple(gen(thread_num)));
      auto coro = Coro<Target, Args...>::New(
          wrapper, this_ptr, args, &ltest::toStringArgs<Args...>, method_name);
      if (ltest::generators::generated_token) {
        coro->SetToken(ltest::generators::generated_token);
        ltest::generators::generated_token.reset();
      }
      return coro;
    };
    ltest::task_builders.push_back(builder);
  }
};

}  // namespace ltest

#define declare_task_name(symbol) const char *symbol##_task_name = #symbol

//// Declares the coroutine. The body will be generated in pass.
//// It is need, because we need to call mangled coroutine function on our cpp
//// level.
//#define declare_coro(symbol, ...) \
//  extern "C" Handle symbol##_coro(void *__VA_OPT__(, ) __VA_ARGS__)
//
//// Declares the struct method for which a coroutine will be generated.
//#define declare_target_method(ret, cls, symbol, ...) \
//  concat_attr(ltesttarget_, symbol) non_atomic ret cls::symbol(__VA_ARGS__)
//
//// Declares the cpp level launcher.
//// It is need, because we need to generate arguments on cpp level.
//// This launcher must be a coroutine itself.
//#define declare_cpp_launcher(generator, symbol)                              \
//  non_atomic_manual extern "C" int symbol##_cpp_launcher(                    \
//      void *this_ptr) noexcept {                                             \
//    auto generated_args = generator();                                       \
//    ltest::SetGenArgs(std::shared_ptr<void>(new std::tuple(generated_args)), \
//                      ltest::toStringList(generated_args));                  \
//    ltest::SetArgsInited(true);                                              \
//    CoroYield();                                                             \
//    return ltest::callCoroutineWithTuple(symbol##_coro, this_ptr,            \
//                                         generated_args);                    \
//  }                                                                          \
//                                                                             \
//  extern "C" Handle symbol##_cpp_launcher_coro(void *this_ptr);
//
//template <typename Function, typename Tuple, size_t... I>
//auto call(Function f, Tuple t, std::index_sequence<I...>) {
//  return f(std::get<I>(t)...);
//}
//
//// Declares the cpp level launcher.
//// It is need, because we need to generate arguments on cpp level.
//// This launcher must be a coroutine itself.
//#define declare_cpp_launcher_for_dual(generator, awaitable, class_name,      \
//                                      symbol)                                \
//  non_atomic_manual extern "C" int symbol##_cpp_launcher(                    \
//      void *this_ptr) noexcept {                                             \
//    auto generated_args = generator();                                       \
//    ltest::SetGenArgs(std::shared_ptr<void>(new std::tuple(generated_args)), \
//                      ltest::toStringList(generated_args));                  \
//    ltest::SetArgsInited(true);                                              \
//    CoroYield();                                                             \
//    static constexpr auto size =                                             \
//        std::tuple_size<decltype(generated_args)>::value;                    \
//    auto awaitable_object =                                                  \
//        call(std::bind(&class_name::symbol, (class_name *)this_ptr),         \
//             generated_args, std::make_index_sequence<size>{});              \
//    auto callback_coroutine = std::coroutine_handle<>(/* TODO */);           \
//    return ltest::callCoroutine(awaitable##_await_suspended_coro,            \
//                                (void *)(&awaitable_object),                 \
//                                callback_coroutine);                         \
//  }                                                                          \
//                                                                             \
//  extern "C" Handle symbol##_cpp_launcher_coro(void *this_ptr);
//
//// Allows to clone the task, starting from the beginning
//// and provide the same arguments.
//#define declare_cloner_launcher(generator, symbol)                \
//  non_atomic_manual extern "C" int symbol##_clone_launcher(       \
//      void *this_ptr, void *raw_args) noexcept {                  \
//    assert(raw_args && "raw_args pointer is nullptr");            \
//    auto generated_args_ptr =                                     \
//        reinterpret_cast<decltype(generator()) *>(raw_args);      \
//    return ltest::callCoroutineWithTuple(symbol##_coro, this_ptr, \
//                                         *generated_args_ptr);    \
//  }                                                               \
//  extern "C" Handle symbol##_clone_launcher_coro(void *this_ptr,  \
//                                                 void *raw_args);
//
//// Allows to clone the task, starting from the beginning
//// and provide the same arguments.
//#define declare_cloner_launcher_dual(generator, awaitable, class_name, symbol) \
//  non_atomic_manual extern "C" int symbol##_clone_launcher(                    \
//      void *this_ptr, void *raw_args) noexcept {                               \
//    assert(raw_args && "raw_args pointer is nullptr");                         \
//    auto generated_args_ptr =                                                  \
//        reinterpret_cast<decltype(generator()) *>(raw_args);                   \
//    static constexpr auto size = std::tuple_size<                              \
//        std::remove_reference_t<decltype(*generated_args_ptr)>>::value;        \
//    /* TODO: allocate awaitable_object on heap */                              \
//    auto awaitable_object =                                                    \
//        call(std::bind(&class_name::symbol, (class_name *)this_ptr),           \
//             *generated_args_ptr, std::make_index_sequence<size>{});           \
//    auto callback_coroutine = std::coroutine_handle<>(/* TODO */);             \
//    return ltest::callCoroutine(awaitable##_await_suspended_coro,              \
//                                (void *)(&awaitable_object),                   \
//                                callback_coroutine);                           \
//  }                                                                            \
//  extern "C" Handle symbol##_clone_launcher_coro(void *this_ptr,               \
//                                                 void *raw_args);
//
//// Declares the cpp level builder. Returns a task.
//#define declare_cpp_builder(symbol)                                         \
//  Task symbol##_cpp_builder(void *this_ptr) {                               \
//    ltest::SetArgsInited(false);                                            \
//    auto hdl = symbol##_cpp_launcher_coro(this_ptr);                        \
//    auto task = Task{hdl, &symbol##_clone_launcher_coro};                   \
//    /* Waiting until builder generate arguments. */                         \
//    while (!ltest::gen_args.inited) {                                       \
//      task.Resume();                                                        \
//    }                                                                       \
//    /*TODO: тут нужно выставлять про блокинг */ \
//    task.SetMeta(std::make_shared<Task::Meta>(                              \
//        std::string{symbol##_task_name}, std::move(ltest::gen_args.args),   \
//        std::move(ltest::gen_args.str_args)));                              \
//    return task;                                                            \
//  }
//
//// Public entrypoint.
//#define target_method(generator, ret, cls, symbol, ...)                 \
//  declare_task_name(symbol);                                            \
//  declare_coro(symbol, __VA_ARGS__);                                    \
//  declare_cpp_launcher(generator, symbol);                              \
//  declare_cloner_launcher(generator, symbol);                           \
//  declare_cpp_builder(symbol);                                          \
//  namespace ltest {                                                     \
//  ltest::TaskBuilderPusher symbol##__task__push{&symbol##_cpp_builder}; \
//  }                                                                     \
//  declare_target_method(ret, cls, symbol, __VA_ARGS__)
//
//// awaitable_method(void, recive_awaiter) {code}
//
//// Symbol is the name of the method that will return the awaitable
//#define awaitable_method(ret, data_structure_class, promise_class,     \
//                         coroutine_handle_arg)                         \
//  /*declare_task_name(await_suspended);*/                              \
//  declare_coro(promise_class##_await_suspended, coroutine_handle_arg); \
//  declare_target_method(ret, data_structure_class::promise_class,      \
//                        promise_class##_await_suspended, coroutine_handle_arg)
//
//// target_method_dual(generator_int, ret)
//
//// Коуртина для symbol не нужна, нужна только для awaitable
//#define target_method_dual(generator, ret, promise_type, cls, symbol, ...) \
//  declare_task_name(symbol);                                               \
//  declare_cpp_launcher_for_dual(generator, promise_type, cls, symbol);     \
//  declare_cloner_launcher_dual(generator, promise_type, cls, symbol);      \
//  declare_cpp_builder(symbol);                                             \
//  namespace ltest {                                                        \
//  ltest::TaskBuilderPusher symbol##__task__push{&symbol##_cpp_builder};    \
//  }                                                                        \
//  ret cls::symbol(__VA_ARGS__)

#define target_method(gen, ret, cls, symbol, ...)          \
  declare_task_name(symbol);                               \
  ltest::TargetMethod<ret, cls __VA_OPT__(, ) __VA_ARGS__> \
      symbol##_ltest_method_cls{symbol##_task_name, gen, &cls::symbol};

#define target_method_dual(generator, ret, promise_type, cls, symbol, ...) \
  declare_task_name(symbol);
