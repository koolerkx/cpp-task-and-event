/**
 * @file Events.hpp
 * @brief Example event definitions for demonstration.
 * @details Defines type-safe event structs using CRTP pattern.
 */
#pragma once

#include "Event.hpp"
#include <string>

struct PlayerDamagedEvent : Event<PlayerDamagedEvent> {
  static constexpr std::string_view EventName = "player.damaged";
  int player_id;
  float damage;
};

struct ItemPickedUpEvent : Event<ItemPickedUpEvent> {
  static constexpr std::string_view EventName = "item.picked_up";
  int item_id;
  std::string item_name;
};

struct SceneLoadedEvent : Event<SceneLoadedEvent> {
  static constexpr std::string_view EventName = "scene.loaded";
  std::string scene_name;
  float load_time_ms;
};
