#pragma once

#include <coroutine>

#include "Task.hpp"


// Primary template for TaskAwaiter<T> - returns the task's result
template <typename T>
struct TaskAwaiter {
  std::shared_ptr<Task<T>> task;
  ThreadPool& pool;

  bool await_ready() {
    return task->IsDone();
  }

  void await_suspend(std::coroutine_handle<> awaiting_coro) {
    auto resumption = std::make_shared<Task<void>>([awaiting_coro]() { awaiting_coro.resume(); });

    task->Then(resumption);

    if (task->IsDone()) {
      resumption->OnPredecessorFinished(pool);
    } else {
      task->TrySchedule(pool);
    }
  }

  T await_resume() {
    return task->GetResult();
  }
};


// Specialization for TaskAwaiter<void> - maintains existing behavior
template <>
struct TaskAwaiter<void> {
  std::shared_ptr<Task<void>> task;
  ThreadPool& pool;

  bool await_ready() {
    return task->IsDone();
  }

  void await_suspend(std::coroutine_handle<> awaiting_coro) {
    auto resumption = std::make_shared<Task<void>>([awaiting_coro]() { awaiting_coro.resume(); });

    task->Then(resumption);

    if (task->IsDone()) {
      resumption->OnPredecessorFinished(pool);
    } else {
      task->TrySchedule(pool);
    }
  }

  void await_resume() {
  }
};
