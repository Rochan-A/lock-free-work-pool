#include <atomic>
#include <cstddef>
#include <iostream>
#include <vector>

class SignalTree {
public:
  // Constructs a SignalTree with the given number of leaves (must be >= 1
  // and ideally a power of two). If not a power of two, we round up
  // internally to the next power of two for the binary tree layout.
  explicit SignalTree(std::size_t num_leaves);

  ~SignalTree() {}

  // Acquire returns the index of a leaf that was successfully reserved (set
  // from 1 -> 0). If none are free, returns -1.
  int Acquire();

  // Release sets the leaf at "leaf_index" (as returned from Acquire) back to 1
  // and updates all ancestor nodes.
  void Release(int leaf_index);

  // Returns how many leaves are currently free (sum of all leaves).
  int FreeCount() const;

private:
  // tree_ is 1-based indexing for convenience in a segment-tree style.
  // tree_[1] is the root.
  std::vector<std::atomic<int>> tree_;

  // number of leaves (rounded up to a power of two)
  std::size_t num_leaves_;

  // index in tree_ where leaves begin
  std::size_t offset_;
};
