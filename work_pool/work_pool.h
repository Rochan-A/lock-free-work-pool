#pragma once

#include <atomic>
#include <deque>
#include <memory>

#include "signal_tree/signal_tree.h"

namespace work_pool {

class WorkPool {
public:
  explicit WorkPool(const size_t capacity);

  ~WorkPool() = default;

  // Acquire a free leaf (if any). Returns -1 if none is free.
  const int Schedule();

  // Number of leaves (capacity).
  const size_t Capacity() const { return capacity_; }

  // Non-copyable, non-assignable
  WorkPool(const WorkPool &) = delete;
  WorkPool &operator=(const WorkPool &) = delete;

private:
  const size_t capacity_{0};
};

} // namespace work_pool
