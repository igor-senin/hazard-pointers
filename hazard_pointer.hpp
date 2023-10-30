#pragma once

namespace hazard {
namespace detail {

struct HazardPointer {
  void* pointer{nullptr};
  void (*reclaimer)(void*){nullptr};

  void Reclaim() {
    reclaimer(pointer);
  }
};

}  // namespace detail
}  // namespace hazard
