#pragma once

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>
#include <vector>

#include "work_pool/task_store.h"

namespace work_pool {

template <class TaskStore> class ThreadPool {
  using Task = typename TaskStore::Task;

public:
  explicit ThreadPool(TaskStore &task_store,
                      size_t num_threads = std::thread::hardware_concurrency())
      : task_store_(task_store), num_threads_(num_threads), done_(false) {}

  void Start() {
    for (std::size_t i = 0; i < num_threads_; ++i)
      workers_.emplace_back([this] { Loop(); });
  }

  ~ThreadPool() {
    done_ = true;
    for (auto &thread : workers_) {
      thread.join();
    }
  }

  ThreadPool(const ThreadPool &) = delete;
  ThreadPool &operator=(const ThreadPool &) = delete;

private:
  void Loop() {
    std::shared_ptr<Task> task;
    while (!done_) {
      // TODO: Rework this logic.
      if (task_store_.WaitDequeueTimed(task, std::chrono::milliseconds(10)) &&
          task && task->exec) {
        task->exec();
      }
    }
  }

  TaskStore &task_store_;
  std::vector<std::thread> workers_;
  size_t num_threads_;
  std::atomic<bool> done_{false};
};

} // namespace work_pool