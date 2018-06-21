/*
* Copyright (c) 2015 - 2018, The Linux Foundation. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are
* met:
*  * Redistributions of source code must retain the above copyright
*    notice, this list of conditions and the following disclaimer.
*  * Redistributions in binary form must reproduce the above
*    copyright notice, this list of conditions and the following
*    disclaimer in the documentation and/or other materials provided
*    with the distribution.
*  * Neither the name of The Linux Foundation nor the names of its
*    contributors may be used to endorse or promote products derived
*    from this software without specific prior written permission.
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
#define LOG_TAG "DisplayBufferAllocator"

#include <stdint.h>
#ifdef TARGET_USES_GRALLOC1
#include "gr_utils.h"
#else
#include <alloc_controller.h>
#include <memalloc.h>
#include <qcom/display/gr.h>
#endif
#ifndef TARGET_USES_GBM
#include <qcom/display/gralloc_priv.h>
#else
#include "common/utils/qmmf_common_utils.h"
#endif
#include <sdm/utils/constants.h>
#include <sdm/utils/debug.h>

#include "display/src/service/qmmf_display_common.h"
#include "display/src/service/qmmf_display_sdm_buffer_allocator.h"
#include "display/src/service/qmmf_display_sdm_debugger.h"

namespace qmmf {

namespace display {
#ifndef TARGET_USES_GRALLOC1
DisplayBufferAllocatorGralloc::DisplayBufferAllocatorGralloc() {
  alloc_controller_ = gralloc::IAllocController::getInstance();
}

DisplayError DisplayBufferAllocatorGralloc::AllocateBuffer(
    BufferInfo *buffer_info) {
  gralloc::alloc_data data;

  const BufferConfig &buffer_config = buffer_info->buffer_config;
  AllocatedBufferInfo *alloc_buffer_info = &buffer_info->alloc_buffer_info;
  MetaBufferInfo *meta_buffer_info = new MetaBufferInfo();

  if (!meta_buffer_info) {
    return kErrorMemory;
  }

  int alloc_flags = INT(GRALLOC_USAGE_PRIVATE_IOMMU_HEAP);
  int error = 0;

  int width = INT(buffer_config.width);
  int height = INT(buffer_config.height);
  int format;

  if (buffer_config.secure) {
    alloc_flags = INT(GRALLOC_USAGE_PRIVATE_MM_HEAP);
    alloc_flags |= INT(GRALLOC_USAGE_PROTECTED);
    data.align = SECURE_ALIGN;
  } else {
    data.align = UINT32(getpagesize());
  }

  if (buffer_config.cache == false) {
    // Allocate uncached buffers
    alloc_flags |= GRALLOC_USAGE_PRIVATE_UNCACHED;
  }

  auto error = SetBufferInfo(buffer_config.format, &format, &alloc_flags);
  if (error != 0) {
    delete meta_buffer_info;
    return kErrorParameters;
  }

  int aligned_width = 0, aligned_height = 0;
  uint32_t buffer_size = getBufferSizeAndDimensions(
      width, height, format, alloc_flags, aligned_width, aligned_height);

  buffer_size = ROUND_UP(buffer_size, data.align) * buffer_config.buffer_count;

  QMMF_DEBUG("%s: buffer size is = %u", __func__, buffer_size);

  data.base = 0;
  data.fd = -1;
  data.offset = 0;
  data.size = buffer_size;
  data.uncached = !buffer_config.cache;

  error = alloc_controller_->allocate(data, alloc_flags);
  if (error != 0) {
    QMMF_ERROR("Error allocating memory size %d uncached %d", data.size,
               data.uncached);
    delete meta_buffer_info;
    return kErrorMemory;
  }

  alloc_buffer_info->fd = data.fd;
  alloc_buffer_info->stride = UINT32(aligned_width);
  alloc_buffer_info->size = buffer_size;

  meta_buffer_info->base_addr = data.base;
  meta_buffer_info->alloc_type = data.allocType;

  buffer_info->private_data = meta_buffer_info;

  return kErrorNone;
}

DisplayError DisplayBufferAllocatorGralloc::FreeBuffer(
    BufferInfo *buffer_info) {
  AllocatedBufferInfo *alloc_buffer_info = &buffer_info->alloc_buffer_info;

  // Deallocate the buffer, only if the buffer fd is valid.
  if (alloc_buffer_info->fd > 0) {
    MetaBufferInfo *meta_buffer_info =
        static_cast<MetaBufferInfo *>(buffer_info->private_data);
    gralloc::IMemAlloc *memalloc =
        alloc_controller_->getAllocator(meta_buffer_info->alloc_type);
    if (memalloc == NULL) {
      QMMF_ERROR("Memalloc handle is NULL, alloc type %d",
                 meta_buffer_info->alloc_type);
      return kErrorResources;
    }

    auto ret = memalloc->free_buffer(meta_buffer_info->base_addr,
                                     alloc_buffer_info->size, 0,
                                     alloc_buffer_info->fd);
    if (ret != 0) {
      QMMF_ERROR("Error freeing buffer base_addr %p size %d fd %d",
                 meta_buffer_info->base_addr, alloc_buffer_info->size,
                 alloc_buffer_info->fd);
      return kErrorMemory;
    }

    alloc_buffer_info->fd = -1;
    alloc_buffer_info->stride = 0;
    alloc_buffer_info->size = 0;

    meta_buffer_info->base_addr = NULL;
    meta_buffer_info->alloc_type = 0;

    delete meta_buffer_info;
    meta_buffer_info = NULL;
  }

  return kErrorNone;
}

uint32_t DisplayBufferAllocatorGralloc::GetBufferSize(BufferInfo *buffer_info) {
  uint32_t align = UINT32(getpagesize());

  const BufferConfig &buffer_config = buffer_info->buffer_config;

  int alloc_flags = INT(GRALLOC_USAGE_PRIVATE_IOMMU_HEAP);

  int width = INT(buffer_config.width);
  int height = INT(buffer_config.height);
  int format;

  if (buffer_config.secure) {
    alloc_flags = INT(GRALLOC_USAGE_PRIVATE_MM_HEAP);
    alloc_flags |= INT(GRALLOC_USAGE_PROTECTED);
    align = SECURE_ALIGN;
  }

  if (buffer_config.cache == false) {
    // Allocate uncached buffers
    alloc_flags |= GRALLOC_USAGE_PRIVATE_UNCACHED;
  }

  if (SetBufferInfo(buffer_config.format, &format, &alloc_flags) < 0) {
    return 0;
  }

  int aligned_width = 0;
  int aligned_height = 0;
  uint32_t buffer_size = getBufferSizeAndDimensions(
      width, height, format, alloc_flags, aligned_width, aligned_height);

  buffer_size = ROUND_UP(buffer_size, align) * buffer_config.buffer_count;

  return buffer_size;
}

DisplayError DisplayBufferAllocatorGralloc::GetBufferInfo(
    BufferInfo *buffer_info, int32_t &aligned_width, int32_t &aligned_height) {
  const BufferConfig &buffer_config = buffer_info->buffer_config;
  int alloc_flags = INT(GRALLOC_USAGE_PRIVATE_IOMMU_HEAP);

  int width = INT(buffer_config.width);
  int height = INT(buffer_config.height);
  int format;

  if (buffer_config.secure) {
    alloc_flags = INT(GRALLOC_USAGE_PRIVATE_MM_HEAP);
    alloc_flags |= INT(GRALLOC_USAGE_PROTECTED);
  }

  if (buffer_config.cache == false) {
    // Allocate uncached buffers
    alloc_flags |= GRALLOC_USAGE_PRIVATE_UNCACHED;
  }

  if (SetBufferInfo(buffer_config.format, &format, &alloc_flags) < 0) {
    QMMF_ERROR("Error Setting buffer info");
    return kErrorNone;
  }

  getBufferSizeAndDimensions(width, height, format, alloc_flags, aligned_width,
                             aligned_height);

  return kErrorNone;
}

int DisplayBufferAllocatorGralloc::SetBufferInfo(LayerBufferFormat format,
                                                 int *target, int *flags) {
  switch (format) {
    case kFormatRGBA8888:
      *target = HAL_PIXEL_FORMAT_RGBA_8888;
      break;
    case kFormatRGBX8888:
      *target = HAL_PIXEL_FORMAT_RGBX_8888;
      break;
    case kFormatRGB888:
      *target = HAL_PIXEL_FORMAT_RGB_888;
      break;
    case kFormatRGB565:
      *target = HAL_PIXEL_FORMAT_RGB_565;
      break;
    case kFormatBGR565:
      *target = HAL_PIXEL_FORMAT_BGR_565;
      break;
    case kFormatBGRA8888:
      *target = HAL_PIXEL_FORMAT_BGRA_8888;
      break;
    case kFormatYCrCb420PlanarStride16:
      *target = HAL_PIXEL_FORMAT_YV12;
      break;
    case kFormatYCrCb420SemiPlanar:
      *target = HAL_PIXEL_FORMAT_YCrCb_420_SP;
      break;
    case kFormatYCbCr420SemiPlanar:
      *target = HAL_PIXEL_FORMAT_YCbCr_420_SP;
      break;
    case kFormatYCbCr422H2V1Packed:
      *target = HAL_PIXEL_FORMAT_YCbCr_422_I;
      break;
    case kFormatYCbCr422H2V1SemiPlanar:
      *target = HAL_PIXEL_FORMAT_YCbCr_422_SP;
      break;
    case kFormatYCbCr420SemiPlanarVenus:
      *target = HAL_PIXEL_FORMAT_YCbCr_420_SP_VENUS;
      break;
    case kFormatYCrCb420SemiPlanarVenus:
      *target = HAL_PIXEL_FORMAT_YCrCb_420_SP_VENUS;
      break;
    case kFormatYCbCr420SPVenusUbwc:
      *target = HAL_PIXEL_FORMAT_YCbCr_420_SP_VENUS_UBWC;
      *flags |= GRALLOC_USAGE_PRIVATE_ALLOC_UBWC;
      break;
    case kFormatRGBA5551:
      *target = HAL_PIXEL_FORMAT_RGBA_5551;
      break;
    case kFormatRGBA4444:
      *target = HAL_PIXEL_FORMAT_RGBA_4444;
      break;
    case kFormatRGBA1010102:
      *target = HAL_PIXEL_FORMAT_RGBA_1010102;
      break;
    case kFormatARGB2101010:
      *target = HAL_PIXEL_FORMAT_ARGB_2101010;
      break;
    case kFormatRGBX1010102:
      *target = HAL_PIXEL_FORMAT_RGBX_1010102;
      break;
    case kFormatXRGB2101010:
      *target = HAL_PIXEL_FORMAT_XRGB_2101010;
      break;
    case kFormatBGRA1010102:
      *target = HAL_PIXEL_FORMAT_BGRA_1010102;
      break;
    case kFormatABGR2101010:
      *target = HAL_PIXEL_FORMAT_ABGR_2101010;
      break;
    case kFormatBGRX1010102:
      *target = HAL_PIXEL_FORMAT_BGRX_1010102;
      break;
    case kFormatXBGR2101010:
      *target = HAL_PIXEL_FORMAT_XBGR_2101010;
      break;
    case kFormatYCbCr420P010:
      *target = HAL_PIXEL_FORMAT_YCbCr_420_P010;
      break;
    case kFormatYCbCr420TP10Ubwc:
      *target = HAL_PIXEL_FORMAT_YCbCr_420_TP10_UBWC;
      break;
    case kFormatRGBA8888Ubwc:
      *target = HAL_PIXEL_FORMAT_RGBA_8888;
      *flags |= GRALLOC_USAGE_PRIVATE_ALLOC_UBWC;
      break;
    case kFormatRGBX8888Ubwc:
      *target = HAL_PIXEL_FORMAT_RGBX_8888;
      *flags |= GRALLOC_USAGE_PRIVATE_ALLOC_UBWC;
      break;
    case kFormatBGR565Ubwc:
      *target = HAL_PIXEL_FORMAT_BGR_565;
      *flags |= GRALLOC_USAGE_PRIVATE_ALLOC_UBWC;
      break;
    case kFormatRGBA1010102Ubwc:
      *target = HAL_PIXEL_FORMAT_RGBA_1010102;
      *flags |= GRALLOC_USAGE_PRIVATE_ALLOC_UBWC;
      break;
    case kFormatRGBX1010102Ubwc:
      *target = HAL_PIXEL_FORMAT_RGBX_1010102;
      *flags |= GRALLOC_USAGE_PRIVATE_ALLOC_UBWC;
      break;
    default:
      QMMF_ERROR("Unsupported format = 0x%x", format);
      return -EINVAL;
  }

  return 0;
}

DisplayError DisplayBufferAllocatorGralloc::GetAllocatedBufferInfo(
    const BufferConfig &buffer_config,
    AllocatedBufferInfo *allocated_buffer_info) {
  int width = INT(buffer_config.width);
  int height = INT(buffer_config.height);
  int alloc_flags = INT(GRALLOC_USAGE_PRIVATE_IOMMU_HEAP);

  if (buffer_config.secure) {
    alloc_flags = INT(GRALLOC_USAGE_PRIVATE_MM_HEAP);
    alloc_flags |= INT(GRALLOC_USAGE_PROTECTED);
  }

  if (buffer_config.cache == false) {
    // Allocate uncached buffers
    alloc_flags |= GRALLOC_USAGE_PRIVATE_UNCACHED;
  }

  int format = 0;
  int error = SetBufferInfo(buffer_config.format, &format, &alloc_flags);
  if (error) {
    QMMF_ERROR("Failed: invalid format = %d", buffer_config.format);
    return kErrorNotSupported;
  }

  uint32_t buffer_size = 0;

  int width_aligned = 0, height_aligned = 0;
  buffer_size = getBufferSizeAndDimensions(width, height, format, alloc_flags,
                                           width_aligned, height_aligned);

  if (buffer_size == 0) {
    QMMF_ERROR("Failed: invalid buffer size = %u", buffer_size);
    return kErrorNotSupported;
  }

  allocated_buffer_info->stride = UINT32(width_aligned);
  allocated_buffer_info->aligned_width = UINT32(width_aligned);
  allocated_buffer_info->aligned_height = UINT32(height_aligned);
  allocated_buffer_info->size = UINT32(buffer_size);

  return kErrorNone;
}
#else
DisplayBufferAllocatorGralloc1::DisplayBufferAllocatorGralloc1() {
  int err = hw_get_module(GRALLOC_HARDWARE_MODULE_ID, &module_);
  if (err != 0) {
    QMMF_ERROR("FATAL: can not get GRALLOC module");
    return;
  }

  err = gralloc1_open(module_, &gralloc_device_);
  if (err != 0) {
    QMMF_ERROR("FATAL: can not open GRALLOC device");
    return;
  }

  if (gralloc_device_ != nullptr) {
    QMMF_ERROR("FATAL: gralloc device is null");
    return;
  }

  CreateBufferDescriptor_ = reinterpret_cast<GRALLOC1_PFN_CREATE_DESCRIPTOR>(
      gralloc_device_->getFunction(gralloc_device_,
                                   GRALLOC1_FUNCTION_CREATE_DESCRIPTOR));
  DestroyBufferDescriptor_ = reinterpret_cast<GRALLOC1_PFN_DESTROY_DESCRIPTOR>(
      gralloc_device_->getFunction(gralloc_device_,
                                   GRALLOC1_FUNCTION_DESTROY_DESCRIPTOR));
  AllocateBuffer_ =
      reinterpret_cast<GRALLOC1_PFN_ALLOCATE>(gralloc_device_->getFunction(
          gralloc_device_, GRALLOC1_FUNCTION_ALLOCATE));
  ReleaseBuffer_ = reinterpret_cast<GRALLOC1_PFN_RELEASE>(
      gralloc_device_->getFunction(gralloc_device_, GRALLOC1_FUNCTION_RELEASE));
  SetBufferDimensions_ = reinterpret_cast<GRALLOC1_PFN_SET_DIMENSIONS>(
      gralloc_device_->getFunction(gralloc_device_,
                                   GRALLOC1_FUNCTION_SET_DIMENSIONS));
  SetBufferFormat_ =
      reinterpret_cast<GRALLOC1_PFN_SET_FORMAT>(gralloc_device_->getFunction(
          gralloc_device_, GRALLOC1_FUNCTION_SET_FORMAT));
  SetConsumerUsage_ = reinterpret_cast<GRALLOC1_PFN_SET_CONSUMER_USAGE>(
      gralloc_device_->getFunction(gralloc_device_,
                                   GRALLOC1_FUNCTION_SET_CONSUMER_USAGE));
  SetProducerUsage_ = reinterpret_cast<GRALLOC1_PFN_SET_PRODUCER_USAGE>(
      gralloc_device_->getFunction(gralloc_device_,
                                   GRALLOC1_FUNCTION_SET_PRODUCER_USAGE));
}

DisplayError DisplayBufferAllocatorGralloc1::AllocateBuffer(
    BufferInfo *buffer_info) {
  const BufferConfig &buffer_config = buffer_info->buffer_config;
  AllocatedBufferInfo *alloc_buffer_info = &buffer_info->alloc_buffer_info;
  uint32_t width = buffer_config.width;
  uint32_t height = buffer_config.height;
  uint64_t alloc_flags = 0;
  DisplayError err = kErrorNone;

  if (buffer_config.secure) {
    alloc_flags |= GRALLOC1_PRODUCER_USAGE_PROTECTED;
  }

  int format;

  if (buffer_config.cache == false) {
    // Allocate uncached buffers
    alloc_flags |= GRALLOC_USAGE_PRIVATE_UNCACHED;
  }

  auto error = SetBufferInfo(buffer_config.format, &format, &alloc_flags);
  if (error != 0) {
    return kErrorParameters;
  }
  gralloc1_producer_usage_t producer_usage =
      static_cast<gralloc1_producer_usage_t>(alloc_flags);
  gralloc1_consumer_usage_t consumer_usage =
      static_cast<gralloc1_consumer_usage_t>(
          alloc_flags | GRALLOC1_CONSUMER_USAGE_HWCOMPOSER);
  gralloc1_buffer_descriptor_t descriptor_id = {};
  buffer_handle_t buf = nullptr;
  private_handle_t *hnd = nullptr;

  // CreateBuffer
  if (CreateBufferDescriptor_(gralloc_device_, &descriptor_id) !=
      GRALLOC1_ERROR_NONE) {
    QMMF_ERROR("CreateBufferDescriptor failed gr_device=%p", gralloc_device_);
    return kErrorParameters;
  }
  if (SetBufferDimensions_(gralloc_device_, descriptor_id, width, height) !=
      GRALLOC1_ERROR_NONE) {
    QMMF_ERROR("SetBufferDimensions failed gr_device=%p desc=%llu",
               gralloc_device_, descriptor_id);
    err = kErrorParameters;
    goto CleanupOnError;
  }
  if (SetBufferFormat_(gralloc_device_, descriptor_id, format) !=
      GRALLOC1_ERROR_NONE) {
    QMMF_ERROR("SetBufferFormat failed gr_device=%p desc=%llu", gralloc_device_,
               descriptor_id);
    err = kErrorParameters;
    goto CleanupOnError;
  }
  if (SetConsumerUsage_(gralloc_device_, descriptor_id, consumer_usage) !=
      GRALLOC1_ERROR_NONE) {
    QMMF_ERROR("SetConsumerUsage failed gr_device=%p desc=%llu",
               gralloc_device_, descriptor_id);
    err = kErrorParameters;
    goto CleanupOnError;
  }
  if (SetProducerUsage_(gralloc_device_, descriptor_id, producer_usage) !=
      GRALLOC1_ERROR_NONE) {
    QMMF_ERROR("SetProducerUsage failed gr_device=%p desc=%llu",
               gralloc_device_, descriptor_id);
    err = kErrorParameters;
    goto CleanupOnError;
  }
  if (AllocateBuffer_(gralloc_device_, 1, &descriptor_id, &buf) !=
      GRALLOC1_ERROR_NONE) {
    QMMF_ERROR("AllocateBuffer failed gr_device=%p desc=%llu", gralloc_device_,
               descriptor_id);
    err = kErrorMemory;
    goto CleanupOnError;
  }

  hnd = (private_handle_t *)buf;  // NOLINT
  alloc_buffer_info->fd = hnd->fd;
  alloc_buffer_info->stride = UINT32(hnd->width);
  alloc_buffer_info->aligned_width = UINT32(hnd->width);
  alloc_buffer_info->aligned_height = UINT32(hnd->height);
  alloc_buffer_info->size = hnd->size;

  buffer_info->private_data = reinterpret_cast<void *>(hnd);
CleanupOnError:
  DestroyBufferDescriptor_(gralloc_device_, descriptor_id);
  return err;
}

DisplayError DisplayBufferAllocatorGralloc1::FreeBuffer(
    BufferInfo *buffer_info) {
  AllocatedBufferInfo *alloc_buffer_info = &buffer_info->alloc_buffer_info;
  buffer_handle_t hnd = static_cast<buffer_handle_t>(buffer_info->private_data);

  ReleaseBuffer_(gralloc_device_, hnd);

  buffer_info->private_data = NULL;
  alloc_buffer_info->fd = -1;
  alloc_buffer_info->stride = 0;
  alloc_buffer_info->size = 0;

  return kErrorNone;
}

uint32_t DisplayBufferAllocatorGralloc1::GetBufferSize(
    BufferInfo *buffer_info) {
  const BufferConfig &buffer_config = buffer_info->buffer_config;
  uint64_t alloc_flags = GRALLOC_USAGE_PRIVATE_IOMMU_HEAP;
  int width = INT(buffer_config.width);
  int height = INT(buffer_config.height);
  int format;

  if (buffer_config.secure) {
    alloc_flags |= GRALLOC_USAGE_PROTECTED;
  }

  if (buffer_config.cache == false) {
    // Allocate uncached buffers
    alloc_flags |= GRALLOC_USAGE_PRIVATE_UNCACHED;
  }

  if (SetBufferInfo(buffer_config.format, &format, &alloc_flags) < 0) {
    return 0;
  }

  uint32_t aligned_width = 0, aligned_height = 0, buffer_size = 0;
  gralloc1_producer_usage_t producer_usage = GRALLOC1_PRODUCER_USAGE_NONE;
  gralloc1_consumer_usage_t consumer_usage = GRALLOC1_CONSUMER_USAGE_NONE;
  // TODO(user): Currently both flags are treated similarly in gralloc
  producer_usage = gralloc1_producer_usage_t(alloc_flags);
  consumer_usage = gralloc1_consumer_usage_t(alloc_flags);
  gralloc1::BufferInfo info(width, height, format, producer_usage,
                            consumer_usage);
  GetBufferSizeAndDimensions(info, &buffer_size, &aligned_width,
                             &aligned_height);

  return buffer_size;
}

DisplayError DisplayBufferAllocatorGralloc1::GetBufferInfo(
    BufferInfo *buffer_info, int32_t &aligned_width, int32_t &aligned_height) {
  const BufferConfig &buffer_config = buffer_info->buffer_config;

  AllocatedBufferInfo alloc_buffer_info;
  if (GetAllocatedBufferInfo(buffer_config, &alloc_buffer_info) != kErrorNone) {
    return kErrorNotSupported;
  }
  QMMF_DEBUG("alloc_buffer_info: width:%u height:%u, size:%u, format: %d",
             alloc_buffer_info.aligned_width, alloc_buffer_info.aligned_height,
             alloc_buffer_info.size, alloc_buffer_info.format);
  aligned_width = alloc_buffer_info.aligned_width;
  aligned_height = alloc_buffer_info.aligned_height;

  return kErrorNone;
}

int DisplayBufferAllocatorGralloc1::SetBufferInfo(LayerBufferFormat format,
                                                  int *target,
                                                  uint64_t *flags) {
  switch (format) {
    case kFormatRGBA8888:
      *target = HAL_PIXEL_FORMAT_RGBA_8888;
      break;
    case kFormatRGBX8888:
      *target = HAL_PIXEL_FORMAT_RGBX_8888;
      break;
    case kFormatRGB888:
      *target = HAL_PIXEL_FORMAT_RGB_888;
      break;
    case kFormatRGB565:
      *target = HAL_PIXEL_FORMAT_RGB_565;
      break;
    case kFormatBGR565:
      *target = HAL_PIXEL_FORMAT_BGR_565;
      break;
    case kFormatBGRA8888:
      *target = HAL_PIXEL_FORMAT_BGRA_8888;
      break;
    case kFormatYCrCb420PlanarStride16:
      *target = HAL_PIXEL_FORMAT_YV12;
      break;
    case kFormatYCrCb420SemiPlanar:
      *target = HAL_PIXEL_FORMAT_YCrCb_420_SP;
      break;
    case kFormatYCbCr420SemiPlanar:
      *target = HAL_PIXEL_FORMAT_YCbCr_420_SP;
      break;
    case kFormatYCbCr422H2V1Packed:
      *target = HAL_PIXEL_FORMAT_YCbCr_422_I;
      break;
    case kFormatYCbCr422H2V1SemiPlanar:
      *target = HAL_PIXEL_FORMAT_YCbCr_422_SP;
      break;
    case kFormatYCbCr420SemiPlanarVenus:
      *target = HAL_PIXEL_FORMAT_YCbCr_420_SP_VENUS;
      break;
    case kFormatYCrCb420SemiPlanarVenus:
      *target = HAL_PIXEL_FORMAT_YCrCb_420_SP_VENUS;
      break;
    case kFormatYCbCr420SPVenusUbwc:
      *target = HAL_PIXEL_FORMAT_YCbCr_420_SP_VENUS_UBWC;
      *flags |= GRALLOC_USAGE_PRIVATE_ALLOC_UBWC;
      break;
    case kFormatRGBA5551:
      *target = HAL_PIXEL_FORMAT_RGBA_5551;
      break;
    case kFormatRGBA4444:
      *target = HAL_PIXEL_FORMAT_RGBA_4444;
      break;
    case kFormatRGBA1010102:
      *target = HAL_PIXEL_FORMAT_RGBA_1010102;
      break;
    case kFormatARGB2101010:
      *target = HAL_PIXEL_FORMAT_ARGB_2101010;
      break;
    case kFormatRGBX1010102:
      *target = HAL_PIXEL_FORMAT_RGBX_1010102;
      break;
    case kFormatXRGB2101010:
      *target = HAL_PIXEL_FORMAT_XRGB_2101010;
      break;
    case kFormatBGRA1010102:
      *target = HAL_PIXEL_FORMAT_BGRA_1010102;
      break;
    case kFormatABGR2101010:
      *target = HAL_PIXEL_FORMAT_ABGR_2101010;
      break;
    case kFormatBGRX1010102:
      *target = HAL_PIXEL_FORMAT_BGRX_1010102;
      break;
    case kFormatXBGR2101010:
      *target = HAL_PIXEL_FORMAT_XBGR_2101010;
      break;
    case kFormatYCbCr420P010:
      *target = HAL_PIXEL_FORMAT_YCbCr_420_P010;
      break;
    case kFormatYCbCr420TP10Ubwc:
      *target = HAL_PIXEL_FORMAT_YCbCr_420_TP10_UBWC;
      break;
    case kFormatRGBA8888Ubwc:
      *target = HAL_PIXEL_FORMAT_RGBA_8888;
      *flags |= GRALLOC_USAGE_PRIVATE_ALLOC_UBWC;
      break;
    case kFormatRGBX8888Ubwc:
      *target = HAL_PIXEL_FORMAT_RGBX_8888;
      *flags |= GRALLOC_USAGE_PRIVATE_ALLOC_UBWC;
      break;
    case kFormatBGR565Ubwc:
      *target = HAL_PIXEL_FORMAT_BGR_565;
      *flags |= GRALLOC_USAGE_PRIVATE_ALLOC_UBWC;
      break;
    case kFormatRGBA1010102Ubwc:
      *target = HAL_PIXEL_FORMAT_RGBA_1010102;
      *flags |= GRALLOC_USAGE_PRIVATE_ALLOC_UBWC;
      break;
    case kFormatRGBX1010102Ubwc:
      *target = HAL_PIXEL_FORMAT_RGBX_1010102;
      *flags |= GRALLOC_USAGE_PRIVATE_ALLOC_UBWC;
      break;
    default:
      QMMF_ERROR("Unsupported format = 0x%x", format);
      return -EINVAL;
  }

  return 0;
}

DisplayError DisplayBufferAllocatorGralloc1::GetAllocatedBufferInfo(
    const BufferConfig &buffer_config,
    AllocatedBufferInfo *allocated_buffer_info) {
  int width = INT(buffer_config.width);
  int height = INT(buffer_config.height);
  uint64_t alloc_flags = GRALLOC_USAGE_PRIVATE_IOMMU_HEAP;

  if (buffer_config.secure) {
    alloc_flags = INT(GRALLOC_USAGE_PRIVATE_MM_HEAP);
    alloc_flags |= INT(GRALLOC_USAGE_PROTECTED);
  }

  if (buffer_config.cache == false) {
    // Allocate uncached buffers
    alloc_flags |= GRALLOC_USAGE_PRIVATE_UNCACHED;
  }

  int format = 0;
  int error = SetBufferInfo(buffer_config.format, &format, &alloc_flags);
  if (error) {
    QMMF_ERROR("Failed: invalid format = %d", buffer_config.format);
    return kErrorNotSupported;
  }

  uint32_t buffer_size = 0;

  uint32_t width_aligned = 0, height_aligned = 0;
  gralloc1_producer_usage_t producer_usage = GRALLOC1_PRODUCER_USAGE_NONE;
  gralloc1_consumer_usage_t consumer_usage = GRALLOC1_CONSUMER_USAGE_NONE;
  // TODO(user): Currently both flags are treated similarly in gralloc
  producer_usage = gralloc1_producer_usage_t(alloc_flags);
  consumer_usage = gralloc1_consumer_usage_t(alloc_flags);
  gralloc1::BufferInfo info(width, height, format, producer_usage,
                            consumer_usage);
  GetBufferSizeAndDimensions(info, &buffer_size, &width_aligned,
                             &height_aligned);

  if (buffer_size == 0) {
    QMMF_ERROR("Failed: invalid buffer size = %u", buffer_size);
    return kErrorNotSupported;
  }

  allocated_buffer_info->stride = UINT32(width_aligned);
  allocated_buffer_info->aligned_width = UINT32(width_aligned);
  allocated_buffer_info->aligned_height = UINT32(height_aligned);
  allocated_buffer_info->size = UINT32(buffer_size);

  return kErrorNone;
}

DisplayError DisplayBufferAllocatorGralloc1::GetBufferLayout(
    const AllocatedBufferInfo &buf_info, uint32_t stride[4], uint32_t offset[4],
    uint32_t *num_planes) {
  // TODO(user): Transition APIs to not need a private handle
  private_handle_t hnd(-1, 0, 0, 0, 0, 0, 0);
  int format = HAL_PIXEL_FORMAT_RGBA_8888;
  uint64_t flags = 0;

  SetBufferInfo(buf_info.format, &format, &flags);
  // Setup only the required stuff, skip rest
  hnd.format = format;
  hnd.width = static_cast<int32_t>(buf_info.aligned_width);
  hnd.height = static_cast<int32_t>(buf_info.aligned_height);
  if (flags & GRALLOC_USAGE_PRIVATE_ALLOC_UBWC) {
    hnd.flags = private_handle_t::PRIV_FLAGS_UBWC_ALIGNED;
  }

  int ret = gralloc1::GetBufferLayout(&hnd, stride, offset, num_planes);
  if (ret < 0) {
    QMMF_ERROR("%s failed!!", __func__);
    return kErrorParameters;
  }

  return kErrorNone;
}

DisplayBufferAllocatorGralloc1::~DisplayBufferAllocatorGralloc1() {
  if (gralloc_device_ != nullptr) {
    int err = gralloc1_close(gralloc_device_);
    if (err != 0) {
      QMMF_ERROR("FATAL: can not close GRALLOC device");
    }
  }
}
#endif
};  // namespace display

};  // namespace qmmf
