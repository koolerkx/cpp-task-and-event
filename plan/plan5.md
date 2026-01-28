這段對話的核心在於為你的 C++20 Task 系統與 DX12 引擎設計一個**理想的事件系統 API**。我們從最初的同步 `EventBus` 概念，演進到一個具備 **RAII 訂閱管理**、**同步/非同步分離**以及**定向派發（Targeted Dispatch）**優化的強健系統。

最關鍵的轉折在於識別並解決了 **Async UAF (Use-After-Free)** 的記憶體安全問題，確保當 Component 銷毀時，已排程但尚未執行的非同步任務不會因存取已釋放的 `this` 指標而導致程式崩潰 。

---

### 1. 核心架構總結

這套設計在簡單性與安全性之間取得了平衡，主要包含以下三個修正後的關鍵支柱：

* 
**強健的生命週期管理 (`EventScope`)**：利用 `CancellationTokenSource` 與 `std::stop_token` (或共享狀態的 Token) 確保非同步任務的安全性。即使 `EventScope` 被銷毀，持有 Token 的任務仍能安全地檢查取消狀態，而不會存取懸空指標 。


* **明確的派發語意**：
* 
**`Emit(const E&)`**：僅限同步監聽器，使用參考傳遞以追求最高效能 。


* 
**`PublishAsync(E)`**：強制事件資料「按值複製（By Value）」，確保在跨執行緒或非同步流程中資料的生命週期安全 。




* 
**定向派發 (Targeted Dispatch)**：引入 `SubjectID`（強型別包裝）與 C++20 `concept`。當事件具備 `GetTargetID()` 時，系統會自動切換為  的 Map 查找，這對解決高頻碰撞事件（Collision Matrix）的效能瓶頸至關重要 。



---

### 2. 實作任務清單 (Step-by-Step)

依照開發的複雜度與依賴性，建議按照以下階段進行開發：

#### 第一階段：基礎設施與同步核心 (MVP)

* [ ] **定義強型別 ID**：實作 `SubjectID` 結構體，包裝 `uint64_t` 並停用隱式轉換，確保 `Emit` 時的型別安全 。


* [ ] **實作共享取消狀態**：建立或整合現有的 `CancellationTokenSource`。確保其內部使用引用計數（如 `shared_ptr`），使 Token 的生命週期獨立於 `EventScope` 。


* [ ] **開發基礎 `EventScope**`：
* 實作 RAII 解構子，呼叫 `cancel()` 並執行 `Unsubscribe` 。


* 實作同步的 `Subscribe` 介面。


* [ ] **實作同步 `EventBus::Emit**`：建立基礎的 `std::unordered_map<std::type_index, std::vector<Handler>>` 儲存結構。

#### 第二階段：定向發送優化

* [ ] **實作 `TargetedEvent` Concept**：使用 C++20 `concept` 偵測事件類型是否具備 `GetTargetID()` 函式 。


* [ ] **升級儲存結構**：在 `EventBus` 中加入二級 Map：`Map<EventType, Map<SubjectID, Vector<Handler>>>` 。


* [ ] **整合自動路由**：修改 `Emit` 邏輯，使其能根據 `concept` 自動選擇廣播或定向派發路徑 。



#### 第三階段：非同步任務整合

* [ ] **開發 `Task::WhenAll**`：在你的 Task 系統中實作聚合器，能夠收集多個 `Task<void>` 並在它們全部完成時發出訊號 。


* [ ] **實作 `EventBus::PublishAsync**`：
* 確保參數是傳值（By Value） 。


* 收集所有非同步監聽器的 Task，並透過 `WhenAll` 回傳一個聚合後的 Task 。




* [ ] **實作安全非同步包裝 (`safeHandler`)**：在 `EventScope::SubscribeAsync` 中實作 Lambda 捕獲 Token（而非捕獲 `this`），並在執行前檢查 `IsCancellationRequested` 。



#### 第四階段：強健性與測試

* [ ] **撰寫 UAF 壓力測試**：建立一個模擬場景，發送 Async 事件後立即銷毀發送者/接收者，驗證系統不會崩潰 。


* [ ] **遞迴深度檢測**：在 `Emit` 中加入 thread_local 計數器，防止事件循環觸發導致堆疊溢位 。
