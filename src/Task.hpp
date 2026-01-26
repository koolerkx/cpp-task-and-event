#pragma once

#include "ThreadPool.hpp"
#include <atomic>
#include <functional>
#include <memory>
#include <vector>

class Task : public std::enable_shared_from_this<Task> {
public:
  explicit Task(std::function<void()> callback)
      : callback_(std::move(callback)) {}

  void Then(std::shared_ptr<Task> next) {
    successors_.push_back(next);
    next->predecessor_count_.fetch_add(1, std::memory_order_relaxed);
  }

  void TrySchedule(ThreadPool &pool) {
    if (predecessor_count_.load(std::memory_order_acquire) == 0) {
      Execute(pool);
    }
  }

  void OnPredecessorFinished(ThreadPool &pool) {
    if (predecessor_count_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
      TrySchedule(pool);
    }
  }

  Task(const Task &) = delete;
  Task &operator=(const Task &) = delete;
  Task(Task &&) = delete;
  Task &operator=(Task &&) = delete;

private:
  void Execute(ThreadPool &pool) {
    auto self = shared_from_this();
    pool.Enqueue([self, &pool]() {
      if (self->callback_) {
        self->callback_();
      }

      for (auto &next : self->successors_) {
        next->OnPredecessorFinished(pool);
      }
    });
  }

  std::function<void()> callback_;
  std::atomic<int> predecessor_count_{0};
  std::vector<std::shared_ptr<Task>> successors_;
};
