#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include "ThreadPool.hpp"

// Forward declaration for primary template
template <typename T>
class Task;

// Specialization for Task<void> must be defined first
// because Task<T> needs to reference it
template <>
class Task<void> : public std::enable_shared_from_this<Task<void>> {
 public:
  explicit Task(std::function<void()> callback) : callback_(std::move(callback)) {
  }

  void Then(std::shared_ptr<Task<void>> next) {
    successors_.push_back(next);
    next->predecessor_count_.fetch_add(1, std::memory_order_relaxed);
  }

  void TrySchedule(ThreadPool& pool) {
    if (predecessor_count_.load(std::memory_order_acquire) == 0) {
      if (!is_scheduled_.exchange(true, std::memory_order_acq_rel)) {
        Execute(pool);
      }
    }
  }

  void OnPredecessorFinished(ThreadPool& pool) {
    if (predecessor_count_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
      TrySchedule(pool);
    }
  }

  bool IsDone() const {
    return is_done_.load(std::memory_order_acquire);
  }

  Task(const Task&) = delete;
  Task& operator=(const Task&) = delete;
  Task(Task&&) = delete;
  Task& operator=(Task&&) = delete;

 private:
  void Execute(ThreadPool& pool) {
    auto self = shared_from_this();
    pool.Enqueue([self, &pool]() {
      if (self->callback_) {
        self->callback_();
      }

      self->is_done_.store(true, std::memory_order_release);

      for (auto& next : self->successors_) {
        next->OnPredecessorFinished(pool);
      }
    });
  }

  std::function<void()> callback_;
  std::atomic<int> predecessor_count_{0};
  std::vector<std::shared_ptr<Task<void>>> successors_;
  std::atomic<bool> is_done_{false};
  std::atomic<bool> is_scheduled_{false};

  template <typename U>
  friend class Task;
};

// Primary template for Task<T> with return value support
template <typename T>
class Task : public std::enable_shared_from_this<Task<T>> {
 public:
  explicit Task(std::function<T()> callback) : callback_(std::move(callback)) {
  }

  // Add a Task<T> successor
  void Then(std::shared_ptr<Task<T>> next) {
    successors_t_.push_back(next);
    next->predecessor_count_.fetch_add(1, std::memory_order_relaxed);
  }

  // Add a Task<void> successor (for coroutine resumption, etc.)
  void Then(std::shared_ptr<Task<void>> next) {
    successors_void_.push_back(next);
    next->predecessor_count_.fetch_add(1, std::memory_order_relaxed);
  }

  void TrySchedule(ThreadPool& pool) {
    if (predecessor_count_.load(std::memory_order_acquire) == 0) {
      if (!is_scheduled_.exchange(true, std::memory_order_acq_rel)) {
        Execute(pool);
      }
    }
  }

  void OnPredecessorFinished(ThreadPool& pool) {
    if (predecessor_count_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
      TrySchedule(pool);
    }
  }

  bool IsDone() const {
    return is_done_.load(std::memory_order_acquire);
  }

  T GetResult() {
    return std::move(*result_);
  }

  Task(const Task&) = delete;
  Task& operator=(const Task&) = delete;
  Task(Task&&) = delete;
  Task& operator=(Task&&) = delete;

 private:
  void Execute(ThreadPool& pool) {
    auto self = this->shared_from_this();
    pool.Enqueue([self, &pool]() {
      if (self->callback_) {
        self->result_ = self->callback_();
      }

      self->is_done_.store(true, std::memory_order_release);

      for (auto& next : self->successors_t_) {
        next->OnPredecessorFinished(pool);
      }
      for (auto& next : self->successors_void_) {
        next->OnPredecessorFinished(pool);
      }
    });
  }

  std::function<T()> callback_;
  std::atomic<int> predecessor_count_{0};
  std::vector<std::shared_ptr<Task<T>>> successors_t_;
  std::vector<std::shared_ptr<Task<void>>> successors_void_;
  std::atomic<bool> is_done_{false};
  std::atomic<bool> is_scheduled_{false};
  std::optional<T> result_;

  friend class Task<void>;
};
