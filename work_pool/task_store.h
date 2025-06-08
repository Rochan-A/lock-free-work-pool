#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <thread>
#include <tuple>

namespace work_pool {

template <class Derived> class TaskStore {
  auto derived() noexcept { return static_cast<Derived *>(this); }
  auto derived() const noexcept { return static_cast<const Derived *>(this); }

public:
  struct Task {
    std::function<void()> exec;

    // Forward any callable into the std::function.
    template <typename FuncType>
    explicit Task(FuncType &&f) : exec(std::forward<FuncType>(f)) {}
    Task() = default;
  };

  // Submit a task to run and returns a future.
  template <typename FuncType, typename... Args>
  auto SubmitAndGetFuture(FuncType &&func, Args &&...args) -> std::future<
      std::invoke_result_t<std::decay_t<FuncType>, std::decay_t<Args>...>> {
    using ResultType =
        std::invoke_result_t<std::decay_t<FuncType>, std::decay_t<Args>...>;

    auto data = std::make_shared<std::tuple<std::decay_t<Args>...>>(
        std::forward<Args>(args)...);

    auto promise = std::make_shared<std::promise<ResultType>>();
    auto future = promise->get_future();

    auto work = [func_ = std::forward<FuncType>(func), data,
                 promise]() mutable {
      if constexpr (std::is_void_v<ResultType>) {
        std::apply(func_, std::move(*data));
        promise->set_value();
      } else {
        promise->set_value(std::apply(func_, std::move(*data)));
      }
    };
    derived()->Enqueue(std::make_shared<Task>(std::move(work)));
    return future;
  }

  // Submit a task to run and a callback to invoke on the result of the task (if
  // any).
  template <typename FuncType, typename CallbackType, typename... Args>
  void Submit(FuncType &&func, CallbackType &&callback, Args &&...args) {
    using ResultType =
        std::invoke_result_t<std::decay_t<FuncType>, std::decay_t<Args>...>;

    auto data = std::make_shared<std::tuple<std::decay_t<Args>...>>(
        std::forward<Args>(args)...);

    auto work = [func_ = std::forward<FuncType>(func),
                 callback_ = std::forward<CallbackType>(callback),
                 data]() mutable {
      if constexpr (std::is_void_v<ResultType>) {
        std::apply(func_, std::move(*data));
        callback_();
      } else {
        callback_(std::apply(func_, std::move(*data)));
      }
    };
    derived()->Enqueue(std::make_shared<Task>(std::move(work)));
  }

  // Enqueues a single item (by moving it).
  inline void Enqueue(std::shared_ptr<Task> task) {
    derived()->EnqueueImpl(task);
  }

  // Blocks the current thread until there's something to dequeue, then dequeues
  // it.
  inline void WaitDequeue(std::shared_ptr<Task> &task) {
    derived()->WaitDequeueImpl(task);
  }

  // Blocks the current thread until either there's something to dequeue
  // or the timeout (specified in microseconds) expires. Returns false
  // without setting `tasl` if the timeout expires, otherwise assigns
  // to `task` and returns true.
  // Using a negative timeout indicates an indefinite timeout,
  // and is thus functionally equivalent to calling WaitDequeue.
  template <class Rep, class Period>
  bool WaitDequeueTimed(std::shared_ptr<Task> &task,
                        const std::chrono::duration<Rep, Period> &duration) {
    return derived()->WaitDequeueTimedImpl(task, duration);
  }

protected:
  TaskStore() = default;
};

} // namespace work_pool