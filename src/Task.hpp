#pragma once

#include <atomic>
#include <condition_variable>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

#include "ThreadPool.hpp"

// Forward declaration for primary template
template <typename T>
class Task;

class TaskBase {
 public:
  virtual ~TaskBase() = default;

  void Wait() {
    std::unique_lock<std::mutex> lock(wait_mutex_);
    wait_cv_.wait(lock, [this] {
      return is_done_.load(std::memory_order_acquire);
    });
  }

  void OnPredecessorFinished(ThreadPool& pool) {
    if (predecessor_count_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
      TrySchedule(pool);
    }
  }

  void TrySchedule(ThreadPool& pool) {
    if (predecessor_count_.load(std::memory_order_acquire) == 0) {
      if (!is_scheduled_.exchange(true, std::memory_order_acq_rel)) {
        Execute(pool);
      }
    }
  }

  bool IsDone() const {
    return is_done_.load(std::memory_order_acquire);
  }

 protected:
  void NotifyFinished() {
    is_done_.store(true, std::memory_order_release);
    wait_cv_.notify_all();
  }

  virtual void Execute(ThreadPool& pool) = 0;
  virtual void NotifySuccessors(ThreadPool& pool) = 0;

  std::atomic<int> predecessor_count_{0};
  std::atomic<bool> is_done_{false};
  std::atomic<bool> is_scheduled_{false};
  std::exception_ptr exception_ = nullptr;
  mutable std::mutex wait_mutex_;
  mutable std::condition_variable wait_cv_;
  std::vector<std::shared_ptr<Task<void>>> successors_void_;

  template <typename U>
  friend class Task;
  template <typename U>
  friend struct TaskAwaiter;
};

// Specialization for Task<void> must be defined first
// because Task<T> needs to reference it
template <>
class Task<void> : public TaskBase,
                   public std::enable_shared_from_this<Task<void>> {
 public:
  explicit Task(std::function<void()> callback)
      : callback_(std::move(callback)) {}

  void Then(std::shared_ptr<Task<void>> next) {
    successors_void_.push_back(next);
    next->predecessor_count_.fetch_add(1, std::memory_order_relaxed);
  }

  Task(const Task&) = delete;
  Task& operator=(const Task&) = delete;
  Task(Task&&) = delete;
  Task& operator=(Task&&) = delete;

 private:
  void Execute(ThreadPool& pool) override {
    auto self = shared_from_this();
    pool.Enqueue([self, &pool]() {
      try {
        if (self->callback_) {
          self->callback_();
        }
      } catch (...) {
        self->exception_ = std::current_exception();
      }
      self->NotifyFinished();
      self->NotifySuccessors(pool);
    });
  }

  void NotifySuccessors(ThreadPool& pool) override {
    for (auto& next : successors_void_) {
      next->OnPredecessorFinished(pool);
    }
  }

  std::function<void()> callback_;
};

// Primary template for Task<T> with return value support
template <typename T>
class Task : public TaskBase,
             public std::enable_shared_from_this<Task<T>> {
 public:
  explicit Task(std::function<T()> callback)
      : callback_(std::move(callback)) {}

  void Then(std::shared_ptr<Task<T>> next) {
    successors_t_.push_back(next);
    next->predecessor_count_.fetch_add(1, std::memory_order_relaxed);
  }

  void Then(std::shared_ptr<Task<void>> next) {
    successors_void_.push_back(next);
    next->predecessor_count_.fetch_add(1, std::memory_order_relaxed);
  }

  T GetResult() {
    if (exception_) {
      std::rethrow_exception(exception_);
    }
    return std::move(*result_);
  }

  Task(const Task&) = delete;
  Task& operator=(const Task&) = delete;
  Task(Task&&) = delete;
  Task& operator=(Task&&) = delete;

 private:
  void Execute(ThreadPool& pool) override {
    auto self = this->shared_from_this();
    pool.Enqueue([self, &pool]() {
      try {
        if (self->callback_) {
          self->result_ = self->callback_();
        }
      } catch (...) {
        self->exception_ = std::current_exception();
      }
      self->NotifyFinished();
      self->NotifySuccessors(pool);
    });
  }

  void NotifySuccessors(ThreadPool& pool) override {
    for (auto& next : successors_t_) {
      next->OnPredecessorFinished(pool);
    }
    for (auto& next : successors_void_) {
      next->OnPredecessorFinished(pool);
    }
  }

  std::function<T()> callback_;
  std::optional<T> result_;
  std::vector<std::shared_ptr<Task<T>>> successors_t_;

  friend class Task<void>;
};
