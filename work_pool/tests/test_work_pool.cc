#include <algorithm>
#include <atomic>
#include <chrono>
#include <gtest/gtest.h>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>

#include "work_pool/work_pool.h"

TEST(WorkPoolTest, BasicInitializationTest) {
  work_pool::WorkPool wp(8);
  EXPECT_EQ(wp.Capacity(), 8);
}
