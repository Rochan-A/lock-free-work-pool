#include <atomic>
#include <cassert>
#include <cmath>
#include <iostream>
#include <memory>
#include <vector>

#include "signal_tree/signal_tree.h"

#define PRINT_TREE(msg, tree, capacity)                                        \
  do {                                                                         \
    std::cout << msg << ": ";                                                  \
    for (size_t i = 1; i < 2 * capacity; ++i) {                                \
      std::cout << tree.at(i).load() << " ";                                   \
    }                                                                          \
    std::cout << "\n";                                                         \
  } while (0)

namespace signal_tree {

SignalTree::SignalTree(const size_t capacity) : capacity_(capacity) {
  assert(capacity > 0 && ((capacity & (capacity - 1)) == 0));

  tree_.resize(2 * capacity);

  // Initialize all leaves to 1 (meaning "free").
  for (size_t i = 0; i < 2 * capacity; ++i) {
    tree_.at(i).store(1);
  }

  // Build sums in the internal nodes by summing children.
  for (size_t i = capacity - 1; i > 0; --i) {
    int left = tree_.at(2 * i).load(std::memory_order_relaxed);
    int right = tree_.at(2 * i + 1).load(std::memory_order_relaxed);
    tree_.at(i).store(left + right, std::memory_order_relaxed);
  }
}

const int SignalTree::Acquire() {
  // Decrement the root’s sum to see if a free slot is available. If the old
  // root value was < 0, there is no free leaf.
  if (tree_.at(1).fetch_sub(1, std::memory_order_seq_cst) <= 0) {
    // We decremented below zero; revert it and fail.
    tree_.at(1).fetch_add(1, std::memory_order_seq_cst);
    return -1;
  }

  std::vector<size_t> path;
  path.push_back(1);

  size_t idx = 1;

  while (idx < capacity_) {
    size_t leftIdx = 2 * idx;
    size_t rightIdx = 2 * idx + 1;

    if (leftIdx >= capacity_) {
      int expected = 1;
      if (tree_[leftIdx].compare_exchange_strong(expected, 0,
                                                 std::memory_order_seq_cst)) {
        idx = leftIdx;
        break;
      }
    }
    if (rightIdx >= capacity_) {
      int expected = 1;
      if (!tree_[rightIdx].compare_exchange_strong(expected, 0,
                                                   std::memory_order_seq_cst)) {
        // revert all internal nodes on path
        for (size_t node : path) {
          tree_[node].fetch_add(1);
        }
        // TODO: make this impossible.
        return -1;
      }
      idx = rightIdx;
      break;
    }

    // Left node:
    if (tree_.at(leftIdx).fetch_sub(1, std::memory_order_seq_cst) <= 0) {
      tree_.at(leftIdx).fetch_add(1, std::memory_order_seq_cst);

      // Right node:
      if (tree_.at(rightIdx).fetch_sub(1, std::memory_order_seq_cst) <= 0) {
        tree_.at(rightIdx).fetch_add(1, std::memory_order_seq_cst);
        // Both children are out of capacity; revert entire path
        for (auto node : path) {
          tree_[node].fetch_add(1, std::memory_order_release);
        }
        // TODO: make this impossible.
        return -1;
      } else {
        idx = rightIdx;
        path.push_back(rightIdx);
      }
    } else {
      idx = leftIdx;
      path.push_back(leftIdx);
    }
  }

  return static_cast<int>(idx - capacity_);
}

void SignalTree::Release(const int &index) {
  if (index < 0 || static_cast<size_t>(index) >= capacity_) {
    throw std::runtime_error("Release() called with invalid index!");
  }

  // Mark leaf as free: 0 -> 1
  size_t leafIndex = capacity_ + static_cast<size_t>(index);
  int expected = 0;
  if (!tree_.at(leafIndex).compare_exchange_strong(expected, 1,
                                                   std::memory_order_seq_cst)) {
    throw std::runtime_error("Releasing a leaf that was 0, CAS failed!");
  }

  // Propagate +1 up the tree to the root.
  size_t parent = leafIndex / 2;
  while (parent >= 1) {
    tree_.at(parent).fetch_add(1, std::memory_order_acq_rel);
    parent /= 2;
  }
}

} // namespace signal_tree