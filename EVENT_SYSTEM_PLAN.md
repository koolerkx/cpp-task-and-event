# Event System 實作計劃

## 執行摘要

基於現有的 C++20 Task System，設計並實作一個**生產級事件系統**，平衡簡單性、功能性、性能與記憶體安全。

**核心原則**：
- ✅ 利用現有基礎設施 (CancellationTokenPtr, ThreadPool, 異常機制)
- ✅ KISS 原則：先做 MVP，逐步擴展
- ✅ 記憶體安全優先 (解決 UAF 問題)
- ✅ 漸進式開發 (每階段可獨立測試)

---

## 現有基礎設施分析

### 1. CancellationToken 系統

**已實作特性**：
```cpp
class CancellationToken {
    void Cancel();                              // 觸發取消訊號
    bool IsCancelled() const;                   // 查詢狀態
    void ThrowIfCancelled() const;              // 拋出異常
    void RegisterCallback(std::function<void()>); // 註冊取消回調
};

using CancellationTokenPtr = std::shared_ptr<CancellationToken>;
```

**關鍵發現**：
- ✅ **已經是 shared ownership** (透過 `std::shared_ptr`)
- ✅ 支援回調註冊 (可用於取消 async handlers)
- ✅ 線程安全 (atomic + mutex)

**結論**：完全符合 plan 中的需求，可直接使用於 EventScope。

---

### 2. Task System 架構

**核心機制**：
```cpp
// 前驅計數 + 異常傳播
std::atomic<int> predecessor_count_{0};
std::exception_ptr exception_ = nullptr;

// 兩種後驅類型
std::vector<std::shared_ptr<Task<void>>> successors_unconditional_;  // Finally
std::vector<std::shared_ptr<Task<void>>> successors_conditional_;    // Then
```

**執行流程**：
1. `OnPredecessorFinished()` → 前驅計數遞減
2. 計數歸零 → `TrySchedule()` → `Execute()` 排入 ThreadPool
3. 執行完成 → `NotifySuccessors()` → 通知所有後驅

**可復用元素**：
- ✅ ThreadPool (執行 async handlers)
- ✅ 異常傳播機制 (event handler 失敗處理)
- ✅ 短路優化邏輯 (exception 時跳過執行)

---

### 3. 缺失的功能

**WhenAll 聚合器**：
- ❌ 目前沒有實作
- 📌 **影響**：無法實作 `PublishAsync()`，因為無法等待多個 async handlers 完成
- 💡 **解決方案**：Phase 4 可選實作，或使用簡化版 (fire-and-forget)

---

## 設計決策：簡化 vs 完整

### 對照表：Plan 理想設計 vs 實作方案

| 特性 | Plan 中的設計 | 實作方案 | 理由 |
|------|--------------|---------|------|
| **EventScope + RAII** | ✅ 必須 | ✅ 必須 | 記憶體安全核心 |
| **Sync Emit** | ✅ 必須 | ✅ Phase 1 | 基礎功能 |
| **Async Handlers** | ✅ 必須 | ✅ Phase 3 | 利用現有 ThreadPool |
| **PublishAsync + WhenAll** | ✅ 建議 | ⚠️ Phase 4 (可選) | 需要額外實作 WhenAll |
| **Targeted Dispatch (二級 Map)** | ✅ 性能優化 | ⚠️ Phase 5 (可選) | 複雜度高，先做基礎版 |
| **SubjectID 強型別** | ✅ 型別安全 | ⚠️ Phase 5 (可選) | 與 Targeted Dispatch 綁定 |
| **TargetedEvent Concept** | ✅ 自動路由 | ⚠️ Phase 5 (可選) | 同上 |

**結論**：
- **MVP (Phase 1-3)**：足以支援 90% 的事件系統需求
- **Advanced (Phase 4-5)**：依實際需求再擴展

---

## Phase 1: 基礎 EventBus (Sync Only)

### 目標

實作一個只支援**同步 handlers** 的基礎 EventBus。

### API 設計

```cpp
// Event.hpp
#pragma once
#include <functional>
#include <memory>
#include <mutex>
#include <typeindex>
#include <unordered_map>
#include <vector>

// --- Event Handler Types ---
template <typename E>
using SyncHandler = std::function<void(const E&)>;

// --- Event Handle (訂閱憑證) ---
class EventHandle {
 public:
  EventHandle() = default;
  explicit EventHandle(std::function<void()> unsubscribe_fn)
      : unsubscribe_(std::move(unsubscribe_fn)) {}

  void Unsubscribe() {
    if (unsubscribe_) {
      unsubscribe_();
      unsubscribe_ = nullptr;
    }
  }

  ~EventHandle() {
    Unsubscribe();
  }

  // Move-only
  EventHandle(const EventHandle&) = delete;
  EventHandle& operator=(const EventHandle&) = delete;
  EventHandle(EventHandle&&) = default;
  EventHandle& operator=(EventHandle&&) = default;

 private:
  std::function<void()> unsubscribe_;
};

// --- Event Bus (MVP) ---
class EventBus {
 public:
  EventBus() = default;

  // 同步發送：立即執行所有 sync handlers
  template <typename E>
  void Emit(const E& event) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto type = std::type_index(typeid(E));

    auto it = sync_handlers_.find(type);
    if (it == sync_handlers_.end()) {
      return;  // 沒有訂閱者
    }

    // 複製一份 handler list (避免在執行期間被修改)
    auto handlers_copy = it->second;

    // Release lock before calling handlers (避免死鎖)
    lock.unlock();

    // 執行所有 handlers
    for (auto& handler_any : handlers_copy) {
      try {
        auto handler = std::any_cast<SyncHandler<E>>(handler_any);
        handler(event);
      } catch (const std::exception& e) {
        // Log exception but continue (一個 handler 失敗不應影響其他 handlers)
        // TODO: 整合 logging system
      }
    }
  }

  // 訂閱介面：返回 EventHandle (RAII 管理)
  template <typename E>
  [[nodiscard]] EventHandle Subscribe(SyncHandler<E> handler) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto type = std::type_index(typeid(E));

    // 儲存到 map (使用 std::any 來統一存儲不同類型的 handler)
    auto& handlers = sync_handlers_[type];
    handlers.push_back(std::make_any<SyncHandler<E>>(std::move(handler)));

    // 建立 unsubscribe 回調 (捕獲 weak_ptr 避免循環引用)
    size_t index = handlers.size() - 1;
    auto unsubscribe_fn = [this, type, index]() {
      std::lock_guard<std::mutex> lock(mutex_);
      auto it = sync_handlers_.find(type);
      if (it != sync_handlers_.end() && index < it->second.size()) {
        // Swap-and-pop 避免 vector 元素移動
        std::swap(it->second[index], it->second.back());
        it->second.pop_back();
      }
    };

    return EventHandle(std::move(unsubscribe_fn));
  }

  EventBus(const EventBus&) = delete;
  EventBus& operator=(const EventBus&) = delete;

 private:
  std::mutex mutex_;
  // Map<EventType, Vector<Handler>>
  std::unordered_map<std::type_index, std::vector<std::any>> sync_handlers_;
};
```

### 測試案例

```cpp
// EventDemo.cpp
#include "Event.hpp"
#include <iostream>

struct PlayerHitEvent {
  int damage;
  std::string attacker;
};

void TestBasicEmit() {
  EventBus bus;

  // 訂閱
  int total_damage = 0;
  auto handle = bus.Subscribe<PlayerHitEvent>([&](const PlayerHitEvent& e) {
    total_damage += e.damage;
    std::cout << "Player hit by " << e.attacker << " for " << e.damage << " damage\n";
  });

  // 發送事件
  bus.Emit(PlayerHitEvent{10, "Goblin"});
  bus.Emit(PlayerHitEvent{20, "Dragon"});

  // 驗證
  assert(total_damage == 30);

  // RAII: handle 超出作用域後自動取消訂閱
}

void TestMultipleSubscribers() {
  EventBus bus;

  int count = 0;
  auto h1 = bus.Subscribe<PlayerHitEvent>([&](const auto& e) { count++; });
  auto h2 = bus.Subscribe<PlayerHitEvent>([&](const auto& e) { count++; });

  bus.Emit(PlayerHitEvent{10, "Test"});

  assert(count == 2);  // 兩個 handlers 都執行
}

void TestUnsubscribe() {
  EventBus bus;

  int count = 0;
  {
    auto handle = bus.Subscribe<PlayerHitEvent>([&](const auto& e) { count++; });
    bus.Emit(PlayerHitEvent{10, "Test"});
    assert(count == 1);
  }  // handle 析構，自動取消訂閱

  bus.Emit(PlayerHitEvent{10, "Test"});
  assert(count == 1);  // 不應再執行
}
```

### 實作檢查清單

- [ ] 建立 `src/EventSystem/Event.hpp`
- [ ] 實作 `EventHandle` (RAII unsubscribe)
- [ ] 實作 `EventBus::Subscribe<E>()`
- [ ] 實作 `EventBus::Emit<E>()`
- [ ] 建立 `src/Demo/EventDemo.cpp`
- [ ] 撰寫基礎測試案例 (3 個測試)
- [ ] 編譯並驗證通過

---

## Phase 2: EventScope (RAII 聚合管理)

### 目標

提供一個**聚合容器**來管理多個訂閱，當 Component 銷毀時自動取消所有訂閱。

### API 設計

```cpp
// EventScope.hpp
#pragma once
#include "Event.hpp"
#include <vector>

class EventScope {
 public:
  EventScope() = default;

  ~EventScope() {
    // RAII: 自動取消所有訂閱
    for (auto& handle : handles_) {
      handle.Unsubscribe();
    }
  }

  // 訂閱並自動管理
  template <typename E>
  void Subscribe(EventBus& bus, SyncHandler<E> handler) {
    handles_.push_back(bus.Subscribe<E>(std::move(handler)));
  }

  // Move-only
  EventScope(const EventScope&) = delete;
  EventScope& operator=(const EventScope&) = delete;
  EventScope(EventScope&&) = default;
  EventScope& operator=(EventScope&&) = default;

 private:
  std::vector<EventHandle> handles_;
};
```

### 使用範例

```cpp
// 模擬一個 Component
class PlayerHealthComponent {
 public:
  explicit PlayerHealthComponent(EventBus& bus) : bus_(bus) {
    // 在建構函式中訂閱事件
    events_.Subscribe<PlayerHitEvent>(bus_, [this](const PlayerHitEvent& e) {
      ApplyDamage(e.damage);
    });

    events_.Subscribe<HealEvent>(bus_, [this](const HealEvent& e) {
      Heal(e.amount);
    });
  }

  // 析構時 events_ 自動取消所有訂閱
  ~PlayerHealthComponent() = default;

 private:
  void ApplyDamage(int damage) { health_ -= damage; }
  void Heal(int amount) { health_ += amount; }

  EventBus& bus_;
  EventScope events_;  // RAII 管理所有訂閱
  int health_ = 100;
};
```

### 測試案例

```cpp
void TestEventScopeLifetime() {
  EventBus bus;
  int event_count = 0;

  {
    EventScope scope;
    scope.Subscribe<PlayerHitEvent>(bus, [&](const auto& e) { event_count++; });

    bus.Emit(PlayerHitEvent{10, "Test"});
    assert(event_count == 1);
  }  // scope 析構，自動取消訂閱

  bus.Emit(PlayerHitEvent{10, "Test"});
  assert(event_count == 1);  // 不應再執行
}

void TestComponentLifecycle() {
  EventBus bus;
  int total_damage = 0;

  {
    PlayerHealthComponent component(bus);
    bus.Emit(PlayerHitEvent{10, "Goblin"});
  }  // component 銷毀

  // 驗證：component 銷毀後事件不應再觸發 (避免 UAF)
  bus.Emit(PlayerHitEvent{10, "Dragon"});
  // 如果沒有正確取消訂閱，這裡會 crash
}
```

### 實作檢查清單

- [ ] 建立 `src/EventSystem/EventScope.hpp`
- [ ] 實作 `EventScope` (vector of EventHandle)
- [ ] 撰寫生命週期測試
- [ ] 撰寫 Component 模擬測試 (驗證 UAF 安全)
- [ ] 驗證通過

---

## Phase 3: Async Handlers (Fire-and-Forget)

### 目標

支援**非同步 handlers**，利用 ThreadPool 執行，但**不等待完成**。

### 關鍵挑戰：記憶體安全

**問題**：如果 async handler 捕獲 `[this]`，而 EventScope 在 handler 執行前被銷毀，會導致 UAF。

**解決方案**：handler 必須捕獲 `CancellationTokenPtr` (已經是 shared ownership)，而非捕獲 `this`。

### API 設計

```cpp
// Event.hpp (擴展)
template <typename E>
using AsyncHandler = std::function<void(const E&, CancellationTokenPtr)>;

class EventBus {
 public:
  // 新增：Async 訂閱
  template <typename E>
  [[nodiscard]] EventHandle SubscribeAsync(AsyncHandler<E> handler) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto type = std::type_index(typeid(E));

    auto& handlers = async_handlers_[type];
    handlers.push_back(std::make_any<AsyncHandler<E>>(std::move(handler)));

    // ... (unsubscribe logic 同 sync 版本)
  }

  // 修改 Emit：同時觸發 sync 和 async handlers
  template <typename E>
  void Emit(const E& event) {
    // 1. 同步執行 sync handlers (同 Phase 1)
    EmitSync(event);

    // 2. 排程 async handlers 到 ThreadPool
    EmitAsync(event);
  }

 private:
  template <typename E>
  void EmitAsync(const E& event) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto type = std::type_index(typeid(E));

    auto it = async_handlers_.find(type);
    if (it == async_handlers_.end()) {
      return;
    }

    auto handlers_copy = it->second;
    lock.unlock();

    for (auto& handler_any : handlers_copy) {
      try {
        auto handler = std::any_cast<AsyncHandler<E>>(handler_any);

        // 複製事件 (by value) 確保生命週期安全
        E event_copy = event;

        // 排入 ThreadPool (fire-and-forget)
        pool_.Enqueue([handler, event_copy]() {
          try {
            // 注意：這裡傳入的 token 是 null (Phase 3 簡化版)
            // Phase 3.5 會整合 EventScope 的 CancellationToken
            handler(event_copy, nullptr);
          } catch (const std::exception& e) {
            // Log exception
          }
        });
      } catch (const std::exception& e) {
        // Log exception
      }
    }
  }

  ThreadPool pool_;  // 新增：EventBus 擁有自己的 ThreadPool
  std::unordered_map<std::type_index, std::vector<std::any>> async_handlers_;
};
```

### EventScope 整合 (Phase 3.5)

```cpp
// EventScope.hpp (擴展)
class EventScope {
 public:
  EventScope() : cancel_token_(MakeCancellationToken()) {}

  ~EventScope() {
    // 1. 先取消所有 async handlers
    cancel_token_->Cancel();

    // 2. 再取消訂閱 (移除 handlers)
    for (auto& handle : handles_) {
      handle.Unsubscribe();
    }
  }

  // 新增：Async 訂閱
  template <typename E>
  void SubscribeAsync(EventBus& bus, std::function<void(const E&)> user_handler) {
    // 關鍵：不捕獲 [this]，而是捕獲 [token]
    auto safe_handler = [token = cancel_token_, user_handler](const E& e, CancellationTokenPtr) {
      // 檢查取消狀態
      if (token->IsCancelled()) {
        return;  // EventScope 已銷毀，直接返回
      }

      // 執行用戶邏輯
      user_handler(e);
    };

    handles_.push_back(bus.SubscribeAsync<E>(std::move(safe_handler)));
  }

 private:
  CancellationTokenPtr cancel_token_;
  std::vector<EventHandle> handles_;
};
```

### 使用範例

```cpp
class AssetLoaderComponent {
 public:
  explicit AssetLoaderComponent(EventBus& bus) : bus_(bus) {
    // Async handler：在 ThreadPool 中執行，不阻塞主線程
    events_.SubscribeAsync<LoadAssetEvent>(bus_, [this](const LoadAssetEvent& e) {
      // 這裡可以執行耗時的 IO 操作
      // 即使 Component 被銷毀，token->IsCancelled() 會阻止執行
      LoadTextureFromDisk(e.path);
    });
  }

 private:
  void LoadTextureFromDisk(const std::string& path) {
    // ... 耗時操作
  }

  EventBus& bus_;
  EventScope events_;
};
```

### 測試案例

```cpp
void TestAsyncHandlerExecution() {
  EventBus bus;
  std::atomic<int> count{0};

  {
    EventScope scope;
    scope.SubscribeAsync<TestEvent>(bus, [&](const auto& e) {
      std::this_thread::sleep_for(100ms);  // 模擬耗時操作
      count++;
    });

    bus.Emit(TestEvent{});
    std::this_thread::sleep_for(200ms);  // 等待 async handler 完成
    assert(count == 1);
  }
}

void TestAsyncHandlerCancellation() {
  EventBus bus;
  std::atomic<int> count{0};

  {
    EventScope scope;
    scope.SubscribeAsync<TestEvent>(bus, [&](const auto& e) {
      std::this_thread::sleep_for(200ms);  // 耗時操作
      count++;
    });

    bus.Emit(TestEvent{});
    // 立即銷毀 scope (不等待 handler 完成)
  }

  std::this_thread::sleep_for(300ms);
  assert(count == 0);  // handler 應該被取消，不執行
}
```

### 實作檢查清單

- [ ] EventBus 新增 `ThreadPool pool_`
- [ ] 實作 `EventBus::SubscribeAsync<E>()`
- [ ] 實作 `EventBus::EmitAsync<E>()`
- [ ] EventScope 新增 `CancellationTokenPtr cancel_token_`
- [ ] 實作 `EventScope::SubscribeAsync<E>()`
- [ ] 撰寫 async handler 測試
- [ ] 撰寫取消測試 (驗證 UAF 安全)
- [ ] 驗證通過

---

## Phase 4 (可選): PublishAsync + WhenAll

### 前置條件

需要先實作 `Task::WhenAll` 聚合器。

### WhenAll 設計

```cpp
// TaskExtensions.hpp (新增)
inline std::shared_ptr<Task<void>> WhenAll(std::vector<std::shared_ptr<Task<void>>> tasks) {
  if (tasks.empty()) {
    // 返回立即完成的 Task
    return std::make_shared<Task<void>>([]() {});
  }

  // 建立一個聚合 Task
  auto aggregate = std::make_shared<Task<void>>([tasks]() {
    // 這個 callback 只有在所有前驅完成後才會執行
    // 實際上不需要做任何事，因為依賴機制已經處理了等待
  });

  // 將所有 tasks 設為聚合 Task 的前驅
  for (auto& task : tasks) {
    task->Finally(aggregate);
  }

  return aggregate;
}
```

### PublishAsync 設計

```cpp
// Event.hpp (擴展)
class EventBus {
 public:
  // 非同步發送：返回 Task<void>，等待所有 async handlers 完成
  template <typename E>
  [[nodiscard]] std::shared_ptr<Task<void>> PublishAsync(E event) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto type = std::type_index(typeid(E));

    auto it = async_handlers_.find(type);
    if (it == async_handlers_.end()) {
      // 沒有訂閱者，返回立即完成的 Task
      return std::make_shared<Task<void>>([]() {});
    }

    auto handlers_copy = it->second;
    lock.unlock();

    // 收集所有 async handlers 的 Task
    std::vector<std::shared_ptr<Task<void>>> handler_tasks;

    for (auto& handler_any : handlers_copy) {
      auto handler = std::any_cast<AsyncHandler<E>>(handler_any);
      E event_copy = event;

      // 建立一個 Task 來執行 handler
      auto task = std::make_shared<Task<void>>([handler, event_copy]() {
        handler(event_copy, nullptr);
      });

      handler_tasks.push_back(task);
    }

    // 使用 WhenAll 聚合所有 Tasks
    return WhenAll(handler_tasks);
  }
};
```

### 使用範例

```cpp
// 場景切換：等待所有 listeners 完成載入
Task<void> ChangeLevel() {
  co_await bus.PublishAsync(SceneUnloadEvent{});
  co_await bus.PublishAsync(SceneLoadEvent{"Level2"});

  Log::Info("Level loaded and all listeners finished!");
}
```

### 實作檢查清單

- [ ] 實作 `Task::WhenAll` in `TaskExtensions.hpp`
- [ ] 實作 `EventBus::PublishAsync<E>()`
- [ ] 撰寫 WhenAll 測試
- [ ] 撰寫 PublishAsync 測試 (驗證等待邏輯)
- [ ] 驗證通過

---

## Phase 5 (可選): Targeted Dispatch 優化

### 目標

解決高頻事件（如碰撞檢測）的性能瓶頸，從  O(N) 廣播變成  O(1) 定向派發。

### 設計概要

```cpp
// SubjectID.hpp
struct SubjectID {
  uint64_t value;
  explicit SubjectID(uint64_t v) : value(v) {}
  bool operator==(const SubjectID& other) const { return value == other.value; }
};

// Hash specialization for unordered_map
namespace std {
template <>
struct hash<SubjectID> {
  size_t operator()(const SubjectID& id) const {
    return std::hash<uint64_t>{}(id.value);
  }
};
}

// TargetedEvent concept
template <typename T>
concept TargetedEvent = requires(T a) {
  { a.GetTargetID() } -> std::convertible_to<SubjectID>;
};
```

### EventBus 擴展

```cpp
class EventBus {
 public:
  // 自動路由：根據 Event 是否有 GetTargetID() 決定路徑
  template <typename E>
  void Emit(const E& event) {
    if constexpr (TargetedEvent<E>) {
      EmitTargeted(event, event.GetTargetID());
    } else {
      EmitGlobal(event);
    }
  }

  // 定向訂閱
  template <typename E>
  [[nodiscard]] EventHandle Subscribe(SubjectID target, SyncHandler<E> handler) {
    // 儲存到二級 Map: Map<EventType, Map<SubjectID, Vector<Handler>>>
    // ...
  }

 private:
  template <typename E>
  void EmitTargeted(const E& event, SubjectID target) {
    // O(1) 查找特定 SubjectID 的 handlers
    // ...
  }

  // 二級 Map
  std::unordered_map<std::type_index,
                     std::unordered_map<SubjectID, std::vector<std::any>>> targeted_handlers_;
};
```

### 使用範例

```cpp
struct CollisionEvent {
  Vector3 location;
  float force;
  SubjectID entity_a;
  SubjectID entity_b;

  // 實作 TargetedEvent concept
  SubjectID GetTargetID() const { return entity_a; }
};

// Component 只訂閱與自己相關的碰撞
class CollisionComponent {
 public:
  CollisionComponent(EventBus& bus, SubjectID entity_id) : bus_(bus), id_(entity_id) {
    // 只訂閱 ID 匹配的碰撞事件
    events_.Subscribe<CollisionEvent>(bus_, id_, [this](const CollisionEvent& e) {
      HandleCollision(e);
    });
  }

 private:
  EventBus& bus_;
  SubjectID id_;
  EventScope events_;
};
```

### 效能對比

| 場景 | 廣播模式 | 定向模式 | 效能提升 |
|------|---------|---------|---------|
| 1000 個物體，2 個碰撞 | O(1000) = 呼叫 1000 次 handlers，998 次無效 | O(1) = 只呼叫 2 次 handlers | **500x** |
| 100 個物體，10 個碰撞 | O(100) = 呼叫 100 次，90 次無效 | O(1) = 只呼叫 10 次 | **10x** |

### 實作檢查清單

- [ ] 建立 `SubjectID.hpp`
- [ ] 實作 `TargetedEvent` concept
- [ ] EventBus 新增二級 Map
- [ ] 實作 `EventBus::Subscribe<E>(SubjectID, handler)`
- [ ] 實作 `EventBus::EmitTargeted<E>()`
- [ ] 撰寫性能測試 (對比廣播 vs 定向)
- [ ] 驗證通過

---

## 檔案結構

```
src/EventSystem/
├── Event.hpp              # EventBus, EventHandle, Emit, Subscribe
├── EventScope.hpp         # EventScope (RAII 管理)
├── SubjectID.hpp          # (Phase 5) 強型別 ID
└── EventConcepts.hpp      # (Phase 5) TargetedEvent concept

src/Demo/
├── EventDemo.cpp          # Phase 1-2 測試
├── AsyncEventDemo.cpp     # Phase 3 測試
├── PublishAsyncDemo.cpp   # Phase 4 測試
└── TargetedEventDemo.cpp  # Phase 5 測試
```

---

## 風險與緩解

### 風險 1: 死鎖 (Deadlock)

**場景**：Event handler 內部又發送事件，導致遞迴鎖定。

**緩解方案**：
```cpp
// Emit 實作中，複製 handlers 後立即釋放鎖
auto handlers_copy = it->second;
lock.unlock();  // 在執行 handlers 前釋放鎖

for (auto& handler : handlers_copy) {
  handler(event);  // 現在 handler 內部可以安全地呼叫 Emit
}
```

### 風險 2: 事件循環 (Event Loop)

**場景**：A 發送 EventX → B 處理後發送 EventY → A 處理後發送 EventX → 無限遞迴。

**緩解方案**：
```cpp
// EventBus 中加入遞迴深度檢測
thread_local int emit_depth = 0;
constexpr int MAX_EMIT_DEPTH = 32;

template <typename E>
void Emit(const E& event) {
  if (++emit_depth > MAX_EMIT_DEPTH) {
    --emit_depth;
    throw std::runtime_error("Event recursion depth exceeded");
  }

  // ... emit logic

  --emit_depth;
}
```

### 風險 3: Handler 異常傳播

**場景**：一個 handler 拋出異常，導致後續 handlers 不執行。

**緩解方案**：
```cpp
// 每個 handler 都包裹在 try-catch 中
for (auto& handler : handlers_copy) {
  try {
    handler(event);
  } catch (const std::exception& e) {
    // Log but continue
    std::cerr << "Event handler exception: " << e.what() << "\n";
  }
}
```

---

## 測試策略

### 單元測試

| 測試案例 | 驗證目標 |
|---------|---------|
| `TestBasicEmit` | 基礎發送與接收 |
| `TestMultipleSubscribers` | 多訂閱者正確性 |
| `TestUnsubscribe` | EventHandle RAII 正確性 |
| `TestEventScopeLifetime` | EventScope RAII 正確性 |
| `TestAsyncHandlerExecution` | Async handler 正確執行 |
| `TestAsyncHandlerCancellation` | Async handler 取消機制 |
| `TestPublishAsyncWait` | PublishAsync 等待邏輯 |
| `TestTargetedDispatch` | 定向派發正確性 |

### 壓力測試

```cpp
void StressTest_HighFrequencyEvents() {
  EventBus bus;
  std::atomic<int> count{0};

  // 1000 個訂閱者
  std::vector<EventScope> scopes(1000);
  for (auto& scope : scopes) {
    scope.Subscribe<TestEvent>(bus, [&](const auto& e) { count++; });
  }

  // 發送 10000 次事件
  for (int i = 0; i < 10000; ++i) {
    bus.Emit(TestEvent{});
  }

  assert(count == 10000 * 1000);  // 驗證所有 handlers 都執行
}
```

### UAF 測試

```cpp
void UAFTest_ComponentDestruction() {
  EventBus bus;

  {
    EventScope scope;
    scope.SubscribeAsync<TestEvent>(bus, [](const auto& e) {
      std::this_thread::sleep_for(1s);  // 模擬耗時操作
    });

    bus.Emit(TestEvent{});
    // 立即銷毀 scope (不等待 async handler)
  }

  std::this_thread::sleep_for(2s);
  // 如果沒有正確實作 CancellationToken，這裡會 crash
  // 驗證：程式正常結束，無 crash
}
```

---

## 性能目標

| 操作 | 目標 |
|------|------|
| `Emit<E>` (sync, 100 subscribers) | < 10 μs |
| `Subscribe<E>` | < 1 μs |
| `Unsubscribe` | < 1 μs |
| `EmitAsync<E>` (async, 100 subscribers) | < 50 μs (enqueue time) |
| `EmitTargeted<E>` (1 subscriber) | < 1 μs |

---

## 總結

### MVP (Phase 1-3) 完成後即可投入使用

**已實現特性**：
- ✅ RAII 訂閱管理 (EventScope)
- ✅ Sync/Async handler 分離
- ✅ 記憶體安全 (CancellationToken)
- ✅ 異常隔離 (handler 失敗不影響其他 handlers)
- ✅ 死鎖預防 (複製 handlers + 釋放鎖)

**適用場景**：
- UI 事件 (Button OnClick)
- 遊戲邏輯事件 (PlayerDied, LevelCompleted)
- 系統事件 (SceneLoaded, AssetReady)

### Advanced Features (Phase 4-5) 按需實作

**Phase 4 (PublishAsync + WhenAll)**：
- 適用於需要等待所有 listeners 完成的場景
- 例如：場景切換、存檔系統

**Phase 5 (Targeted Dispatch)**：
- 適用於高頻事件優化
- 例如：物理碰撞、傷害計算

---

## 下一步

1. **確認 MVP 範圍**：是否同意 Phase 1-3 為初始目標？
2. **開始實作 Phase 1**：基礎 EventBus (預計 2-3 小時)
3. **逐步驗證**：每個 Phase 完成後執行測試，確保穩定性
4. **文檔與範例**：為 DX12 引擎整合提供使用指南

**準備好開始實作了嗎？**
