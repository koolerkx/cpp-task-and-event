#pragma once

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>

#include "CancellationToken.hpp"

class TimeoutGuard {
 public:
  template <typename Rep, typename Period>
  TimeoutGuard(CancellationTokenPtr token, std::chrono::duration<Rep, Period> timeout) : token_(token), stop_timer_(false) {
    timer_thread_ = std::thread([this, timeout]() {
      auto deadline = std::chrono::steady_clock::now() + timeout;

      while (!stop_timer_.load(std::memory_order_acquire)) {
        auto now = std::chrono::steady_clock::now();
        if (now >= deadline) {
          token_->Cancel();
          return;
        }

        auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now);

        auto sleep_time = std::min(remaining, std::chrono::milliseconds(10));

        std::this_thread::sleep_for(sleep_time);
      }
    });
  }

  ~TimeoutGuard() {
    stop_timer_.store(true, std::memory_order_release);
    if (timer_thread_.joinable()) {
      timer_thread_.join();
    }
  }

  TimeoutGuard(const TimeoutGuard&) = delete;
  TimeoutGuard& operator=(const TimeoutGuard&) = delete;
  TimeoutGuard(TimeoutGuard&&) = delete;
  TimeoutGuard& operator=(TimeoutGuard&&) = delete;

 private:
  CancellationTokenPtr token_;
  std::atomic<bool> stop_timer_;
  std::thread timer_thread_;
};
