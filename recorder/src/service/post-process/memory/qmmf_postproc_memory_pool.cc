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

#define LOG_TAG "RecorderPostProcMemPool"

#include <dlfcn.h>
#include <hardware/hardware.h>

#include "interface/qmmf_postproc.h"
#include "../factory/qmmf_postproc_factory.h"

#include "qmmf_postproc_memory_pool.h"

namespace qmmf {

namespace recorder {

MemPool::MemPool()
    : alloc_device_interface_(nullptr),
      buffers_allocated_(0),
      pending_buffer_count_(0),
      params_({}),
      abort_(false),
      signal_buffer_return_(false) {

  QMMF_INFO("%s: Enter", __func__);
  QMMF_INFO("%s: Exit (%p)", __func__, this);
}

MemPool::~MemPool() {
  QMMF_INFO("%s: Enter", __func__);
}

int32_t MemPool::Initialize(const MemPoolParams &params) {
  status_t ret = NO_ERROR;

  params_ = params;

  alloc_device_interface_ = AllocDeviceFactory::CreateAllocDevice();

  // Allocate mem alloc slots.
  if (params_.max_buffer_count > 0) {
    mem_alloc_slots_ = new IBufferHandle[params_.max_buffer_count];
    if (mem_alloc_slots_ == nullptr) {
      QMMF_ERROR("%s: Unable to allocate buffer handles!", __func__);
      ret = NO_MEMORY;
      goto FAIL;
    }
  } else {
    mem_alloc_slots_ = nullptr;
  }

  return ret;

FAIL:
  if (nullptr != alloc_device_interface_) {
    AllocDeviceFactory::DestroyAllocDevice(alloc_device_interface_);
    alloc_device_interface_ = nullptr;
  }
  return ret;
}

status_t MemPool::Delete() {
  QMMF_INFO("%s: Enter", __func__);

  if (!mem_alloc_buffers_.empty()) {
    for (auto& it : mem_alloc_buffers_) {
      FreeHWMemBuffer(it.first);
    }
    mem_alloc_buffers_.clear();
  }

  if (mem_alloc_slots_) {
    delete[] mem_alloc_slots_;
    mem_alloc_slots_ = nullptr;
  }

  if (alloc_device_interface_) {
    AllocDeviceFactory::DestroyAllocDevice(alloc_device_interface_);
    alloc_device_interface_ = nullptr;
  }

  buffers_allocated_ = 0;
  pending_buffer_count_ = 0;
  signal_buffer_return_ = false;
  QMMF_INFO("%s: Exit (%p)", __func__, this);

  return NO_ERROR;
}

status_t MemPool::ReturnBufferLocked(const StreamBuffer &buffer) {
  if (pending_buffer_count_ == 0) {
    QMMF_ERROR("%s: Not expecting any buffers!", __func__);
    return INVALID_OPERATION;
  }
  std::lock_guard<std::mutex> lock(buffer_lock_);

  if (mem_alloc_buffers_.count(buffer.handle) == 0) {
    QMMF_ERROR("%s: Buffer %p returned that wasn't allocated by this node",
        __func__, buffer.handle);
    return BAD_VALUE;
  }

  mem_alloc_buffers_[buffer.handle] = true;
  pending_buffer_count_--;

  if (signal_buffer_return_ == true && pending_buffer_count_ == 0) {
    wait_for_return_.Signal();
    signal_buffer_return_ = false;
  }

  wait_for_buffer_.Signal();
  return NO_ERROR;
}

status_t MemPool::GetBuffer(StreamBuffer* buffer) {
  std::unique_lock<std::mutex> lock(buffer_lock_);
  std::chrono::nanoseconds wait_time(kBufferWaitTimeout);

  buffer->fd = -1;

  if (mem_alloc_slots_ == nullptr) {
    QMMF_ERROR("%s: Error alloc slots!", __func__);
    return NO_ERROR;
  }

  if (pending_buffer_count_ == params_.max_buffer_count) {
    QMMF_VERBOSE("%s: Already retrieved maximum buffers (%d), waiting"
        " on a free one",  __func__, params_.max_buffer_count);

    auto ret = wait_for_buffer_.WaitFor(lock, wait_time);
    if (abort_) {
      QMMF_ERROR("%s: Wait for output buffer aborted", __func__);
      return BAD_VALUE;
    } else if (ret != 0) {
      QMMF_ERROR("%s: Wait for output buffer return timed out",
                 __func__);
      return TIMED_OUT;
    }
  }
  auto ret = GetBufferLocked(buffer);
  if (NO_ERROR != ret) {
    QMMF_ERROR("%s: Failed to retrieve output buffer", __func__);
    return ret;
  }
  return NO_ERROR;
}

status_t MemPool::WaitUntilBufferReturned() {
  QMMF_VERBOSE("%s: E", __func__);

  std::unique_lock<std::mutex> lock(buffer_lock_);
  std::chrono::nanoseconds wait_time(kReturnWaitTimeout);

  signal_buffer_return_ = true;
  while (pending_buffer_count_) {
    QMMF_VERBOSE("%s: Wait for %d pending buffers to be returned",
        __func__, pending_buffer_count_);

    auto ret = wait_for_return_.WaitFor(lock, wait_time);
    if (ret != 0) {
      QMMF_ERROR("%s: Wait for pending buffer returns timed out", __func__);
      return TIMED_OUT;
    }
  }

  QMMF_VERBOSE("%s: X", __func__);
  return NO_ERROR;
}

status_t MemPool::Abort() {
  std::unique_lock<std::mutex> lock(buffer_lock_);
  abort_ = true;
  wait_for_buffer_.Signal();
  return NO_ERROR;
}

status_t MemPool::GetBufferLocked(StreamBuffer* buffer) {

  status_t ret = NO_ERROR;
  int32_t idx = -1;
  IBufferHandle handle = nullptr;

  //Only pre-allocate buffers in case no valid streamBuffer
  //is passed as an argument.
  if (nullptr != buffer) {
    for (auto& it : mem_alloc_buffers_) {
      if (it.second) {
        handle = it.first;
        it.second = false;
        break;
      }
    }
  }
  // Find the slot of the available alloc buffer.
  if (nullptr != handle) {
    for (uint32_t i = 0; i < buffers_allocated_; i++) {
      if (mem_alloc_slots_[i] == handle) {
        idx = i;
        break;
      }
    }
  } else if ((nullptr == handle) &&
             (buffers_allocated_ < params_.max_buffer_count)) {
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
    pending_buffer_count_++;
  }
  return ret;
}

status_t MemPool::PopulateMetaInfo(CameraBufferMetaData &info,

                                   IBufferHandle &handle) {

  int alignedW, alignedH;
  auto ret = alloc_device_interface_->Perform(handle,
    IAllocDevice::AllocDeviceAction::GetStride, static_cast<void*>(&alignedW));
  if (MemAllocError::kAllocOk != ret) {
    QMMF_ERROR("%s: Unable to query stride&scanline: %d\n", __func__, ret);
    return NO_MEMORY;
  }
  ret = alloc_device_interface_->Perform(handle,
      IAllocDevice::AllocDeviceAction::GetHeight, static_cast<void*>(&alignedH));
    if (MemAllocError::kAllocOk != ret) {
      QMMF_ERROR("%s: Unable to query stride&scanline: %d\n", __func__, ret);
      return NO_MEMORY;
    }

  switch (handle->GetFormat()) {
    case HAL_PIXEL_FORMAT_BLOB:
      info.format = BufferFormat::kBLOB;
      info.num_planes = 1;
      info.plane_info[0].width = params_.width;
      info.plane_info[0].height = params_.height;
      info.plane_info[0].stride = alignedW;
      info.plane_info[0].scanline = alignedH;
      info.plane_info[0].size = params_.max_size;
      info.plane_info[0].offset = 0;
      break;
    case HAL_PIXEL_FORMAT_YCbCr_420_SP_VENUS:
    case HAL_PIXEL_FORMAT_NV12_ENCODEABLE:
      info.format = BufferFormat::kNV12;
      info.num_planes = 2;
      info.plane_info[0].width = params_.width;
      info.plane_info[0].height = params_.height;
      info.plane_info[0].stride = alignedW;
      info.plane_info[0].scanline = alignedH;
      info.plane_info[0].size = alignedW * alignedH;
      info.plane_info[0].offset = 0;
      info.plane_info[1].width = params_.width;
      info.plane_info[1].height = params_.height/2;
      info.plane_info[1].stride = alignedW;
      info.plane_info[1].scanline = alignedH/2;
      info.plane_info[1].size = alignedW * (alignedH / 2);
      info.plane_info[1].offset = alignedW * alignedH;
      break;
    case HAL_PIXEL_FORMAT_YCbCr_420_SP_VENUS_UBWC:
      info.format = BufferFormat::kNV12UBWC;
      info.num_planes = 2;
      info.plane_info[0].width = params_.width;
      info.plane_info[0].height = params_.height;
      info.plane_info[0].stride = alignedW;
      info.plane_info[0].scanline = alignedH;
      info.plane_info[0].size = alignedW * alignedH;
      info.plane_info[0].offset = 0;
      info.plane_info[1].width = params_.width;
      info.plane_info[1].height = params_.height/2;
      info.plane_info[1].stride = alignedW;
      info.plane_info[1].scanline = alignedH/2;
      info.plane_info[1].size = alignedW * (alignedH / 2);
      info.plane_info[1].offset = alignedW * alignedH;
      break;
    case HAL_PIXEL_FORMAT_NV21_ZSL:
      info.format = BufferFormat::kNV21;
      info.num_planes = 2;
      info.plane_info[0].width = params_.width;
      info.plane_info[0].height = params_.height;
      info.plane_info[0].stride = alignedW;
      info.plane_info[0].scanline = alignedH;
      info.plane_info[0].size = alignedW * alignedH;
      info.plane_info[0].offset = 0;
      info.plane_info[1].width = params_.width;
      info.plane_info[1].height = params_.height/2;
      info.plane_info[1].stride = alignedW;
      info.plane_info[1].scanline = alignedH/2;
      info.plane_info[1].size = alignedW * (alignedH / 2);
      info.plane_info[1].offset = alignedW * alignedH;
      break;
    case HAL_PIXEL_FORMAT_YCbCr_422_888:
      info.format = BufferFormat::kNV16;
      info.num_planes = 2;
      info.plane_info[0].width = params_.width;
      info.plane_info[0].height = params_.height;
      info.plane_info[0].stride = alignedW;
      info.plane_info[0].scanline = alignedH;
      info.plane_info[0].size = alignedW * alignedH;
      info.plane_info[0].offset = 0;
      info.plane_info[1].width = params_.width;
      info.plane_info[1].height = params_.height;
      info.plane_info[1].stride = alignedW;
      info.plane_info[1].scanline = alignedH;
      info.plane_info[1].size = alignedW * alignedH;
      info.plane_info[1].offset = alignedW * alignedH;
      break;
    case HAL_PIXEL_FORMAT_RAW8:
      info.format = BufferFormat::kRAW8;
      info.num_planes = 1;
      info.plane_info[0].width = params_.width;
      info.plane_info[0].height = params_.height;
      info.plane_info[0].stride = alignedW;
      info.plane_info[0].scanline = alignedH;
      info.plane_info[0].size = alignedW * alignedH;
      info.plane_info[0].offset = 0;
      break;
    case HAL_PIXEL_FORMAT_RAW10:
      info.format = BufferFormat::kRAW10;
      info.num_planes = 1;
      info.plane_info[0].width = params_.width;
      info.plane_info[0].height = params_.height;
      info.plane_info[0].stride = alignedW;
      info.plane_info[0].scanline = alignedH;
      info.plane_info[0].size = alignedW * alignedH;
      info.plane_info[0].offset = 0;
      break;
    case HAL_PIXEL_FORMAT_RAW12:
      info.format = BufferFormat::kRAW12;
      info.num_planes = 1;
      info.plane_info[0].width = params_.width;
      info.plane_info[0].height = params_.height;
      info.plane_info[0].stride = alignedW;
      info.plane_info[0].scanline = alignedH;
      info.plane_info[0].size = alignedW * alignedH;
      info.plane_info[0].offset = 0;
      break;
    case HAL_PIXEL_FORMAT_RAW16:
      info.format = BufferFormat::kRAW16;
      info.num_planes = 1;
      info.plane_info[0].width = params_.width;
      info.plane_info[0].height = params_.height;
      info.plane_info[0].stride = alignedW;
      info.plane_info[0].scanline = alignedH;
      info.plane_info[0].size = alignedW * alignedH;
      info.plane_info[0].offset = 0;
      break;
    default:
      QMMF_ERROR("%s: Unsupported format: %d", __func__,
                 handle->GetFormat());
      return NAME_NOT_FOUND;
  }
  return NO_ERROR;
}

status_t MemPool::AllocHWMemBuffer(IBufferHandle &buf) {

  status_t ret      = NO_ERROR;
  uint32_t width    = params_.width;
  uint32_t height   = params_.height;
  int32_t  format   = params_.format;
  MemAllocFlags  usage = params_.alloc_flags;
  uint32_t max_size = params_.max_size;

  if (!width || !height) {
    width = height = 1;
  }

  uint32_t stride = 0;
  if (0 < max_size) {
    // Blob buffers are expected to get allocated with width equal to blob
    // max size and height equal to 1.
    alloc_device_interface_->AllocBuffer(buf, static_cast<int>(max_size),
                                 static_cast<int>(1), format,
                                 usage, &stride);
  } else {
    alloc_device_interface_->AllocBuffer(buf, static_cast<int>(width),
                                 static_cast<int>(height), format,
                                 usage, &stride);
  }
  if (NO_ERROR != ret) {
    QMMF_ERROR("%s: Failed to allocate buffer", __func__);
  }
  return ret;
}

status_t MemPool::FreeHWMemBuffer(IBufferHandle buf) {
  MemAllocError ret = alloc_device_interface_->FreeBuffer(buf);
  return ret == MemAllocError::kAllocOk ? NO_ERROR : BAD_VALUE;
}

}; //namespace recorder.

}; //namespace qmmf.
