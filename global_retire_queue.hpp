#pragma once

#include <atomic>
#include <forward_list>

namespace hazard {
namespace detail {

// MPSC lock-free (Treiber's) stack
template <typename T>
class GlobalRetireQueue {
 public:
  struct Node {
    T value;
    Node* next;
  };

 public:
  void Enqueue(T value) {
    Node* new_node = new Node{std::move(value), head_.load()};

    while (!head_.compare_exchange_weak(new_node->next, new_node)) {
      // pass
    }
  }

  size_t EnqueueList(Node* new_head) {
    if (new_head == nullptr) {
      return 0;
    }

    size_t size = 1;
    Node* last = new_head;
    while (last->next != nullptr) {
      last = last->next;
      ++size;
    }

    while (!head_.compare_exchange_weak(last->next, new_head)) {
      // pass
    }
    return size;
  }

  size_t EnqueueForwardList(std::forward_list<T> fl) {
    Node* prev = nullptr;
    while (!fl.empty()) {
      Node* new_node = new Node{fl.front(), prev};
      prev = new_node;
      fl.pop_front();
    }

    return EnqueueList(prev);
  }

  Node* DequeueAll() {
    return head_.exchange(nullptr);
  }

  ~GlobalRetireQueue() {
    while (head_.load() != nullptr) {
      delete head_.exchange(head_.load()->next);
    }
  }

 private:
  static void Reclaim(void* ptr) {
    delete reinterpret_cast<T*>(ptr);
  }

 private:
  std::atomic<Node*> head_{nullptr};
};

}  // namespace detail
}  // namespace hazard
