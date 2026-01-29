/**
 * @file PublishAsyncDemo.cpp
 * @brief Demonstrates awaitable PublishAsync API with exception propagation and cancellation.
 * @details Tests:
 *   - Demo 1: Basic awaitable async event with parallel handlers
 *   - Demo 2: Exception propagation from handlers to awaiter
 *   - Demo 3: Cancellation token integration
 */

#include <atomic>
#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>

#include "CancellationToken.hpp"
#include "CoroTask.hpp"
#include "Event.hpp"
#include "EventBus.hpp"
#include "Task.hpp"
#include "TaskAwaiter.hpp"
#include "ThreadPool.hpp"

namespace PublishAsyncDemo {

struct TestAwaitableEvent : Event<TestAwaitableEvent> {
  static constexpr std::string_view EventName = "test.awaitable";
  std::string resource_name;
};

struct TestExceptionEvent : Event<TestExceptionEvent> {
  static constexpr std::string_view EventName = "test.exception";
  bool should_fail;
};

struct TestCancellationEvent : Event<TestCancellationEvent> {
  static constexpr std::string_view EventName = "test.cancellation";
  int task_id;
};

CoroTask<void> Demo1_BasicAwaitableAsync(ThreadPool& pool) {
  std::cout << "\n=== Demo 1: Basic PublishAsync ===\n";

  auto bus = std::make_shared<EventBus>(pool);
  std::atomic<int> handler_count{0};

  auto h1 = bus->Subscribe<TestAwaitableEvent>([&](const TestAwaitableEvent& event) {
    std::cout << "  Handler 1 processing: " << event.resource_name << "\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    handler_count++;
  });

  auto h2 = bus->Subscribe<TestAwaitableEvent>([&](const TestAwaitableEvent& event) {
    std::cout << "  Handler 2 processing: " << event.resource_name << "\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    handler_count++;
  });

  auto h3 = bus->Subscribe<TestAwaitableEvent>([&](const TestAwaitableEvent& event) {
    std::cout << "  Handler 3 processing: " << event.resource_name << "\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(75));
    handler_count++;
  });

  std::cout << "Publishing event and waiting for all handlers...\n";
  auto task = bus->PublishAsync(TestAwaitableEvent{.resource_name = "texture.png"});

  co_await TaskAwaiter<void>{task, pool};

  std::cout << "All handlers completed! Count: " << handler_count << "\n";
  assert(handler_count == 3);
  std::cout << "\u2713 Demo 1 passed\n";
}

CoroTask<void> Demo2_ExceptionPropagation(ThreadPool& pool) {
  std::cout << "\n=== Demo 2: Exception Propagation ===\n";

  auto bus = std::make_shared<EventBus>(pool);
  std::atomic<int> handler_count{0};

  auto h1 = bus->Subscribe<TestExceptionEvent>([&](const TestExceptionEvent& event) {
    std::cout << "  Handler 1: OK\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    handler_count++;
  });

  auto h2 = bus->Subscribe<TestExceptionEvent>([&](const TestExceptionEvent& event) {
    std::cout << "  Handler 2: Throwing exception\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    if (event.should_fail) {
      throw std::runtime_error("Handler 2 failed");
    }
    handler_count++;
  });

  auto h3 = bus->Subscribe<TestExceptionEvent>([&](const TestExceptionEvent& event) {
    std::cout << "  Handler 3: OK (executed despite Handler 2 failure)\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    handler_count++;
  });

  std::cout << "Publishing event with should_fail=true...\n";
  auto task = bus->PublishAsync(TestExceptionEvent{.should_fail = true});

  bool caught_exception = false;
  try {
    co_await TaskAwaiter<void>{task, pool};
  } catch (const std::runtime_error& e) {
    std::cout << "\u2713 Caught exception: " << e.what() << "\n";
    caught_exception = true;
  }

  assert(caught_exception);
  std::cout << "Completed handlers: " << handler_count << " (Handler 2 threw, but all executed)\n";
  std::cout << "\u2713 Demo 2 passed\n";
}

CoroTask<void> Demo3_Cancellation(ThreadPool& pool) {
  std::cout << "\n=== Demo 3: Cancellation ===\n";

  auto bus = std::make_shared<EventBus>(pool);
  auto token = MakeCancellationToken();
  std::atomic<int> handler_count{0};

  std::vector<EventHandle> handles;
  for (int i = 0; i < 5; ++i) {
    handles.push_back(bus->Subscribe<TestCancellationEvent>([&, i](const TestCancellationEvent& event) {
      std::cout << "  Handler " << i + 1 << " starting (task_id=" << event.task_id << ")\n";
      std::this_thread::sleep_for(std::chrono::milliseconds(200));
      std::cout << "  Handler " << i + 1 << " completed\n";
      handler_count++;
    }));
  }

  std::cout << "Publishing event with cancellation token...\n";
  auto task = bus->PublishAsync(TestCancellationEvent{.task_id = 42}, token);

  std::thread canceller([token]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    std::cout << "Cancelling token...\n";
    token->Cancel();
  });

  bool caught_cancellation = false;
  try {
    co_await TaskAwaiter<void>{task, pool};
  } catch (const TaskCancelledException& e) {
    std::cout << "\u2713 Caught TaskCancelledException: " << e.what() << "\n";
    caught_cancellation = true;
  }

  canceller.join();

  assert(caught_cancellation);
  std::cout << "Completed handlers: " << handler_count << " (some may have started before cancellation)\n";
  std::cout << "\u2713 Demo 3 passed\n";
}

void RunAll() {
  std::cout << "\n======================================\n";
  std::cout << "=== PublishAsync Demo Suite ===\n";
  std::cout << "======================================\n";

  {
    ThreadPool pool(4);
    auto coro = Demo1_BasicAwaitableAsync(pool);
    coro.Wait();
  }

  {
    ThreadPool pool(4);
    auto coro = Demo2_ExceptionPropagation(pool);
    coro.Wait();
  }

  {
    ThreadPool pool(4);
    auto coro = Demo3_Cancellation(pool);
    coro.Wait();
  }

  std::cout << "\n=== All PublishAsync demos passed! ===" << std::endl;
}

}  // namespace PublishAsyncDemo
