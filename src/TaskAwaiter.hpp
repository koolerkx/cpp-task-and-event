#pragma once

#include <coroutine>

#include "Task.hpp"


struct TaskAwaiter {
  std::shared_ptr<Task> task;
  ThreadPool& pool;

  bool await_ready() {
    return task->IsDone();
  }

  void await_suspend(std::coroutine_handle<> awaiting_coro) {
    auto resumption = std::make_shared<Task>([awaiting_coro]() { awaiting_coro.resume(); });

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
