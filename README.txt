Lock-free Work Pool
-------------------

Implements the tree-like data structure and threaded work pool described in this CppCon Talk:
https://youtu.be/oj-_vpZNMVw.

Signal Tree
-----------

Signal tree is a lock-free data structure for quickly scheduling resources to run on a pool of
workers. Conceptually, the tree has:
* a fixed number of leaves, each representing a single resource, each leaf contains an integer that
  is 1 if available, and 0 if used.
* internal nodes store the sum of their children’s values, so the root’s value is the total number
  of free leaves in the entire tree.

Signal tree exposes two key methods, Schedule() and Release().

Acquire()
---------

* Attempting to acquire a resource starts with checking the root: if it is 0, all resources are in
  use. Otherwise, we traverse downward toward a leaf with value 1.
* Upon finding that leaf: atomically set the leaf to 0 (indicating the resource is now taken) and
  propagate the change up the tree.
* Internal nodes are updated by CAS (Compare-And-Swap) operations to reflect the decremented value.
* Resource associated with that leaf is returned.

Release()
---------

* When a resource is returned, identify its corresponding leaf.
* Atomically set that leaf to 1 and propagate the change up the tree (through CAS operations) so
  that each ancestor’s sum is incremented accordingly.

Why use this data-structure?
----------------------------

* O(log n) complexity to find a free resource.
* All operations use atomic primitives, improving performance in highly concurrent environments.
* Constant-time availability check (if root is 0, no free resources).
* Bounded memory usage.


Implementation Notes
--------------------

* Signal tree can be an n-ary tree (using binary atomic operations). Currently, I've only implemented
  a binary tree.
* TODO: Implement thread-backed work pool.