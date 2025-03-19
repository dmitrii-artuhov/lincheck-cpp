#pragma once

#include <atomic>
#include <iostream>

// TODO: write proper delegration to std::atomic<T> here.
// This class is intended to be the entry point
// for the weak memory logic later.
template<class T>
struct LTestAtomic {
  std::atomic<T> a;

  T load(std::memory_order order = std::memory_order_seq_cst) const {
    std::cout << "LTestAtomic load()" << std::endl;
    return a.load(order);
  }

  void store(T desired, std::memory_order order = std::memory_order_seq_cst) noexcept {
    std::cout << "LTestAtomic store()" << std::endl;
    a.store(desired, order);
  }

  T fetch_add(T arg, std::memory_order order = std::memory_order_seq_cst) noexcept {
    std::cout << "LTestAtomic fetch_add()" << std::endl;
    return a.fetch_add(arg, order);
  }

  bool compare_exchange_strong(T& expected, T val, std::memory_order order = std::memory_order_seq_cst) noexcept {
    std::cout << "LTestAtomic compare_exchange_strong()" << std::endl;
    return a.compare_exchange_strong(expected, val, order);
  }

  T operator=(T desired) noexcept {
    std::cout << "LTestAtomic operator=()" << std::endl;
    store(desired);
    return desired;
  }
};