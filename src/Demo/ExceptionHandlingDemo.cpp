#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>

#include "CoroTask.hpp"
#include "Task.hpp"
#include "TaskAwaiter.hpp"
#include "ThreadPool.hpp"


struct AssetLoadException : public std::exception {
  std::string message;
  AssetLoadException(std::string msg) : message(std::move(msg)) {
  }
  const char* what() const noexcept override {
    return message.c_str();
  }
};

CoroTask<void> BasicVoidExceptionDemo(ThreadPool& pool) {
  std::cout << "=== Basic Void Exception Demo ===\n";

  auto task = std::make_shared<Task<void>>([] { throw std::runtime_error("Something went wrong in void task!"); });

  task->TrySchedule(pool);

  try {
    co_await TaskAwaiter<void>{task, pool};
    std::cout << "Task completed (should not reach here)\n";
  } catch (const std::exception& e) {
    std::cout << "Caught exception: " << e.what() << "\n\n";
  }
}

CoroTask<void> IntReturnExceptionDemo(ThreadPool& pool) {
  std::cout << "=== Int Return Exception Demo ===\n";

  auto task = std::make_shared<Task<int>>([]() -> int {
    throw std::runtime_error("Failed to compute result!");
    return 42;
  });

  task->TrySchedule(pool);

  try {
    int result = co_await TaskAwaiter<int>{task, pool};
    std::cout << "Got result: " << result << " (should not reach here)\n";
  } catch (const std::exception& e) {
    std::cout << "Caught exception: " << e.what() << "\n\n";
  }
}

CoroTask<void> CustomExceptionDemo(ThreadPool& pool) {
  std::cout << "=== Custom Exception Demo ===\n";

  auto task = std::make_shared<Task<std::string>>([]() -> std::string {
    throw AssetLoadException("Failed to load texture: missing_file.png");
    return std::string{"texture_data"};
  });

  task->TrySchedule(pool);

  try {
    std::string data = co_await TaskAwaiter<std::string>{task, pool};
    std::cout << "Loaded: " << data << "\n";
  } catch (const AssetLoadException& e) {
    std::cout << "Caught custom exception: " << e.what() << "\n";
  } catch (const std::exception& e) {
    std::cout << "Caught generic exception: " << e.what() << "\n";
  }
  std::cout << "\n";
}

CoroTask<void> DAGExceptionPropagationDemo(ThreadPool& pool) {
  std::cout << "=== DAG Exception Propagation Demo ===\n";

  auto taskA = std::make_shared<Task<void>>([] { std::cout << "[Stage A] Initialization complete\n"; });

  auto taskB = std::make_shared<Task<void>>([] {
    std::cout << "[Stage B] About to throw...\n";
    throw std::runtime_error("Stage B failed!");
  });

  auto taskC = std::make_shared<Task<void>>([] { std::cout << "[Stage C] This should still run\n"; });

  taskA->Then(taskC);
  taskB->Then(taskC);

  co_await TaskAwaiter<void>{taskA, pool};
  std::cout << "Task A completed\n";

  try {
    co_await TaskAwaiter<void>{taskB, pool};
    std::cout << "Task B completed (should not reach here)\n";
  } catch (const std::exception& e) {
    std::cout << "Caught exception from B: " << e.what() << "\n";
  }

  co_await TaskAwaiter<void>{taskC, pool};
  std::cout << "Task C completed\n\n";
}

CoroTask<void> ParallelExceptionDemo(ThreadPool& pool) {
  std::cout << "=== Parallel Exception Demo ===\n";

  auto task1 = std::make_shared<Task<int>>([] {
    std::cout << "[Task 1] Computing...\n";
    return 100;
  });

  auto task2 = std::make_shared<Task<int>>([] {
    std::cout << "[Task 2] Computing...\n";
    throw std::runtime_error("Task 2 failed!");
    return 200;
  });

  auto task3 = std::make_shared<Task<int>>([] {
    std::cout << "[Task 3] Computing...\n";
    return 300;
  });

  task1->TrySchedule(pool);
  task2->TrySchedule(pool);
  task3->TrySchedule(pool);

  int sum = 0;
  int success_count = 0;

  try {
    int r1 = co_await TaskAwaiter<int>{task1, pool};
    sum += r1;
    success_count++;
    std::cout << "Task 1 result: " << r1 << "\n";
  } catch (const std::exception& e) {
    std::cout << "Task 1 failed: " << e.what() << "\n";
  }

  try {
    int r2 = co_await TaskAwaiter<int>{task2, pool};
    sum += r2;
    success_count++;
    std::cout << "Task 2 result: " << r2 << "\n";
  } catch (const std::exception& e) {
    std::cout << "Task 2 failed: " << e.what() << "\n";
  }

  try {
    int r3 = co_await TaskAwaiter<int>{task3, pool};
    sum += r3;
    success_count++;
    std::cout << "Task 3 result: " << r3 << "\n";
  } catch (const std::exception& e) {
    std::cout << "Task 3 failed: " << e.what() << "\n";
  }

  std::cout << "Summary: " << success_count << "/3 succeeded, sum = " << sum << "\n\n";
}

CoroTask<void> GetResultExceptionDemo(ThreadPool& pool) {
  std::cout << "=== GetResult Exception Demo ===\n";

  auto task = std::make_shared<Task<double>>([]() -> double {
    throw std::runtime_error("Division by zero!");
    return 3.14;
  });

  task->TrySchedule(pool);

  try {
    co_await TaskAwaiter<double>{task, pool};
  } catch (const std::exception& e) {
    std::cout << "Caught via co_await: " << e.what() << "\n";
  }
  std::cout << "\n";
}

void RunAllExceptionHandlingDemos() {
  std::cout << "==============================================\n";
  std::cout << "=== Exception Handling Demo Suite ===\n";
  std::cout << "==============================================\n\n";

  {
    ThreadPool pool;
    auto coro = BasicVoidExceptionDemo(pool);
    coro.Wait();
  }

  {
    ThreadPool pool;
    auto coro = IntReturnExceptionDemo(pool);
    coro.Wait();
  }

  {
    ThreadPool pool;
    auto coro = CustomExceptionDemo(pool);
    coro.Wait();
  }

  {
    ThreadPool pool;
    auto coro = DAGExceptionPropagationDemo(pool);
    coro.Wait();
  }

  {
    ThreadPool pool;
    auto coro = ParallelExceptionDemo(pool);
    coro.Wait();
  }

  {
    ThreadPool pool;
    auto coro = GetResultExceptionDemo(pool);
    coro.Wait();
  }

  std::cout << "=== All exception demos completed ===\n";
}
