/**
 * @file CollisionFilterDemo.cpp
 * @brief Demonstrates targeted event dispatch with collision filtering.
 * @details Shows separation of concerns: EventBus handles routing,
 *          Physics System handles emission filtering,
 *          Components handle reception filtering.
 */

#include <array>
#include <cassert>
#include <iostream>
#include <memory>
#include <vector>

#include "EventBus.hpp"
#include "Events.hpp"
#include "SubjectID.hpp"
#include "ThreadPool.hpp"

namespace CollisionFilterDemo {

class CollisionMatrix {
 public:
  void SetFilter(EntityCategory a, EntityCategory b, bool enabled) {
    size_t idx_a = static_cast<size_t>(a);
    size_t idx_b = static_cast<size_t>(b);
    matrix_[idx_a][idx_b] = enabled;
  }

  bool ShouldCollide(EntityCategory a, EntityCategory b) const {
    size_t idx_a = static_cast<size_t>(a);
    size_t idx_b = static_cast<size_t>(b);
    return matrix_[idx_a][idx_b];
  }

 private:
  static constexpr size_t kCategoryCount = static_cast<size_t>(EntityCategory::COUNT);
  std::array<std::array<bool, kCategoryCount>, kCategoryCount> matrix_{};
};

// Physics System - Responsible for filtering at emission
class PhysicsSystem {
 public:
  PhysicsSystem(std::shared_ptr<EventBus> bus, const CollisionMatrix& matrix) : bus_(bus), collision_matrix_(matrix) {
  }

  // Filter at source: only emit when collision matrix allows
  void EmitCollision(uint64_t entity_a, uint64_t entity_b, EntityCategory cat_a, EntityCategory cat_b, float force) {
    if (!collision_matrix_.ShouldCollide(cat_a, cat_b)) {
      return;  // Don't emit at all
    }

    // Emit to entity_a
    bus_->EmitTargeted(
      CollisionEvent{.entity_a_id = entity_a, .entity_b_id = entity_b, .category_a = cat_a, .category_b = cat_b, .force = force},
      SubjectID(entity_a));

    // Emit to entity_b (reversed)
    bus_->EmitTargeted(
      CollisionEvent{.entity_a_id = entity_b, .entity_b_id = entity_a, .category_a = cat_b, .category_b = cat_a, .force = force},
      SubjectID(entity_b));
  }

 private:
  std::shared_ptr<EventBus> bus_;
  const CollisionMatrix& collision_matrix_;
};

// Component - Simple subscription, no filter (filtering done at source)
class PlayerCollisionComponent {
 public:
  PlayerCollisionComponent(std::shared_ptr<EventBus> bus, uint64_t entity_id) : bus_(bus), entity_id_(entity_id), collision_count_(0) {
    // Simple subscription - no filter parameter!
    auto handle = bus_->SubscribeTargeted<CollisionEvent>(SubjectID(entity_id), [this](const CollisionEvent& event) {
      collision_count_++;
      std::cout << "  Player " << entity_id_ << " collided with entity " << event.entity_b_id
                << " (category: " << static_cast<int>(event.category_b) << ", force: " << event.force << ")\n";
    });

    handles_.push_back(std::move(handle));
  }

  int GetCollisionCount() const {
    return collision_count_;
  }

 private:
  std::shared_ptr<EventBus> bus_;
  uint64_t entity_id_;
  int collision_count_;
  std::vector<EventHandle> handles_;
};

// Component with state-based filtering (filter at sink)
class InvinciblePlayerComponent {
 public:
  InvinciblePlayerComponent(std::shared_ptr<EventBus> bus, uint64_t entity_id)
      : bus_(bus), entity_id_(entity_id), collision_count_(0), is_invincible_(false) {
    auto handle = bus_->SubscribeTargeted<CollisionEvent>(SubjectID(entity_id), [this](const CollisionEvent& event) {
      // Filter at sink: check internal state
      if (is_invincible_) {
        std::cout << "  Player " << entity_id_ << " is invincible, ignoring collision\n";
        return;
      }

      collision_count_++;
      std::cout << "  Player " << entity_id_ << " took damage from collision\n";
    });

    handles_.push_back(std::move(handle));
  }

  void SetInvincible(bool invincible) {
    is_invincible_ = invincible;
  }

  int GetCollisionCount() const {
    return collision_count_;
  }

 private:
  std::shared_ptr<EventBus> bus_;
  uint64_t entity_id_;
  int collision_count_;
  bool is_invincible_;
  std::vector<EventHandle> handles_;
};

void TestTargetedDispatch() {
  std::cout << "\nTest: Targeted Dispatch (No Filter)\n";

  ThreadPool pool(4);
  auto bus = std::make_shared<EventBus>(pool);

  int player1_count = 0;
  int player2_count = 0;

  auto handle1 = bus->SubscribeTargeted<CollisionEvent>(SubjectID(1), [&player1_count](const CollisionEvent&) { player1_count++; });

  auto handle2 = bus->SubscribeTargeted<CollisionEvent>(SubjectID(2), [&player2_count](const CollisionEvent&) { player2_count++; });

  bus->EmitTargeted(
    CollisionEvent{
      .entity_a_id = 1, .entity_b_id = 100, .category_a = EntityCategory::Player, .category_b = EntityCategory::Wall, .force = 10.0f},
    SubjectID(1));

  bus->EmitTargeted(
    CollisionEvent{
      .entity_a_id = 1, .entity_b_id = 101, .category_a = EntityCategory::Player, .category_b = EntityCategory::Enemy, .force = 12.0f},
    SubjectID(1));

  bus->EmitTargeted(
    CollisionEvent{
      .entity_a_id = 2, .entity_b_id = 200, .category_a = EntityCategory::Player, .category_b = EntityCategory::Wall, .force = 15.0f},
    SubjectID(2));

  std::cout << "  Player 1 collisions: " << player1_count << " (expected: 2)\n";
  std::cout << "  Player 2 collisions: " << player2_count << " (expected: 1)\n";
  assert(player1_count == 2);
  assert(player2_count == 1);
  std::cout << "  PASS\n";
}

void TestSourceFiltering() {
  std::cout << "\nTest: Source Filtering (Physics System)\n";

  ThreadPool pool(4);
  auto bus = std::make_shared<EventBus>(pool);

  // Setup collision matrix: Player only collides with Wall
  CollisionMatrix matrix;
  matrix.SetFilter(EntityCategory::Player, EntityCategory::Wall, true);
  matrix.SetFilter(EntityCategory::Player, EntityCategory::Enemy, false);
  matrix.SetFilter(EntityCategory::Player, EntityCategory::Projectile, false);

  PhysicsSystem physics(bus, matrix);
  PlayerCollisionComponent player(bus, 1);

  // Physics System filters at source
  physics.EmitCollision(1, 100, EntityCategory::Player, EntityCategory::Wall, 10.0f);       // Should emit
  physics.EmitCollision(1, 200, EntityCategory::Player, EntityCategory::Enemy, 15.0f);      // Filtered (no emit)
  physics.EmitCollision(1, 300, EntityCategory::Player, EntityCategory::Wall, 20.0f);       // Should emit
  physics.EmitCollision(1, 400, EntityCategory::Player, EntityCategory::Projectile, 5.0f);  // Filtered (no emit)

  std::cout << "  Collision count: " << player.GetCollisionCount() << " (expected: 2)\n";
  assert(player.GetCollisionCount() == 2);
  std::cout << "  PASS - Physics System filtered at source\n";
}

void TestSinkFiltering() {
  std::cout << "\nTest: Sink Filtering (Component State)\n";

  ThreadPool pool(4);
  auto bus = std::make_shared<EventBus>(pool);

  InvinciblePlayerComponent player(bus, 1);

  // Normal state: take damage
  bus->EmitTargeted(
    CollisionEvent{
      .entity_a_id = 1, .entity_b_id = 100, .category_a = EntityCategory::Player, .category_b = EntityCategory::Enemy, .force = 10.0f},
    SubjectID(1));

  std::cout << "  Count after hit 1: " << player.GetCollisionCount() << " (expected: 1)\n";
  assert(player.GetCollisionCount() == 1);

  // Become invincible
  player.SetInvincible(true);

  // Invincible: ignore damage
  bus->EmitTargeted(
    CollisionEvent{
      .entity_a_id = 1, .entity_b_id = 101, .category_a = EntityCategory::Player, .category_b = EntityCategory::Enemy, .force = 15.0f},
    SubjectID(1));

  std::cout << "  Count after hit 2 (invincible): " << player.GetCollisionCount() << " (expected: 1, no change)\n";
  assert(player.GetCollisionCount() == 1);

  // Back to normal
  player.SetInvincible(false);

  bus->EmitTargeted(
    CollisionEvent{
      .entity_a_id = 1, .entity_b_id = 102, .category_a = EntityCategory::Player, .category_b = EntityCategory::Enemy, .force = 20.0f},
    SubjectID(1));

  std::cout << "  Count after hit 3 (normal): " << player.GetCollisionCount() << " (expected: 2)\n";
  assert(player.GetCollisionCount() == 2);
  std::cout << "  PASS - Component filtered at sink based on state\n";
}

void TestPerformanceComparison() {
  std::cout << "\nTest: Performance Comparison (Broadcast vs Targeted)\n";

  ThreadPool pool(4);
  auto bus = std::make_shared<EventBus>(pool);

  CollisionMatrix matrix;
  matrix.SetFilter(EntityCategory::Player, EntityCategory::Wall, true);

  PhysicsSystem physics(bus, matrix);

  std::vector<std::unique_ptr<PlayerCollisionComponent>> entities;
  for (uint64_t i = 0; i < 1000; ++i) {
    entities.push_back(std::make_unique<PlayerCollisionComponent>(bus, i));
  }

  // Physics System emits to BOTH entities in collision
  // Collision 1: entity 10 vs 20 → 2 emissions (to 10 and to 20)
  // Collision 2: entity 50 vs 60 → 2 emissions (to 50 and to 60)
  physics.EmitCollision(10, 20, EntityCategory::Player, EntityCategory::Wall, 5.0f);
  physics.EmitCollision(50, 60, EntityCategory::Player, EntityCategory::Wall, 8.0f);

  int total_collisions = 0;
  for (const auto& entity : entities) {
    total_collisions += entity->GetCollisionCount();
  }

  std::cout << "  Total collisions: " << total_collisions << " (expected: 4, 2 collisions × 2 entities each)\n";
  assert(total_collisions == 4);
  std::cout << "  PASS - Only targeted entities received events (250x improvement over broadcast)\n";
}

void TestUnsubscribeTargeted() {
  std::cout << "\nTest: Unsubscribe Targeted\n";

  ThreadPool pool(4);
  auto bus = std::make_shared<EventBus>(pool);

  int count = 0;

  {
    auto handle = bus->SubscribeTargeted<CollisionEvent>(SubjectID(1), [&count](const CollisionEvent&) { count++; });

    bus->EmitTargeted(
      CollisionEvent{
        .entity_a_id = 1, .entity_b_id = 100, .category_a = EntityCategory::Player, .category_b = EntityCategory::Wall, .force = 10.0f},
      SubjectID(1));

    std::cout << "  Count before unsubscribe: " << count << " (expected: 1)\n";
    assert(count == 1);
  }  // handle destroyed, auto unsubscribe

  bus->EmitTargeted(
    CollisionEvent{
      .entity_a_id = 1, .entity_b_id = 101, .category_a = EntityCategory::Player, .category_b = EntityCategory::Wall, .force = 12.0f},
    SubjectID(1));

  std::cout << "  Count after unsubscribe: " << count << " (expected: 1)\n";
  assert(count == 1);
  std::cout << "  PASS\n";
}

void TestEmptyTarget() {
  std::cout << "\nTest: Empty Target (No Handlers)\n";

  ThreadPool pool(4);
  auto bus = std::make_shared<EventBus>(pool);

  bus->EmitTargeted(
    CollisionEvent{
      .entity_a_id = 999, .entity_b_id = 1000, .category_a = EntityCategory::Player, .category_b = EntityCategory::Wall, .force = 10.0f},
    SubjectID(999));

  std::cout << "  PASS - No crash when emitting to non-existent target\n";
}

void RunAll() {
  std::cout << "\n=== Collision Filtering Tests (Simplified API) ===\n";
  TestTargetedDispatch();
  TestSourceFiltering();
  TestSinkFiltering();
  TestPerformanceComparison();
  TestUnsubscribeTargeted();
  TestEmptyTarget();
  std::cout << "\nAll Collision Filtering tests passed!\n";
  std::cout << "\nKey Design Principles:\n";
  std::cout << "  - EventBus: Simple routing (O(1) targeted dispatch)\n";
  std::cout << "  - Physics System: Filter at source (collision matrix)\n";
  std::cout << "  - Components: Filter at sink (state-based logic)\n";
  std::cout << "  - Result: Clean separation of concerns!\n";
}

}  // namespace CollisionFilterDemo
