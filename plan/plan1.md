這是一個非常好的切入點。在進入實作細節之前，先定義「理想的 API（Ideal API）」是架構設計中最重要的一步。

針對你的需求（基於 C++20 Task System、PoC 階段但考量未來整合、DX12 引擎背景），以及你提供的現有 Sync Lifecycle（Update/Render），我設計了一套**混合式事件系統（Hybrid Event System）**的 API 介面。

這套設計的核心哲學是：**區分「即時訊號（Signals）」與「非同步流程（Async Events）」**，並利用 RAII 解決訂閱管理的混亂問題。

以下是針對你的專案構想的 Ideal API Design。

---

### 1. 核心概念：三種通訊模式

為了避免所有東西都混在一個全域 EventBus 裡造成「義大利麵條式代碼」，我們將 API 分為三個層次：

1. **Delegate/Signal (單體訊號)**：像 Unreal 的 `OnClicked`，物件對物件的直接溝通。
2. **Global/Scene Bus (廣播總線)**：解耦的系統間溝通（如：成就系統監聽 `EnemyKilled`）。
3. **Async Request (任務整合)**：需要等待回應或長時間執行的事件（如：`LoadLevel`）。

---

### 2. The Ideal API (Usage Code)

#### A. 定義事件 (Event Definition)

保持簡單，使用純 Struct。利用 C++20/23 特性，我們不需要繼承任何基類。

```cpp
// 簡單的資料事件
struct PlayerHitEvent {
    int damage;
    Vector3 location;
    EntityID attacker;
};

// 請求型事件 (預期會有 Async Task 回應)
struct SceneLoadRequest {
    std::string sceneName;
    bool additive;
};

```

#### B. 訂閱管理 (Subscription Management - The RAII Way)

這是解決「忘記取消訂閱」導致 Crash 的關鍵。我們引入 `EventScope` 概念（類似你的 `Subscription` 但更強大），通常作為 Component 的成員變數。

```cpp
class PlayerHealthComponent : public Component {
    // 1. 定義一個 EventScope，當 Component 被銷毀時，自動取消所有註冊的事件
    EventScope events_; 

public:
    void OnStart() override {
        // API 亮點：使用 fluent syntax 綁定事件
        // this->GetOwner() 可能是全域 Bus 或 Scene Bus
        
        // 範例 1: 同步監聽 (Sync Listener) - 適合邏輯計算、UI 更新
        events_.Subscribe<PlayerHitEvent>(GetSceneBus(), [this](const PlayerHitEvent& e) {
            this->ApplyDamage(e.damage);
        });

        // 範例 2: 非同步監聽 (Async Listener) - 整合你的 Task System
        // 當監聽者回傳 Task<void> 時，系統知道這是一個長時間操作
        events_.SubscribeAsync<SceneLoadRequest>(GetGlobalBus(), [this](const SceneLoadRequest& e) -> Task<void> {
            co_await ShowLoadingScreen();
            // 這裡可以執行複雜的 Task Chain
            co_await LoadAssetsAsync(e.sceneName); 
            co_await HideLoadingScreen();
        });
    }
};

```

#### C. 發送事件 (Dispatching)

發送端需要能區分「射後不理」和「等待完成」。

```cpp
class CombatSystem : public System {
public:
    // Case 1: Fire and Forget (同步廣播)
    // 適合：特效生成、音效播放、UI 數值變動
    void ProcessCombat() {
        // Emit 是同步的，所有 Sync Listener 會立刻執行完畢
        // Async Listener 會被排入 Task System 的 Thread Pool，但 Emit 本身不等待它們
        GetSceneBus().Emit(PlayerHitEvent{ 10, {0,0,0}, enemyId }); 
    }

    // Case 2: Await Execution (非同步等待)
    // 適合：流程控制，如「切換場景」、「存檔」
    Task<void> ChangeLevel() {
        // PublishAsync 回傳一個 Task
        // 這個 Task 會等到所有訂閱者 (Listeners) 的 Task 都完成後才算完成
        // 這利用了你的 Task System 的 .Then() 或 Predecessor Count 機制
        co_await GetGlobalBus().PublishAsync(SceneLoadRequest{ "Level2", false });
        
        Log::Info("Level Loaded and all listeners finished!");
    }
};

```

---

### 3. API 細節設計 (Header Mockup)

這是我建議的介面定義，這將決定你如何實作底層。

```cpp
// EventHandle: 單一訂閱的控制權 (可手動開關)
struct EventHandle {
    void Unsubscribe();
    void SetEnabled(bool enabled);
};

// EventScope: 聚合管理 (Component 用)
class EventScope {
public:
    ~EventScope(); // Destructor 自動呼叫所有內部的 Unsubscribe

    // 語法糖：綁定並自動納入管理
    template <typename E, typename Func>
    void Subscribe(EventBus& bus, Func&& handler);
    
    template <typename E, typename Func>
    void SubscribeAsync(EventBus& bus, Func&& handler);
};

// EventBus: 核心介面
class EventBus {
public:
    // 1. 同步發送：立即執行所有同步 Handler，非同步 Handler 丟進 ThreadPool
    template <typename E>
    void Emit(const E& event);

    // 2. 非同步發送：回傳 Task，該 Task 會等待所有 Async Handler 完成
    template <typename E>
    [[nodiscard]] Task<void> PublishAsync(const E& event);

    // 3. 註冊介面
    template <typename E>
    [[nodiscard]] EventHandle Subscribe(std::function<void(const E&)> handler);

    // 4. 支援 Task 的註冊介面
    template <typename E>
    [[nodiscard]] EventHandle SubscribeAsync(std::function<Task<void>(const E&)> handler);
};

```

---

### 4. 解決你的「痛點」分析

根據你提到的舊有 `event_system.h` 和新的 Task System，這套 Ideal API 如何解決問題：

1. **整合混亂的 Dispatch Moment**：
* **Sync Logic (Update/Render)**: 使用 `Emit`。它保證在當前 Frame、當前 Stack 執行完畢（針對一般邏輯）。
* **Heavy Logic (IO/AI)**: 使用 `SubscribeAsync` 搭配 `Task System`。它不會卡住 Main Loop。


2. **Component 生命周期管理**：
* 所有的 `Subscribe` 都必須透過 `EventScope` 進行（或是強制回傳 `[[nodiscard]] Subscription`）。
* 這確保了當 GameObject 被 `Destroy` 時，不會有懸空的指針指向已釋放的 Component。


3. **Unreal 風格的 Delegates (Optional)**：
* 如果需要物件對物件（例如：Button 對 Window），可以實作一個輕量版的 `Delegate<Args...>`，不走 EventBus，直接存 `std::vector<Callback>`。
* API: `button.OnClick.Add(this, &MyMenu::OnPlay);`



### 5. 進階：Filter 與 Context (可選功能)

為了避免 EventBus 變成全域垃圾桶，API 可以支援 Context：

```cpp
// 只監聽特定 Source 的事件
events_.Subscribe<CollisionEvent>(GetSceneBus(), [this](const CollisionEvent& e) {
    // 過濾邏輯通常寫在 Lambda 內部最快，不需要把 API 弄得太複雜
    if (e.TargetID != this->GetID()) return; 
    HandleCollision();
});

```

### 總結

你的 **Next Step** 應該是：
確認這套 API 的「呼叫方式」是否符合你的開發習慣？特別是 `co_await bus.PublishAsync(...)` 這種寫法。

如果你滿意這個 API Surface，我們就可以討論如何利用你現有的 `Task.hpp` (Promise/Future/Predecessor) 來實作那個 `PublishAsync`，因為那會涉及到如何把多個 Listeners 回傳的 `Task<void>` 聚合成一個 `Task<void>` (Task::WhenAll)。