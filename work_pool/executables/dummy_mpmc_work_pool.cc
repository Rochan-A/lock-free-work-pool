#include "work_pool/lock_free_mpmc.h"
#include "work_pool/thread_pool.h"

#include <iostream>

int add(int a, int b) {
  std::cout << "(tid="
            << std::hash<std::thread::id>{}(std::this_thread::get_id())
            << ")\n";
  return a + b;
}

int main() {
  work_pool::MPMCTaskStore task_store;

  work_pool::ThreadPool<work_pool::MPMCTaskStore> thread_pool(task_store, 5);
  thread_pool.Start();

  std::cout << "Main tid="
            << std::hash<std::thread::id>{}(std::this_thread::get_id()) << "\n";

  auto future = task_store.SubmitAndGetFuture(&add, 2, 40);

  std::cout << "2 + 40 = " << future.get() << '\n'; // blocking

  task_store.Submit(
      [](std::string s) {
        std::cout << s;
        return;
      },
      []() {
        std::cout << "(tid="
                  << std::hash<std::thread::id>{}(std::this_thread::get_id())
                  << ") Done\n";
      },
      "hello from the pool");
}