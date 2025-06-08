#pragma once

#include <chrono>

#include "blockingconcurrentqueue.h"
#include "work_pool/task_store.h"

namespace work_pool {

// Multi-Producer Multi-Consumer Lock Free Queue.
// Wrapper around moodycamel::BlockingConcurrentQueue.
class MPMCTaskStore : public TaskStore<MPMCTaskStore> {
public:
  using Base = TaskStore<MPMCTaskStore>;
  using Task = typename Base::Task;

  MPMCTaskStore() = default;
  ~MPMCTaskStore() = default;

  void EnqueueImpl(std::shared_ptr<Task> task) {
    queue_.enqueue(std::move(task));
  }

  void WaitDequeueImpl(std::shared_ptr<Task> &task) {
    queue_.wait_dequeue(task);
  }

  template <class Rep, class Period>
  bool
  WaitDequeueTimedImpl(std::shared_ptr<Task> &task,
                       const std::chrono::duration<Rep, Period> &duration) {
    return queue_.wait_dequeue_timed(task, duration);
  }

  MPMCTaskStore(const MPMCTaskStore &) = delete;
  MPMCTaskStore &operator=(const MPMCTaskStore &) = delete;

private:
  moodycamel::BlockingConcurrentQueue<std::shared_ptr<Task>> queue_;
};

} // namespace work_pool
