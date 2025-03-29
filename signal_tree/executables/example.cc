#include <atomic>
#include <cstddef>
#include <iostream>
#include <vector>

#include "signal_tree/signal_tree.h"

// A small example usage
int main() {
  // Create a SignalTree with 8 leaves (we will store 1 in each leaf).
  SignalTree tree(8);

  // Acquire a leaf (in single-threaded example).
  int leaf_idx = tree.Acquire();
  std::cout << "Acquired leaf index: " << leaf_idx << std::endl;

  // Release it again.
  tree.Release(leaf_idx);
  std::cout << "Released leaf index: " << leaf_idx << std::endl;

  return 0;
}
