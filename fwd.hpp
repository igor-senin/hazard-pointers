#pragma once

#include <atomic>

#include <hazard/global_retire_queue.hpp>
#include <hazard/hazard_pointer.hpp>

namespace hazard {

class Mutator;

struct ThreadState;

namespace detail {

template <typename T>
class GlobalRetireQueue;

struct HazardPointer;

}  // namespace detail

class Manager {
 public:
  static Manager* Get() {
    static Manager instance;
    return &instance;
  }

  Mutator MakeMutator();

  void Collect();

  size_t FetchAddHazardous() noexcept;

  size_t FetchSubHazardous() noexcept;

  detail::GlobalRetireQueue<detail::HazardPointer>& /*TODO*/
  GetGlobalRetireQueue() noexcept;

  ~Manager();

 private:
  detail::GlobalRetireQueue<detail::HazardPointer> global_retired_;

  std::atomic<size_t> total_hazardous_{0};
};

}  // namespace hazard
