#pragma once

#include <coroutine>
#include <exception>

template <typename T = void>
struct CoroTask {
  struct promise_type {
    std::exception_ptr exception_ = nullptr;

    CoroTask get_return_object() {
      return CoroTask{std::coroutine_handle<promise_type>::from_promise(*this)};
    }

    std::suspend_never initial_suspend() {
      return {};
    }
    std::suspend_always final_suspend() noexcept {
      return {};
    }

    void return_void() {
    }

    void unhandled_exception() {
      exception_ = std::current_exception();
    }
  };

  std::coroutine_handle<promise_type> handle;

  CoroTask() noexcept : handle(nullptr) {
  }
  CoroTask(std::coroutine_handle<promise_type> h) noexcept : handle(h) {
  }

  CoroTask(const CoroTask&) = delete;
  CoroTask& operator=(const CoroTask&) = delete;

  CoroTask(CoroTask&& other) noexcept : handle(other.handle) {
    other.handle = nullptr;
  }

  CoroTask& operator=(CoroTask&& other) noexcept {
    if (this != &other) {
      if (handle) {
        handle.destroy();
      }
      handle = other.handle;
      other.handle = nullptr;
    }
    return *this;
  }

  ~CoroTask() {
    if (handle) {
      handle.destroy();
    }
  }

  void rethrow_if_exception() const {
    if (handle && handle.promise().exception_) {
      std::rethrow_exception(handle.promise().exception_);
    }
  }
};
