/**
 * @file EventBusDemo.cpp
 * @brief Comprehensive test suite for EventBus functionality.
 */

#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>

#include "CancellationToken.hpp"
#include "EventBus.hpp"
#include "ThreadPool.hpp"

namespace EventBusDemo {

// Tests basic synchronous event emission with multiple subscribers
// Shows: event dispatch, multiple handlers, call counting
void TestBasicEmit() {
  std::cout << "\nTest 1: Basic Sync Emit\n";

  ThreadPool pool(4);
  auto bus = std::make_shared<EventBus>(pool);

  int call_count = 0;
  auto handle1 = bus->Subscribe("test.event", [&](std::any) { call_count++; });
  auto handle2 = bus->Subscribe("test.event", [&](std::any) { call_count++; });
  auto handle3 = bus->Subscribe("test.event", [&](std::any data) {
    if (data.has_value()) {
      float damage = std::any_cast<float>(data);
      std::cout << "Player took " << damage << " damage\n";
    }
  });

  bus->Emit("test.event", 10.0f);
  std::cout << "Call count: " << call_count << " (expected: 2)\n";
  assert(call_count == 2);
}

// Tests unsubscribe functionality and lifetime management
// Shows: removing individual subscribers, verification
void TestUnsubscribe() {
  std::cout << "\nTest 2: Unsubscribe\n";

  ThreadPool pool(4);
  auto bus = std::make_shared<EventBus>(pool);

  int call_count = 0;
  auto handle1 = bus->Subscribe("test.event", [&](std::any) { call_count++; });
  auto handle2 = bus->Subscribe("test.event", [&](std::any) { call_count++; });
  auto handle3 = bus->Subscribe("test.event", [&](std::any) { call_count++; });

  bus->Emit("test.event", 0);
  std::cout << "First emit - call count: " << call_count << " (expected: 3)\n";
  assert(call_count == 3);

  handle2.Unsubscribe();
  call_count = 0;

  bus->Emit("test.event", 0);
  std::cout << "After unsubscribe handler 2 - call count: " << call_count << " (expected: 2)\n";
  assert(call_count == 2);

  handle1.Unsubscribe();
  handle3.Unsubscribe();
  call_count = 0;

  bus->Emit("test.event", 0);
  std::cout << "After unsubscribe all - call count: " << call_count << " (expected: 0)\n";
  assert(call_count == 0);
}

// Tests asynchronous event emission
// Shows: async dispatch, thread pool usage
void TestAsyncEmit() {
  std::cout << "\nTest 3: Async Emit\n";

  ThreadPool pool(4);
  auto bus = std::make_shared<EventBus>(pool);

  std::atomic<int> call_count{0};
  auto handle1 = bus->Subscribe("test.async", [&](std::any) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    call_count++;
  });
  auto handle2 = bus->Subscribe("test.async", [&](std::any) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    call_count++;
  });

  bus->EmitAsync("test.async", 0);

  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  std::cout << "Async call count: " << call_count << " (expected: 2)\n";
  assert(call_count == 2);
}

// Tests cancellation before async event emission
// Shows: early cancellation prevents event handlers from running
void TestCancellation() {
  std::cout << "\nTest 4: Cancellation Before Emit\n";

  ThreadPool pool(4);
  auto bus = std::make_shared<EventBus>(pool);

  auto token = MakeCancellationToken();
  token->Cancel();

  std::atomic<int> call_count{0};
  auto handle = bus->Subscribe("test.cancel", [&](std::any) { call_count++; });

  bus->EmitAsync("test.cancel", 0, token);

  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  std::cout << "After cancel before emit - call count: " << call_count << " (expected: 0)\n";
  assert(call_count == 0);
}

// Tests cancellation during async event emission (realistic scenario)
// Shows: cancelling while handlers are executing, partial completion
void TestCancellationDuringEmit() {
  std::cout << "\nTest 5: Cancellation During Emit (Realistic Scenario)\n";

  ThreadPool pool(4);
  auto bus = std::make_shared<EventBus>(pool);

  auto token = MakeCancellationToken();
  std::atomic<int> call_count{0};

  for (int i = 0; i < 10; ++i) {
    bus->Subscribe("test.cancel.during", [&](std::any) {
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
      call_count++;
    });
  }

  bus->EmitAsync("test.cancel.during", 0, token);

  std::this_thread::sleep_for(std::chrono::milliseconds(5));
  token->Cancel();

  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  std::cout << "After cancel during emit - call count: " << call_count << " (expected: < 10, some handlers cancelled)\n";
  assert(call_count < 10);
}

// Tests subscription handle lifetime safety
// Shows: unsubscribe after EventBus destruction doesn't crash
void TestHandleLifetime() {
  std::cout << "\nTest 6: Handle Lifetime Safety\n";

  ThreadPool pool(4);
  auto bus = std::make_shared<EventBus>(pool);

  int call_count = 0;
  auto handle = bus->Subscribe("test.lifetime", [&](std::any) { call_count++; });

  bus->Emit("test.lifetime", 0);
  assert(call_count == 1);

  bus.reset();

  handle.Unsubscribe();
  std::cout << "EventBus destroyed, handle.Unsubscribe() did not crash\n";
}

// Tests handling multiple different event types
// Shows: independent event channels, selective subscription
void TestMultipleEvents() {
  std::cout << "\nTest 7: Multiple Event Types\n";

  ThreadPool pool(4);
  auto bus = std::make_shared<EventBus>(pool);

  int event_a_count = 0;
  int event_b_count = 0;

  auto handle_a = bus->Subscribe("event.a", [&](std::any) { event_a_count++; });
  auto handle_b = bus->Subscribe("event.b", [&](std::any) { event_b_count++; });

  bus->Emit("event.a", 0);
  bus->Emit("event.b", 0);
  bus->Emit("event.a", 0);

  std::cout << "Event A count: " << event_a_count << " (expected: 2)\n";
  std::cout << "Event B count: " << event_b_count << " (expected: 1)\n";
  assert(event_a_count == 2);
  assert(event_b_count == 1);
}

// Runs all EventBus test suite
// Shows: comprehensive validation of EventBus functionality
void RunAll() {
  std::cout << "\n=== Event Bus Tests ===\n";
  TestBasicEmit();
  TestUnsubscribe();
  TestAsyncEmit();
  TestCancellation();
  TestCancellationDuringEmit();
  TestHandleLifetime();
  TestMultipleEvents();
  std::cout << "\nAll Event Bus tests passed!\n";
}

}  // namespace EventBusDemo
