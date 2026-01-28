#include <chrono>
#include <iostream>
#include <thread>

#include "CancellationToken.hpp"
#include "CoroTask.hpp"
#include "Task.hpp"
#include "TaskAwaiter.hpp"
#include "TaskExtensions.hpp"
#include "ThreadPool.hpp"

using namespace std::chrono_literals;

// Demo 1: Basic cancellation - check before work
CoroTask<void> BasicCancellationDemo(ThreadPool& pool) {
  std::cout << "=== Basic Cancellation Demo ===\n";

  auto token = MakeCancellationToken();

  auto task = WithCancellation<int>(
    [] {
      std::cout << "[Thread " << std::this_thread::get_id() << "] Working...\n";
      std::this_thread::sleep_for(100ms);
      return 42;
    },
    token);

  std::this_thread::sleep_for(5ms);
  std::cout << "[Main] Cancelling BEFORE task starts...\n";
  token->Cancel();

  task->TrySchedule(pool);

  try {
    int result = co_await TaskAwaiter<int>{task, pool};
    std::cout << "Result: " << result << " (task completed before cancel)\n";
  } catch (const TaskCancelledException& e) {
    std::cout << "Caught: " << e.what() << "\n";
  }
  std::cout << "\n";
}

// Demo 2: Timeout - using WithTimeout with quick operation (demonstrates successful completion)
CoroTask<void> TimeoutDemo(ThreadPool& pool) {
  std::cout << "=== Timeout Demo (Successful Completion) ===\n";

  auto task = WithTimeout<std::string>(
    [] {
      std::cout << "[Thread " << std::this_thread::get_id() << "] Quick operation (50ms)...\n";
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
      return std::string{"Success"};
    },
    std::chrono::milliseconds(100));

  task->TrySchedule(pool);

  try {
    std::string result = co_await TaskAwaiter<std::string>{task, pool};
    std::cout << "Result: " << result << " (task completed within timeout)\n";
  } catch (const TaskCancelledException& e) {
    std::cout << "Caught timeout: " << e.what() << "\n";
  }
  std::cout << "\n";
}

// Demo 3: Polling cancellation - cooperative long-running task
CoroTask<void> PollingCancellationDemo(ThreadPool& pool) {
  std::cout << "=== Polling Cancellation Demo ===\n";

  auto token = MakeCancellationToken();

  auto task = WithPollingCancellation<int>(
    [](CancellationTokenPtr token) {
      std::cout << "[Thread " << std::this_thread::get_id() << "] Starting iterations...\n";

      for (int i = 0; i < 10; ++i) {
        token->ThrowIfCancelled();

        std::cout << "  Iteration " << i << "\n";
        std::this_thread::sleep_for(30ms);
      }

      return 100;
    },
    token);

  task->TrySchedule(pool);

  std::this_thread::sleep_for(100ms);
  std::cout << "[Main] Cancelling during iteration...\n";
  token->Cancel();

  try {
    int result = co_await TaskAwaiter<int>{task, pool};
    std::cout << "Result: " << result << " (should not reach here)\n";
  } catch (const TaskCancelledException& e) {
    std::cout << "Caught: " << e.what() << "\n";
  }
  std::cout << "\n";
}

// Demo 4: Successful completion within timeout
CoroTask<void> SuccessfulTimeoutDemo(ThreadPool& pool) {
  std::cout << "=== Successful Within Timeout Demo ===\n";

  auto task = WithTimeout<int>(
    [] {
      std::cout << "[Thread " << std::this_thread::get_id() << "] Quick operation...\n";
      std::this_thread::sleep_for(50ms);
      return 777;
    },
    200ms);

  task->TrySchedule(pool);

  try {
    int result = co_await TaskAwaiter<int>{task, pool};
    std::cout << "Result: " << result << " (success!)\n";
  } catch (const TaskCancelledException& e) {
    std::cout << "Caught: " << e.what() << " (should not happen)\n";
  }
  std::cout << "\n";
}

// Demo 5: Multiple tasks sharing cancellation token
CoroTask<void> SharedCancellationDemo(ThreadPool& pool) {
  std::cout << "=== Shared Cancellation Token Demo ===\n";

  auto token = MakeCancellationToken();

  auto task1 = WithCancellation<int>(
    [] {
      std::cout << "[Task 1] Working...\n";
      std::this_thread::sleep_for(200ms);
      return 1;
    },
    token);

  auto task2 = WithCancellation<int>(
    [] {
      std::cout << "[Task 2] Working...\n";
      std::this_thread::sleep_for(200ms);
      return 2;
    },
    token);

  auto task3 = WithCancellation<int>(
    [] {
      std::cout << "[Task 3] Working...\n";
      std::this_thread::sleep_for(200ms);
      return 3;
    },
    token);

  std::this_thread::sleep_for(5ms);
  std::cout << "[Main] Cancelling all tasks BEFORE they start...\n";
  token->Cancel();

  task1->TrySchedule(pool);
  task2->TrySchedule(pool);
  task3->TrySchedule(pool);

  int success = 0;
  int cancelled = 0;

  try {
    co_await TaskAwaiter<int>{task1, pool};
    success++;
  } catch (const TaskCancelledException&) {
    cancelled++;
  }

  try {
    co_await TaskAwaiter<int>{task2, pool};
    success++;
  } catch (const TaskCancelledException&) {
    cancelled++;
  }

  try {
    co_await TaskAwaiter<int>{task3, pool};
    success++;
  } catch (const TaskCancelledException&) {
    cancelled++;
  }

  std::cout << "Summary: " << success << " succeeded, " << cancelled << " cancelled\n\n";
}

// Demo 6: Cancellation callback registration
CoroTask<void> CancellationCallbackDemo(ThreadPool& pool) {
  std::cout << "=== Cancellation Callback Demo ===\n";

  auto token = MakeCancellationToken();

  token->RegisterCallback([] { std::cout << "[Callback 1] Resource cleanup triggered\n"; });

  token->RegisterCallback([] { std::cout << "[Callback 2] Logging cancellation event\n"; });

  auto task = WithCancellation<void>(
    [] {
      std::cout << "[Task] Doing work...\n";
      std::this_thread::sleep_for(200ms);
    },
    token);

  std::this_thread::sleep_for(5ms);
  std::cout << "[Main] Triggering cancellation BEFORE task starts...\n";
  token->Cancel();

  task->TrySchedule(pool);

  try {
    co_await TaskAwaiter<void>{task, pool};
  } catch (const TaskCancelledException& e) {
    std::cout << "Task cancelled: " << e.what() << "\n";
  }
  std::cout << "\n";
}

// Demo 7: DAG with partial cancellation
CoroTask<void> DAGCancellationDemo(ThreadPool& pool) {
  std::cout << "=== DAG Partial Cancellation Demo ===\n";

  auto token_a = MakeCancellationToken();
  auto token_b = MakeCancellationToken();

  auto taskA = WithCancellation<void>(
    [] {
      std::cout << "[Task A] Running...\n";
      std::this_thread::sleep_for(50ms);
      std::cout << "[Task A] Done\n";
    },
    token_a);

  auto taskB = WithCancellation<void>(
    [] {
      std::cout << "[Task B] Running...\n";
      std::this_thread::sleep_for(150ms);
      std::cout << "[Task B] Done\n";
    },
    token_b);

  auto taskC = std::make_shared<Task<void>>([] { std::cout << "[Task C] Final step\n"; });

  taskA->Finally(taskC);
  taskB->Finally(taskC);

  std::this_thread::sleep_for(5ms);
  std::cout << "[Main] Cancelling Task B BEFORE it starts...\n";
  token_b->Cancel();

  taskA->TrySchedule(pool);
  taskB->TrySchedule(pool);

  try {
    co_await TaskAwaiter<void>{taskA, pool};
    std::cout << "Task A completed\n";
  } catch (const TaskCancelledException&) {
    std::cout << "Task A cancelled\n";
  }

  try {
    co_await TaskAwaiter<void>{taskB, pool};
    std::cout << "Task B completed\n";
  } catch (const TaskCancelledException&) {
    std::cout << "Task B cancelled\n";
  }

  co_await TaskAwaiter<void>{taskC, pool};
  std::cout << "Task C completed (A succeeded, so C runs)\n\n";
}

void RunAllCancellationDemos() {
  std::cout << "==============================================\n";
  std::cout << "=== Cancellation & Timeout Demo Suite ===\n";
  std::cout << "==============================================\n\n";

  {
    ThreadPool pool;
    auto coro = BasicCancellationDemo(pool);
    coro.Wait();
  }

  {
    ThreadPool pool;
    auto coro = TimeoutDemo(pool);
    coro.Wait();
  }

  {
    ThreadPool pool;
    auto coro = PollingCancellationDemo(pool);
    coro.Wait();
  }

  {
    ThreadPool pool;
    auto coro = SuccessfulTimeoutDemo(pool);
    coro.Wait();
  }

  {
    ThreadPool pool;
    auto coro = SharedCancellationDemo(pool);
    coro.Wait();
  }

  {
    ThreadPool pool;
    auto coro = CancellationCallbackDemo(pool);
    coro.Wait();
  }

  {
    ThreadPool pool;
    auto coro = DAGCancellationDemo(pool);
    coro.Wait();
  }

  std::cout << "=== All cancellation demos completed ===\n";
}
