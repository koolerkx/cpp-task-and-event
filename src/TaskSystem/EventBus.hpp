/**
 * @file EventBus.hpp
 * @brief Memory-safe, thread-safe event bus with async support and cancellation.
 * @details Provides pub-sub event system with RAII handles, stable ID-based subscriptions,
 *          and integration with ThreadPool for async event dispatch.
 *
 * Key Features:
 * - Sync/Async emit with optional cancellation
 * - RAII EventHandle for automatic cleanup
 * - Thread-safe handler storage with unique_lock + snapshot pattern
 * - ID-based subscriptions (unsubscribe is O(1) and doesn't invalidate other handles)
 *
 * @code{.cpp}
 * ThreadPool pool(4);
 * auto bus = std::make_shared<EventBus>(pool);
 *
 * auto handle = bus->Subscribe("player.damaged", [](std::any data) {
 *     int damage = std::any_cast<int>(data);
 *     std::cout << "Player took " << damage << " damage\n";
 * });
 *
 * bus->Emit("player.damaged", 25);  // Sync
 * bus->EmitAsync("player.damaged", 30);  // Async
 *
 * handle.Unsubscribe();  // Manual cleanup (auto on destruction)
 * @endcode
 */

#pragma once

#include <any>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include "CancellationToken.hpp"
#include "Event.hpp"
#include "ThreadPool.hpp"

class EventBus;

using EventHandler = std::function<void(std::any)>;

class EventHandle {
 public:
  EventHandle(std::weak_ptr<EventBus> bus, std::string event_name, uint64_t handler_id)
      : bus_(std::move(bus)), event_name_(std::move(event_name)), handler_id_(handler_id) {
  }

  void Unsubscribe();

  EventHandle(EventHandle&&) = default;
  EventHandle& operator=(EventHandle&&) = default;

  EventHandle(const EventHandle&) = delete;
  EventHandle& operator=(const EventHandle&) = delete;

 private:
  std::weak_ptr<EventBus> bus_;
  std::string event_name_;
  uint64_t handler_id_;
};

class EventBus : public std::enable_shared_from_this<EventBus> {
 public:
  explicit EventBus(ThreadPool& pool) : pool_(pool) {
  }

  void Emit(const std::string& event_name, std::any data);

  void EmitAsync(const std::string& event_name, std::any data);

  void EmitAsync(const std::string& event_name, std::any data, CancellationTokenPtr token);

  EventHandle Subscribe(const std::string& event_name, EventHandler handler);

  template<typename E>
  requires EventType<E>
  void Emit(const E& event) {
    std::string name(E::GetEventName());
    Emit(name, std::any(event));
  }

  template<typename E>
  requires EventType<E>
  EventHandle Subscribe(std::function<void(const E&)> handler) {
    std::string name(E::GetEventName());

    auto wrapper = [handler, name](std::any data) {
      try {
        const E& event = std::any_cast<const E&>(data);
        handler(event);
      } catch (const std::bad_any_cast& e) {
        std::cerr << "EventBus: Type mismatch for event '" << name << "': " << e.what() << "\n";
      }
    };
    return Subscribe(name, std::move(wrapper));
  }

  template<typename E>
  requires EventType<E>
  void EmitAsync(const E& event) {
    std::string name(E::GetEventName());
    EmitAsync(name, std::any(event));
  }

  template<typename E>
  requires EventType<E>
  void EmitAsync(const E& event, CancellationTokenPtr token) {
    std::string name(E::GetEventName());
    EmitAsync(name, std::any(event), token);
  }

  EventBus(const EventBus&) = delete;
  EventBus& operator=(const EventBus&) = delete;

 private:
  friend class EventHandle;
  void Unsubscribe(const std::string& event_name, uint64_t handler_id);

  ThreadPool& pool_;
  uint64_t next_handler_id_{0};
  std::mutex handlers_mutex_;
  std::unordered_map<std::string, std::unordered_map<uint64_t, EventHandler>> event_handlers_;
};
