# Type-Safe EventBus Usage Guide

## Overview

Type-safe EventBus API 提供編譯期型別檢查，使用 **CRTP (Curiously Recurring Template Pattern)** 實現。

**核心特性**：
- ✅ 編譯期型別安全
- ✅ Event struct 作為 schema
- ✅ 100% 向後相容
- ✅ 單一 EventBus 實例
- ✅ IDE 自動補全

---

## Quick Start

### 1. Define Event

```cpp
#include "Event.hpp"

struct PlayerDamagedEvent : Event<PlayerDamagedEvent> {
  static constexpr std::string_view EventName = "player.damaged";
  int player_id;
  float damage;
};
```

### 2. Subscribe (Type-Safe)

```cpp
ThreadPool pool(4);
auto bus = std::make_shared<EventBus>(pool);

auto handle = bus->Subscribe<PlayerDamagedEvent>([](const PlayerDamagedEvent& event) {
  std::cout << "Player " << event.player_id << " took " << event.damage << " damage\n";
});
```

### 3. Emit (Type-Safe)

```cpp
// Sync emit
bus->Emit(PlayerDamagedEvent{.player_id = 1, .damage = 25.0f});

// Async emit
bus->EmitAsync(PlayerDamagedEvent{.player_id = 2, .damage = 30.0f});

// Async with cancellation
auto token = MakeCancellationToken();
bus->EmitAsync(PlayerDamagedEvent{.player_id = 3, .damage = 15.0f}, token);
```

---

## Compile-Time Safety

### ✅ Valid Event

```cpp
struct MyEvent : Event<MyEvent> {
  static constexpr std::string_view EventName = "my.event";
  int data;
};

bus->Emit(MyEvent{.data = 42});  // OK
```

### ❌ Invalid Event (Won't Compile)

```cpp
struct InvalidEvent {};  // No Event<T> inheritance

bus->Emit(InvalidEvent{});  // Compile error: EventType concept not satisfied
```

---

## Backward Compatibility

Type-erased API 完全保留，可以混用：

```cpp
// Type-safe subscribe
bus->Subscribe<PlayerDamagedEvent>([](const PlayerDamagedEvent& e) {
  std::cout << "Type-safe handler\n";
});

// Type-erased subscribe (legacy)
bus->Subscribe("player.damaged", [](std::any data) {
  std::cout << "Legacy handler\n";
});

// Type-safe emit
bus->Emit(PlayerDamagedEvent{.player_id = 1, .damage = 10.0f});

// ✅ Both handlers will be called!
```

---

## API Reference

### Event Base Class

```cpp
template<typename Derived>
struct Event {
  static constexpr std::string_view GetEventName();
};

template<typename T>
concept EventType = requires {
  { T::EventName } -> std::convertible_to<std::string_view>;
} && std::is_base_of_v<Event<T>, T>;
```

### EventBus Methods

```cpp
// Type-safe emit (sync)
template<typename E> requires EventType<E>
void Emit(const E& event);

// Type-safe subscribe
template<typename E> requires EventType<E>
EventHandle Subscribe(std::function<void(const E&)> handler);

// Type-safe async emit
template<typename E> requires EventType<E>
void EmitAsync(const E& event);

// Type-safe async emit with cancellation
template<typename E> requires EventType<E>
void EmitAsync(const E& event, CancellationTokenPtr token);
```

---

## Examples

See `src/Demo/TypeSafeEventDemo.cpp` for comprehensive examples:
- Basic type-safe emit/subscribe
- Multiple event types
- Async emit
- Backward compatibility
- Cancellation support

---

## Implementation Details

**Files**:
- `src/TaskSystem/Event.hpp` - CRTP base class & concept
- `src/TaskSystem/EventBus.hpp` - Type-safe method overloads
- `src/Demo/Events.hpp` - Example event definitions
- `src/Demo/TypeSafeEventDemo.cpp` - Test suite

**Design**:
- Template methods delegate to existing type-erased API
- No runtime overhead (templates inlined by compiler)
- Type mismatch at runtime logs error but doesn't crash

---

## Migration Guide

**現有程式碼無需修改** - type-erased API 完全保留。

**Opt-in 遷移**（針對常用 events）：

1. 定義 Event struct
2. 將 Subscribe 改為 type-safe 版本
3. 將 Emit 改為 type-safe 版本

**Before**:
```cpp
bus->Emit("player.damaged", std::any(25));
bus->Subscribe("player.damaged", [](std::any data) {
  int damage = std::any_cast<int>(data);
});
```

**After**:
```cpp
struct PlayerDamagedEvent : Event<PlayerDamagedEvent> {
  static constexpr std::string_view EventName = "player.damaged";
  int damage;
};

bus->Emit(PlayerDamagedEvent{.damage = 25});
bus->Subscribe<PlayerDamagedEvent>([](const PlayerDamagedEvent& event) {
  int damage = event.damage;  // No casting needed
});
```
