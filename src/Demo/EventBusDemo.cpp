/**
 * @file EventBusDemo.cpp
 * @brief Comprehensive test suite for EventBus functionality.
 */

#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>

#include "CancellationToken.hpp"
#include "Event.hpp"
#include "EventBus.hpp"
#include "ThreadPool.hpp"

namespace EventBusDemo {

struct TestEvent : Event<TestEvent> {
  static constexpr std::string_view EventName = "test.event";
  float damage;
};

struct TestAsyncEvent : Event<TestAsyncEvent> {
  static constexpr std::string_view EventName = "test.async";
  int value;
};

struct TestCancelEvent : Event<TestCancelEvent> {
  static constexpr std::string_view EventName = "test.cancel";
  int value;
};

struct TestCancelDuringEvent : Event<TestCancelDuringEvent> {
  static constexpr std::string_view EventName = "test.cancel.during";
  int value;
};

struct TestLifetimeEvent : Event<TestLifetimeEvent> {
  static constexpr std::string_view EventName = "test.lifetime";
  int value;
};

struct EventA : Event<EventA> {
  static constexpr std::string_view EventName = "event.a";
  int value;
};

struct EventB : Event<EventB> {
  static constexpr std::string_view EventName = "event.b";
  int value;
};

// Tests basic synchronous event emission with multiple subscribers
// Shows: event dispatch, multiple handlers, call counting
void TestBasicEmit() {
  std::cout << "\nTest 1: Basic Sync Emit\n";

  ThreadPool pool(4);
  auto bus = std::make_shared<EventBus>(pool);

  int call_count = 0;
  auto handle1 = bus->Subscribe<TestEvent>([&](const TestEvent&) { call_count++; });
  auto handle2 = bus->Subscribe<TestEvent>([&](const TestEvent&) { call_count++; });
  auto handle3 = bus->Subscribe<TestEvent>([&](const TestEvent& event) { std::cout << "Player took " << event.damage << " damage\n"; });

  bus->Emit(TestEvent{.damage = 10.0f});
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
  auto handle1 = bus->Subscribe<TestEvent>([&](const TestEvent&) { call_count++; });
  auto handle2 = bus->Subscribe<TestEvent>([&](const TestEvent&) { call_count++; });
  auto handle3 = bus->Subscribe<TestEvent>([&](const TestEvent&) { call_count++; });

  bus->Emit(TestEvent{.damage = 0.0f});
  std::cout << "First emit - call count: " << call_count << " (expected: 3)\n";
  assert(call_count == 3);

  handle2.Unsubscribe();
  call_count = 0;

  bus->Emit(TestEvent{.damage = 0.0f});
  std::cout << "After unsubscribe handler 2 - call count: " << call_count << " (expected: 2)\n";
  assert(call_count == 2);

  handle1.Unsubscribe();
  handle3.Unsubscribe();
  call_count = 0;

  bus->Emit(TestEvent{.damage = 0.0f});
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
  auto handle1 = bus->Subscribe<TestAsyncEvent>([&](const TestAsyncEvent&) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    call_count++;
  });
  auto handle2 = bus->Subscribe<TestAsyncEvent>([&](const TestAsyncEvent&) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    call_count++;
  });

  bus->EmitAsync(TestAsyncEvent{.value = 0});

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
  auto handle = bus->Subscribe<TestCancelEvent>([&](const TestCancelEvent&) { call_count++; });

  bus->EmitAsync(TestCancelEvent{.value = 0}, token);

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
    bus->Subscribe<TestCancelDuringEvent>([&](const TestCancelDuringEvent&) {
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
      call_count++;
    });
  }

  bus->EmitAsync(TestCancelDuringEvent{.value = 0}, token);

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
  auto handle = bus->Subscribe<TestLifetimeEvent>([&](const TestLifetimeEvent&) { call_count++; });

  bus->Emit(TestLifetimeEvent{.value = 0});
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

  auto handle_a = bus->Subscribe<EventA>([&](const EventA&) { event_a_count++; });
  auto handle_b = bus->Subscribe<EventB>([&](const EventB&) { event_b_count++; });

  bus->Emit(EventA{.value = 0});
  bus->Emit(EventB{.value = 0});
  bus->Emit(EventA{.value = 0});

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
