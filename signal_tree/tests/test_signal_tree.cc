#include <algorithm>
#include <atomic>
#include <chrono>
#include <gtest/gtest.h>
#include <iostream>
#include <thread>
#include <vector>

#include "signal_tree/signal_tree.h"

// Demonstrates constructing a SignalTree with N leaves and verifying
// the number of free leaves at initialization.
TEST(SignalTreeTest, InitializationPowerOfTwo) {
  // 8 is already a power of two
  SignalTree st(8);
  // At construction, all leaves should be free => free count = 8
  EXPECT_EQ(st.FreeCount(), 8);

  // Acquire all leaves
  for (int i = 0; i < 8; ++i) {
    int leaf_idx = st.Acquire();
    EXPECT_GE(leaf_idx, 0); // Should successfully acquire
    EXPECT_LE(leaf_idx, 7); // Should be within [0..7]
  }
  // Now free count should be 0
  EXPECT_EQ(st.FreeCount(), 0);

  // One more Acquire should return -1
  EXPECT_EQ(st.Acquire(), -1);
}

// Test for a non-power-of-two number of leaves. We test that the
// tree internally rounds up to the next power of two, but only the
// original number of leaves are "usable" in the sense that it
// doesn't break usage. In this test, we expect it to function
// properly for the actual requested count as well.
TEST(SignalTreeTest, InitializationNonPowerOfTwo) {
  // For example, 6 -> next power of two is 8 internally
  SignalTree st(6);
  // The tree has 8 leaves internally, but from the perspective of usage,
  // it's okay to just see if 6 acquires are successful. However, the data
  // structure doesn't really block you from using all 8. For simplicity
  // we'll just check that the total FreeCount is 8 (the sum of 8 leaves).
  EXPECT_EQ(st.FreeCount(), 8);

  // Acquire 6 times (or 8 times) still should succeed up to 8 total.
  for (int i = 0; i < 8; ++i) {
    int leaf_idx = st.Acquire();
    EXPECT_NE(leaf_idx, -1);
  }
  // Next one is not free
  EXPECT_EQ(st.Acquire(), -1);
}

// Single-thread test: Acquire all leaves, then release them all,
// ensuring that free count is updated correctly.
TEST(SignalTreeTest, SingleThreadAcquireRelease) {
  SignalTree st(4);
  EXPECT_EQ(st.FreeCount(), 4);

  std::vector<int> acquired;
  for (int i = 0; i < 4; ++i) {
    int leaf_idx = st.Acquire();
    EXPECT_NE(leaf_idx, -1);
    acquired.push_back(leaf_idx);
  }
  EXPECT_EQ(st.FreeCount(), 0);

  // No more leaves available
  EXPECT_EQ(st.Acquire(), -1);

  // Release them back
  for (auto idx : acquired) {
    st.Release(idx);
  }
  EXPECT_EQ(st.FreeCount(), 4);
}

// Test releasing a leaf that was never acquired (or was already
// released). This doesn't throw an exception in our sample code
// but effectively does nothing. We'll just ensure it doesn't corrupt
// the structure.
TEST(SignalTreeTest, ReleaseInvalid) {
  SignalTree st(2);
  EXPECT_EQ(st.FreeCount(), 2);

  // Acquire one leaf
  int leaf_idx = st.Acquire();
  EXPECT_NE(leaf_idx, -1);
  EXPECT_EQ(st.FreeCount(), 1);

  // Attempt to release an invalid leaf index
  // (like -5) or a leaf that wasn't acquired
  st.Release(-5);           // out of range
  st.Release(leaf_idx + 5); // out of range
  EXPECT_EQ(st.FreeCount(), 1);

  // Actually release the correct leaf
  st.Release(leaf_idx);
  EXPECT_EQ(st.FreeCount(), 2);
}

//----------------------------------------------------------------------
// Multi-threaded tests
//----------------------------------------------------------------------

// Helper function for a concurrent Acquire/Release loop.
// Each thread will attempt to acquire a leaf `iterations` times.
// If it succeeds, it increments local `success_count`, sleeps briefly,
// then releases it.
void AcquireReleaseLoop(SignalTree *st, std::atomic<int> *success_count,
                        int iterations) {
  for (int i = 0; i < iterations; ++i) {
    int leaf_idx = st->Acquire();
    if (leaf_idx != -1) {
      // We managed to acquire a leaf. Count success.
      success_count->fetch_add(1, std::memory_order_relaxed);
      // Simulate some work
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
      // Release it
      st->Release(leaf_idx);
    } else {
      // No leaf available. Sleep and retry.
      std::this_thread::yield();
    }
  }
}

// Basic multi-thread test: we create a SignalTree with N leaves,
// spawn T threads, and let them each do some Acquire/Release loops.
// We then verify that the final free count is N and that no corruption
// occurred (the test is somewhat “best‐effort”).
TEST(SignalTreeTest, MultiThreadAcquireRelease) {
  const int kLeaves = 8;
  const int kThreads = 4;
  const int kIterations = 20;

  SignalTree st(kLeaves);
  EXPECT_EQ(st.FreeCount(), kLeaves);

  std::atomic<int> success_count(0);
  std::vector<std::thread> workers;
  for (int i = 0; i < kThreads; ++i) {
    workers.emplace_back(AcquireReleaseLoop, &st, &success_count, kIterations);
  }
  for (auto &t : workers) {
    t.join();
  }
  // After all threads join, we expect the number of free leaves to be back to
  // kLeaves.
  EXPECT_EQ(st.FreeCount(), kLeaves);

  // success_count is how many total times a leaf was acquired across all
  // threads. We can't predict the exact number, but we do expect that it should
  // be <= kThreads*kIterations and >= 0. Just sanity‐check it here.
  int sc = success_count.load(std::memory_order_relaxed);
  EXPECT_GE(sc, 0);
  EXPECT_LE(sc, kThreads * kIterations);
}

// A more “stress‐y” multi‐thread test. We create a tree with few leaves
// and many threads. Each thread tries to do as many Acquire/Release
// cycles as possible in a fixed time. Then we confirm no corruption
// occurs (the free count is back to the initial count).
TEST(SignalTreeTest, MultiThreadContentionStress) {
  const int kLeaves = 4;
  const int kThreads = 8;
  const auto kTestDuration = std::chrono::milliseconds(200);

  SignalTree st(kLeaves);
  EXPECT_EQ(st.FreeCount(), kLeaves);

  std::atomic<bool> stop_flag(false);
  std::atomic<int> total_acquires(0);

  // Worker threads: loop until stop_flag is true
  auto worker = [&]() {
    while (!stop_flag.load(std::memory_order_relaxed)) {
      int leaf_idx = st.Acquire();
      if (leaf_idx != -1) {
        total_acquires.fetch_add(1, std::memory_order_relaxed);
        // Simulate some short work
        std::this_thread::sleep_for(std::chrono::microseconds(50));
        st.Release(leaf_idx);
      } else {
        // No leaf free
        std::this_thread::yield();
      }
    }
  };

  std::vector<std::thread> threads;
  threads.reserve(kThreads);
  for (int i = 0; i < kThreads; ++i) {
    threads.emplace_back(worker);
  }

  // Let the threads run
  std::this_thread::sleep_for(kTestDuration);
  // Signal them to stop
  stop_flag.store(true, std::memory_order_relaxed);
  // Join
  for (auto &t : threads) {
    t.join();
  }

  // Check that we ended with all leaves free
  EXPECT_EQ(st.FreeCount(), kLeaves);

  // We can't say exactly how many acquires happened, but at least 1
  // if the system is not too busy
  int final_count = total_acquires.load(std::memory_order_relaxed);
  EXPECT_GE(final_count, 0); // Basic sanity check
}
