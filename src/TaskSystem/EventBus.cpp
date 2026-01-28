/**
 * @file EventBus.cpp
 * @brief Implementation of EventBus and EventHandle.
 */

#include "EventBus.hpp"

#include <iostream>
#include <vector>

void EventHandle::Unsubscribe() {
  if (auto bus = bus_.lock()) {
    bus->Unsubscribe(event_name_, handler_id_);
  }
}

void EventBus::Emit(const std::string& event_name, std::any data) {
  // Take the registered handler
  std::vector<EventHandler> handlers_snapshot;  // prevent long lock holds and potential deadlocks
  {
    std::unique_lock<std::mutex> lock(handlers_mutex_);
    auto event_it = event_handlers_.find(event_name);
    if (event_it != event_handlers_.end()) {
      handlers_snapshot.reserve(event_it->second.size());
      for (const auto& [id, handler] : event_it->second) {
        handlers_snapshot.push_back(handler);  // copy assignment
      }
    }
  }

  // Execute the registered handler
  for (auto& handler : handlers_snapshot) {
    try {
      handler(data);
    } catch (const std::exception& e) {
      std::cerr << "EventBus: Handler exception in '" << event_name << "': " << e.what() << "\n";
    }
  }
}

void EventBus::EmitAsync(const std::string& event_name, std::any data) {
  // Take the registered handler
  std::vector<EventHandler> handlers_snapshot;  // prevent long lock holds and potential deadlocks
  {
    std::unique_lock<std::mutex> lock(handlers_mutex_);
    auto event_it = event_handlers_.find(event_name);
    if (event_it != event_handlers_.end()) {
      handlers_snapshot.reserve(event_it->second.size());
      for (const auto& [id, handler] : event_it->second) {
        handlers_snapshot.push_back(handler);  // copy assignment
      }
    }
  }

  // Execute the registered handler
  for (auto& handler : handlers_snapshot) {
    pool_.Enqueue([handler, data]() {
      try {
        handler(data);
      } catch (const std::exception& e) {
        std::cerr << "EventBus: Async handler exception: " << e.what() << "\n";
      }
    });
  }
}

void EventBus::EmitAsync(const std::string& event_name, std::any data, CancellationTokenPtr token) {
  if (token && token->IsCancelled()) {
    return;
  }

  // Take the registered handler
  std::vector<EventHandler> handlers_snapshot;  // prevent long lock holds and potential deadlocks
  {
    std::unique_lock<std::mutex> lock(handlers_mutex_);
    auto event_it = event_handlers_.find(event_name);
    if (event_it != event_handlers_.end()) {
      handlers_snapshot.reserve(event_it->second.size());
      for (const auto& [id, handler] : event_it->second) {
        handlers_snapshot.push_back(handler);  // copy assignment
      }
    }
  }

  // Execute the registered handler
  for (auto& handler : handlers_snapshot) {
    if (token && token->IsCancelled()) {
      break;
    }

    pool_.Enqueue([handler, data, token]() {
      if (token && token->IsCancelled()) {
        return;
      }
      try {
        handler(data);
      } catch (const std::exception& e) {
        std::cerr << "EventBus: Async handler exception: " << e.what() << "\n";
      }
    });
  }
}

EventHandle EventBus::Subscribe(const std::string& event_name, EventHandler handler) {
  uint64_t handler_id;
  {
    std::unique_lock<std::mutex> lock(handlers_mutex_);
    handler_id = next_handler_id_++;
    event_handlers_[event_name][handler_id] = std::move(handler);
  }
  return EventHandle(weak_from_this(), event_name, handler_id);
}

void EventBus::Unsubscribe(const std::string& event_name, uint64_t handler_id) {
  std::unique_lock<std::mutex> lock(handlers_mutex_);
  auto event_it = event_handlers_.find(event_name);
  if (event_it != event_handlers_.end()) {
    event_it->second.erase(handler_id);  // remove the id and handler callback from map
    if (event_it->second.empty()) {
      event_handlers_.erase(event_it);  // remove the event if no handler registered
    }
  }
}
