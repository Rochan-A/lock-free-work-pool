#include <atomic>
#include <cstddef>
#include <iostream>
#include <memory>
#include <vector>

namespace signal_tree {

template <typename T, size_t kCapacity> class SignalTree {
public:
  explicit SignalTree();

  ~SignalTree() {}

  // Acquire returns the index of a leaf that was successfully reserved (set
  // from 1 -> 0). If none are free, returns -1.
  std::shared_ptr<T> Acquire();

  // Release sets the leaf at "leaf_index" (as returned from Acquire) back to 1
  // and updates all ancestor nodes.
  void Release(std::shared_ptr<T> &item);

  // Returns true if the tree has free elements. Not race-free.
  const bool IsFree() const { return tree_[0]; };

  // Number of objects in the signal tree (i.e., number of leaves).
  const size_t Capacity() const { return kCapacity; }

private:
  // tree_ is 1-based indexing for convenience in a segment-tree style.
  // tree_[1] is the root.
  std::vector<std::atomic<int>> tree_;

  // Container that stores objects associated with leaves.
  std::array<T, kCapacity> objects_;

  // index in tree_ where leaves begin
  std::size_t offset_;
};

} // namespace signal_tree
