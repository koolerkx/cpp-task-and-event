/**
 * @file EventBus.cpp
 * @brief Implementation of EventBus and EventHandle.
 */

#include "EventBus.hpp"

void EventHandle::Unsubscribe() {
  if (auto bus = bus_.lock()) {
    bus->Unsubscribe(event_type_, handler_id_);
  }
}

void EventBus::Unsubscribe(std::type_index event_type, uint64_t handler_id) {
  std::unique_lock<std::mutex> lock(handlers_mutex_);
  auto event_it = event_handlers_.find(event_type);
  if (event_it != event_handlers_.end()) {
    event_it->second.erase(handler_id);
    if (event_it->second.empty()) {
      event_handlers_.erase(event_it);
    }
  }
}
