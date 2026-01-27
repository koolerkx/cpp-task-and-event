#include <chrono>
#include <iostream>
#include <thread>

#include "CoroTask.hpp"
#include "Task.hpp"
#include "TaskAwaiter.hpp"
#include "ThreadPool.hpp"

CoroTask<void> SimpleAwaitDemo(ThreadPool& pool) {
  std::cout << "[Thread " << std::this_thread::get_id() << "] Coroutine started\n";

  auto task = std::make_shared<Task<void>>([] {
    std::cout << "[Thread " << std::this_thread::get_id() << "] Task executing...\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  });

  co_await TaskAwaiter<void>{task, pool};

  std::cout << "[Thread " << std::this_thread::get_id() << "] Task completed, coroutine resumed!\n";
}

CoroTask<void> SequentialAwaitDemo(ThreadPool& pool) {
  std::cout << "\n=== Sequential Await Demo ===\n";

  auto task1 = std::make_shared<Task<void>>([] {
    std::cout << "[Thread " << std::this_thread::get_id() << "] Loading mesh...\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  });

  co_await TaskAwaiter<void>{task1, pool};
  std::cout << "[Thread " << std::this_thread::get_id() << "] Mesh loaded!\n";

  auto task2 = std::make_shared<Task<void>>([] {
    std::cout << "[Thread " << std::this_thread::get_id() << "] Loading texture...\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  });

  co_await TaskAwaiter<void>{task2, pool};
  std::cout << "[Thread " << std::this_thread::get_id() << "] Texture loaded!\n";

  auto task3 = std::make_shared<Task<void>>([] {
    std::cout << "[Thread " << std::this_thread::get_id() << "] Creating material...\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  });

  co_await TaskAwaiter<void>{task3, pool};
  std::cout << "[Thread " << std::this_thread::get_id() << "] Material ready!\n";
}

CoroTask<void> ParallelAwaitDemo(ThreadPool& pool) {
  std::cout << "\n=== Parallel Await Demo ===\n";

  auto taskA = std::make_shared<Task<void>>([] {
    std::cout << "[Thread " << std::this_thread::get_id() << "] Task A: Loading assets...\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  });

  auto taskB = std::make_shared<Task<void>>([] {
    std::cout << "[Thread " << std::this_thread::get_id() << "] Task B: Compiling shaders...\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  });

  auto taskC = std::make_shared<Task<void>>([] {
    std::cout << "[Thread " << std::this_thread::get_id() << "] Task C: Initializing physics...\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  });

  taskA->TrySchedule(pool);
  taskB->TrySchedule(pool);
  taskC->TrySchedule(pool);

  co_await TaskAwaiter<void>{taskA, pool};
  std::cout << "[Thread " << std::this_thread::get_id() << "] Task A done\n";

  co_await TaskAwaiter<void>{taskB, pool};
  std::cout << "[Thread " << std::this_thread::get_id() << "] Task B done\n";

  co_await TaskAwaiter<void>{taskC, pool};
  std::cout << "[Thread " << std::this_thread::get_id() << "] Task C done\n";

  std::cout << "[Thread " << std::this_thread::get_id() << "] All parallel tasks completed!\n";
}

CoroTask<void> MixedDemo(ThreadPool& pool) {
  std::cout << "\n=== Mixed Sequential + Parallel Demo ===\n";

  auto taskA = std::make_shared<Task<void>>([] {
    std::cout << "[Thread " << std::this_thread::get_id() << "] Phase 1: Initialize engine\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  });

  co_await TaskAwaiter<void>{taskA, pool};
  std::cout << "[Thread " << std::this_thread::get_id() << "] Engine initialized\n";

  auto taskB = std::make_shared<Task<void>>([] {
    std::cout << "[Thread " << std::this_thread::get_id() << "] Phase 2a: Load scene\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(75));
  });

  auto taskC = std::make_shared<Task<void>>([] {
    std::cout << "[Thread " << std::this_thread::get_id() << "] Phase 2b: Load audio\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(75));
  });

  taskB->TrySchedule(pool);
  taskC->TrySchedule(pool);

  co_await TaskAwaiter<void>{taskB, pool};
  co_await TaskAwaiter<void>{taskC, pool};

  std::cout << "[Thread " << std::this_thread::get_id() << "] Phase 2 complete (parallel)\n";

  auto taskD = std::make_shared<Task<void>>([] {
    std::cout << "[Thread " << std::this_thread::get_id() << "] Phase 3: Start render loop\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  });

  co_await TaskAwaiter<void>{taskD, pool};
  std::cout << "[Thread " << std::this_thread::get_id() << "] Initialization complete!\n";
}

void RunAllCoroutineDemos() {
  std::cout << "======================================\n";
  std::cout << "=== C++20 Coroutine Async/Await Demo ===\n";
  std::cout << "======================================\n\n";

  {
    std::cout << "=== Simple Await Demo ===\n";
    ThreadPool pool;
    auto coro = SimpleAwaitDemo(pool);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    std::cout << std::endl;
  }

  {
    auto coro = SequentialAwaitDemo(*new ThreadPool());
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
  }

  {
    auto coro = ParallelAwaitDemo(*new ThreadPool());
    std::this_thread::sleep_for(std::chrono::milliseconds(400));
  }

  {
    auto coro = MixedDemo(*new ThreadPool());
    std::this_thread::sleep_for(std::chrono::milliseconds(400));
  }

  std::cout << "\n=== All coroutine demos completed ===" << std::endl;
}
