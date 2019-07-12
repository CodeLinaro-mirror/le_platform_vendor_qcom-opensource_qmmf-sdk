/*
 * Copyright (c) 2018, 2019, The Linux Foundation. All rights reserved.
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

#define LOG_TAG "RecorderFakeCamera"

#include <thread>
#include <algorithm>
#include <chrono>
#include <fcntl.h>
#include <math.h>
#include <sys/mman.h>
#include <sys/mman.h>
#include <fstream>

#include "qmmf_fake_camera.h"
#include "recorder/src/service/qmmf_recorder_utils.h"

namespace qmmf {

namespace recorder {

std::vector<int32_t> FakeCamera::supported_fps_ = {
  15,
  30,
  60
};

const std::string FakeCamera::kCameraBufferPath = "/data/misc/qmmf/";
const uint32_t FakeCamera::kBufferWaitTimeout = 4000000000; // 4 sec

FakeCamera::FakeCamera()
    : camera_id_(-1),
      stream_id_(0),
      stream_id_count_(10),
      result_cb_(nullptr),
      error_cb_(nullptr),
      streamon_(false),
      abort_(false),
      buffer_producer_impl_(nullptr),
      stream_thread_(nullptr),
      frame_number_(0),
      buffer_data_(nullptr),
      buffer_count_(0) {

  camera_start_params_ = {};
  mem_pool_params_ = {};

  BufferProducerImpl<FakeCamera> *producer_impl;
  producer_impl = new BufferProducerImpl<FakeCamera>(this);
  buffer_producer_impl_ = producer_impl;

  // Add AE_LOCK tag to the metadata as it required due to AE state checks.
  // Without it the camera is crashing.
  uint8_t fwk_ae_lock = ANDROID_CONTROL_AE_LOCK_OFF;
  metadata_.update(ANDROID_CONTROL_AE_LOCK, &fwk_ae_lock, 1);
}

FakeCamera::~FakeCamera() {

  if (nullptr != buffer_data_) {
    delete[] buffer_data_;
    buffer_data_ = nullptr;
  }
}

void FakeCamera::SetFlushCb(FlushCb &cb) {}

status_t FakeCamera::OpenCamera(const uint32_t camera_id,
                                const CameraStartParam &param,
                                const ResultCb &cb,
                                const ErrorCb &errcb) {

  camera_id_ = camera_id;
  camera_start_params_ = param;
  result_cb_ = cb;
  error_cb_ = errcb;

  QMMF_INFO("%s: FakeCamera(%u) Opened Successfully!", __func__, camera_id_);
  return NO_ERROR;
}

status_t FakeCamera::CloseCamera(const uint32_t camera_id) {

  assert(camera_id_ == camera_id);

  QMMF_INFO("%s: FakeCamera(%u) Closed Successfully!", __func__, camera_id_);
  return NO_ERROR;
}

status_t FakeCamera::WaitAecToConverge(const uint32_t timeout) {

  return NO_ERROR;
}

status_t FakeCamera::SetUpCapture(const SnapshotParam& param,
                                  const uint32_t num_images) {
  return NO_ERROR;
}

status_t FakeCamera::CaptureImage(const std::vector<CameraMetadata> &meta,
                                  const StreamSnapshotCb& cb) {

  return NO_ERROR;
}

status_t FakeCamera::ConfigImageCapture(const ImageConfigParam &config) {

  return NO_ERROR;
}

status_t FakeCamera::CancelCaptureImage() {

  return NO_ERROR;
}

status_t FakeCamera::CreateStream(const StreamParam& param,
                                  const VideoExtraParam& extra_param) {

  int32_t ret = NO_ERROR;
  std::string ext;

  stream_param_ = param;
  stream_id_ = stream_id_count_++;
  frame_number_ = 0;

  QMMF_INFO("%s: width = %d, height = %d, format = %d", __func__,
      param.width, param.height, param.format);

  mem_pool_params_.width = stream_param_.width;
  mem_pool_params_.height = stream_param_.height;
  mem_pool_params_.format = Common::FromQmmfToHalFormat(param.format);

  if (mem_pool_params_.format== HAL_PIXEL_FORMAT_RAW10) {
    ext = ".raw10";
  } else if (mem_pool_params_.format == HAL_PIXEL_FORMAT_RAW12) {
    ext = ".raw12";
  } else if (mem_pool_params_.format == HAL_PIXEL_FORMAT_RAW8) {
    ext = ".raw8";
  } else {
    ext = ".yuv";
  }

  mem_pool_params_.max_buffer_count = 10;
  mem_pool_params_.alloc_flags =  IMemAllocUsage::kSwWriteOften |
                                    IMemAllocUsage::kSwReadOften |
                                    IMemAllocUsage::kHwFb |
                                    IMemAllocUsage::kVideoEncoder;

  mem_pool_params_.max_size = 0;

  ret = mem_pool_.Initialize(mem_pool_params_);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s Failed to initialize Memory pool: %d", __func__, ret);
    return ret;
  }

  std::string file_path(kCameraBufferPath);
  file_path += "camera_buffer_";
  file_path += std::to_string(param.width) + "x";
  file_path += std::to_string(param.height);
  file_path += ext;

  std::ifstream file(file_path, std::ios::binary | std::ios::ate);

  if (file.is_open()) {
    std::streampos size;
    size = file.tellg();
    buffer_data_ = new char [size];
    file.seekg (0, std::ios::beg);
    file.read (buffer_data_, size);
    file.close();
  } else {
    QMMF_ERROR("%s: %s file not found!", __func__, file_path.c_str());
    return -1;
  }

  if (stream_thread_ == nullptr) {
    stream_thread_ = new std::thread(StreamThreadLoop, this);
    if (stream_thread_ == nullptr) {
      QMMF_ERROR("%s: Unable to create thread\n", __func__);
      return -1;

    }
    abort_ = false;
  } else {
    QMMF_ERROR("%s: Stream already created!\n", __func__);
    return -1;
  }

  return ret;
}

status_t FakeCamera::DeleteStream(const uint32_t track_id) {

  abort_ = true;
  if (stream_thread_ != nullptr) {
    stream_thread_->join();
    delete(stream_thread_);
    stream_thread_ = nullptr;
  }
  std::unique_lock<std::mutex> lock(buffer_lock_);
  std::chrono::nanoseconds wait_time(kBufferWaitTimeout);

  if (buffer_count_ > 0) {
    auto ret = wait_for_buffer_.WaitFor(lock, wait_time);
    if (ret != 0) {
      QMMF_ERROR("%s: Wait for buffer return timed out. Pending buffers: %d",
                 __func__, buffer_count_);
      return TIMED_OUT;
    }
  }

  UnMapBufs();
  mem_pool_.Delete();
  mem_pool_params_ = {};

  if (nullptr != buffer_data_) {
    delete[] buffer_data_;
    buffer_data_ = nullptr;
  }
  return NO_ERROR;
}

status_t FakeCamera::AddConsumer(const uint32_t& track_id,
                                 sp<IBufferConsumer>& consumer) {

  assert(buffer_producer_impl_.get() != nullptr);
  buffer_producer_impl_->AddConsumer(consumer);
  consumer->SetProducerHandle(buffer_producer_impl_);
  return NO_ERROR;
}

status_t FakeCamera::RemoveConsumer(const uint32_t& track_id,
                                    sp<IBufferConsumer>& consumer) {
  assert(consumer.get() != nullptr);
  assert(buffer_producer_impl_.get() != nullptr);

  buffer_producer_impl_->RemoveConsumer(consumer);
  QMMF_DEBUG("%s: Consumer(%p) has been removed (%p)."
      "Total number of consumer = %d",  __func__, consumer.get()
      , this, buffer_producer_impl_->GetNumConsumer());

  return NO_ERROR;
}

status_t FakeCamera::StartStream(const uint32_t track_id) {

  streamon_ = true;
  return NO_ERROR;
}

status_t FakeCamera::StopStream(const uint32_t track_id) {

  streamon_ = false;
  return NO_ERROR;
}

status_t FakeCamera::SetCameraParam(const CameraMetadata &meta) {

  metadata_.append(meta);
  return NO_ERROR;
}

status_t FakeCamera::GetCameraParam(CameraMetadata &meta) {

  meta.append(metadata_);
  return NO_ERROR;
}

status_t FakeCamera::GetDefaultCaptureParam(CameraMetadata &meta) {

  meta.append(metadata_);
  return NO_ERROR;
}

status_t FakeCamera::ReturnImageCaptureBuffer(const uint32_t camera_id,
                                              const int32_t buffer_id) {

  return NO_ERROR;
}

std::vector<int32_t>& FakeCamera::GetSupportedFps() {

  return supported_fps_;
}

void FakeCamera::NotifyBufferReturned(StreamBuffer& buffer) {

  std::lock_guard<std::mutex> lock(buffer_lock_);
  status_t ret = mem_pool_.ReturnBufferLocked(buffer);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s: Buffer return Error", __func__);
    assert(0);
  }
  buffer_count_--;
  if (buffer_count_ == 0) {
    wait_for_buffer_.Signal();
  }
}

status_t FakeCamera::GetCameraBuffer(StreamBuffer& buffer) {

  status_t ret;
  do {
    ret = mem_pool_.GetBuffer(&buffer);
  } while (ret == TIMED_OUT);

  if (ret != NO_ERROR) {
    QMMF_ERROR("%s: fail to get buffer", __func__);
    return ret;
  }

  ret = MapBuf(buffer);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s: fail to map buffer", __func__);
    return ret;
  }

  buffer_count_++;

  return NO_ERROR;
}


status_t FakeCamera::FillBuffer(StreamBuffer& buffer) {

  buffer.timestamp = (int64_t)(((float)frame_number_ /
      stream_param_.framerate) * 1000000000LL);
  buffer.stream_id = stream_id_;
  buffer.frame_number = ++frame_number_;
  buffer.camera_id = camera_id_;

  memcpy(buffer.data, buffer_data_, buffer.size);

  return NO_ERROR;
}

void *FakeCamera::StreamThreadLoop(void *userdata) {

  FakeCamera *pme = reinterpret_cast<FakeCamera *>(userdata);
  if (nullptr == pme) {
    return nullptr;
  }

  int64_t frame_duration =
    static_cast<int64_t>(1000.0f / pme->stream_param_.framerate);

  auto time_now = std::chrono::system_clock::now();
  std::chrono::system_clock::time_point time_next(time_now);
  bool run = true;

  while (run == true && pme->abort_ == false) {
    if(pme->buffer_producer_impl_->GetNumConsumer() > 0 &&
       pme->streamon_) {

      StreamBuffer buffer{};

      if(pme->GetCameraBuffer(buffer) != NO_ERROR) {
        run = false;
        break;
      }

      std::this_thread::sleep_until(time_next);
      time_now = std::chrono::system_clock::now();
      time_next = time_now + std::chrono::milliseconds(frame_duration);

      pme->FillBuffer(buffer);

      pme->buffer_producer_impl_->NotifyBuffer(buffer);

      QMMF_INFO("%s: NotifyBuffer!, timestamp = %llu\n", __func__, buffer.timestamp);
    } else {
      QMMF_DEBUG("%s: No consumer!\n",  __func__);
      std::this_thread::sleep_for(std::chrono::milliseconds(frame_duration));
    }
  }

  QMMF_INFO("%s: Exit Thread run %d\n", __func__, run);

  return nullptr;
}


status_t FakeCamera::MapBuf(StreamBuffer& buffer) {

  void *vaaddr = nullptr;

  if (buffer.fd == -1) {
    QMMF_ERROR("%s: Error Invalid FD", __func__);
    return BAD_VALUE;
  }

  QMMF_DEBUG("%s: buffer.fd=%d buffer.size=%d", __func__,
             buffer.fd, buffer.size);

  if (mapped_buffs_.count(buffer.fd) == 0) {
    vaaddr = mmap(nullptr, buffer.size, PROT_READ  | PROT_WRITE,
        MAP_SHARED, buffer.fd, 0);
    if (vaaddr == MAP_FAILED) {
        QMMF_ERROR("%s:  ION mmap failed: error(%s):(%d)", __func__
          , strerror(errno), errno);;
        return BAD_VALUE;
    }
    buffer.data = vaaddr;
    map_data_t map;
    map.addr = vaaddr;
    map.size = buffer.size;
    mapped_buffs_[buffer.fd] = map;
    buffer.data = vaaddr;
  } else {
    buffer.data = mapped_buffs_[buffer.fd].addr;
  }

  return NO_ERROR;
}

void FakeCamera::UnMapBufs() {

  for (auto iter : mapped_buffs_) {
    auto map = iter.second;
    if (map.addr) {
      QMMF_INFO("%s: Unmap addr(%p) size(%d)", __func__, map.addr, map.size);
      munmap(map.addr, map.size);
    }
  }
  mapped_buffs_.clear();
}

}; // namespace recoder

}; // namespace qmmf
