#pragma once

#include <atomic>
#include <iostream>

#include "lib.h"
#include "wmm.h"

// This class is intended to be the entry point
// for the weak memory logic.
template <class T>
class LTestAtomic {
  std::atomic<T> atomicValue;
  int locationId;
  ExecutionGraph& wmmGraph = ExecutionGraph::getInstance();

 public:
#if __cplusplus >= 201703L  // C++17
  static constexpr bool is_always_lock_free =
      std::atomic<T>::is_always_lock_free;
#endif

  // Constructors
  constexpr LTestAtomic() noexcept: LTestAtomic(T{}) {}
  constexpr LTestAtomic(T desired) noexcept : atomicValue(desired) {
    locationId = wmmGraph.RegisterLocation(desired);
  }
  LTestAtomic(const LTestAtomic&) = delete;
  LTestAtomic& operator=(const LTestAtomic&) = delete;
  LTestAtomic& operator=(const LTestAtomic&) volatile = delete;

  // load
  T load(std::memory_order order = std::memory_order_seq_cst) const noexcept {
    if (this_coro) {
      // std::cout << "Load: coro id=" << this_coro->GetId() << ", thread=" << this_thread_id
      //           << ", name=" << this_coro->GetName() << std::endl;
      T value = wmmGraph.Load<T>(locationId, this_thread_id, WmmUtils::OrderFromStd(order));
      return value;
    }
    
    return atomicValue.load(order);
  }

  T load(std::memory_order order = std::memory_order_seq_cst) const
      volatile noexcept {
    return load(order);
  }

  // store
  void store(T desired,
             std::memory_order order = std::memory_order_seq_cst) noexcept {
    atomicValue.store(desired, order);
    if (this_coro) {
      // std::cout << "Store: coro id=" << this_coro->GetId() << ", thread=" << this_thread_id
      //           << ", name=" << this_coro->GetName() << std::endl;
      wmmGraph.Store(locationId, this_thread_id, WmmUtils::OrderFromStd(order), desired);
    }
  }

  void store(T desired, std::memory_order order =
                            std::memory_order_seq_cst) volatile noexcept {
    store(desired, order);
  }

  // operator=
  T operator=(T desired) noexcept {
    store(desired);
    return desired;
  }

  T operator=(T desired) volatile noexcept {
    store(desired);
    return desired;
  }

  // is_lock_free
  bool is_lock_free() const noexcept { return atomicValue.is_lock_free(); }

  bool is_lock_free() const volatile noexcept {
    return atomicValue.is_lock_free();
  }

  // operator T()
  operator T() const noexcept { return load(); }

  operator T() const volatile noexcept { return load(); }

  // exchange
  T exchange(T desired,
             std::memory_order order = std::memory_order_seq_cst) noexcept {
    return atomicValue.exchange(desired, order);
  }

  T exchange(T desired, std::memory_order order =
                            std::memory_order_seq_cst) volatile noexcept {
    return atomicValue.exchange(desired, order);
  }

  // compare_exchange_weak
  bool compare_exchange_weak(
    T& expected,
    T desired,
    std::memory_order success,
    std::memory_order failure
  ) noexcept {
    // we want to prevent actual atomics from overriding 'expected' value on rmw failure
    T myExpected = expected;
    bool value = atomicValue.compare_exchange_weak(myExpected, desired, success, failure);

    if (this_coro) {
      // std::cout << "Compare exchange weak: coro id=" << this_coro->GetId() << ", thread=" << this_thread_id
      //           << ", name=" << this_coro->GetName() << std::endl;
      auto [rmwSuccess, readValue] = wmmGraph.ReadModifyWrite(
        locationId, this_thread_id,
        &expected, desired,
        WmmUtils::OrderFromStd(success), 
        WmmUtils::OrderFromStd(failure)
      );
      value = rmwSuccess;
    }
    else {
      expected = myExpected; // update expected only if we are not in a coroutine
    }

    return value;
  }

  bool compare_exchange_weak(
    T& expected,
    T desired,
    std::memory_order success,
    std::memory_order failure
  ) volatile noexcept {
    return compare_exchange_weak(expected, desired, success, failure);
  }

  bool compare_exchange_weak(
    T& expected, T desired,
    std::memory_order order = std::memory_order_seq_cst
  ) noexcept {
    return compare_exchange_weak(expected, desired, order, std::memory_order_seq_cst);
  }

  bool compare_exchange_weak(
    T& expected, T desired,
    std::memory_order order = std::memory_order_seq_cst
  ) volatile noexcept {
    return compare_exchange_weak(expected, desired, order, std::memory_order_seq_cst);
  }

  // compare_exchange_strong
  bool compare_exchange_strong(
    T& expected,
    T desired,
    std::memory_order success,
    std::memory_order failure
  ) noexcept {
    // we want to prevent actual atomics from overriding 'expected' value on rmw failure
    T myExpected = expected;
    bool value = atomicValue.compare_exchange_strong(myExpected, desired, success, failure);
    
    if (this_coro) {
      auto [rmwSuccess, readValue] = wmmGraph.ReadModifyWrite(
        locationId, this_thread_id,
        &expected, desired,
        WmmUtils::OrderFromStd(success), 
        WmmUtils::OrderFromStd(failure)
      );
      value = rmwSuccess;
    }
    else {
      expected = myExpected; // update expected only if we are not in a coroutine
    }
    
    return value;
  }

  bool compare_exchange_strong(
    T& expected,
    T desired,
    std::memory_order success,
    std::memory_order failure
  ) volatile noexcept {
    return compare_exchange_strong(expected, desired, success, failure);
  }

  bool compare_exchange_strong(
      T& expected, T desired,
      std::memory_order order = std::memory_order_seq_cst) noexcept {
    return compare_exchange_strong(expected, desired, order, std::memory_order_seq_cst);
  }

  bool compare_exchange_strong(
      T& expected, T desired,
      std::memory_order order = std::memory_order_seq_cst) volatile noexcept {
    return compare_exchange_strong(expected, desired, order, std::memory_order_seq_cst);
  }

// wait
#if __cplusplus >= 202002L  // C++20
  void wait(T old, std::memory_order order =
                       std::memory_order_seq_cst) const noexcept {
    atomicValue.wait(old, order);
  }

  void wait(T old, std::memory_order order = std::memory_order_seq_cst) const
      volatile noexcept {
    atomicValue.wait(old, order);
  }

  // notify_one
  void notify_one() noexcept { atomicValue.notify_one(); }

  void notify_one() volatile noexcept { atomicValue.notify_one(); }

  // notify all
  void notify_all() noexcept { atomicValue.notify_all(); }

  void notify_all() volatile noexcept { atomicValue.notify_all(); }
#endif

  // fetch_add
  T fetch_add(T arg,
              std::memory_order order = std::memory_order_seq_cst) noexcept {
    return atomicValue.fetch_add(arg, order);
  }

  T fetch_add(T arg, std::memory_order order =
                         std::memory_order_seq_cst) volatile noexcept {
    return atomicValue.fetch_add(arg, order);
  }

  // TODO: fix ambiguity with specialization for T*
  // T* fetch_add(std::ptrdiff_t arg, std::memory_order order =
  // std::memory_order_seq_cst) noexcept {
  //   return atomicValue.fetch_add(arg, order);
  // }

  // T* fetch_add(std::ptrdiff_t arg, std::memory_order order =
  // std::memory_order_seq_cst) volatile noexcept {
  //   return atomicValue.fetch_add(arg, order);
  // }

  // fetch_sub
  T fetch_sub(T arg,
              std::memory_order order = std::memory_order_seq_cst) noexcept {
    return atomicValue.fetch_sub(arg, order);
  }

  T fetch_sub(T arg, std::memory_order order =
                         std::memory_order_seq_cst) volatile noexcept {
    return atomicValue.fetch_sub(arg, order);
  }

  // TODO: fix ambiguity with specialization for T*
  // T* fetch_sub(std::ptrdiff_t arg, std::memory_order order =
  // std::memory_order_seq_cst) noexcept {
  //   return atomicValue.fetch_sub(arg, order);
  // }

  // T* fetch_sub(std::ptrdiff_t arg, std::memory_order order =
  // std::memory_order_seq_cst) volatile noexcept {
  //   return atomicValue.fetch_sub(arg, order);
  // }

  // operator+=
  T operator+=(T arg) noexcept { return atomicValue.operator+=(arg); }

  T operator+=(T arg) volatile noexcept { return atomicValue.operator+=(arg); }

  // TODO: fix ambiguity with specialization for T*
  // T* operator+=(std::ptrdiff_t arg) noexcept {
  //   return atomicValue.operator+=(arg);
  // }

  // T* operator+=(std::ptrdiff_t arg) volatile noexcept {
  //   return atomicValue.operator+=(arg);
  // }

  // operator-=
  T operator-=(T arg) noexcept { return atomicValue.operator-=(arg); }

  T operator-=(T arg) volatile noexcept { return atomicValue.operator-=(arg); }

  // TODO: fix ambiguity with specialization for T*
  // T* operator-=(std::ptrdiff_t arg) noexcept {
  //   return atomicValue.operator-=(arg);
  // }

  // T* operator-=(std::ptrdiff_t arg) volatile noexcept {
  //   return atomicValue.operator-=(arg);
  // }

  // fetch_max
  T fetch_max(T arg,
              std::memory_order order = std::memory_order_seq_cst) noexcept {
    return atomicValue.fetch_max(arg, order);
  }

  T fetch_max(T arg, std::memory_order order =
                         std::memory_order_seq_cst) volatile noexcept {
    return atomicValue.fetch_max(arg, order);
  }

  // fetch_min
  T fetch_min(T arg,
              std::memory_order order = std::memory_order_seq_cst) noexcept {
    return atomicValue.fetch_min(arg, order);
  }

  T fetch_min(T arg, std::memory_order order =
                         std::memory_order_seq_cst) volatile noexcept {
    return atomicValue.fetch_min(arg, order);
  }

  // operator++
  T operator++() noexcept { return atomicValue.operator++(); }

  T operator++() volatile noexcept { return atomicValue.operator++(); }

  T operator++(int) noexcept { return atomicValue.operator++(0); }

  T operator++(int) volatile noexcept { return atomicValue.operator++(0); }

  // operator--
  T operator--() noexcept { return atomicValue.operator--(); }

  T operator--() volatile noexcept { return atomicValue.operator--(); }

  T operator--(int) noexcept { return atomicValue.operator--(0); }

  T operator--(int) volatile noexcept { return atomicValue.operator--(0); }

  // fetch_and
  T fetch_and(T arg,
              std::memory_order order = std::memory_order_seq_cst) noexcept {
    return atomicValue.fetch_and(arg, order);
  }

  T fetch_and(T arg, std::memory_order order =
                         std::memory_order_seq_cst) volatile noexcept {
    return atomicValue.fetch_and(arg, order);
  }

  // fetch_or
  T fetch_or(T arg,
             std::memory_order order = std::memory_order_seq_cst) noexcept {
    return atomicValue.fetch_or(arg, order);
  }

  T fetch_or(T arg, std::memory_order order =
                        std::memory_order_seq_cst) volatile noexcept {
    return atomicValue.fetch_or(arg, order);
  }

  // fetch_xor
  T fetch_xor(T arg,
              std::memory_order order = std::memory_order_seq_cst) noexcept {
    return atomicValue.fetch_xor(arg, order);
  }

  T fetch_xor(T arg, std::memory_order order =
                         std::memory_order_seq_cst) volatile noexcept {
    return atomicValue.fetch_xor(arg, order);
  }

  // operator&=
  T operator&=(T arg) noexcept { return atomicValue.operator&=(arg); }

  T operator&=(T arg) volatile noexcept { return atomicValue.operator&=(arg); }

  // operator|=
  T operator|=(T arg) noexcept { return atomicValue.operator|=(arg); }

  T operator|=(T arg) volatile noexcept { return atomicValue.operator|=(arg); }

  // operator^=
  T operator^=(T arg) noexcept { return atomicValue.operator^=(arg); }

  T operator^=(T arg) volatile noexcept { return atomicValue.operator^=(arg); }
};