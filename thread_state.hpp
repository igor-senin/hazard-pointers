#pragma once

#include <hazard/hazard_pointer.hpp>

#include <array>
#include <atomic>
#include <forward_list>
#include <thread>
#include <vector>

namespace hazard {

struct ThreadState;

std::atomic<ThreadState*> thread_state_head{nullptr};

thread_local ThreadState* state = nullptr;

struct ThreadState {
  // using AtomicVoidPtr = twist::ed::stdlike::atomic<void*>;

  struct AtomicVoidPtr {
    std::atomic<void*> ptr{nullptr};

    void store(void* p) {
      ptr.store(p);
    }

    void* load() {
      return ptr.load();
    }

    bool operator<(const void* const& rhs) {
      return ptr.load() < rhs;
    }

    bool operator>(const void* const& rhs) {
      return ptr.load() > rhs;
    }

    bool operator<=(const void* const& rhs) {
      return ptr.load() <= rhs;
    }

    bool operator>=(const void* const& rhs) {
      return ptr.load() >= rhs;
    }

    bool operator==(const void* const& rhs) {
      return ptr.load() == rhs;
    }

    bool operator!=(const void* const& rhs) {
      return ptr.load() != rhs;
    }
  };

  std::array<AtomicVoidPtr, 10> protected_; // TODO: arbitrary number, not 10
  std::forward_list<detail::HazardPointer> retired_;

  size_t retired_size_{0};

  ThreadState* next_{nullptr};

  ThreadState() {
    while (!thread_state_head.compare_exchange_weak(next_, this)) {
      // pass
    }
  }

  template <typename Func>
  void Iterate(Func&& callback) {
    ThreadState* iter = this;

    while (iter != nullptr) {
      ThreadState* next = iter->next_;
      callback(iter);
      iter = next;
    }
  }

  void EmplaceRetired(detail::HazardPointer ptr) {
    retired_.emplace_front(ptr);
    ++retired_size_;
  }
};

}  // namespace hazard
