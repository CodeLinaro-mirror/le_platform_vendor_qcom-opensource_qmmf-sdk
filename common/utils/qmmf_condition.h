/*
 * Copyright (c) 2017, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <cstdint>
#include <cerrno>
#include <cassert>
#include <chrono>
#include <mutex>

namespace qmmf {

#if defined(_GLIBCXX_HAS_GTHREADS) && defined(_GLIBCXX_USE_C99_STDINT_TR1)

/**
 * Custom condition implementation using platform independent
 * libstdc++ gthread and based on c++11 std::condition_variable.
 */
class QCondition {
  typedef std::chrono::system_clock system_clock_t;
  typedef std::chrono::steady_clock steady_clock_t;

 public:
  QCondition() noexcept;
  ~QCondition() noexcept;

  QCondition(const QCondition&) = delete;
  QCondition& operator=(const QCondition&) = delete;

  /**
   * Unblocks one of the threads waiting on this condition.
   */
  void Signal();

  /**
   * Unblocks all threads currently waiting on this condition.
   */
  void SignalAll();

  /**
   * Blocks the current thread until the condition is notified via Signal(),
   * SignalAll() or a spurious wakeup occurs.
   */
  void Wait(std::unique_lock<std::mutex>& lock);

  /**
   * This overload may be used to ignore spurious wakeups.
   * Blocks the current thread until the condition is notified via Signal(),
   * SignalAll().
   */
  template<typename _Predicate>
  void Wait(std::unique_lock<std::mutex>& lock, _Predicate p) {
    while (!p()) {
      Wait(lock);
    }
  }

  /**
   * Blocks the current thread until the condition is notified via Signal(),
   * SignalAll(), a spurious wakeup occurs or until specified time point has
   * been reached. Accepts either system clock or system clock.
   */
  template<typename _Clock, typename _Duration>
  int32_t WaitUntil(std::unique_lock<std::mutex>& lock,
                    const std::chrono::time_point<_Clock, _Duration>& tp) {
    return WaitUntilImpl(lock, tp);
  }

  /**
   * This overload may be used to ignore spurious wakeups.
   * Blocks the current thread until the condition is notified via Signal(),
   * SignalAll() or until specified time point has been reached. Accepts either
   * system clock or system clock.
   */
  template<typename _Clock, typename _Duration, typename _Predicate>
  int32_t WaitUntil(std::unique_lock<std::mutex>& lock,
                    const std::chrono::time_point<_Clock, _Duration>& tp,
                    _Predicate p) {
    while (!p()) {
      if (wait_until(lock, tp) == -ETIMEDOUT) {
        return p();
      }
    }
    return true;
  }

  /**
   * Blocks the current thread until the condition is notified via Signal(),
   * SignalAll(), a spurious wakeup occurs or after the specified timeout
   * duration. Uses steady clock.
   */
  template<typename _Rep, typename _Period>
  int32_t WaitFor(std::unique_lock<std::mutex>& lock,
                  const std::chrono::duration<_Rep, _Period>& timeout) {
    return WaitUntil(lock, steady_clock_t::now() + timeout);
  }

  /**
   * This overload may be used to ignore spurious wakeups.
   * Blocks the current thread until the condition is notified via Signal(),
   * SignalAll() or after the specified timeout duration. Uses steady clock.
   */
  template<typename _Rep, typename _Period, typename _Predicate>
  int32_t WaitFor(std::unique_lock<std::mutex>& lock,
                  const std::chrono::duration<_Rep, _Period>& timeout,
                  _Predicate p) {
    return WaitUntil(lock, steady_clock_t::now() + timeout, std::move(p));
  }

 private:
#ifdef __GTHREAD_COND_INIT
  __gthread_cond_t cond_ = __GTHREAD_COND_INIT;
#else
  __gthread_cond_t cond_;
#endif

  template<typename _Clock, typename _Duration>
  int32_t WaitUntilImpl(std::unique_lock<std::mutex>& lock,
                        const std::chrono::time_point<_Clock, _Duration>& tp) {
    // Sync unknown clock to system clock.
    const auto delta = tp - _Clock::now();
    const auto stp = system_clock_t::now() + delta;

    auto sec = std::chrono::time_point_cast<std::chrono::seconds>(stp);
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(stp - sec);

    __gthread_time_t timestamp = {
      static_cast<std::time_t>(sec.time_since_epoch().count()),
      static_cast<long>(ns.count())
    };

    __gthread_cond_timedwait(&cond_, lock.mutex()->native_handle(), &timestamp);

    // Check the timeout condition based on the given unknown clock.
    return (_Clock::now() < tp) ? 0 : -ETIMEDOUT;
  }
};

#endif // _GLIBCXX_HAS_GTHREADS && _GLIBCXX_USE_C99_STDINT_TR1

};  // namespace qmmf.
