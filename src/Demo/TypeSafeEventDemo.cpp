/**
 * @file TypeSafeEventDemo.cpp
 * @brief Comprehensive tests for type-safe EventBus API.
 */

#include <cassert>
#include <chrono>
#include <iostream>
#include <memory>
#include <thread>

#include "EventBus.hpp"
#include "Events.hpp"
#include "ThreadPool.hpp"

namespace TypeSafeEventDemo {

// Tests basic type-safe event emission and subscription
// Shows: event dispatch with type safety, subscribing to PlayerDamagedEvent, emitting events, accumulating damage
void TestTypeSafeBasic() {
  std::cout << "\nTest 1: Type-Safe Basic Emit/Subscribe\n";

  ThreadPool pool(4);
  auto bus = std::make_shared<EventBus>(pool);

  int total_damage = 0;
  auto handle = bus->Subscribe<PlayerDamagedEvent>([&](const PlayerDamagedEvent& event) {
    total_damage += static_cast<int>(event.damage);
    std::cout << "  Player " << event.player_id << " took " << event.damage << " damage\n";
  });

  bus->Emit(PlayerDamagedEvent{.player_id = 1, .damage = 25.0f});
  bus->Emit(PlayerDamagedEvent{.player_id = 2, .damage = 30.0f});

  std::cout << "  Total damage: " << total_damage << " (expected: 55)\n";
  assert(total_damage == 55);
  std::cout << "  PASS\n";
}

// Tests handling multiple event types
// Shows: distinguishing different event types, subscribing to PlayerDamagedEvent and ItemPickedUpEvent, emitting mixed events, counting
// occurrences
void TestMultipleEventTypes() {
  std::cout << "\nTest 2: Multiple Event Types\n";

  ThreadPool pool(4);
  auto bus = std::make_shared<EventBus>(pool);

  int damage_count = 0;
  int item_count = 0;

  auto damage_handle = bus->Subscribe<PlayerDamagedEvent>([&](const PlayerDamagedEvent& event) {
    damage_count++;
    std::cout << "  Damage event: player_id=" << event.player_id << ", damage=" << event.damage << "\n";
  });

  auto item_handle = bus->Subscribe<ItemPickedUpEvent>([&](const ItemPickedUpEvent& event) {
    item_count++;
    std::cout << "  Picked up: " << event.item_name << " (id=" << event.item_id << ")\n";
  });

  bus->Emit(PlayerDamagedEvent{.player_id = 1, .damage = 10.0f});
  bus->Emit(ItemPickedUpEvent{.item_id = 101, .item_name = "Health Potion"});
  bus->Emit(PlayerDamagedEvent{.player_id = 2, .damage = 15.0f});

  std::cout << "  Damage events: " << damage_count << " (expected: 2)\n";
  std::cout << "  Item events: " << item_count << " (expected: 1)\n";
  assert(damage_count == 2);
  assert(item_count == 1);
  std::cout << "  PASS\n";
}

// Tests asynchronous type-safe event emission
// Shows: async dispatch using thread pool, subscribing to SceneLoadedEvent, emitting asynchronously, checking callback execution
void TestTypeSafeAsync() {
  std::cout << "\nTest 3: Type-Safe Async Emit\n";

  ThreadPool pool(4);
  auto bus = std::make_shared<EventBus>(pool);

  std::atomic<int> async_call_count{0};

  auto handle = bus->Subscribe<SceneLoadedEvent>([&](const SceneLoadedEvent& event) {
    async_call_count++;
    std::cout << "  Scene loaded: " << event.scene_name << " (took " << event.load_time_ms << "ms)\n";
  });

  bus->EmitAsync(SceneLoadedEvent{.scene_name = "Level1", .load_time_ms = 123.45f});

  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  std::cout << "  Async call count: " << async_call_count.load() << " (expected: 1)\n";
  assert(async_call_count.load() == 1);
  std::cout << "  PASS\n";
}

// Tests asynchronous event emission with cancellation
// Shows: cancellation tokens for async events, emitting with token, cancelling, verifying no further callbacks
void TestAsyncWithCancellation() {
  std::cout << "\nTest 4: Type-Safe Async Emit with Cancellation\n";

  ThreadPool pool(4);
  auto bus = std::make_shared<EventBus>(pool);

  std::atomic<int> call_count{0};

  auto handle = bus->Subscribe<SceneLoadedEvent>([&](const SceneLoadedEvent& event) {
    call_count++;
    std::cout << "  Scene loaded: " << event.scene_name << "\n";
  });

  auto token = MakeCancellationToken();
  bus->EmitAsync(SceneLoadedEvent{.scene_name = "Level2", .load_time_ms = 200.0f}, token);

  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  std::cout << "  Call count before cancellation: " << call_count.load() << " (expected: 1)\n";
  assert(call_count.load() == 1);

  token->Cancel();
  bus->EmitAsync(SceneLoadedEvent{.scene_name = "Level3", .load_time_ms = 300.0f}, token);

  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  std::cout << "  Call count after cancellation: " << call_count.load() << " (expected: 1, no change)\n";
  assert(call_count.load() == 1);
  std::cout << "  PASS\n";
}

void RunAll() {
  std::cout << "\n=== Type-Safe Event Bus Tests ===\n";
  TestTypeSafeBasic();
  TestMultipleEventTypes();
  TestTypeSafeAsync();
  TestAsyncWithCancellation();
  std::cout << "\nAll Type-Safe Event Bus tests passed!\n";
}

}  // namespace TypeSafeEventDemo
