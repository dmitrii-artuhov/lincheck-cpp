#include <cassert>
#include <coroutine>
#include <iostream>
#include <optional>
#include <string>

// Stub.
// TODO: implement.
#define EXECUTE(...)

template <class T> struct Task {
  struct promise_type {
    using coro_handle = std::coroutine_handle<promise_type>;

    promise_type() {}

    auto get_return_object() { return coro_handle::from_promise(*this); }
    auto initial_suspend() { return std::suspend_always{}; }
    auto final_suspend() noexcept { return std::suspend_always{}; }
    void return_value(T value) { res = value; }
    void unhandled_exception() { std::terminate(); }

    std::optional<T> res;
    std::optional<coro_handle> parent_handler;
    std::optional<coro_handle> child_handler;
  };

  ~Task<T>() {
    if (handle) {
      handle.destroy();
    }
  }

  using coro_handle = std::coroutine_handle<promise_type>;
  Task<T>(coro_handle handle) : handle(handle) {}
  Task<T>(Task<T> &&other_task) {
    handle = other_task.handle;
    other_task.handle = nullptr;
  }

  coro_handle handle{nullptr};
};

template <class T> auto operator co_await(Task<T> &task) {
  struct task_awaiter {
    using promise_type = typename Task<T>::promise_type;

    task_awaiter(promise_type &p) : Promise(p) {}

    constexpr bool await_ready() const noexcept { return false; }

    void await_suspend(std::coroutine_handle<promise_type> parent) noexcept {
      Promise.parent_handler = parent;
      promise_type &parent_promise = parent.promise();
      parent_promise.child_handler =
          std::coroutine_handle<promise_type>::from_promise(Promise);
    }

    constexpr T await_resume() const noexcept { return Promise.res.value(); };

    promise_type &Promise;
  };

  return task_awaiter{task.handle.promise()};
}

template <class T> struct TaskWrapper {
  TaskWrapper(Task<T> &&task) : task(std::move(task)) {
    cur_handle = task.handle;
  }

  TaskWrapper(TaskWrapper<T> &&other_wrapper)
      : task(std::move(other_wrapper.task)) {
    cur_handle = task.handle;
  }

  // Проверяем, что внешняя корутина вернула значение.
  bool has_next() { return !task.handle.promise().res.has_value(); }

  // Возвращенное значение внешней корутины.
  T get_result() { return task.handle.promise().res.value(); }

  void next(bool skip_atomics) {
    if (!skip_atomics) {
      step(true);
      return;
    }
    step(false);
    while (cur_handle != task.handle) {
      step(false);
    }
  }

  void step(bool skip_return) {
    assert(has_next());

    // Текущий хэндлер завершился на предыдущем шаге.
    if (cur_handle.promise().res.has_value()) {
      // Переставляем cur_handle на родителя.
      cur_handle = cur_handle.promise().parent_handler.value();
      // Говорим, что у родителя больше нет ребенка.
      cur_handle.promise().child_handler.reset();
      if (skip_return) {
        next(skip_return);
      }
      return;
    }

    // Исполняем текущий хэндл.
    cur_handle.resume();
    // Если он в процессе породил ребенка, то меняем handle на него.
    if (cur_handle.promise().child_handler.has_value()) {
      cur_handle = cur_handle.promise().child_handler.value();
    }
  }

  Task<T> task;
  decltype(Task<T>::handle) cur_handle;
};

template <typename T> T execute(Task<T> &task) {
  auto cur_handle = task.handle;
  std::cout << "start" << std::endl;
  while (true) {
    if (cur_handle.promise().res.has_value()) {
      std::cout << "have returned" << std::endl;
      // Функция уже вернула значение, это значит, что нужно вернуться
      // вверх по стэку.
      if (!cur_handle.promise().parent_handler.has_value()) {
        break;
      }
      cur_handle = cur_handle.promise().parent_handler.value();
      cur_handle.promise().child_handler.reset();
    } else {
      std::cout << "execute current" << std::endl;
      // Нужно исполнить текущего челика и если вдруг он позвал кого-то другого,
      // то нужно это понять.
      cur_handle.resume();
      if (cur_handle.promise().child_handler.has_value()) {
        cur_handle = cur_handle.promise().child_handler.value();
      }
    }
  }
  auto res = cur_handle.promise().res.value();
  std::cout << "res = " << res << std::endl;
  return res;
}
