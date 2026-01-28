這是一個非常核心的問題，觸及了事件系統在實際 C++ 遊戲開發中最容易遇到的兩個坑：**記憶體管理（Dangling Pointers）** 與 **效能瓶頸（Event Flooding）**。

我們來逐一完善這個「理想 API」。

---

### 1. 關於 Unsubscribe 與 EventScope

**簡答：**
**是的，你絕對需要 Unsubscribe。**
如果不取消訂閱，當 Component 被銷毀（例如怪物死亡、場景切換）後，EventBus 內部仍然持有指向該 Component 成員函式的指標。當下一個事件觸發時，系統會嘗試呼叫一個已經釋放的記憶體位址（Use-After-Free），導致遊戲直接 Crash。

**EventScope 的概念與實作範例：**
我們不想手動在 `OnDestroy` 裡寫十幾行 `Unsubscribe`。`EventScope` 利用 RAII（資源取得即初始化）原則，充當所有訂閱憑證（Tokens）的容器。

**Ideal API Usage (使用者看到的樣子):**

```cpp
class MyComponent : public Component {
    EventScope events_; // 1. 定義在成員變數 (跟著 Component 生死)

public:
    void OnStart() {
        // 2. 透過 events_ 進行訂閱
        // 當 MyComponent 被銷毀時，events_ 也會被銷毀
        // events_ 的解構子會自動把這裡註冊的所有監聽器從 Bus 移除
        events_.Subscribe<PlayerHitEvent>(GetGlobalBus(), [this](const auto& e){
             this->OnHit(e);
        });
    }
};

```

**EventScope Implementation Logic (底層邏輯):**
這是一個只存 `EventHandle` 的容器。

```cpp
class EventScope {
    std::vector<EventHandle> handles_; // 儲存所有的訂閱憑證

public:
    // 禁止複製，避免管理混亂 (Move is okay)
    EventScope(const EventScope&) = delete;

    // 解構子：自動清理
    ~EventScope() {
        for (auto& handle : handles_) {
            handle.Unsubscribe(); // 告訴 Bus：這個監聽者已經不在了
        }
    }

    // 輔助函式：註冊並自動加入清單
    template <typename E, typename Func>
    void Subscribe(EventBus& bus, Func&& callback) {
        // 呼叫 Bus 拿到憑證，然後存起來
        EventHandle handle = bus.Subscribe<E>(std::forward<Func>(callback));
        handles_.push_back(std::move(handle));
    }
};

```

---

### 2. 關於過濾器 (`if (e.TargetID != this->GetID())`) 與 Collision Matrix

**簡答：**
你提到的 `if (TargetID != ID)` 是「廣播（Broadcast）」模式。

* **對於低頻事件（如 `LevelLoaded`）：** 這樣寫完全沒問題。
* **對於高頻事件（如 `Collision`）：** 這樣寫**效能會非常差**。

試想：場景有 1000 個物體。如果有兩個物體碰撞，發出一個 `OnCollision` 事件。

* **廣播模式**：Bus 呼叫 1000 個監聽器。998 個物體執行 `if` 檢查後 return，只有 2 個真正執行邏輯。浪費了 99.8% 的 CPU 呼叫開銷。

**理想 API 的改進：引入「頻道（Channels）」或「主體（Subjects）」**

為了支援你提到的 Collision Matrix 概念（只通知相關的人），我們的 API 應該支援 **Targeted Dispatch（定向發送）**。

#### 修改後的 Ideal API：

我們在 `Subscribe` 和 `Emit` 時增加一個可選的 `Subject`（通常是 EntityID 或 Enum）。

**A. 註冊時指定「我只關心誰」**

```cpp
// 在 Component::OnStart 裡面
// 選項 1: 監聽「全域」事件 (例如遊戲暫停)
events_.Subscribe<GamePauseEvent>(Bus, ...);

// 選項 2: 監聽「特定物件」的事件 (例如「我這個 Entity」發生的碰撞)
// 這裡傳入 this->GetID()，表示我只訂閱發送給 "這個 ID" 的 CollisionEvent
events_.Subscribe<CollisionEvent>(Bus, this->GetID(), [this](const CollisionEvent& e){
    // 不需要寫 if (e.TargetID != ID) 了
    // 系統保證只有針對我的碰撞才會呼叫這裡
    HandleCollision(e);
});

```

**B. 發送時指定「這是給誰的」**

```cpp
// 在物理系統 (Physics System) 裡面
void ResolveCollision(Entity a, Entity b) {
    // 定向發送：Bus 內部使用 Hash Map 快速找到 ID=a.id 的監聽者清單，而不遍歷所有人
    Bus.Emit(CollisionEvent{...}, a.id); 
    Bus.Emit(CollisionEvent{...}, b.id);
}

```

#### 底層資料結構變化 (Concept)

為了支援這個功能，EventBus 內部的儲存結構會從單純的 `List` 變成 `Map`。

* **舊結構 (僅支援廣播):**
`Map<EventType, Vector<Listener>>`
* **新結構 (支援定向):**
`Map<EventType, Map<EntityID, Vector<Listener>>>`

這樣一來，當物理系統發出 `Emit(..., targetID)` 時，Bus 的時間複雜度是  (Hash lookup) 找到那一個物件的 Listener，而不是  (Loop all entities)。

### 總結您的 Ideal API 設計圖

綜合以上兩點，這就是你現在需要的完整介面定義：

```cpp
// 1. 範圍管理 (解決 Q1)
class EventScope {
    // 自動管理生命週期
    void Subscribe(EventBus& bus, EventType type, Callback cb);
    void Subscribe(EventBus& bus, EventType type, EntityID subject, Callback cb); // 支援定向
};

// 2. 核心總線
class EventBus {
public:
    // --- 同步 (Sync) ---
    // 全域廣播
    void Emit(const Event& e); 
    // 定向發送 (解決 Q2)
    void Emit(const Event& e, EntityID target); 

    // --- 非同步 (Async / Task) ---
    // 發送請求並等待 Task 完成
    Task<void> PublishAsync(const Event& e);
    
    // --- 訂閱 ---
    // 一般訂閱
    EventHandle Subscribe(Callback cb);
    // 定向訂閱 (只接收指定 ID 的事件)
    EventHandle Subscribe(EntityID subject, Callback cb);
};

```

這樣的設計既安全（自動 Unsubscribe），又高效（解決了碰撞事件的廣播浪費問題）。