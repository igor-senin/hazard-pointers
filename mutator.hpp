#pragma once

#include <hazard/fwd.hpp>
#include <hazard/global_retire_queue.hpp>
#include <hazard/thread_state.hpp>

#include <algorithm>
#include <atomic>
#include <cstdlib>

namespace hazard {

class Mutator {
  template <typename T>
  using AtomicPtr = std::atomic<T*>;

 public:
  Mutator(Manager* manager)
      : manager_{manager} {
    if (state == nullptr) {
      state = new ThreadState{};
    }
  }

  template <typename T>
  T* Protect(size_t index, AtomicPtr<T>& ptr) {
    T* p;
    do {
      p = ptr.load();
      Announce(index, p);
    } while (p != ptr.load());

    manager_->FetchAddHazardous();

    return p;
  }

  template <typename T>
  void Announce(size_t index, T* ptr) {
    state->protected_[index].store(ptr);
  }

  template <typename T>
  void Retire(T* ptr) {
    // Check that ptr is protected
    auto pos = std::find(std::begin(state->protected_),
                         std::end(state->protected_), ptr);  // AtomicVoidPtr*
    if (pos == std::end(state->protected_)) {
      return;
    }

    // Now ptr is not protected by this thread
    (*pos).store(nullptr);

    state->EmplaceRetired({ptr, [](void* p) {
                             delete reinterpret_cast<T*>(p);
                           }});

    if (state->retired_size_ > 2 * manager_->FetchSubHazardous()) {
      manager_->Collect();
    }
  }

  void Clear() {
    // Free local protected pointers
    for (auto& item : state->protected_) {
      item.store(nullptr);
    }

    // Move all local retired nodes to global retired queue
    manager_->GetGlobalRetireQueue().EnqueueForwardList(
        std::move(state->retired_));
    state->retired_size_ = 0;
  }

  ~Mutator() {
    Clear();
  }

  Manager* manager_;
};

}  // namespace hazard
