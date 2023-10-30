#pragma once

#include <hazard/fwd.hpp>
#include <hazard/global_retire_queue.hpp>
#include <hazard/hazard_pointer.hpp>
#include <hazard/mutator.hpp>

#include <algorithm>
#include <atomic>
#include <vector>

namespace hazard {

/* class Manager */
/* Common for all threads. One instance per each lock-free structure */

Mutator Manager::MakeMutator() {
  return Mutator{this};
}

void Manager::Collect() {
  // 1. Collect protected pointers from all threads
  std::vector<void*> plist;
  thread_state_head.load()->Iterate([&](ThreadState* curr_state) {
    for (auto& ptr : curr_state->protected_) {
      if (auto p = ptr.load()) {
        plist.push_back(p);
      }
    }
  });
  // 2. Sort plist
  std::sort(plist.begin(), plist.end());
  plist.erase(std::unique(plist.begin(), plist.end()), plist.end());
  // 3. Capture global retire queue
  decltype(global_retired_)::Node* global = global_retired_.DequeueAll();
  while (global != nullptr) {
    state->retired_.emplace_front(std::move(global->value));
    auto next = global->next;
    delete global;
    global = next;
  }
  // 4. Try reclaim local retired pointers
  auto captured = std::move(state->retired_);
  state->retired_size_ = 0;

  while (!captured.empty()) {
    detail::HazardPointer hazptr = std::move(captured.front());
    captured.pop_front();
    if (std::binary_search(plist.begin(), plist.end(), hazptr.pointer)) {
      state->EmplaceRetired(std::move(hazptr));
      continue;
    }

    hazptr.Reclaim();
  }
}

size_t Manager::FetchAddHazardous() noexcept {
  return total_hazardous_.fetch_add(1);
}

size_t Manager::FetchSubHazardous() noexcept {
  return total_hazardous_.fetch_sub(1);
}

detail::GlobalRetireQueue<detail::HazardPointer>& /*TODO*/
Manager::GetGlobalRetireQueue() noexcept {
  return global_retired_;
}

Manager::~Manager() {
  decltype(global_retired_)::Node* global = global_retired_.DequeueAll();
  while (global != nullptr) {
    global->value.Reclaim();
    auto next = global->next;
    delete global;
    global = next;
  }

  //  thread_state_head.load()->Iterate([](ThreadState* current) {
  //    delete current;
  //  });
}

}  // namespace hazard
