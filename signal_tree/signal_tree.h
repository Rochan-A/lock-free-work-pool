#pragma once

#include <atomic>
#include <deque>
#include <memory>

namespace signal_tree {

class SignalTree {
public:
  explicit SignalTree(const size_t capacity);

  ~SignalTree() = default;

  // Acquire a free leaf (if any). Returns -1 if none is free.
  const int Acquire();

  // Release a leaf back to free state.
  void Release(const int &index);

  // True if there is at least one free leaf in the tree.
  const bool IsFree() const {
    return tree_[1].load(std::memory_order_acquire) > 0;
  }

  // Returns the number of free leaves in the tree.
  const int FreeCount() const {
    return tree_[1].load(std::memory_order_acquire);
  }

  // Number of leaves (capacity).
  const size_t Capacity() const { return capacity_; }

  // Non-copyable, non-assignable
  SignalTree(const SignalTree &) = delete;
  SignalTree &operator=(const SignalTree &) = delete;

private:
  const size_t capacity_{0};

  // We store 2*kCapacity nodes in a segment-tree layout:
  //    - Internal nodes [1..kCapacity-1] store sums of children.
  //    - Leaves [kCapacity..2*kCapacity-1] store 1 (free) or 0 (acquired).
  // Index 0 is unused, so that node i has children (2*i) and (2*i + 1).
  std::deque<std::atomic<int>> tree_;
};

} // namespace signal_tree
