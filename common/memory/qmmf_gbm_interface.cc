/*
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
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
#include <hardware/gralloc.h>
#include <fcntl.h>

#include "qmmf_gbm_interface.h"

#ifndef LOG_TAG
#define LOG_TAG "GBM Allocator"
#endif


const std::unordered_map<int32_t, int32_t> GBMUsage::usage_flag_map_ = {
  // TODO: keep this map updated with GBM enhancements
  {IMemAllocUsage::kHwCameraZsl,      0},
  {IMemAllocUsage::kPrivateAllocUbwc, GBM_BO_USAGE_UBWC_ALIGNED_QTI},
  {IMemAllocUsage::kPrivateIommUHeap, 0},
  {IMemAllocUsage::kPrivateMmHeap,    0},
  {IMemAllocUsage::kPrivateUncached,  GBM_BO_USAGE_UNCACHED_QTI},
  {IMemAllocUsage::kProtected,        GBM_BO_USAGE_PROTECTED_QTI},
  {IMemAllocUsage::kSwReadOften,      0},
  {IMemAllocUsage::kSwWriteOften,     0},
  {IMemAllocUsage::kVideoEncoder,     GBM_BO_USAGE_VIDEO_ENCODER_QTI}};

const std::unordered_map<int32_t, int32_t> GBMUsage::gralloc_usage_flag_map_ = {
  //TODO: remove when repacking to buffer_handle_t is no longer needed
  {IMemAllocUsage::kHwCameraZsl,      GRALLOC_USAGE_HW_CAMERA_ZSL},
  {IMemAllocUsage::kPrivateAllocUbwc, GRALLOC_USAGE_PRIVATE_ALLOC_UBWC},
  {IMemAllocUsage::kProtected,        GRALLOC_USAGE_PROTECTED},
  {IMemAllocUsage::kSwReadOften,      GRALLOC_USAGE_SW_READ_OFTEN},
  {IMemAllocUsage::kSwWriteOften,     GRALLOC_USAGE_SW_WRITE_OFTEN},
  {IMemAllocUsage::kHwFb,             GRALLOC_USAGE_HW_FB},
  {IMemAllocUsage::kVideoEncoder,     private_handle_t::PRIV_FLAGS_VIDEO_ENCODER}};

int32_t GBMUsage::ToLocal(int32_t common) const {
  int32_t local_usage = 0;
  for (auto &it : usage_flag_map_) {
    if (it.first & common) {
      local_usage |= it.second;
    }
  }
  QMMF_VERBOSE("%s: local_usage(0x%x)", __func__, local_usage);
  return local_usage;
}

int32_t GBMUsage::ToLocal(MemAllocFlags common) const {
  int32_t local_usage = 0;
  for (auto &it : usage_flag_map_) {
    if (it.first & common.flags) {
      local_usage |= it.second;
    }
  }
  QMMF_VERBOSE("%s: local_usage(0x%x)", __func__, local_usage);
  return local_usage;
}

int32_t GBMUsage::ToGralloc(MemAllocFlags common) const {
  int32_t gralloc;
  gralloc = 0;
  for (auto &it : gralloc_usage_flag_map_) {
    if (it.first & common.flags) {
      gralloc |= it.second;
    }
  }
  QMMF_VERBOSE("%s: gralloc(0x%x)", __func__, gralloc);
  return gralloc;
}

MemAllocFlags GBMUsage::ToCommon(int32_t local) const {
  MemAllocFlags common;
  common.flags = 0;
  for (auto &it : usage_flag_map_) {
    if (it.second & local) {
      common.flags |= it.first;
    }
  }
  QMMF_VERBOSE("%s: common.flags(0x%x)", __func__, common.flags);
  return common;
}

MemAllocFlags GBMUsage::GrallocToCommon(int32_t gralloc) const {
  MemAllocFlags common;
  common.flags = 0;
  for (auto &it : gralloc_usage_flag_map_) {
    if (it.second & gralloc) {
      common.flags |= it.first;
    }
  }
  QMMF_VERBOSE("%s: common.flags(0x%x)", __func__, common.flags);
  return common;
}

int32_t GBMUsage::LocalToGralloc(int32_t local) const {
  MemAllocFlags common = ToCommon(local);
  QMMF_VERBOSE("%s: gralloc(0x%x)", __func__, ToGralloc(common));
  return (ToGralloc(common));
}

int32_t GBMUsage::GrallocToLocal(int32_t gralloc) const {
  MemAllocFlags common = GrallocToCommon(gralloc);
  QMMF_VERBOSE("%s: local(0x%x)", __func__, ToLocal(common));
  return (ToLocal(common));
}

const std::unordered_map<uint32_t, uint32_t> GBMBuffer::to_gbm_ = {
  // TODO: keep this map updated with GBM enhancements
  {HAL_PIXEL_FORMAT_BGRA_8888,               GBM_FORMAT_BGRA8888},
  {HAL_PIXEL_FORMAT_RGB_565,                 GBM_FORMAT_RGB565},
  {HAL_PIXEL_FORMAT_RGB_888,                 GBM_FORMAT_RGB888},
  {HAL_PIXEL_FORMAT_RGBA_1010102,            GBM_FORMAT_RGBA1010102},
  {HAL_PIXEL_FORMAT_RGBA_8888,               GBM_FORMAT_RGBA8888},
  {HAL_PIXEL_FORMAT_RGBX_8888,               GBM_FORMAT_RGBX8888},

  {HAL_PIXEL_FORMAT_BLOB,                    GBM_FORMAT_BLOB},
  {HAL_PIXEL_FORMAT_RAW10,                   GBM_FORMAT_RAW10},
  {HAL_PIXEL_FORMAT_RAW16,                   GBM_FORMAT_RAW16},
  {HAL_PIXEL_FORMAT_RAW8,                    0},
  {HAL_PIXEL_FORMAT_RAW12,                   0},

  {HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED,  GBM_FORMAT_IMPLEMENTATION_DEFINED},

  {HAL_PIXEL_FORMAT_NV12_ENCODEABLE,         GBM_FORMAT_NV12_ENCODEABLE},
  {HAL_PIXEL_FORMAT_YCbCr_420_SP_VENUS,      GBM_FORMAT_YCbCr_420_SP_VENUS},
  {HAL_PIXEL_FORMAT_YCbCr_420_SP_VENUS_UBWC, GBM_FORMAT_YCbCr_420_SP_VENUS_UBWC},

  {HAL_PIXEL_FORMAT_YCbCr_420_888,           GBM_FORMAT_YCbCr_420_888},
  {HAL_PIXEL_FORMAT_YCbCr_422_SP,            GBM_FORMAT_YCbCr_422_SP},
  {HAL_PIXEL_FORMAT_YCbCr_422_I,             0},
  {HAL_PIXEL_FORMAT_YCrCb_420_SP,            0},
  {HAL_PIXEL_FORMAT_YV12,                    0},
  {HAL_PIXEL_FORMAT_YCbCr_422_888,           0},

  {HAL_PIXEL_FORMAT_NV21_ZSL,                GBM_FORMAT_NV21_ZSL},
};

const std::unordered_map<int32_t, int32_t> GBMBuffer::from_gbm_ = {
  // TODO: keep this map updated with GBM enhancements
  {GBM_FORMAT_BGRA8888,                 HAL_PIXEL_FORMAT_BGRA_8888},
  {GBM_FORMAT_RGB565,                   HAL_PIXEL_FORMAT_RGB_565},
  {GBM_FORMAT_RGB888,                   HAL_PIXEL_FORMAT_RGB_888},
  {GBM_FORMAT_RGBA1010102,              HAL_PIXEL_FORMAT_RGBA_1010102},
  {GBM_FORMAT_RGBA8888,                 HAL_PIXEL_FORMAT_RGBA_8888},
  {GBM_FORMAT_RGBX8888,                 HAL_PIXEL_FORMAT_RGBX_8888},

  {GBM_FORMAT_BLOB,                     HAL_PIXEL_FORMAT_BLOB},
  {GBM_FORMAT_RAW10,                    HAL_PIXEL_FORMAT_RAW10},
  {GBM_FORMAT_RAW16,                    HAL_PIXEL_FORMAT_RAW16},

  {GBM_FORMAT_IMPLEMENTATION_DEFINED,   HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED},

  {GBM_FORMAT_NV12_ENCODEABLE,          HAL_PIXEL_FORMAT_NV12_ENCODEABLE},
  {GBM_FORMAT_YCbCr_420_SP_VENUS,       HAL_PIXEL_FORMAT_YCbCr_420_SP_VENUS},
  {GBM_FORMAT_YCbCr_420_SP_VENUS_UBWC,  HAL_PIXEL_FORMAT_YCbCr_420_SP_VENUS_UBWC},

  {GBM_FORMAT_YCbCr_422_SP,             HAL_PIXEL_FORMAT_YCbCr_422_SP},
  {GBM_FORMAT_YCbCr_420_888,            HAL_PIXEL_FORMAT_YCbCr_420_888},

  {GBM_FORMAT_NV21_ZSL,                 HAL_PIXEL_FORMAT_NV21_ZSL},
};

struct gbm_bo *GBMBuffer::GetNativeHandle() const{
  return generic_handle_;
}

void GBMBuffer::SetNativeHandle(struct gbm_bo *bo) {
  generic_handle_ = bo;
}

buffer_handle_t &GBMBuffer::RepackToGralloc()
{
  if (!gralloc_handle_) {
    private_handle_t *priv_hnd = new private_handle_t(GetFD(), GetSize(),
      (int)GBMUsage().ToGralloc(GetUsage()), 0, GetFormat(), GetWidth(),
      GetHeight());
    assert(priv_hnd != NULL);
    gralloc_handle_ = static_cast<buffer_handle_t>(priv_hnd);
  }

  return gralloc_handle_;
}

int GBMBuffer::GetUsage ()
{
  MemAllocFlags cmn = GBMUsage().ToCommon(generic_handle_->usage_flags);
  return cmn.flags;
}

int GBMBuffer::GetFD() { return gbm_bo_get_fd(generic_handle_); }

int GBMBuffer::GetFormat() {
  int format = 0;
  uint32_t gbm_format = 0;

  assert(nullptr != generic_handle_);
  gbm_format = gbm_bo_get_format(generic_handle_);
  QMMF_VERBOSE("%s: gbm_format = 0x%x", __func__, gbm_format);

  for (auto &it : from_gbm_) {
    if ((uint32_t)it.first == gbm_format) {
      format = (int)it.second;
      break;
    }
  }

  if (!format) {
    QMMF_ERROR("%s: Format not found!", __func__);
  }
  return format;
}
uint32_t GBMBuffer::GetSize() {
  uint32_t bo_size = 0;
  int ret;
  ret = gbm_perform(GBM_PERFORM_GET_BO_SIZE, generic_handle_, &bo_size);
  assert(ret == GBM_ERROR_NONE);
  return bo_size;
}
uint32_t GBMBuffer::GetWidth() { return gbm_bo_get_width(generic_handle_); }

uint32_t GBMBuffer::GetHeight() { return gbm_bo_get_height(generic_handle_); }

uint32_t GBMBuffer::GetLocalFormat (int common)
{
  uint32_t gbm_format = 0;
  for (auto &it : to_gbm_) {
    if ((int)it.first == common) {
      gbm_format = it.second;
    }
  }
  return gbm_format;
}


GBMDevice::GBMDevice() {
  gbm_fd_ = open("/dev/dri/card0", O_RDWR);
  if (gbm_fd_ < 0) {
    QMMF_WARN("%s: Falling back to /dev/ion \n", __func__);
    gbm_fd_ = open("/dev/ion", O_RDWR);
  }
  assert(gbm_fd_ >= 0);
  gbm_device_ = gbm_create_device(gbm_fd_);
  assert(gbm_device_ != nullptr);
}

GBMDevice::~GBMDevice() { gbm_device_destroy(gbm_device_); close(gbm_fd_); }

gbm_device *GBMDevice::GetDevice() const {
  return (gbm_device_);
}

MemAllocError GBMDevice::AllocBuffer(IBufferHandle& handle, int32_t width,
  int32_t height, int32_t format, MemAllocFlags usage, uint32_t *stride)
{
  handle = new GBMBuffer;
  GBMBuffer* gbm_hnd = static_cast<GBMBuffer*>(handle);
  struct gbm_bo *bo;
  uint32_t local_usage = 0;
  uint32_t gbm_format = 0;

  local_usage = GBMUsage().ToLocal(usage);
  gbm_format = gbm_hnd->GetLocalFormat(format);

  bo = gbm_bo_create(gbm_device_, (uint32_t)width, (uint32_t)height,
    gbm_format, local_usage);
  if (!bo) {
    QMMF_ERROR("%s: Error in Allocate\n", __func__);
    return MemAllocError::kAllocFail;
  }

  gbm_hnd->SetNativeHandle(bo);
  *stride = gbm_bo_get_stride(bo);

  return MemAllocError::kAllocOk;
}

MemAllocError GBMDevice::FreeBuffer(IBufferHandle handle) {
  GBMBuffer *b = static_cast<GBMBuffer *>(handle);
  assert(b != nullptr);
  gbm_bo_destroy(b->GetNativeHandle());
  delete handle;
  return MemAllocError::kAllocOk;
}

MemAllocError GBMDevice::Perform(const IBufferHandle& handle,
                                 AllocDeviceAction action, void* result) {
  GBMBuffer *bo = static_cast<GBMBuffer *>(handle);
  if (nullptr == bo) {
    return MemAllocError::kAllocFail;
  }
  switch (action) {
    case AllocDeviceAction::GetHeight: {
      *static_cast<int32_t*>(result) = gbm_bo_get_height(bo->GetNativeHandle());
      return MemAllocError::kAllocOk;
    }
    case AllocDeviceAction::GetStride: {
      *static_cast<int32_t*>(result) = gbm_bo_get_stride(bo->GetNativeHandle());
      return MemAllocError::kAllocOk;
    }
    case AllocDeviceAction::GetAlignedWidth: {
      uint32_t align_width;
      auto ret = gbm_perform(GBM_PERFORM_GET_BO_ALIGNED_WIDTH,
                             bo->GetNativeHandle(), &align_width);
      if(ret == GBM_ERROR_NONE) {
        *static_cast<int32_t*>(result) = align_width;
        return MemAllocError::kAllocOk;
      } else {
        QMMF_ERROR("%s: Get aligned width action failed.", __func__);
        return MemAllocError::kAllocFail;
      }
    }
    case AllocDeviceAction::GetAlignedHeight: {
      uint32_t align_height;
      auto ret = gbm_perform(GBM_PERFORM_GET_BO_ALIGNED_HEIGHT,
                             bo->GetNativeHandle(), &align_height);
      if(ret == GBM_ERROR_NONE) {
        *static_cast<int32_t*>(result) = align_height;
        return MemAllocError::kAllocOk;
      } else {
        QMMF_ERROR("%s: Get aligned height action failed.", __func__);
        return MemAllocError::kAllocFail;
      }
    }
    default:
      QMMF_ERROR("%s: Unrecognized action to perform.", __func__);
      return MemAllocError::kAllocFail;
  }

}
