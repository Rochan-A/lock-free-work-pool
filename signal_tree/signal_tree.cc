#include <atomic>
#include <cassert>
#include <cmath>
#include <memory>
#include <vector>

#include "signal_tree/signal_tree.h"

namespace signal_tree {

SignalTree::SignalTree(const size_t capacity) : capacity_(capacity) {
  assert(capacity > 0 && ((capacity & (capacity - 1)) == 0));

  tree_.resize(capacity);

  // Initialize all leaves to 1 (meaning "free").
  for (size_t i = capacity; i < 2 * capacity; ++i) {
    tree_.emplace_back(1);
  }

  // Build sums in the internal nodes by summing children.
  for (size_t i = capacity - 1; i > 0; --i) {
    int left = tree_.at(2 * i).load(std::memory_order_relaxed);
    int right = tree_.at(2 * i + 1).load(std::memory_order_relaxed);
    tree_.at(i).store(left + right, std::memory_order_relaxed);
  }

  // Node 0 is unused, so we ignore it here.
}

const int SignalTree::Acquire() {
  // Decrement the root’s sum to see if a free slot is available. If the old
  // root value was < 0, there is no free leaf.
  std::atomic<int> &root = tree_.at(1);
  if (root.fetch_sub(1, std::memory_order_acquire) < 0) {
    // We decremented below zero; revert it and fail.
    root.fetch_add(1, std::memory_order_release);
    return -1;
  }

  // Find a free leaf by traversing top-down, using *loads* only. We rely on the
  // fact that we already decremented the root, so at least one leaf should
  // still be free.
  size_t idx = 1;
  while (idx < capacity_) {
    // Examine the left child's sum:
    std::atomic<int> &node = tree_.at(2 * idx);
    if (node.load(std::memory_order_relaxed) > 0) {
      // Go left if it still has a free leaf.
      idx = 2 * idx;
    } else {
      // Otherwise go right.
      idx = 2 * idx + 1;
    }
  }
  // At this point, idx is a leaf [kCapacity..2*kCapacity-1].

  // Try to mark this leaf as acquired: 1 -> 0.
  int expected = 1;
  if (!tree_.at(idx).compare_exchange_strong(
          expected, 0, std::memory_order_acq_rel, std::memory_order_relaxed)) {
    // Unexpected?
    throw std::runtime_error("Expected leaf to have 1, failed to acquire!");
  }

  // Success! idx - kCapacity is the “leaf index” in [0..kCapacity-1].
  return static_cast<int>(idx - capacity_);
}

void SignalTree::Release(const int &index) {
  if (index < 0 || static_cast<size_t>(index) >= capacity_) {
    throw std::runtime_error("Release() called with invalid index!");
  }

  // Mark leaf as free: 0 -> 1
  size_t leafIndex = capacity_ + static_cast<size_t>(index);
  int expected = 0;
  if (!tree_.at(leafIndex).compare_exchange_strong(
          expected, 1, std::memory_order_acq_rel, std::memory_order_relaxed)) {
    // Leaf wasn’t acquired in the first place, or a race occurred.
    throw std::runtime_error("Releasing a leaf that was not acquired!");
  }

  // Propagate +1 up the tree to the root.
  size_t parent = leafIndex / 2;
  while (parent >= 1) {
    tree_.at(parent).fetch_add(1, std::memory_order_acq_rel);
    parent /= 2;
  }
}

} // namespace signal_tree