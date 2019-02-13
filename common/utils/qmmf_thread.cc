/*
* Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
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

#include "qmmf_thread.h"

#include <cerrno>
#include <exception>

#include "common/utils/qmmf_log.h"

namespace qmmf {

int32_t ThreadHelper::Run(const std::string& name) {

  std::lock_guard<std::mutex> l(lock_);
  if (IsState(ThreadHelperState::kActive)) {
    QMMF_WARN("%s: %s thread already started!", __func__, name_.c_str());
    return -EALREADY;
  }

  if (IsState(ThreadHelperState::kToIdle)) {
    QMMF_WARN("%s: %s thread is pending exit!", __func__, name_.c_str());
    return -EBUSY;
  }

  try {
    thread_ = std::thread([this]() -> void { MainLoop(); });
  } catch (const std::exception &e) {
    QMMF_ERROR("%s: Unable to create thread %s, exception: %s !", __func__,
        name_.c_str(), e.what());
    return -EINTR;
  }

  name_ = name;
  ChangeState(ThreadHelperState::kActive);
  return 0;
}

void ThreadHelper::RequestExit() {

  std::lock_guard<std::mutex> l(lock_);
  if (IsState(ThreadHelperState::kIdle)) {
    QMMF_WARN("%s: %s thread hasn't been started!", __func__, name_.c_str());
    return;
  }

  if (IsState(ThreadHelperState::kToIdle)) {
    QMMF_WARN("%s: %s thread is pending exit!", __func__, name_.c_str());
    return;
  }

  ChangeState(ThreadHelperState::kToIdle);
  thread_.detach();
}

void ThreadHelper::RequestExitAndWait() {

  std::lock_guard<std::mutex> l(lock_);
  if (IsState(ThreadHelperState::kIdle)) {
    QMMF_WARN("%s: %s thread hasn't been started!", __func__, name_.c_str());
    return;
  }

  if (IsState(ThreadHelperState::kToIdle)) {
    QMMF_WARN("%s: %s thread is pending exit!", __func__, name_.c_str());
    WaitState(ThreadHelperState::kIdle);
    return;
  }

  ChangeState(ThreadHelperState::kToIdle);
  thread_.join();
}

void ThreadHelper::ChangeState(const ThreadHelperState& state) {

  std::lock_guard<std::mutex> l(state_lock_);
  state_ = state;
  state_updated_.Signal();
}

void ThreadHelper::WaitState(const ThreadHelperState& state) {

  std::unique_lock<std::mutex> l(state_lock_);
  state_updated_.Wait(l, [&]() { return (state_ == state); });
}

bool ThreadHelper::IsState(const ThreadHelperState& state) {

  std::lock_guard<std::mutex> l(state_lock_);
  return (state_ == state) ? true : false;
}

void ThreadHelper::MainLoop(bool active) {

  while (active) {
    active = ThreadLoop();
    active |= !IsState(ThreadHelperState::kToIdle);
  }

  ChangeState(ThreadHelperState::kIdle);
  return;
}

};  // namespace qmmf.
