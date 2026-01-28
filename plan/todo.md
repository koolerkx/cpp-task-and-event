# TODO / 進度追蹤 ✅

> 本檔記錄 Event 系統實作進度、關鍵設計決策與未來工作項目。技術名詞保留為 English（例如 `EventBus`, `ThreadPool`, `CancellationToken`）。

---

## 核心設計決策 🔧

**設計原則：平衡簡單性與功能性** — 將設計拆成 5 個漸進 Phase，先完成 MVP（Phase 1-3），Advanced features 視需求再開發。

| 階段 | 功能 | 狀態 | 理由 |
|------|------|------|------|
| Phase 1 | 基礎 `EventBus` (Sync Only) | ✅ MVP 核心 | 足以支援 90% 場景 |
| Phase 2 | `EventScope` (RAII 管理) | ✅ MVP 核心 | 記憶體安全關鍵 |
| Phase 3 | Async Handlers (fire-and-forget) | ✅ MVP 核心 | 利用現有 `ThreadPool` |
| Phase 4 | `PublishAsync` + `WhenAll` | ⚠️ 可選 | 需要先實作 `Task::WhenAll` |
| Phase 5 | Targeted Dispatch (二級 Map) | ⚠️ 可選 | 高頻事件優化 (如碰撞) |

> **結論**：完成 Phase 1-3 可立即投入生產，Phase 4-5 為進階優化選項。

---

## 充分利用現有基礎設施 ✅⚠️

- ✅ **`CancellationTokenPtr`**（已是 `std::shared_ptr`）：符合 "shared cancellation state" 的需求。
- ✅ **`ThreadPool`**（固定大小 worker threads）：可直接用於執行 async handlers。
- ✅ 異常傳播機制（`std::exception_ptr`）：可復用於 handler 失敗處理。
- ❌ **缺失**：`WhenAll` 尚未實作 → 影響 `PublishAsync()` 的完整實現。可先以 fire-and-forget 模式支援 Phase 3，Phase 4 再補 `WhenAll`。

---

## 記憶體安全優先（避免 UAF）🛡️

核心原則：**不可直接捕獲 `this` 到非同步 handler**。應該捕獲 shared state（例如 cancellation token）並在 handler 開頭檢查取消狀態。

錯誤範例（會造成 UAF）：

```cpp
// ❌ 危險：捕獲 this，EventScope 被銷毀後會 UAF
events_.SubscribeAsync<Event>(bus, [this](const Event& e) {
    this->Handle(e); // 危險
});
```

正確範例（安全）：

```cpp
// ✅ 安全：捕獲 cancel token 和 user handler
auto safe_handler = [token = cancel_token_, user_handler](const E& e) {
    if (token->IsCancelled()) return; // 安全檢查
    user_handler(e);
};
```

**`EventScope` 的析構流程**：
1. `cancel_token_->Cancel()` → 讓所有 pending async handlers 在執行時能檢查並退出。
2. `Unsubscribe()` → 從 `EventBus` 移除 handlers。
3. 即使 `ThreadPool` 尚有未執行的 task，因 `IsCancelled()` 檢查而直接返回，避免使用已釋放的物件。

---

## 實作計劃亮點（API 預覽）✨

Phase 1-2（Sync）：

```cpp
EventBus bus;
EventScope scope;

// Subscribe
scope.Subscribe<PlayerHitEvent>(bus, [](const PlayerHitEvent& e) {
    std::cout << "Hit for " << e.damage << " damage\n";
});

// Emit
bus.Emit(PlayerHitEvent{10, "Goblin"});
```

Phase 3（Async, fire-and-forget）：

```cpp
// Async handler 在 ThreadPool 執行，不阻塞主線程
scope.SubscribeAsync<LoadAssetEvent>(bus, [](const LoadAssetEvent& e) {
    LoadTextureFromDisk(e.path); // 耗時 IO
});
```

Phase 4（PublishAsync，需要 `WhenAll`）：

```cpp
// 等待所有 listeners 完成 (需實作 WhenAll)
co_await bus.PublishAsync(SceneLoadEvent{"Level2"});
```

Phase 5（Targeted Dispatch）：

```cpp
struct CollisionEvent {
    SubjectID GetTargetID() const { return entity_a; } // TargetedEvent concept
};

// 只訂閱特定 entity 的碰撞 (O(1) vs O(N))
scope.Subscribe<CollisionEvent>(bus, my_entity_id, [](const auto& e) {
    HandleCollision(e);
});
```

---

## 下一步與待辦事項 📝

- [x] 完成 Phase 1-3（MVP）
- [ ] 針對 Phase 3 增加取消檢查與錯誤傳播測試
- [ ] 規劃與實作 `Task::WhenAll`（Phase 4 前置）
- [ ] 評估 Phase 5 的資料結構（二級 Map）與性能需求

---

如果你想，我可以：
- 幫你把 `src/TaskSystem` 中的 `WhenAll` 草案寫成 PR 範本 💡
- 或針對 `EventBus` 的 `SubscribeAsync` / `Unsubscribe` 實作做 code review 🔍

需要我優先處理哪一項？ 🔧
