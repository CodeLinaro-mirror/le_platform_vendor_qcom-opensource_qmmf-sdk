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

#define TAG "CameraModule"

#include <sys/mman.h>
#include <utils/Mutex.h>

#include "qmmf_camera_module.h"

namespace qmmf {

namespace recorder {

CameraModule::CameraModule(String8 name, IPostProcCameraContext* context)
    : name_(name),
      id_(-1),
      reprocess_flag_(false),
      ready_to_start_(false) {
  QMMF_INFO("%s:%s: Enter", TAG, __func__);

  reprocess_factory_ = ReprocessFactory::getInstance();

  camera_reprocess_ = reprocess_factory_->getReprocEngine(name_, context);
  assert(camera_reprocess_.get() != nullptr);



  memset(&caps_, 0x0, sizeof(ReprocCaps));
  camera_reprocess_->GetCapabilities(&caps_);

  QMMF_INFO("%s:%s: Exit (0x%p)", TAG, __func__, this);
}

CameraModule::~CameraModule() {
  QMMF_INFO("%s:%s: Enter ", TAG, __func__);

  //unmap
  for (uint32_t i = 0; i < mapped_buffs_.size(); i++) {
    if (mapped_buffs_.valueAt(i).addr) {
      auto map = mapped_buffs_.valueAt(i);
      QMMF_INFO("%s:%s: Unmap %p", TAG, __func__, map.addr);
      munmap(map.addr, map.size);
    }
  }

  mapped_buffs_.clear();

  camera_reprocess_.clear();

  QMMF_INFO("%s:%s: Exit (0x%p)", TAG, __func__, this);
}

int32_t CameraModule::Create(const int32_t stream_id,
                           const ReprocParam& input,
                           const ReprocParam& output,
                           const uint32_t frame_rate,
                           const uint32_t num_images,
                           const void* static_meta,
                           const void* context) {
  QMMF_INFO("%s:%s: Enter", TAG, __func__);

  if (ready_to_start_) {
    QMMF_ERROR("%s:%s: Failed: Already configured.", TAG, __func__);
    return BAD_VALUE;
  }

  if (reprocess_flag_) {
    QMMF_ERROR("%s:%s: Failed: Wrong state.", TAG, __func__);
    return BAD_VALUE;
  }

  id_ = camera_reprocess_->Create(stream_id, input, output,
                                  frame_rate,
                                  num_images,
                                  static_meta, context);
  assert(id_ >= 0);

  ready_to_start_ = true;

  QMMF_INFO("%s:%s: Exit reproc_ID: %d", TAG, __func__, id_);
  return id_;
}

status_t CameraModule::Start() {
  QMMF_INFO("%s:%s: Enter", TAG, __func__);
  if (!ready_to_start_) {
    return BAD_VALUE;
  }

  if (camera_reprocess_.get() != nullptr) {
    sp<IReprocessCallbacks> cb = this;
    camera_reprocess_->SetCallBacks(cb);
    camera_reprocess_->Start();
  }

  reprocess_flag_ = true;
  Run("Camera Module");


  QMMF_INFO("%s:%s: Exit", TAG, __func__);
  return NO_ERROR;
}

status_t CameraModule::Stop() {
  QMMF_INFO("%s:%s: Enter stop Id_: %d", TAG, __func__, id_);

  RequestExitAndWait();
  QMMF_INFO("%s:%s: The Lip thread is stopped Id_: %d", TAG, __func__, id_);
  ready_to_start_ = false;

  StreamBuffer buffer, b;
  memset(&b, 0x0, sizeof(b));
  b.fd = -1;

  std::lock_guard<std::mutex> lock(wait_lock_);
  auto iter = input_buffer_.begin();
  while (iter != input_buffer_.end()) {
    buffer = *iter;
    iter = input_buffer_.erase(iter);
    QMMF_INFO("%s:%s: back to client node: FD: %d", TAG, __func__, buffer.fd);
    ReprocessLibCallback(b, buffer);
  }

  if (camera_reprocess_.get() != nullptr) {
    camera_reprocess_->Stop();
  }
  reprocess_flag_ = false;

  camera_reprocess_->ClearCallBacks();
  QMMF_INFO("%s:%s: Exit stop Id_: %d", TAG, __func__, id_);
  return NO_ERROR;
}

status_t CameraModule::Delete() {
  QMMF_INFO("%s:%s: Enter ", TAG, __func__);

  //unmap
  for (uint32_t i = 0; i < mapped_buffs_.size(); i++) {
    if (mapped_buffs_.valueAt(i).addr) {
      auto map = mapped_buffs_.valueAt(i);
      QMMF_INFO("%s:%s: Unmap %p", TAG, __func__, map.addr);
      munmap(map.addr, map.size);
    }
  }

  mapped_buffs_.clear();

  if (camera_reprocess_.get() != nullptr) {
    camera_reprocess_->Delete();
  }

  reprocess_flag_ = false;
  ready_to_start_ = false;

  QMMF_INFO("%s:%s: Exit", TAG, __func__);
  return NO_ERROR;
}

void CameraModule::AddBuff(StreamBuffer in_buff) {
  std::lock_guard<std::mutex> lock(wait_lock_);
  input_buffer_.push_back(in_buff);
  wait_for_buffer_.notify_one();
}

void CameraModule::AddResult(const void* result) {
  if (camera_reprocess_.get() != nullptr) {
    camera_reprocess_->AddResult(result);
  }
}

status_t CameraModule::ReturnBuff(StreamBuffer buffer) {
  QMMF_INFO("%s:%s: StreamBuffer(%p) ts: %lld, streamId: %d fd: %d", TAG,
       __func__, buffer.handle, buffer.timestamp, buffer.stream_id, buffer.fd);

  if (camera_reprocess_.get() != nullptr) {
    camera_reprocess_->ReturnBuff(buffer);
  }
  return NO_ERROR;
}

void* CameraModule::MapBuff(StreamBuffer& buffer) {
  void *vaaddr = nullptr;

  if (buffer.fd == -1) {
    return vaaddr;
  }

  if (mapped_buffs_.indexOfKey(buffer.fd) < 0 || mapped_buffs_.isEmpty()) {
    vaaddr = mmap(nullptr, buffer.size, PROT_READ  | PROT_WRITE,
        MAP_SHARED, buffer.fd, 0);
    if (vaaddr == MAP_FAILED) {
        QMMF_ERROR("%s:%s  ION mmap failed: %s (%d)", TAG, __func__,
            strerror(errno), errno);
    }
    buffer.data = vaaddr;
    map_data_t map;
    map.addr = vaaddr;
    map.size = buffer.size;
    mapped_buffs_.add(buffer.fd, map);
  } else {
    vaaddr = mapped_buffs_.valueFor(buffer.fd).addr;
  }
  return vaaddr;
}

bool CameraModule::ThreadLoop() {
  bool status = true;

  StreamBuffer buffer;
  {
    std::unique_lock<std::mutex> lock(wait_lock_);
    std::chrono::nanoseconds wait_time(kFrameTimeout);

    while (input_buffer_.empty()) {
      auto ret = wait_for_buffer_.wait_for(lock, wait_time);
      if (ret == std::cv_status::timeout) {
        QMMF_VERBOSE("%s:%s: Wait for frame available timed out Copy",
            TAG, __func__);
        return true;
      }
    }
    auto iter = input_buffer_.begin();
    buffer = *iter;
    input_buffer_.erase(iter);
  }

  StreamBuffer b;
  memset(&b, 0x0, sizeof(b));
  GetBuffer(&b);

  b.stream_id = id_;
  b.timestamp = buffer.timestamp;
  b.frame_number = buffer.frame_number;
  b.camera_id = buffer.camera_id;
  b.flags = buffer.flags;
  b.info = buffer.info;
  b.data = MapBuff(b);

  if (reprocess_flag_ == true) {
    if (camera_reprocess_.get() != nullptr) {
      status = camera_reprocess_->Process(buffer, b);
    }
  }

  return status;
}

}; // namespace recoder

}; // namespace qmmf
