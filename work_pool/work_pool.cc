#include <atomic>
#include <cassert>
#include <cmath>
#include <iostream>
#include <memory>
#include <vector>

#include "work_pool/work_pool.h"

namespace work_pool {

WorkPool::WorkPool(const size_t capacity) : capacity_(capacity) {
  assert(capacity > 0 && ((capacity & (capacity - 1)) == 0));
}

} // namespace work_pool