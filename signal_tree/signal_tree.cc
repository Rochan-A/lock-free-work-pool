#include "signal_tree/signal_tree.h"
#include <atomic>
#include <cstddef>
#include <iostream>
#include <vector>

SignalTree::SignalTree(std::size_t num_leaves) {
  // Round up num_leaves to the next power of two for convenience.
  // (If you already have a multiple of two, you can remove this step.)
  num_leaves_ = 1;
  while (num_leaves_ < num_leaves) {
    num_leaves_ <<= 1; // power of two
  }

  // The tree is a binary tree stored in an array.
  // Number of internal nodes is num_leaves_ - 1.
  // So total size is (num_leaves_ - 1) + num_leaves_ = 2 * num_leaves_ - 1.
  std::size_t total_size = 2 * num_leaves_ - 1;
  tree_.resize(total_size + 1); // +1 so we can use 1-based indexing.

  // Leaves start at offset_ = num_leaves_.
  offset_ = num_leaves_;

  // Initialize all leaves to 1 (meaning "free").
  for (std::size_t i = offset_; i < offset_ + num_leaves_; ++i) {
    tree_[i].store(1, std::memory_order_relaxed);
  }

  // Build sums in the internal nodes (indices [1..offset_-1]).
  for (std::size_t i = offset_ - 1; i >= 1; --i) {
    tree_[i].store(tree_[2 * i].load(std::memory_order_relaxed) +
                       tree_[2 * i + 1].load(std::memory_order_relaxed),
                   std::memory_order_relaxed);
  }
}

int SignalTree::Acquire() {
  while (true) {
    // Check root; if it's zero, then no leaves are free.
    int root_val = tree_[1].load(std::memory_order_acquire);
    if (root_val == 0) {
      // No available leaves
      return -1;
    }

    // We do a top-down traversal to find a free leaf (value=1).
    std::size_t idx = 1; // start at root
    while (idx < offset_) {
      // Descend to left or right child depending on which has a positive sum.
      // We'll read the left child's sum; if it is > 0, we go left,
      // else we go right.
      int left_val = tree_[2 * idx].load(std::memory_order_acquire);
      if (left_val > 0) {
        idx = 2 * idx;
      } else {
        idx = 2 * idx + 1;
      }
    }

    // Now idx is at a leaf. Try to set it from 1 -> 0.
    int expected = 1;
    if (tree_[idx].compare_exchange_strong(expected, 0,
                                           std::memory_order_acq_rel,
                                           std::memory_order_relaxed)) {
      // Succeeded in reserving this leaf. We must now propagate the
      // decrement up the tree. We'll do a loop up to the root. The
      // difference is 1 (we changed from 1 to 0).
      std::size_t parent = idx / 2;
      std::size_t child = idx;
      while (parent >= 1) {
        tree_[parent].fetch_sub(1, std::memory_order_acq_rel);
        child = parent;
        parent /= 2;
      }

      // Return the leaf index for later Release calls.
      return static_cast<int>(idx - offset_);
    }
    // If the CAS failed, that means another thread took the leaf at the
    // same time. We retry from the root.
  }
}

void SignalTree::Release(int leaf_index) {
  if (leaf_index < 0) {
    return;
  }

  std::size_t idx = offset_ + static_cast<std::size_t>(leaf_index);
  // Attempt to set from 0 -> 1. In a correct usage scenario, the
  // leaf should be 0 if it has been acquired.
  int expected = 0;
  if (!tree_[idx].compare_exchange_strong(
          expected, 1, std::memory_order_acq_rel, std::memory_order_relaxed)) {
    // This would be an error in correct usage,
    // but we do not attempt to handle it here.
    return;
  }

  // Propagate the increment (+1) up to the root.
  std::size_t parent = idx / 2;
  while (parent >= 1) {
    tree_[parent].fetch_add(1, std::memory_order_acq_rel);
    parent /= 2;
  }
}

int SignalTree::FreeCount() const {
  // Root holds sum of all leaves
  return tree_[1].load(std::memory_order_acquire);
}
