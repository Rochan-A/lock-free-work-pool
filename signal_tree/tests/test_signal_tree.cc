#include <algorithm>
#include <atomic>
#include <chrono>
#include <gtest/gtest.h>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>

#include "signal_tree/signal_tree.h"

TEST(SignalTreeTest, BasicInitializationTest) {
  signal_tree::SignalTree st(8);
  EXPECT_EQ(st.Capacity(), 8);
  EXPECT_TRUE(st.IsFree());
}

TEST(SignalTreeTest, AcquireAllLeavesSingleThread) {
  signal_tree::SignalTree st(8);
  std::vector<int> indices;

  for (int i = 0; i < 8; ++i) {
    int leaf_idx = st.Acquire();
    EXPECT_GE(leaf_idx, 0);
    EXPECT_LT(leaf_idx, 8);
    indices.push_back(leaf_idx);
  }
  EXPECT_FALSE(st.IsFree());

  // One more Acquire should fail (-1)
  EXPECT_EQ(st.Acquire(), -1);
}

TEST(SignalTreeTest, AcquireAndReleaseSingleThread) {
  signal_tree::SignalTree st(4);
  EXPECT_EQ(st.Capacity(), 4);

  int leaf1 = st.Acquire();
  int leaf2 = st.Acquire();
  EXPECT_GE(leaf1, 0);
  EXPECT_GE(leaf2, 0);
  EXPECT_NE(leaf1, leaf2);

  // Release one leaf
  st.Release(leaf1);
  EXPECT_TRUE(st.IsFree());

  // Acquire again
  int leaf3 = st.Acquire();
  EXPECT_GE(leaf3, 0);
  EXPECT_LE(leaf3, 3);
  EXPECT_NE(leaf2, leaf3);

  // Release all
  st.Release(leaf2);
  st.Release(leaf3);
  EXPECT_TRUE(st.IsFree());
}

TEST(SignalTreeTest, AcquireBeyondCapacity) {
  signal_tree::SignalTree st(2);
  EXPECT_TRUE(st.IsFree());

  // Acquire 2 leaves
  int leaf1 = st.Acquire();
  int leaf2 = st.Acquire();
  EXPECT_GE(leaf1, 0);
  EXPECT_GE(leaf2, 0);

  // Next acquire should fail (-1)
  EXPECT_EQ(-1, st.Acquire());

  // Release one
  st.Release(leaf1);
  // Now we should be able to acquire once again
  int leaf3 = st.Acquire();
  EXPECT_GE(leaf3, 0);
  EXPECT_LT(leaf3, 2);

  EXPECT_EQ(-1, st.Acquire());
}

TEST(SignalTreeTest, DoubleReleaseShouldFail) {
  signal_tree::SignalTree st(2);

  int leaf = st.Acquire();
  EXPECT_GE(leaf, 0);

  st.Release(leaf);
  EXPECT_THROW(st.Release(leaf), std::runtime_error);
}

TEST(SignalTreeTest, ReleaseInvalidIndexShouldThrow) {
  signal_tree::SignalTree st(4);

  int leaf = st.Acquire();
  EXPECT_GE(leaf, 0);

  // Attempt to release out-of-range indexes
  EXPECT_THROW(st.Release(-1), std::runtime_error);
  EXPECT_THROW(st.Release(999), std::runtime_error);

  st.Release(leaf);
}

//----------------------------------------------------------------------
// Multi-threaded tests
//----------------------------------------------------------------------

void AcquireReleaseLoop(signal_tree::SignalTree &st,
                        std::atomic<int> &success_count, int iterations) {
  for (int i = 0; i < iterations; ++i) {
    int leaf_idx = st.Acquire();
    if (leaf_idx != -1) {
      // We managed to acquire a leaf. Count success.
      success_count.fetch_add(1, std::memory_order_relaxed);
      // Simulate some work
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
      // Release it
      st.Release(leaf_idx);
    } else {
      // No leaf available. Sleep/yield and retry.
      std::this_thread::yield();
    }
  }
}

/**
 * Basic multi-thread test: we create a SignalTree with N leaves,
 * spawn T threads, and let them each do some Acquire/Release loops.
 * Then verify final free count is back to N and check total successful
 * acquires for sanity.
 */
TEST(SignalTreeTest, MultiThreadAcquireRelease) {
  const int kLeaves = 8;
  const int kThreads = 4;
  const int kIterations = 20;

  signal_tree::SignalTree st(kLeaves);
  EXPECT_EQ(st.FreeCount(), kLeaves);

  std::atomic<int> success_count(0);
  std::vector<std::thread> workers;
  workers.reserve(kThreads);

  for (int i = 0; i < kThreads; ++i) {
    workers.emplace_back([&st, &success_count]() {
      AcquireReleaseLoop(st, success_count, kIterations);
    });
  }

  for (auto &t : workers) {
    t.join();
  }

  EXPECT_EQ(st.FreeCount(), kLeaves);

  // success_count is how many total times a leaf was acquired across all
  // threads. We can't predict the exact number, but it should be <=
  // T*kIterations and >= 0. Just do a simple sanity check:
  int sc = success_count.load(std::memory_order_relaxed);
  EXPECT_GE(sc, 0);
  EXPECT_LE(sc, kThreads * kIterations);
}

/**
 * We create a tree with few leaves and many threads. Each thread tries to do as
 * many Acquire/Release cycles as possible in a fixed time. Then we confirm no
 * corruption occurs (the free count is back to the initial count).
 */
TEST(SignalTreeTest, MultiThreadContentionStress) {
  const int kLeaves = 4;
  const int kThreads = 8;
  const auto kTestDuration = std::chrono::milliseconds(200);

  signal_tree::SignalTree st(kLeaves);
  EXPECT_EQ(st.FreeCount(), kLeaves);

  std::atomic<bool> stop_flag(false);
  std::atomic<int> total_acquires(0);

  // Worker threads: loop until stop_flag is set.
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

  std::this_thread::sleep_for(kTestDuration);
  // Signal them to stop
  stop_flag.store(true, std::memory_order_relaxed);

  for (auto &t : threads) {
    t.join();
  }

  EXPECT_EQ(st.FreeCount(), kLeaves);

  int final_count = total_acquires.load(std::memory_order_relaxed);
  EXPECT_GE(final_count, 0);
}

void AcquireReleaseOwnershipCheck(signal_tree::SignalTree &st,
                                  std::vector<std::atomic<bool>> &ownership,
                                  std::atomic<int> &total_acquires,
                                  int iterations_per_thread) {
  for (int i = 0; i < iterations_per_thread; ++i) {
    int leaf = st.Acquire();
    if (leaf != -1) {
      // We successfully acquired a leaf. Mark ownership.
      bool expected = false;
      bool tookOwnership = ownership[leaf].compare_exchange_strong(
          expected, true, std::memory_order_acq_rel, std::memory_order_relaxed);

      // If this fails, it means the same leaf was already "true",
      // i.e., two threads have the same leaf concurrently -> BUG.
      EXPECT_TRUE(tookOwnership)
          << "ERROR: Leaf " << leaf << " was already owned by another thread!";

      total_acquires.fetch_add(1, std::memory_order_relaxed);

      // Simulate some work
      std::this_thread::sleep_for(std::chrono::microseconds(100));

      // Release ownership
      ownership[leaf].store(false, std::memory_order_release);

      // Release the leaf back to the tree
      st.Release(leaf);
    } else {
      // If no leaf is available, yield so other threads can run
      std::this_thread::yield();
    }
  }
}

/**
 * Multi-thread stress test with ownership checks.
 *
 * We create a small number of leaves (kLeaves), but many threads (kThreads).
 * Each thread tries to Acquire/Release leaves repeatedly. Whenever a thread
 * acquires a leaf, it sets an ownership flag to TRUE. If another thread also
 * acquired that leaf simultaneously, we detect it immediately (test fails).
 */
TEST(SignalTreeTest, MultiThreadOwnershipCheck) {
  const int kLeaves = 4;       // Small # of leaves to force contention
  const int kThreads = 8;      // # of worker threads
  const int kIterations = 100; // # of Acquire/Release cycles per thread

  signal_tree::SignalTree st(kLeaves);

  // Initially, all leaves should be free
  EXPECT_TRUE(st.IsFree());
  EXPECT_EQ(st.FreeCount(), kLeaves);

  // We'll track ownership of each leaf via an atomic<bool>.
  std::vector<std::atomic<bool>> ownership(kLeaves);
  for (int i = 0; i < kLeaves; ++i) {
    ownership[i].store(false, std::memory_order_relaxed);
  }

  // Count how many times leaves were successfully acquired across all threads
  std::atomic<int> total_acquires(0);

  // Create worker threads
  std::vector<std::thread> threads;
  threads.reserve(kThreads);
  for (int i = 0; i < kThreads; ++i) {
    threads.emplace_back([&]() {
      AcquireReleaseOwnershipCheck(st, ownership, total_acquires, kIterations);
    });
  }

  // Join worker threads
  for (auto &t : threads) {
    t.join();
  }

  // Final checks
  // 1. No leaves should remain "owned":
  for (int i = 0; i < kLeaves; ++i) {
    EXPECT_FALSE(ownership[i].load(std::memory_order_relaxed))
        << "Leaf " << i << " remained 'true' after all threads joined.";
  }

  // 2. All leaves should be free in the SignalTree
  EXPECT_EQ(st.FreeCount(), kLeaves)
      << "All leaves should be free after all Acquire/Release cycles.";

  // 3. We can't say exactly how many times leaves were acquired, but:
  int final_acquires = total_acquires.load(std::memory_order_relaxed);
  std::cout << "Total successful acquires across all threads = "
            << final_acquires << "\n";
  EXPECT_GE(final_acquires, 0)
      << "Basic sanity check on total successful acquires.";
}
