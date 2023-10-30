#pragma once

#include <hazard/manager.hpp>

#include <atomic>
#include <optional>

// Michael-Scott unbounded MPMC lock-free queue

template <typename T>
class LockFreeQueue {
  struct Node {
    std::atomic<Node*> next{nullptr};
    std::optional<T> item;
  };

 public:
  explicit LockFreeQueue(hazard::Manager* gc)
      : head_{new Node{}},
        tail_{head_.load()},
        gc_{gc} {
  }

  LockFreeQueue()
      : LockFreeQueue(hazard::Manager::Get()) {
  }

  void Push(T item) {
    Node* node = new Node{nullptr, std::move(item)};
    Node* t;
    auto mutator = gc_->MakeMutator();

    while (true) {
      t = mutator.Protect(0, tail_);

      Node* next = t->next.load();

      if (next != nullptr) {
        tail_.compare_exchange_weak(t, next);
        continue;
      }

      Node* temp = nullptr;
      if (t->next.compare_exchange_weak(temp, node)) {
        break;
      }
    }

    tail_.compare_exchange_strong(t, node);
  }

  std::optional<T> TryPop() {
    std::optional<T> result;
    auto mutator = gc_->MakeMutator();

    while (true) {
      Node* h = mutator.Protect(0, head_);
      Node* t = tail_.load();
      Node* next = mutator.Protect(1, head_.load()->next);

      if (next == nullptr) {
        return std::nullopt;
      }
      if (t == h) {
        tail_.compare_exchange_weak(t, next);
        continue;
      }

      if (head_.compare_exchange_weak(h, next)) {
        result = std::move(*next->item);
        mutator.Retire(next);
        break;
      }
    }
    return std::move(*result);
  }

  ~LockFreeQueue() {
    Node* curr = head_.load();

    while (curr != nullptr) {
      Node* next = curr->next.load();
      delete curr;
      curr = next;
    }
  }

 private:
  std::atomic<Node*> head_{nullptr};
  std::atomic<Node*> tail_{nullptr};

  hazard::Manager* gc_;
};
