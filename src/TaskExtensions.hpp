#pragma once

#include <chrono>
#include <memory>

#include "CancellationToken.hpp"
#include "Task.hpp"
#include "TimeoutGuard.hpp"

template <typename T>
std::shared_ptr<Task<T>> WithCancellation(std::function<T()> work, CancellationTokenPtr token) {
  return std::make_shared<Task<T>>([work = std::move(work), token]() -> T {
    token->ThrowIfCancelled();
    return work();
  });
}

template <>
inline std::shared_ptr<Task<void>> WithCancellation(std::function<void()> work, CancellationTokenPtr token) {
  return std::make_shared<Task<void>>([work = std::move(work), token]() {
    token->ThrowIfCancelled();
    work();
  });
}

template <typename T, typename Rep, typename Period>
std::shared_ptr<Task<T>> WithTimeout(
  std::function<T()> work, std::chrono::duration<Rep, Period> timeout, CancellationTokenPtr* out_token = nullptr) {
  auto token = MakeCancellationToken();
  if (out_token) {
    *out_token = token;
  }

  auto task = std::make_shared<Task<T>>([work = std::move(work), token, timeout]() -> T {
    TimeoutGuard guard(token, timeout);
    token->ThrowIfCancelled();
    return work();
  });

  return task;
}

template <typename Rep, typename Period>
std::shared_ptr<Task<void>> WithTimeout(
  std::function<void()> work, std::chrono::duration<Rep, Period> timeout, CancellationTokenPtr* out_token = nullptr) {
  auto token = MakeCancellationToken();
  if (out_token) {
    *out_token = token;
  }

  auto task = std::make_shared<Task<void>>([work = std::move(work), token, timeout]() {
    TimeoutGuard guard(token, timeout);
    token->ThrowIfCancelled();
    work();
  });

  return task;
}

template <typename T>
std::shared_ptr<Task<T>> WithPollingCancellation(std::function<T(CancellationTokenPtr)> work, CancellationTokenPtr token) {
  return std::make_shared<Task<T>>([work = std::move(work), token]() -> T { return work(token); });
}

template <>
inline std::shared_ptr<Task<void>> WithPollingCancellation(std::function<void(CancellationTokenPtr)> work, CancellationTokenPtr token) {
  return std::make_shared<Task<void>>([work = std::move(work), token]() { work(token); });
}
