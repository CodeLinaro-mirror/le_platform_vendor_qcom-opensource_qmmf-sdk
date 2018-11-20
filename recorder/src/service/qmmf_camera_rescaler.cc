/*
* Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
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

#define LOG_TAG "RecorderRescaler"

#include <chrono>
#include <map>
#include <sys/prctl.h>

#ifndef DISABLE_RESCALER_COLORSPACE
#include "qdMetaData.h"
#endif

#include "recorder/src/service/qmmf_camera_rescaler.h"
#include "recorder/src/service/qmmf_recorder_utils.h"

#include "common/resizer-neon/qmmf_resizer_neon.h"
#include "common/resizer-c2d/qmmf_resizer_c2d.h"
#include "common/resizer-fastCV/qmmf_resizer_fastCV.h"

namespace qmmf {

namespace recorder {

using ::std::chrono::high_resolution_clock;
using ::std::chrono::microseconds;
using ::std::chrono::nanoseconds;
using ::std::chrono::time_point;
using ::std::chrono::duration_cast;

CameraRescalerBase::CameraRescalerBase()
    :CameraRescalerThread() {
  QMMF_INFO("%s: Enter", __func__);
  char prop[PROPERTY_VALUE_MAX];
  memset(prop, 0, sizeof(prop));
  property_get("persist.qmmf.rescaler.type", prop, "Neon");
  std::string name = prop;

  if (name == "Neon") {
    rescaler_ = new NEONResizer();
  } else if (name == "FastCV") {
    rescaler_ = new FastCVResizer();
  } else {
    rescaler_ = new C2DResizer();
  }

  memset(prop, 0, sizeof(prop));
  property_get("persist.qipcam.rescaler.perf", prop, "0");
  uint32_t value = (uint32_t) atoi(prop);
  print_process_time_ = (value == 1) ? true : false;

  property_get(PRESERVE_ASPECT_RATIO, prop, "1");
  value = (uint32_t) atoi(prop);
  rescaler_->aspect_ratio_preserve_ = (value == 1) ? true : false;

  rescaler_->Init();
  QMMF_INFO("%s: Exit (%p)", __func__, this);
}

CameraRescalerBase::~CameraRescalerBase() {
  QMMF_INFO("%s: Enter", __func__);
  rescaler_->DeInit();
  delete rescaler_;
  QMMF_INFO("%s: Exit (%p)", __func__, this);
}

status_t CameraRescalerBase::Validate(const uint32_t& width,
                                      const uint32_t& height,
                                      const BufferFormat& fmt) {
  if (rescaler_ == nullptr) {
    QMMF_ERROR("%s: Missing Rescaler engine!!!", __func__);
    return BAD_VALUE;
  }

  if (rescaler_->ValidateOutput(width, height, fmt) != RESIZER_STATUS_OK) {
    QMMF_ERROR("%s: Validation Error!!!", __func__);
    return BAD_VALUE;
  }

  return NO_ERROR;
}

status_t CameraRescalerBase::ReturnBufferToBufferPool(
    const StreamBuffer &buffer) {
  status_t ret = ReturnBufferLocked(buffer);
  if (NO_ERROR != ret) {
    QMMF_ERROR("%s: Failed to return buffer to memory pool", __func__);
  }
  return ret;
}

void CameraRescalerBase::FlushBufs() {
  std::unique_lock<std::mutex> lock(wait_lock_);

  StreamBuffer buffer;
  auto iter = bufs_list_.begin();
  while (iter != bufs_list_.end()) {
    iter = bufs_list_.begin();
    buffer = *iter;
    bufs_list_.erase(iter);
    QMMF_INFO("%s: back to client node: FD: %d", __func__, buffer.fd);
    ReturnBufferToProducer(buffer);
  }
}

void CameraRescalerBase::AddBuf(StreamBuffer& buffer) {
  QMMF_DEBUG("%s: Enter", __func__);
  std::unique_lock<std::mutex> lock(wait_lock_);
  bufs_list_.push_back(buffer);
  wait_.Signal();
}

bool CameraRescalerBase::ThreadLoop() {
  bool status = true;

  StreamBuffer in_buffer;
  {
    std::unique_lock<std::mutex> lock(wait_lock_);
    std::chrono::nanoseconds wait_time(kFrameTimeout);
    while (bufs_list_.empty()) {
      auto ret = wait_.WaitFor(lock, wait_time);
      if (ret != 0) {
        QMMF_DEBUG("%s: Wait for frame available timed out", __func__);
        // timeout loop again
        return true;
      }
    }
    auto iter = bufs_list_.begin();
    in_buffer = *iter;
    bufs_list_.erase(iter);
  }

  StreamBuffer out_buffer{};
  GetFreeOutputBuffer(&out_buffer);

  out_buffer.stream_id    = 0x55aa;
  out_buffer.timestamp    = in_buffer.timestamp;
  out_buffer.frame_number = in_buffer.frame_number;
  out_buffer.camera_id    = in_buffer.camera_id;
  out_buffer.flags        = in_buffer.flags;

  QMMF_DEBUG("%s: Thread map", __func__);
  auto ret = MapBuf(out_buffer);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s: fail to map in_buffer", __func__);
    ReturnBufferToBufferPool(out_buffer);
    ReturnBufferToProducer(in_buffer);
    return true;
  }

  void *vaaddr = nullptr;
  bool in_buff_map = false;
  if (in_buffer.data == nullptr) {
    vaaddr = mmap(nullptr, in_buffer.size, PROT_READ  | PROT_WRITE,
        MAP_SHARED, in_buffer.fd, 0);
    QMMF_DEBUG("%s: Thread map in buff done", __func__);
    in_buff_map = true;
    in_buffer.data = vaaddr;
  }

  time_point<high_resolution_clock>   start_time;
  if (print_process_time_) {
    start_time = high_resolution_clock::now();
  }

  if (rescaler_) {
    rescaler_->Draw(in_buffer, out_buffer);
  }

  if(print_process_time_) {
    time_point<high_resolution_clock> curr_time = high_resolution_clock::now();
    uint64_t time_diff = duration_cast<microseconds>
                             (curr_time - start_time).count();
    QMMF_INFO("%s: stream_id(%d) Full ProcessingTime=%lld",
        __func__, in_buffer.stream_id, time_diff);
  }

  if (in_buff_map) {
    munmap(in_buffer.data, in_buffer.size);
    in_buffer.data = nullptr;
  }

  ReturnBufferToProducer(in_buffer);
  NotifyBufferToClient(out_buffer);

  return status;
}

status_t CameraRescalerBase::MapBuf(StreamBuffer& buffer) {
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
        QMMF_ERROR("%s: ION mmap failed: error(%s):(%d)", __func__,
            strerror(errno), errno);
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

void CameraRescalerBase::UnMapBufs() {
  for (auto iter : mapped_buffs_) {
    auto map = iter.second;
    if (map.addr) {
      QMMF_INFO("%s: Unmap addr(%p) size(%d)", __func__,
          map.addr, map.size);
      munmap(map.addr, map.size);
    }
  }
  mapped_buffs_.clear();
}

status_t CameraRescalerBase::Configure(const std::string& json_config_data) {
  auto ret = rescaler_->Configure(json_config_data);
  if (ret != RESIZER_STATUS_OK) {
    return BAD_VALUE;
  }
  return NO_ERROR;
}

int32_t CameraRescalerThread::Run(const std::string &name) {
  int32_t res = 0;

  std::lock_guard<std::mutex> lock(lock_);
  if (running_) {
    QMMF_ERROR("%s: Thread %s already started!\n", __func__, name_.c_str());
    res = -ENOSYS;
    goto exit;
  }

  abort_ = false;
  running_ = true;
  thread_ = new std::thread(MainLoop, this);
  if (thread_ == nullptr) {
    QMMF_ERROR("%s: Unable to create thread\n", __func__);
    running_ = false;
    goto exit;
  }

  if (name.empty()) {
    // use thread id as name
    std::stringstream ss;
    ss << thread_->get_id();
    name_ = ss.str();
  } else {
    name_ = name;
  }

  QMMF_INFO("%s: Thread %s is running\n", __func__, name_.c_str());

exit:
  return res;
}

void CameraRescalerThread::RequestExit() {
  std::lock_guard<std::mutex> lock(lock_);
  if (thread_ == nullptr || running_ == false) {
    QMMF_ERROR("%s: Thread %s is not running\n", __func__, name_.c_str());
    return;
  }

  abort_ = true;
}

void CameraRescalerThread::RequestExitAndWait() {
  std::lock_guard<std::mutex> lock(lock_);
  if (thread_ == nullptr) {
    QMMF_ERROR("%s: Thread %s is stopped\n", __func__, name_.c_str());
    return;
  }

  abort_ = true;
  thread_->join();
  delete(thread_);
  thread_ = nullptr;
}

void *CameraRescalerThread::MainLoop(void *userdata) {
  prctl(PR_SET_NAME, "CamRescaleMain", 0, 0, 0);

  CameraRescalerThread *pme = reinterpret_cast<CameraRescalerThread *>(userdata);
  if (nullptr == pme) {
    pme->running_ = false;
    return nullptr;
  }

  bool run = true;
  while (pme->abort_ == false && run == true) {
    run = pme->ThreadLoop();
  }

  pme->running_ = false;
  return nullptr;
}

bool CameraRescalerThread::ExitPending() {
  std::lock_guard<std::mutex> lock(lock_);
  return (abort_ == true && running_ == true)  ? false : true;
}

#define RESCALER_BUFFERS_CNT (13)

CameraRescalerMemPool::CameraRescalerMemPool()
    : alloc_device_interface_(nullptr),
      mem_alloc_slots_(nullptr),
      buffers_allocated_(0),
      pending_buffer_count_(0),
      buffer_cnt_(RESCALER_BUFFERS_CNT) {
  QMMF_INFO("%s: Enter", __func__);
  QMMF_INFO("%s: Exit (%p)", __func__, this);
}

CameraRescalerMemPool::~CameraRescalerMemPool() {
  QMMF_INFO("%s: Enter", __func__);

  if (!mem_alloc_buffers_.empty()) {
    for (auto const& it : mem_alloc_buffers_) {
      FreeHWMemBuffer(it.first);
    }
    mem_alloc_buffers_.clear();
  }
  if (nullptr != mem_alloc_slots_) {
    delete[] mem_alloc_slots_;
  }
  if (nullptr != alloc_device_interface_) {
    delete alloc_device_interface_;
    alloc_device_interface_ = nullptr;
  }
  QMMF_INFO("%s: Exit (%p)", __func__, this);
}

int32_t CameraRescalerMemPool::Initialize(uint32_t width,
                                          uint32_t height,
                                          int32_t  format) {
  status_t ret = NO_ERROR;

  init_params_.width = width;
  init_params_.height = height;
  init_params_.format = format;

  alloc_device_interface_ = AllocDeviceFactory::CreateAllocDevice();
  if (nullptr == alloc_device_interface_) {
    QMMF_ERROR("%s: Could not create alloc device", __func__);
    goto FAIL;
  }

  // Allocate mem alloc slots.
  if (buffer_cnt_ > 0) {
    mem_alloc_slots_ = new IBufferHandle[buffer_cnt_];
    if (mem_alloc_slots_ == nullptr) {
      QMMF_ERROR("%s: Unable to allocate buffer handles!", __func__);
      ret = NO_MEMORY;
      goto FAIL;
    }
  } else {
    mem_alloc_slots_ = nullptr;
  }

  return NO_ERROR;

FAIL:
  if (nullptr != alloc_device_interface_) {
    delete alloc_device_interface_;
    alloc_device_interface_ = nullptr;
  }
  return -1;
}

status_t CameraRescalerMemPool::ReturnBufferLocked(const StreamBuffer &buffer) {

  if (pending_buffer_count_ == 0) {
    QMMF_ERROR("%s: Not expecting any buffers!", __func__);
    return INVALID_OPERATION;
  }

  std::lock_guard<std::mutex> lock(buffer_lock_);

  if (mem_alloc_buffers_.find(buffer.handle) == mem_alloc_buffers_.end()) {
    QMMF_ERROR("%s: Buffer %p returned that wasn't allocated by this node",
        __func__, buffer.handle);
    return BAD_VALUE;
  }

  mem_alloc_buffers_.at(buffer.handle) = true;
  --pending_buffer_count_;

  wait_for_buffer_.Signal();
  return NO_ERROR;
}

status_t CameraRescalerMemPool::GetFreeOutputBuffer(StreamBuffer* buffer) {

  status_t ret = NO_ERROR;
  std::unique_lock<std::mutex> lock(buffer_lock_);

  buffer->fd = -1;

  if (mem_alloc_slots_ == nullptr) {
    QMMF_ERROR("%s: Error mem alloc slots!", __func__);
    return NO_ERROR;
  }

  while (pending_buffer_count_ == buffer_cnt_) {
    QMMF_VERBOSE("%s: Already retrieved maximum buffers (%d), waiting"
        " on a free one",  __func__, buffer_cnt_);

    std::chrono::nanoseconds wait_time(kBufferWaitTimeout);
    auto status = wait_for_buffer_.WaitFor(lock, wait_time);
    if (status != 0) {
      QMMF_ERROR("%s: Wait for output buffer return timed out", __func__);
    }
  }
  ret = GetBufferLocked(buffer);
  if (NO_ERROR != ret) {
    QMMF_ERROR("%s: Failed to retrieve output buffer", __func__);
  }

  return ret;
}

status_t CameraRescalerMemPool::GetBufferLocked(StreamBuffer* buffer) {
  status_t ret = NO_ERROR;
  int32_t idx = -1;
  IBufferHandle handle = nullptr;

  //Only pre-allocate buffers in case no valid stream Buffer
  //is passed as an argument.
  if (nullptr != buffer) {
    for (auto& it : mem_alloc_buffers_) {
      if(it.second == true) {
        handle = it.first;
        it.second = false;
        break;
      }
    }
  }
  // Find the slot of the available buffer.
  if (nullptr != handle) {
    for (uint32_t i = 0; i < buffers_allocated_; i++) {
      if (mem_alloc_slots_[i] == handle) {
        idx = i;
        break;
      }
    }
  } else if ((nullptr == handle) &&
             (buffers_allocated_ < buffer_cnt_)) {
    ret = AllocHWMemBuffer(handle);
    if (NO_ERROR != ret) {
      return ret;
    }
    idx = buffers_allocated_;
    mem_alloc_slots_[idx] = handle;
    mem_alloc_buffers_.emplace(mem_alloc_slots_[idx], (nullptr == buffer));
    buffers_allocated_++;
  }

  if ((nullptr == handle) || (0 > idx)) {
    QMMF_ERROR("%s: Unable to allocate or find a free buffer!",
               __func__);
    return INVALID_OPERATION;
  }

  if (nullptr != buffer) {
    buffer->handle = mem_alloc_slots_[idx];
    ret = PopulateMetaInfo(buffer->info, buffer->handle);
    if (NO_ERROR != ret) {
      QMMF_ERROR("%s: Failed to populate buffer meta info", __func__);
      return ret;
    }
    buffer->fd = buffer->handle->GetFD();
    buffer->size = buffer->handle->GetSize();
    ++pending_buffer_count_;
  }

  return ret;
}

status_t CameraRescalerMemPool::PopulateMetaInfo(CameraBufferMetaData &info,
                                   IBufferHandle &handle) {

  int alignedW, alignedH;
  auto ret = alloc_device_interface_->Perform(handle,
      IAllocDevice::AllocDeviceAction::GetAlignedHeight,
      static_cast<void*>(&alignedH));
  if (MemAllocError::kAllocOk != ret) {
    QMMF_ERROR("%s: Unable to query stride&scanline: %d\n", __func__, ret);
    return BAD_VALUE;
  }

  ret = alloc_device_interface_->Perform(handle,
      IAllocDevice::AllocDeviceAction::GetAlignedWidth,
      static_cast<void*>(&alignedW));
  if (MemAllocError::kAllocOk != ret) {
    QMMF_ERROR("%s: Unable to query stride&scanline: %d\n", __func__, ret);
    return BAD_VALUE;
  }

  switch (handle->GetFormat()) {
    case HAL_PIXEL_FORMAT_YCbCr_420_888:
    case HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED:
    case HAL_PIXEL_FORMAT_YCbCr_420_SP_VENUS:
    case HAL_PIXEL_FORMAT_NV12_ENCODEABLE:
      info.format = BufferFormat::kNV12;
      info.num_planes = 2;
      info.plane_info[0].width = init_params_.width;
      info.plane_info[0].height = init_params_.height;
      info.plane_info[0].stride = alignedW;
      info.plane_info[0].scanline = alignedH;
      info.plane_info[0].size = alignedW * alignedH;
      info.plane_info[0].offset = 0;
      info.plane_info[1].width = init_params_.width;
      info.plane_info[1].height = init_params_.height/2;
      info.plane_info[1].stride = alignedW;
      info.plane_info[1].scanline = alignedH/2;
      info.plane_info[1].size = alignedW * (alignedH / 2);
      info.plane_info[1].offset = alignedW * alignedH;
      break;
    case HAL_PIXEL_FORMAT_YCbCr_420_SP_VENUS_UBWC:
      info.format = BufferFormat::kNV12UBWC;
      info.num_planes = 2;
      info.plane_info[0].width = init_params_.width;
      info.plane_info[0].height = init_params_.height;
      info.plane_info[0].stride = alignedW;
      info.plane_info[0].scanline = alignedH;
      info.plane_info[0].size = alignedW * alignedH;
      info.plane_info[0].offset = 0;
      info.plane_info[1].width = init_params_.width;
      info.plane_info[1].height = init_params_.height/2;
      info.plane_info[1].stride = alignedW;
      info.plane_info[1].scanline = alignedH/2;
      info.plane_info[1].size = alignedW * (alignedH / 2);
      info.plane_info[1].offset = alignedW * alignedH;
      break;
    case HAL_PIXEL_FORMAT_RGB_888:
      info.format = BufferFormat::kRGB;
      info.num_planes = 1;
      info.plane_info[0].width = init_params_.width;
      info.plane_info[0].height = init_params_.height;
      info.plane_info[0].stride = alignedW;
      info.plane_info[0].scanline = alignedH;
      break;
    case HAL_PIXEL_FORMAT_NV21_ZSL:
      info.format = BufferFormat::kNV21;
      info.num_planes = 2;
      info.plane_info[0].width = init_params_.width;
      info.plane_info[0].height = init_params_.height;
      info.plane_info[0].stride = alignedW;
      info.plane_info[0].scanline = alignedH;
      info.plane_info[0].size = alignedW * alignedH;
      info.plane_info[0].offset = 0;
      info.plane_info[1].width = init_params_.width;
      info.plane_info[1].height = init_params_.height/2;
      info.plane_info[1].stride = alignedW;
      info.plane_info[1].scanline = alignedH/2;
      info.plane_info[1].size = alignedW * (alignedH / 2);
      info.plane_info[1].offset = alignedW * alignedH;
      break;
    case HAL_PIXEL_FORMAT_YCbCr_422_888:
      info.format = BufferFormat::kNV16;
      info.num_planes = 2;
      info.plane_info[0].width = init_params_.width;
      info.plane_info[0].height = init_params_.height;
      info.plane_info[0].stride = alignedW;
      info.plane_info[0].scanline = alignedH;
      info.plane_info[0].size = alignedW * alignedH;
      info.plane_info[0].offset = 0;
      info.plane_info[1].width = init_params_.width;
      info.plane_info[1].height = init_params_.height;
      info.plane_info[1].stride = alignedW;
      info.plane_info[1].scanline = alignedH;
      info.plane_info[1].size = alignedW * alignedH;
      info.plane_info[1].offset = alignedW * alignedH;
      break;
    default:
      QMMF_ERROR("%s: Unsupported format: %d", __func__,
                 handle->GetFormat());
      return NAME_NOT_FOUND;
  }

  return NO_ERROR;
}

status_t CameraRescalerMemPool::AllocHWMemBuffer(IBufferHandle &buf) {

  uint32_t width    = init_params_.width;
  uint32_t height   = init_params_.height;
  int32_t  format   = init_params_.format;
  MemAllocFlags usage(0);

  usage.flags |= IMemAllocUsage::kSwWriteOften | IMemAllocUsage::kSwReadOften;
  usage.flags |= IMemAllocUsage::kHwFb | IMemAllocUsage::kVideoEncoder;

  char prop[PROPERTY_VALUE_MAX];
  memset(prop, 0, sizeof(prop));
  property_get("persist.qmmf.ubwcstream.enable", prop, "0");
  if (atoi(prop) == 1) {
    // Handle UBWC aligned Buffer
    usage.flags |= IMemAllocUsage::kPrivateAllocUbwc;
  }

  if (!width || !height) {
    width = height = 1;
  }

  uint32_t stride = 0;

  MemAllocError ret = alloc_device_interface_->AllocBuffer(buf,
    static_cast<int>(width), static_cast<int>(height), format, usage, &stride);
  if (MemAllocError::kAllocOk != ret) {
    QMMF_ERROR("%s: Failed to allocate alloc buffer", __func__);
    return NO_MEMORY;
  }
#ifndef DISABLE_RESCALER_COLORSPACE
  int32_t color_space = ITU_R_601_FR;
  private_handle_t *priv_handle = const_cast<private_handle_t *>(
      static_cast<const private_handle_t *>(*buf));

  auto status = setMetaData(priv_handle, UPDATE_COLOR_SPACE,
                    static_cast<void *>(&color_space));

  if (NO_ERROR != ret) {
    QMMF_ERROR("%s  setMetaData Failed: (%d)", __func__, status);
    return status;
  }
#endif
  return NO_ERROR;
}

status_t CameraRescalerMemPool::FreeHWMemBuffer(IBufferHandle buf) {
  MemAllocError ret = alloc_device_interface_->FreeBuffer(buf);
  return ret == MemAllocError::kAllocOk ? NO_ERROR : BAD_VALUE;
}

CameraRescaler::CameraRescaler()
  : CameraRescalerBase(),
    is_stop_(false), frc_(nullptr) {
  QMMF_INFO("%s: Enter", __func__);

  BufferConsumerImpl<CameraRescaler> *impl;
  impl = new BufferConsumerImpl<CameraRescaler>(this);
  buffer_consumer_impl_ = impl;

  BufferProducerImpl<CameraRescaler> *producer_impl;
  producer_impl = new BufferProducerImpl<CameraRescaler>(this);
  buffer_producer_impl_ = producer_impl;

  QMMF_INFO("%s: Exit (0x%p)", __func__, this);
}

CameraRescaler::~CameraRescaler() {
  QMMF_INFO("%s: Enter", __func__);
  buffer_producer_impl_.clear();
  buffer_consumer_impl_.clear();
  QMMF_INFO("%s: Exit (0x%p)", __func__, this);
}


status_t CameraRescaler::AddConsumer(const sp<IBufferConsumer>& consumer) {
  if (consumer.get() == nullptr) {
    QMMF_ERROR("%s: Input consumer is nullptr", __func__);
    return BAD_VALUE;
  }

  buffer_producer_impl_->AddConsumer(consumer);
  consumer->SetProducerHandle(buffer_producer_impl_);
  QMMF_VERBOSE("%s: Consumer(%p) has been added.", __func__,
      consumer.get());

  return NO_ERROR;
}

uint32_t CameraRescaler::GetNumConsumer() {
  return buffer_producer_impl_->GetNumConsumer();
}

status_t CameraRescaler::RemoveConsumer(sp<IBufferConsumer>& consumer) {
  if(buffer_producer_impl_->GetNumConsumer() == 0) {
    QMMF_ERROR("%s: There are no connected consumers!", __func__);
    return INVALID_OPERATION;
  }
  buffer_producer_impl_->RemoveConsumer(consumer);

  return NO_ERROR;
}

sp<IBufferConsumer>& CameraRescaler::GetCopyConsumerIntf() {
  return buffer_consumer_impl_;
}

bool CameraRescaler::IsFrameSkip() {
  if (frc_.get() != nullptr) {
    if (frc_->FrameSkip()) {
      QMMF_DEBUG("%s: Skip frame", __func__);
      return true;
    }
  }
  return false;
}

void CameraRescaler::OnFrameAvailable(StreamBuffer& buffer) {
  QMMF_DEBUG("%s: Camera %u: Frame %d is available",
      __func__, buffer.camera_id, buffer.frame_number);

  if (IsStop() || IsFrameSkip()) {
    QMMF_DEBUG("%s: IsStop", __func__);
    ReturnBufferToProducer(buffer);
    return;
  }

  AddBuf(buffer);
}

void CameraRescaler::NotifyBufferReturned(const StreamBuffer& buffer) {
  QMMF_DEBUG("%s: Stream buffer(handle %p) returned", __func__,
      buffer.handle);
  ReturnBufferToBufferPool(buffer);
}

status_t CameraRescaler::NotifyBufferToClient(StreamBuffer &buffer) {
  status_t ret = NO_ERROR;
  std::lock_guard<std::mutex> lock(consumer_lock_);
  if(buffer_producer_impl_->GetNumConsumer() > 0) {
    buffer_producer_impl_->NotifyBuffer(buffer);
  } else {
    QMMF_DEBUG("%s: No consumer, simply return buffer back to"
        " memory pool!",  __func__);
    ret = ReturnBufferToBufferPool(buffer);
  }
  return ret;
}

status_t CameraRescaler::ReturnBufferToProducer(StreamBuffer &buffer) {
  QMMF_DEBUG("%s: Enter", __func__);
  const sp<IBufferConsumer> consumer = buffer_consumer_impl_;
  if (consumer.get() == nullptr) {
    QMMF_ERROR("%s: Failed to retrieve buffer consumer for camera(%d)!",
               __func__, buffer.camera_id);
    return BAD_VALUE;
  }
  consumer->GetProducerHandle()->NotifyBufferReturned(buffer);

  QMMF_DEBUG("%s: Exit", __func__);
  return NO_ERROR;
}

status_t CameraRescaler::Start() {
  QMMF_INFO("%s: Enter", __func__);
  std::lock_guard<std::mutex> lock(stop_lock_);
  is_stop_ = false;
  QMMF_INFO("%s: Start thread", __func__);
  Run(LOG_TAG);
  QMMF_INFO("%s: Exit", __func__);
  return NO_ERROR;
}

status_t CameraRescaler::Stop() {
  QMMF_INFO("%s: Enter", __func__);
  {
    std::lock_guard<std::mutex> lock(stop_lock_);
    is_stop_ = true;
  }
  QMMF_INFO("%s: Stop thread", __func__);
  RequestExitAndWait();
  QMMF_INFO("%s: Stop thread done", __func__);
  UnMapBufs();
  QMMF_INFO("%s: unmap done", __func__);
  QMMF_INFO("%s: Exit", __func__);
  return NO_ERROR;
}

bool CameraRescaler::IsStop() {
  std::lock_guard<std::mutex> lock(stop_lock_);
  return is_stop_;
}

status_t CameraRescaler::Init(const uint32_t& width, const uint32_t& height,
                              const BufferFormat& fmt,
                              const float& in_fps, const float& out_fps) {

  if ((width == 0) || (height == 0)) {
    QMMF_ERROR("%s: Invalid dimensions: %ux%u!", __func__, width, height);
    return BAD_VALUE;
  }

  if (Validate(width, height, fmt) != NO_ERROR) {
    QMMF_ERROR("%s: Error: Unsupported in params.!!!", __func__);
    return BAD_VALUE;
  }

  char prop[PROPERTY_VALUE_MAX];
  property_get("persist.qmmf.ubwcstream.enable", prop, "0");
  bool is_ubwc_stream_enabled = atoi(prop);

  auto format = Common::FromQmmfToHalFormat(fmt);
  if (is_ubwc_stream_enabled && fmt == BufferFormat::kNV12) {
    format = HAL_PIXEL_FORMAT_YCbCr_420_SP_VENUS_UBWC;
  }

  // Frame Rate Controller creation
  frc_ = std::make_shared<FrameRateController>(in_fps, out_fps,
                                               "RescalerFrameController");
  if (!frc_.get()) {
    QMMF_ERROR("%s: Can't Instantiate FrameRateController!!", __func__);
    return NO_MEMORY;
  }

  auto ret = Initialize(width, height, format);
  return ret;
}

}; //namespace recorder

}; //namespace qmmf
