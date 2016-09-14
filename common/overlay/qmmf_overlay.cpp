/*
* Copyright (c) 2016, The Linux Foundation. All rights reserved.
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

#define LOG_TAG "Overlay"

#include <utils/Log.h>
#include <algorithm>
#include <fcntl.h>
#include <dirent.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <media/msm_media_info.h>
#include <string.h>
#include <cstring>
#include <assert.h>
#if USE_SKIA
#include <SkSurface.h>
#include <SkString.h>
#include <SkBitmap.h>
#include <SkBlurMaskFilter.h>
#endif

#include "qmmf-sdk/qmmf_overlay.h"
#include "qmmf_overlay_item.h"

namespace qmmf {

namespace overlay {

using namespace android;

Overlay::Overlay()
    :frame_width_(0), frame_height_(0),
     target_c2dsurface_id_(-1), ion_device_(-1),
     num_active_overlays_(0), id_(0) {
}

Overlay::~Overlay() {

  OVDBG_INFO("%s: Enter ",__func__);
  for (auto &iter : overlay_items_) {
    if (iter.second)
    delete iter.second;
  }
  overlay_items_.clear();

  if(target_c2dsurface_id_) {
    c2dDestroySurface(target_c2dsurface_id_);
    target_c2dsurface_id_ = 0;
    OVDBG_INFO("%s: Destroyed c2d Target Surface", __func__);
  }

  if(ion_device_)
    close(ion_device_);

  OVDBG_INFO("%s: Exit ",__func__);
}

int32_t Overlay::Init(const TargetBufferFormat& format) {

  OVDBG_LEVEL2("%s:Enter",__func__);
  uint32_t c2dColotFormat = GetC2dColorFormat(format);
  // Create dummy C2D surface, it is required to Initialize
  // C2D driver before calling any c2d Apis.
  C2D_YUV_SURFACE_DEF surface_def = {
    c2dColotFormat,
    1 * 4,
    1 * 4,
    (void*)0xaaaaaaaa,
    (void*)0xaaaaaaaa,
    1 * 4,
    (void*)0xaaaaaaaa,
    (void*)0xaaaaaaaa,
    1 * 4,
    (void*)0xaaaaaaaa,
    (void*)0xaaaaaaaa,
    1 * 4,
  };

  auto ret = c2dCreateSurface(&target_c2dsurface_id_, C2D_TARGET,
                              (C2D_SURFACE_TYPE)(C2D_SURFACE_YUV_HOST
                              |C2D_SURFACE_WITH_PHYS
                              |C2D_SURFACE_WITH_PHYS_DUMMY),
                               &surface_def);
  if(ret != C2D_STATUS_OK) {
    OVDBG_ERROR("%s: c2dCreateSurface failed!",__func__);
    return ret;
  }

  ion_device_ = open("/dev/ion", O_RDONLY);
  if (ion_device_ < 0) {
    OVDBG_ERROR("%s: Ion dev open failed %s\n", __func__,strerror(errno));
    c2dDestroySurface(target_c2dsurface_id_);
    target_c2dsurface_id_ = 0;
    return -1;
  }

  OVDBG_LEVEL2("%s: Exit",__func__);
  return ret;
}

int32_t Overlay::CreateOverlayItem(OverlayParam& param, uint32_t* overlay_id) {

  OVDBG_LEVEL2("%s:Enter ", __func__);
  OverlayItem* overlayItem = nullptr;
  switch(param.type) {
    case OverlayType::kDateType:
      overlayItem = new OverlayItemDateAndTime(ion_device_);
      break;
    case OverlayType::kUserText:
      overlayItem = new OverlayItemText(ion_device_);
      break;
    case OverlayType::kStaticImage:
      overlayItem = new OverlayItemStaticImage(ion_device_);
      break;
    case OverlayType::kBoundingBox:
      overlayItem = new OverlayItemBoundingBox(ion_device_);
      break;
    case OverlayType::kPrivacyMask:
      overlayItem = new OverlayItemPrivacyMask(ion_device_);
      break;
    default:
      OVDBG_ERROR("%s: OverlayType(%d) not supported!", __func__,
           param.type);
      break;
  }

  if(!overlayItem) {
    OVDBG_ERROR("%s: OverlayItem type(%d) failed!", __func__, param.type);
    return NO_INIT;
  }

  auto ret = overlayItem->Init(param);
  if(ret != C2D_STATUS_OK) {
    OVDBG_ERROR("%s:OverlayItem failed of type(%d)", __func__, param.type);
    delete overlayItem;
    return ret;
  }

  // StaticImage and PrivacyMask type overlayItems never be dirty
  // Their contents are static, all other items are dirty at Init
  // time and will be marked as dirty whenever their configuration
  // changes at run time after first draw.
  if((param.type == OverlayType::kStaticImage)
    || (param.type == OverlayType::kPrivacyMask)) {
    overlayItem->MarkDirty(false);
  } else {
    overlayItem->MarkDirty(true);
  }

  *overlay_id = ++id_;
  overlay_items_.insert({*overlay_id, overlayItem});
  OVDBG_INFO("%s:OverlayItem Type(%d) Id(%d) Created Successfully !",__func__,
      param.type, *overlay_id);

  OVDBG_LEVEL2("%s:Exit ", __func__);
  return ret;
}

int32_t Overlay::DeleteOverlayItem(uint32_t overlay_id) {

  OVDBG_LEVEL2("%s:Enter ", __func__);
  std::lock_guard<std::mutex> lock(lock_);

  int32_t ret = 0;
  if(!IsOverlayItemValid(overlay_id)) {
      OVDBG_ERROR("%s: overlay_id(%d) is not valid!",__func__, overlay_id);
      return BAD_VALUE;
  }
  OverlayItem* overlayItem = overlay_items_.at(overlay_id);
  assert(overlayItem != nullptr);
  delete overlayItem;
  overlay_items_.erase(overlay_id);
  OVDBG_INFO("%s: overlay_id(%d) & overlayItem(0x%x) Removed from map",
      __func__, overlay_id, overlayItem);

  OVDBG_LEVEL2("%s:Exit ", __func__);
  return ret;
}

int32_t Overlay::GetOverlayParams(uint32_t overlay_id,
                                  OverlayParam& param) {
  int32_t ret = 0;
  if(!IsOverlayItemValid(overlay_id)) {
      OVDBG_ERROR("%s: overlay_id(%d) is not valid!",__func__, overlay_id);
      return BAD_VALUE;
  }
  OverlayItem* overlayItem = overlay_items_.at(overlay_id);
  assert(overlayItem != nullptr);

  memset(&param, 0x0, sizeof param);
  overlayItem->GetParameters(param);
  return ret;
}

int32_t Overlay::UpdateOverlayParams(uint32_t overlay_id,
                                     OverlayParam& param) {

  OVDBG_LEVEL2("%s:Enter ", __func__);
  std::lock_guard<std::mutex> lock(lock_);

  if(!IsOverlayItemValid(overlay_id)) {
      OVDBG_ERROR("%s: overlay_id(%d) is not valid!",__func__, overlay_id);
      return BAD_VALUE;
  }
  OverlayItem* overlayItem = overlay_items_.at(overlay_id);
  assert(overlayItem != nullptr);

  OVDBG_LEVEL2("%s:Exit ", __func__);
  return overlayItem->UpdateParameters(param);
}

int32_t Overlay::EnableOverlayItem(uint32_t overlay_id) {

  OVDBG_LEVEL2("%s: Enter", __func__);
  std::lock_guard<std::mutex> lock(lock_);

  int32_t ret = 0;
  if(!IsOverlayItemValid(overlay_id)) {
      OVDBG_ERROR("%s: overlay_id(%d) is not valid!",__func__, overlay_id);
      return BAD_VALUE;
  }
  OverlayItem* overlayItem = overlay_items_.at(overlay_id);
  assert(overlayItem != nullptr);

  overlayItem->Activate(true);
  OVDBG_LEVEL1("%s: OverlayItem Id(%d) Activated", __func__, overlay_id);

  OVDBG_LEVEL2("%s: Exit", __func__);
  return ret;
}

int32_t Overlay::DisableOverlayItem(uint32_t overlay_id) {

  OVDBG_LEVEL2("%s: Enter", __func__);
  std::lock_guard<std::mutex> lock(lock_);

  int32_t ret = 0;
  if(!IsOverlayItemValid(overlay_id)) {
      OVDBG_ERROR("%s: overlay_id(%d) is not valid!",__func__, overlay_id);
      return BAD_VALUE;
  }
  OverlayItem* overlayItem = overlay_items_.at(overlay_id);
  assert(overlayItem != nullptr);

  overlayItem->Activate(false);
  OVDBG_LEVEL1("%s: OverlayItem Id(%d) DeActivated", __func__, overlay_id);

  OVDBG_LEVEL2("%s: Exit", __func__);
  return ret;
}

int32_t Overlay::ApplyOverlay(const OverlayTargetBuffer& buffer) {

  OVDBG_LEVEL2("%s: Enter", __func__);
  int32_t ret = 0;
  std::lock_guard<std::mutex> lock(lock_);

  size_t numActiveOverlays = 0;
  bool isItemsActive = false;
  for (auto &iter : overlay_items_) {
    if ((iter).second->IsActive()) {
      isItemsActive = true;
    }
  }
  if(!isItemsActive) {
    OVDBG_LEVEL2("%s: No overlayItem is Active!", __func__);
    return ret;
  }
  assert(buffer.ion_fd != 0);
  assert(buffer.width != 0 && buffer.height != 0);
  assert(buffer.frame_len != 0);

  OVDBG_LEVEL2("%s: OverlayTargetBuffer: ion_fd = %d",__func__, buffer.ion_fd);
  OVDBG_LEVEL2("%s: OverlayTargetBuffer: Width = %d & Height = %d & frameLength"
      " =% d", __func__, buffer.width, buffer.height, buffer.frame_len);
  OVDBG_LEVEL2("%s: OverlayTargetBuffer: format = %d", __func__, buffer.format);

  void* bufVaddr = mmap(NULL, buffer.frame_len, PROT_READ  | PROT_WRITE,
                                              MAP_SHARED, buffer.ion_fd, 0);
  if(!bufVaddr) {
    OVDBG_ERROR("%s: mmap failed!", __func__);
    return UNKNOWN_ERROR;
  }

  // Map input YUV buffer to GPU.
  void *gpuAddr = NULL;
  ret = c2dMapAddr(buffer.ion_fd, bufVaddr, buffer.frame_len, 0,
                   KGSL_USER_MEM_TYPE_ION, &gpuAddr);
  if(ret != C2D_STATUS_OK) {
    OVDBG_ERROR("%s: c2dMapAddr failed!",__func__);
    goto EXIT;
  }

  // Target surface format.
  C2D_YUV_SURFACE_DEF surface_def;
  surface_def.format  = GetC2dColorFormat(buffer.format);
  surface_def.width   = buffer.width;
  surface_def.height  = buffer.height;
  int32_t planeYLen;
  switch (surface_def.format) {
    case C2D_COLOR_FORMAT_420_NV12:
      //Y plane stride.
      surface_def.stride0 = VENUS_Y_STRIDE(COLOR_FMT_NV12,
              surface_def.width);

      //UV plane stride.
      surface_def.stride1 = VENUS_UV_STRIDE(COLOR_FMT_NV12,
              surface_def.width);

      //UV plane hostptr.
      planeYLen = surface_def.stride0 * VENUS_Y_SCANLINES(COLOR_FMT_NV12,
              surface_def.height);

      break;
    case C2D_COLOR_FORMAT_420_NV21:
      //Y plane stride.
      surface_def.stride0 = VENUS_Y_STRIDE(COLOR_FMT_NV21,
              surface_def.width);

      //UV plane stride.
      surface_def.stride1 = VENUS_UV_STRIDE(COLOR_FMT_NV21,
              surface_def.width);

      //UV plane hostptr.
      planeYLen = surface_def.stride0 * VENUS_Y_SCANLINES(COLOR_FMT_NV21,
              surface_def.height);

      break;
    default:
      OVDBG_ERROR("%s: Unknown format: %d", __func__, surface_def.format);
      goto EXIT;
  }

  OVDBG_LEVEL1("%s: surface_def.stride0 = %d ",__func__, surface_def.stride0);
  OVDBG_LEVEL1("%s: planeYLen = %d",__func__, planeYLen);

  //Y plane hostptr.
  surface_def.plane0  = (void*)bufVaddr;
  //Y plane Gpu address.
  surface_def.phys0   = (void*)gpuAddr;

  surface_def.plane1  = (void*)((intptr_t)bufVaddr + planeYLen);

  //UV plane Gpu address.
  surface_def.phys1 = (void*)((intptr_t)gpuAddr + planeYLen);

  //Create C2d target surface outof camera buffer. camera buffer
  //is target surface where c2d blits different types of overlays
  //static logo, system time and date.
  ret = c2dUpdateSurface(target_c2dsurface_id_, C2D_SOURCE,
                         (C2D_SURFACE_TYPE)(C2D_SURFACE_YUV_HOST
                         |C2D_SURFACE_WITH_PHYS), &surface_def);
  if(ret != C2D_STATUS_OK) {
    OVDBG_ERROR("%s: c2dUpdateSurface failed!",__func__);
    goto EXIT;
  }

  // Iterate all dirty overlay Items, and update them.
  for (auto &iter : overlay_items_) {
    if ((iter).second->IsActive()) {
      ret = (iter).second->UpdateAndDraw();
      if(ret != 0) {
        OVDBG_ERROR("%s: Update & Draw failed for Item=%d", __func__,
            (iter).first);
      }
    }
  }

  C2dObjects c2d_objects;
  memset(&c2d_objects, 0x0, sizeof c2d_objects);
  // Iterate all updated overlayItems, and get coordinates.
  for (auto &iter : overlay_items_) {
    int32_t i = 0;
    DrawInfo draw_info;
    memset(&draw_info, 0x0, sizeof draw_info);
    OverlayItem *overlay_item = (iter).second;
    if(overlay_item->IsActive()) {
      overlay_item->GetDrawInfo(buffer.width, buffer.height,
          &draw_info);
      c2d_objects.objects[i].surface_id  = draw_info.c2dSurfaceId;
      c2d_objects.objects[i].config_mask = C2D_ALPHA_BLEND_SRC_ATOP
                                           |C2D_TARGET_RECT_BIT;
      c2d_objects.objects[i].target_rect.x       = draw_info.x << 16;
      c2d_objects.objects[i].target_rect.y       = draw_info.y << 16;
      c2d_objects.objects[i].target_rect.width   = draw_info.width << 16;
      c2d_objects.objects[i].target_rect.height  = draw_info.height << 16;

      OVDBG_LEVEL2("%s: c2d_objects[%d].surface_id=%d", __func__, i,
          c2d_objects.objects[i].surface_id);
      OVDBG_LEVEL2("%s: c2d_objects[%d].target_rect.x=%d", __func__, i,
          draw_info.x);
      OVDBG_LEVEL2("%s: c2d_objects[%d].target_rect.y=%d", __func__, i,
          draw_info.y);
      OVDBG_LEVEL2("%s: c2d_objects[%d].target_rect.width=%d", __func__, i,
          draw_info.width);
      OVDBG_LEVEL2("%s: c2d_objects[%d].target_rect.height=%d", __func__, i,
          draw_info.height);
      ++numActiveOverlays;
      ++i;
    }
  }

  OVDBG_LEVEL2("%s: numActiveOverlays=%d", __func__, numActiveOverlays);
  for(size_t i = 0; i < (numActiveOverlays-1); i++) {
    c2d_objects.objects[i].next = &c2d_objects.objects[i+1];
  }

  ret = c2dDraw(target_c2dsurface_id_, 0, 0, 0, 0, c2d_objects.objects,
                numActiveOverlays);
  if(ret != C2D_STATUS_OK) {
    OVDBG_ERROR("%s: c2dDraw failed!",__func__);
    goto EXIT;
  }

  ret = c2dFinish(target_c2dsurface_id_);
  if(ret != C2D_STATUS_OK) {
    OVDBG_ERROR("%s: c2dFinish failed!",__func__);
    goto EXIT;
  }
  // Unmap camera buffer from GPU after draw is completed.
  ret = c2dUnMapAddr(gpuAddr);
  if(ret != C2D_STATUS_OK) {
    OVDBG_ERROR("%s: c2dUnMapAddr failed!",__func__);
    goto EXIT;
  }

EXIT:
  if (bufVaddr) {
    munmap(bufVaddr, buffer.frame_len);
  }

  OVDBG_LEVEL2("%s: Exit ",__func__);
  return ret;
}

uint32_t Overlay::GetC2dColorFormat(const TargetBufferFormat& format) {

  uint32_t c2dColorFormat = C2D_COLOR_FORMAT_420_NV12;
  switch (format) {
    case TargetBufferFormat::kYUVNV12:
      c2dColorFormat = C2D_COLOR_FORMAT_420_NV12;
      break;
    case TargetBufferFormat::kYUVNV21:
      c2dColorFormat = C2D_COLOR_FORMAT_420_NV21;
      break;
    default:
      OVDBG_ERROR("%s: Unsupported buffer format: %d", __func__, format);
      break;
  }
  OVDBG_LEVEL2("%s:Selected C2D ColorFormat=%d",__func__, c2dColorFormat);
  return c2dColorFormat;
}

bool Overlay::IsOverlayItemValid(uint32_t overlay_id) {

  OVDBG_LEVEL1("%s: Enter overlay_id(%d)",__func__, overlay_id);
  bool valid = false;
  for (auto& iter : overlay_items_) {
    if (overlay_id == (iter).first) {
      valid = true;
      break;
    }
  }
  OVDBG_LEVEL1("%s: Exit overlay_id(%d)",__func__, overlay_id);
  return valid;
}

OverlayItem::OverlayItem(int32_t ion_device)
    :x_(0), y_(0), width_(0), height_(0),
     c2dsurface_id_(-1), gpu_addr_(NULL),
     vaddr_(NULL), ion_fd_(0), size_(0),
     dirty_(false), ion_device_(ion_device),
     is_active_(false) {
  OVDBG_LEVEL2("%s:Enter ", __func__);
  memset(&handle_data_, 0x0, sizeof handle_data_);
  location_type_ = OverlayLocationType::kBottomLeft;
  OVDBG_LEVEL2("%s:Exit ", __func__);
}

OverlayItem::~OverlayItem() {

  //Unmap overlay gpu address.
  if(gpu_addr_) {
    c2dUnMapAddr(gpu_addr_);
    gpu_addr_ = NULL;
    OVDBG_INFO("%s: Unmapped GPU address type(%d)", __func__, type_);
  }
  if(vaddr_) {
    munmap(vaddr_, size_);
    vaddr_ = NULL;
  }
  //Destroy source overlay surface.
  if(c2dsurface_id_) {
    c2dDestroySurface(c2dsurface_id_);
    c2dsurface_id_ = -1;
    OVDBG_INFO("%s: Destroyed c2d Surface type(%d)",__func__, type_);
  }
  //Free overlay ION memory.
  if(ion_fd_) {
    ioctl(ion_device_, ION_IOC_FREE, &handle_data_);
    close(ion_fd_);
    ion_fd_ = -1;
    OVDBG_INFO("%s: Destroyed ION buffer type(%d)",__func__, type_);
  }
}

void OverlayItem::MarkDirty(bool dirty) {
  dirty_ = dirty;
  OVDBG_LEVEL2("%s: OverlayItem Type(%d) marked dirty!", __func__, type_);
}

void OverlayItem::Activate(bool value) {
  is_active_ = value;
  OVDBG_LEVEL2("%s: OverlayItem Type(%d) Activated!", __func__, type_);
}

int32_t OverlayItem::AllocateIonMemory(IonMemInfo& mem_info, uint32_t size) {

  OVDBG_LEVEL2("%s:Enter",__func__);
  struct ion_allocation_data alloc;
  struct ion_fd_data ionFdData;
  void *data = NULL;
  int ionType = 0x1 << ION_IOMMU_HEAP_ID;
  int32_t ret = 0;

  memset(&alloc, 0, sizeof(ion_allocation_data));
  alloc.len = size;
  alloc.len = (alloc.len + 4095) & (~4095);
  alloc.align = 4096;
  alloc.flags = ION_FLAG_CACHED;
  alloc.heap_id_mask = ionType;
  ret = ioctl(ion_device_, ION_IOC_ALLOC, &alloc);
  if (ret < 0) {
    OVDBG_ERROR("%s:ION allocation failed\n",__func__);
    goto ION_ALLOC_FAILED;
  }

  memset(&ionFdData, 0, sizeof(ion_fd_data));
  ionFdData.handle = alloc.handle;
  ret = ioctl(ion_device_, ION_IOC_SHARE, &ionFdData);
  if (ret < 0) {
    OVDBG_ERROR("%s:ION map failed %s\n",__func__,strerror(errno));
    goto ION_MAP_FAILED;
  }

  data = mmap(NULL, alloc.len, PROT_READ | PROT_WRITE, MAP_SHARED,
              ionFdData.fd, 0);

  if (data == MAP_FAILED) {
    OVDBG_ERROR("%s:ION mmap failed: %s (%d)\n",__func__, strerror(errno),
        errno);
    goto ION_MAP_FAILED;
  }

  memset(&mem_info.handle_data, 0, sizeof(mem_info.handle_data));
  mem_info.handle_data.handle  = ionFdData.handle;
  mem_info.fd                 = ionFdData.fd;
  mem_info.size               = alloc.len;
  mem_info.vaddr              = data;

  OVDBG_LEVEL2("%s:Exit ",__func__);
  return ret;

ION_MAP_FAILED:
  memset(&mem_info.handle_data, 0, sizeof(mem_info.handle_data));
  mem_info.handle_data.handle = ionFdData.handle;
  ioctl(ion_device_, ION_IOC_FREE, &mem_info.handle_data);
ION_ALLOC_FAILED:
  close(ion_device_);
  return -1;
}

OverlayItemStaticImage::OverlayItemStaticImage(int32_t ion_device)
    :OverlayItem(ion_device), image_path_() {
  OVDBG_LEVEL2("%s: Enter", __func__);
  type_ = OverlayType::kStaticImage;
  OVDBG_LEVEL2("%s: Exit", __func__);
}

OverlayItemStaticImage::~OverlayItemStaticImage() {
  OVDBG_LEVEL2("%s: Enter", __func__);
  image_path_.clear();
  OVDBG_LEVEL2("%s: Exit", __func__);
}

int32_t OverlayItemStaticImage::Init(OverlayParam& param) {

  OVDBG_LEVEL2("%s: Enter", __func__);
  int32_t ret = 0;

  if(param.image_info.width <= 0 || param.image_info.height <= 0) {
    OVDBG_ERROR("%s: Image Width & Height is not correct!", __func__);
    return BAD_VALUE;
  }

  location_type_ = param.location;
  width_        = param.image_info.width;
  height_       = param.image_info.height;

  image_path_.setTo(param.image_info.image_location,
      strlen(param.image_info.image_location) + 1);

  ret = CreateSurface();
  if(ret != 0) {
    OVDBG_ERROR("%s: createLogoSurface failed!", __func__);
    return ret;
  }
  OVDBG_LEVEL2("%s: Exit", __func__);
  return ret;
}

int32_t OverlayItemStaticImage::UpdateAndDraw() {
  // Nothing to update, contents are static.
  // Never marked as dirty.
  return OK;
}

void OverlayItemStaticImage::GetDrawInfo(uint32_t targetWidth,
                                         uint32_t targetHeight,
                                         DrawInfo* draw_info) {

  OVDBG_LEVEL2("%s: Enter", __func__);
  draw_info->width  = width_;
  draw_info->height = height_;
  int32_t xMargin = targetWidth * OVERLAYITEM_X_MARGIN_PERCENT/100;
  int32_t yMargin = targetHeight * OVERLAYITEM_Y_MARGIN_PERCENT/100;
  int32_t x = 0;
  int32_t y = 0;

  switch (location_type_) {
    case OverlayLocationType::kTopLeft:
      x = xMargin;
      y = yMargin;
      break;
    case OverlayLocationType::kTopRight:
      x = targetWidth - (width_ + xMargin);
      y = yMargin;
      break;
    case OverlayLocationType::kCenter:
      x = (targetWidth - width_)/2;
      y = (targetHeight - height_)/2;
      break;
    case OverlayLocationType::kBottomLeft:
      x = xMargin;
      y = targetHeight - (height_ + yMargin);
      break;
    case OverlayLocationType::kBottomRight:
      x = targetWidth - (width_ + xMargin);
      y = targetHeight - (height_ + yMargin);
      break;
    case OverlayLocationType::kNone:
    default:
      x = x_;
      y = y_;
      break;
  }
  draw_info->x            = x;
  draw_info->y            = y;
  draw_info->c2dSurfaceId = c2dsurface_id_;

  OVDBG_LEVEL2("%s: Exit", __func__);
}

void OverlayItemStaticImage::GetParameters(OverlayParam& param) {

  OVDBG_LEVEL2("%s:Enter ",__func__);
  param.type             = OverlayType::kStaticImage;
  param.location         = location_type_;
  param.image_info.width  = width_;
  param.image_info.height = height_;
  std::string str(image_path_.string());
  str.copy(param.image_info.image_location, image_path_.length());
  OVDBG_LEVEL2("%s:Exit ",__func__);
}

int32_t OverlayItemStaticImage::UpdateParameters(OverlayParam& param) {

  OVDBG_LEVEL2("%s:Enter ",__func__);
  int32_t ret = 0;

  if(strcmp(image_path_.string(), param.image_info.image_location) != 0) {
    OVDBG_ERROR("%s: Image Path Can't be changed at run time!!", __func__);
    return BAD_VALUE;
  }

  if(param.image_info.width <= 0 || param.image_info.height <= 0) {
    OVDBG_ERROR("%s: Image Width & Height is not correct!", __func__);
    return BAD_VALUE;
  }

  location_type_ = param.location;
  width_        = param.image_info.width;
  height_       = param.image_info.height;

  OVDBG_LEVEL2("%s:Exit ",__func__);
  return ret;
}

int32_t OverlayItemStaticImage::CreateSurface() {

  OVDBG_LEVEL2("%s:Enter ",__func__);
  int32_t   ret = 0;
  uint32_t size = width_ * height_ * 4;

  IonMemInfo mem_info;
  memset(&mem_info, 0x0, sizeof(IonMemInfo));

  ret = AllocateIonMemory(mem_info, size);
  if(0 != ret) {
    OVDBG_ERROR("%s:AllocateIonMemory failed",__func__);
    return ret;
  }
  uint32_t* pixels = (uint32_t*)mem_info.vaddr;

  //Load raw logo image file.
  FILE *file = 0;
  size_t bytes;

  file = fopen(image_path_.string(), "rb");
  if(file) {
    bytes = fread(pixels, 1, size, file);
    OVDBG_INFO("%s: Total btyes = %d",__func__,bytes);
    if(bytes != size) {
      OVDBG_ERROR("%s: Raw file format is not correct",__func__);
      fclose(file);
      goto ERROR;
    }
    fclose(file);
  } else {
    OVDBG_ERROR("%s: (%s)File open Failed!!",__func__, image_path_.string());
    goto ERROR;
  }

  //Map ARGB ION buffer to GPU.
  ret = c2dMapAddr(mem_info.fd, mem_info.vaddr, mem_info.size, 0,
                   KGSL_USER_MEM_TYPE_ION, &gpu_addr_);
  if(ret != C2D_STATUS_OK) {
    OVDBG_ERROR("%s: c2dMapAddr failed!",__func__);
    goto ERROR;
  }

  C2D_RGB_SURFACE_DEF c2dSurfaceDef;
  c2dSurfaceDef.format = C2D_FORMAT_SWAP_ENDIANNESS| C2D_COLOR_FORMAT_8888_RGBA;
  c2dSurfaceDef.width  = width_;
  c2dSurfaceDef.height = height_;
  c2dSurfaceDef.buffer = mem_info.vaddr;
  c2dSurfaceDef.phys   = gpu_addr_;
  c2dSurfaceDef.stride = width_ * 4;

  //Create source c2d surface.
  ret = c2dCreateSurface(&c2dsurface_id_, C2D_SOURCE,
                         (C2D_SURFACE_TYPE)(C2D_SURFACE_RGB_HOST
                         |C2D_SURFACE_WITH_PHYS), &c2dSurfaceDef);
  if(ret != C2D_STATUS_OK) {
    OVDBG_ERROR("%s: c2dCreateSurface failed!",__func__);
    goto ERROR;
  }
  ion_fd_        = mem_info.fd;
  vaddr_        = mem_info.vaddr;
  size_         = mem_info.size;
  handle_data_   = mem_info.handle_data;

  OVDBG_LEVEL2("%s: Exit ",__func__);
  return ret;
ERROR:
  ioctl(ion_device_, ION_IOC_FREE, &handle_data_);
  close(ion_fd_);
  ion_fd_ = -1;
  return ret;
}

OverlayItemDateAndTime::OverlayItemDateAndTime(int32_t ion_device)
    :OverlayItem(ion_device) {
  OVDBG_LEVEL2("%s:Enter ", __func__);
  memset(&date_time_type_, 0x0, sizeof date_time_type_);
  date_time_type_.time_format = OverlayTimeFormatType::kHHMM_24HR;
  date_time_type_.date_format = OverlayDateFormatType::kMMDDYYYY;
  type_                     = OverlayType::kDateType;
  OVDBG_LEVEL2("%s:Exit", __func__);
}

OverlayItemDateAndTime::~OverlayItemDateAndTime() {
  OVDBG_LEVEL2("%s:Enter ", __func__);
  OVDBG_LEVEL2("%s:Exit ", __func__);
}

int32_t OverlayItemDateAndTime::Init(OverlayParam& param) {

  OVDBG_LEVEL2("%s: Enter", __func__);
  location_type_ = param.location;
  text_color_    = param.text_color;

  date_time_type_.date_format = param.date_time.date_format;
  date_time_type_.time_format = param.date_time.time_format;
  width_  = DATETIME_TEXT_BUF_WIDTH;
  height_ = DATETIME_TEXT_BUF_HEIGHT;

  auto ret = CreateSurface();
  if(ret != 0) {
    OVDBG_ERROR("%s: createLogoSurface failed!", __func__);
    return ret;
  }
  OVDBG_LEVEL2("%s: Exit", __func__);
  return ret;
}

int32_t OverlayItemDateAndTime::UpdateAndDraw() {

  OVDBG_LEVEL2("%s: Enter", __func__);
  int32_t ret = 0;
  if(!dirty_)
      return ret;
#if USE_SKIA

#ifndef DEBUG_BACKGROUND_SURFACE
  canvas_->clear(SK_AlphaOPAQUE);
#else
  canvas_->clear(SK_ColorDKGRAY);
#endif

  SkPaint paint;
  paint.setColor(text_color_);
  paint.setTextSize(SkIntToScalar(DATETIME_PIXEL_SIZE));
  paint.setAntiAlias(true);
  paint.setTextScaleX(1);

  struct timeval tv;
  time_t nowtime;
  struct tm *time;
  char dateBuf[40];
  char timeBuf[40];

  gettimeofday(&tv, NULL);
  nowtime = tv.tv_sec;
  time = localtime(&nowtime);

  switch(date_time_type_.date_format) {
    case OverlayTimeFormatType::kYYYYMMDD:
      strftime(dateBuf, sizeof dateBuf, "%Y/%m/%d", time);
      break;
    case OverlayTimeFormatType::kMMDDYYYY:
    default:
      strftime(dateBuf, sizeof dateBuf, "%m/%d/%Y", time);
      break;
  }
  switch(date_time_type_.time_format) {
    case OverlayTimeFormatType::kHHMMSS_24HR:
      strftime(timeBuf, sizeof timeBuf, "%H:%M:%S", time);
      break;
    case OverlayTimeFormatType::kHHMMSS_AMPM:
      strftime(timeBuf, sizeof timeBuf, "%r", time);
      break;
    case OverlayTimeFormatType::kHHMM_24HR:
      strftime(timeBuf, sizeof timeBuf, "%H:%M", time);
      break;
    case OverlayTimeFormatType::kHHMM_AMPM:
    default:
      strftime(timeBuf, sizeof timeBuf, "%I:%M %p", time);
      break;
  }
  OVDBG_LEVEL2("%s: date:time (%s:%s)", __func__, dateBuf, timeBuf);

  int32_t dateLen = strlen(dateBuf);
  int32_t timeLen = strlen(timeBuf);

  //(0,0) is at topleft corner of skia buffer.
  int32_t xDate = 0;
  int32_t yDate = DATETIME_TEXT_BUF_HEIGHT/2;
  SkString dateText(dateBuf, dateLen);
  canvas_->drawText(dateText.c_str(), dateText.size(), xDate, yDate, paint);

  SkString timeText(timeBuf, timeLen);
  int32_t perCharSize = DATETIME_TEXT_BUF_WIDTH/dateText.size();
  int32_t xTime = (DATETIME_TEXT_BUF_WIDTH - (timeText.size() * perCharSize));
  xTime = xTime > 0 ? (xTime) : 0;
  int32_t yTime = DATETIME_TEXT_BUF_HEIGHT - DATETIME_PIXEL_SIZE/2;
  canvas_->drawText(timeText.c_str(), timeText.size(), xTime, yTime, paint);
  canvas_->flush();
  usleep(1000);
#endif
  MarkDirty(true);
  OVDBG_LEVEL2("%s: Exit", __func__);
  return ret;
}

void OverlayItemDateAndTime::GetDrawInfo(uint32_t targetWidth,
                                         uint32_t targetHeight,
                                         DrawInfo* draw_info) {

  OVDBG_LEVEL2("%s:Enter ",__func__);
  draw_info->width  = targetWidth * DATETIME_TARGET_WIDTH_PERCENT/100;
  draw_info->height = targetHeight * DATETIME_TARGET_HEIGHT_PERCENT/100;

  int32_t xMargin = targetWidth * OVERLAYITEM_X_MARGIN_PERCENT/100;
  int32_t yMargin = targetHeight * OVERLAYITEM_Y_MARGIN_PERCENT/100;
  int32_t x = 0;
  int32_t y = 0;

  //(0,0) is at topleft corner.
  switch (location_type_) {
    case OverlayLocationType::kTopLeft:
      x = xMargin;
      y = yMargin;
      break;
    case OverlayLocationType::kTopRight:
      x = targetWidth - (draw_info->width + xMargin);
      y = yMargin;
      break;
    case OverlayLocationType::kCenter:
      x = (targetWidth - draw_info->width)/2;
      y = (targetHeight - draw_info->height)/2;
      break;
    case OverlayLocationType::kBottomLeft:
      x = xMargin;
      y = targetHeight - (draw_info->height + yMargin);
      break;
    case OverlayLocationType::kBottomRight:
      x = targetWidth - (draw_info->width + xMargin);
      y = targetHeight - (draw_info->height + yMargin);
      break;
    case OverlayLocationType::kNone:
    default:
      break;
  }
  draw_info->x            = x;
  draw_info->y            = y;
  draw_info->c2dSurfaceId = c2dsurface_id_;
  OVDBG_LEVEL2("%s:Exit ",__func__);
}

void OverlayItemDateAndTime::GetParameters(OverlayParam& param) {

  OVDBG_LEVEL2("%s:Enter ",__func__);
  param.type      = OverlayType::kDateType;
  param.location  = location_type_;
  param.text_color = text_color_;
  param.date_time.date_format = date_time_type_.date_format;
  param.date_time.time_format = date_time_type_.time_format;
  OVDBG_LEVEL2("%s:Exit ",__func__);
}

int32_t OverlayItemDateAndTime::UpdateParameters(OverlayParam& param) {

  OVDBG_LEVEL2("%s:Enter ",__func__);
  int32_t ret = 0;
  location_type_ = param.location;
  text_color_    = param.text_color;

  date_time_type_.date_format = param.date_time.date_format;
  date_time_type_.time_format = param.date_time.time_format;
  OVDBG_LEVEL2("%s:Exit ",__func__);
  return ret;
}

int32_t OverlayItemDateAndTime::CreateSurface() {

  OVDBG_LEVEL2("%s: Enter", __func__);
  int32_t ret = 0;
  int32_t size = width_ * height_ * 4;
  IonMemInfo mem_info;
  memset(&mem_info, 0x0, sizeof(IonMemInfo));

  ret = AllocateIonMemory(mem_info, size);
  if(0 != ret) {
    OVDBG_ERROR("%s:AllocateIonMemory failed",__func__);
    return ret;
  }
  void* pixels = mem_info.vaddr;
  OVDBG_LEVEL1("%s: ION memory allocated fd = %d",__func__,mem_info.fd);

#if USE_SKIA
  //Create Skia canvas outof ION memory.
  SkImageInfo imageInfo;
  memset(&imageInfo, 0x0, sizeof(imageInfo));
  imageInfo.fWidth     = width_;
  imageInfo.fHeight    = height_;
  imageInfo.fColorType = kRGBA_8888_SkColorType;
  imageInfo.fAlphaType = kPremul_SkAlphaType;

  canvas_ = SkCanvas::NewRasterDirect(imageInfo, mem_info.vaddr,
                                      width_ *4);
  if(!canvas_) {
    OVDBG_ERROR("%s: Skia Creation failed!!",__func__);
    goto ERROR;
  }
#endif
  //Draw system time on Skia canvas.
  UpdateAndDraw();

  //Setup c2d.
  ret = c2dMapAddr(mem_info.fd, mem_info.vaddr, mem_info.size,
                     0, KGSL_USER_MEM_TYPE_ION, &gpu_addr_);
  if(ret != C2D_STATUS_OK) {
    OVDBG_ERROR("%s: c2dMapAddr failed!",__func__);
    goto ERROR;
  }

  C2D_RGB_SURFACE_DEF c2dSurfaceDef;
  c2dSurfaceDef.format = C2D_FORMAT_SWAP_ENDIANNESS| C2D_COLOR_FORMAT_8888_RGBA;
  c2dSurfaceDef.width  = width_;
  c2dSurfaceDef.height = height_;
  c2dSurfaceDef.buffer = mem_info.vaddr;
  c2dSurfaceDef.phys   = gpu_addr_;
  c2dSurfaceDef.stride = width_ * 4;

  //Create source c2d surface.
  ret = c2dCreateSurface(&c2dsurface_id_, C2D_SOURCE,
                         (C2D_SURFACE_TYPE)(C2D_SURFACE_RGB_HOST
                         |C2D_SURFACE_WITH_PHYS), &c2dSurfaceDef);
  if(ret != C2D_STATUS_OK) {
    OVDBG_ERROR("%s: c2dCreateSurface failed!",__func__);
    goto ERROR;
  }

  ion_fd_        = mem_info.fd;
  vaddr_        = mem_info.vaddr;
  size_         = mem_info.size;
  handle_data_   = mem_info.handle_data;

  OVDBG_LEVEL2("%s: Exit", __func__);
  return ret;
ERROR:
  ioctl(ion_device_, ION_IOC_FREE, &handle_data_);
  close(ion_fd_);
  ion_fd_ = -1;
  return ret;
}

OverlayItemBoundingBox::OverlayItemBoundingBox(int32_t ion_device)
    :OverlayItem(ion_device), bbox_name_(), text_height_(0) {
  OVDBG_INFO("%s: Enter", __func__);
  type_ = OverlayType::kBoundingBox;
  OVDBG_INFO("%s: Exit", __func__);
}

OverlayItemBoundingBox::~OverlayItemBoundingBox() {
  OVDBG_INFO("%s: Enter", __func__);
  bbox_name_.clear();
  OVDBG_INFO("%s: Exit", __func__);
}

int32_t OverlayItemBoundingBox::Init(OverlayParam& param) {

  OVDBG_LEVEL2("%s: Enter", __func__);
  if((param.bounding_box.width <= 0) || (param.bounding_box.height <= 0)) {
    return BAD_VALUE;
  }
  if(param.bounding_box.start_x < 0 || param.bounding_box.start_y < 0) {
    return BAD_VALUE;
  }

  x_          = param.bounding_box.start_x;
  y_          = param.bounding_box.start_y;
  width_      = param.bounding_box.width;
  height_     = param.bounding_box.height;
  bbox_color_  = param.text_color;

  int32_t textLen = strlen(param.bounding_box.box_name);

  int32_t textLimit = std::min(textLen + 1, BOUNDING_BOX_TEXT_LIMIT);
  bbox_name_.setTo(param.bounding_box.box_name, textLimit);
  auto ret = CreateSurface();
  if(ret != 0) {
    OVDBG_ERROR("%s: CreateSurface failed!", __func__);
    return NO_INIT;
  }
  OVDBG_LEVEL2("%s: Exit", __func__);
  return ret;
}

int32_t OverlayItemBoundingBox::UpdateAndDraw() {

  OVDBG_LEVEL2("%s: Enter ", __func__);
  int32_t ret = 0;

  if(!dirty_) {
    OVDBG_LEVEL1("%s: Item is not dirty! Don't draw!", __func__);
    return ret;
  }
#if USE_SKIA
  if (width_ > 0 && height_ > 0) {

#ifndef DEBUG_BACKGROUND_SURFACE
  canvas_->clear(SK_AlphaOPAQUE);
#else
  canvas_->clear(SK_ColorDKGRAY);
#endif
    //  Bounding Box and text are drawn on same Skia buffer.
    //  ----------
    //  | TEXT   |
    //  ----------
    //  |        |
    //  |  BOX   |
    //  |        |
    //  ----------
    SkPaint paintBox, paintText;
    paintText.setColor(bbox_color_);
    paintBox.setColor(bbox_color_);

    paintText.setTextSize(SkIntToScalar(BOUNDING_BOX_TEXT_SIZE));
    paintText.setAntiAlias(true);

    paintBox.setStrokeWidth(BOUNDING_BOX_STROKE_WIDTH);
    paintBox.setStyle(SkPaint::kStroke_Style);

    int32_t xText = 0, yText = 0;
    int32_t xBBox = 0, yBBox = 0;
    if(bbox_name_.length() > 1) {
      SkString text(bbox_name_.string(), bbox_name_.length());
      // Text size is always 20% of buffer height.
      yText  = BOUNDING_BOX_BUF_HEIGHT * BOUNDING_BOX_TEXT_PERCENT/100;
      // Margin between text and bouding box rect.
      yText  = yText - BOUNDING_BOX_TEXT_MARGIN;
      canvas_->drawText(text.c_str(), text.size(), 0, yText, paintText);
    }
    yBBox = yText > 0 ? BOUNDING_BOX_TEXT_SIZE : 0;
    int32_t boxWidth  = BOUNDING_BOX_BUF_WIDTH;
    int32_t boxHeight = BOUNDING_BOX_BUF_HEIGHT - yBBox;
    text_height_ = yText;
    canvas_->drawRect(SkRect::MakeXYWH(xBBox, yBBox, boxWidth, boxHeight),
                      paintBox);
    canvas_->flush();
    usleep(3000);
  }
#endif
  MarkDirty(false);
  OVDBG_LEVEL2("%s: Exit", __func__);
  return ret;
}

void OverlayItemBoundingBox::GetDrawInfo(uint32_t targetWidth,
                                         uint32_t targetHeight,
                                         DrawInfo* draw_info) {
  OVDBG_LEVEL2("%s: Enter", __func__);
  //Cut Text portion while scaling up bounding box to stream size.
  int32_t textPortion = BOUNDING_BOX_BUF_HEIGHT * BOUNDING_BOX_TEXT_PERCENT/100;
  textPortion        += BOUNDING_BOX_TEXT_MARGIN;
  int32_t ratio          = height_ / BOUNDING_BOX_BUF_HEIGHT;
  draw_info->x            = x_;
  draw_info->y            = y_ - (ratio * textPortion);
  draw_info->width        = width_;
  draw_info->height       = height_ + (ratio * textPortion);
  draw_info->c2dSurfaceId = c2dsurface_id_;
  OVDBG_LEVEL2("%s: Exit", __func__);
}

void OverlayItemBoundingBox::GetParameters(OverlayParam& param) {

  OVDBG_LEVEL2("%s:Enter ",__func__);
  param.type      = OverlayType::kBoundingBox;
  param.location  = OverlayLocationType::kNone;
  param.text_color = bbox_color_;
  param.bounding_box.start_x = x_;
  param.bounding_box.start_y = y_;
  param.bounding_box.width  = width_;
  param.bounding_box.height = height_;
  std::string str(bbox_name_.string());
  str.copy(param.bounding_box.box_name, bbox_name_.length());
  OVDBG_LEVEL2("%s:Exit ",__func__);
}

int32_t OverlayItemBoundingBox::UpdateParameters(OverlayParam& param) {

  OVDBG_LEVEL2("%s:Enter ",__func__);
  int32_t ret = 0;

  if((param.bounding_box.width <= 0) || (param.bounding_box.height <= 0)) {
      return BAD_VALUE;
  }
  if(param.bounding_box.start_x < 0 || param.bounding_box.start_y < 0) {
      return BAD_VALUE;
  }
  x_          = param.bounding_box.start_x;
  y_          = param.bounding_box.start_y;
  width_      = param.bounding_box.width;
  height_     = param.bounding_box.height;
  bbox_color_  = param.text_color;

  bbox_name_.clear();
  int32_t textLen = strlen(param.bounding_box.box_name);

  int32_t textLimit = std::min(textLen + 1, BOUNDING_BOX_TEXT_LIMIT);
  bbox_name_.setTo(param.bounding_box.box_name, textLimit);
  MarkDirty(true);
  OVDBG_LEVEL2("%s:Exit ",__func__);
  return ret;
}

int32_t OverlayItemBoundingBox::CreateSurface() {

  OVDBG_LEVEL2("%s: Enter", __func__);
  int32_t size = BOUNDING_BOX_BUF_WIDTH * BOUNDING_BOX_BUF_HEIGHT * 4;

  IonMemInfo mem_info;
  memset(&mem_info, 0x0, sizeof(IonMemInfo));
  auto ret = AllocateIonMemory(mem_info, size);
  if(0 != ret) {
    OVDBG_ERROR("%s:AllocateIonMemory failed",__func__);
    return ret;
  }
  void* pixels = mem_info.vaddr;
  OVDBG_LEVEL1("%s: Ion memory allocated fd(%d)", __func__, mem_info.fd);
#if USE_SKIA
  //Create Skia canvas outof ION memory.
  SkImageInfo imageInfo;
  memset(&imageInfo, 0x0, sizeof(imageInfo));
  imageInfo.fWidth     = BOUNDING_BOX_BUF_WIDTH;
  imageInfo.fHeight    = BOUNDING_BOX_BUF_HEIGHT;
  imageInfo.fColorType = kRGBA_8888_SkColorType;
  imageInfo.fAlphaType = kPremul_SkAlphaType;

  canvas_ = SkCanvas::NewRasterDirect(imageInfo, mem_info.vaddr,
                                      BOUNDING_BOX_BUF_WIDTH *4);
  if(!canvas_) {
    OVDBG_ERROR("%s: Skia Creation failed!!", __func__);
    goto ERROR;
  }
#endif
  //Setup c2d.
  ret = c2dMapAddr(mem_info.fd, mem_info.vaddr, mem_info.size, 0,
                   KGSL_USER_MEM_TYPE_ION, &gpu_addr_);
  if(ret != C2D_STATUS_OK) {
    OVDBG_ERROR("%s: c2dMapAddr failed!", __func__);
    goto ERROR;
  }

  C2D_RGB_SURFACE_DEF c2dSurfaceDef;
  c2dSurfaceDef.format = C2D_FORMAT_SWAP_ENDIANNESS | C2D_COLOR_FORMAT_8888_RGBA;
  c2dSurfaceDef.width  = BOUNDING_BOX_BUF_WIDTH;
  c2dSurfaceDef.height = BOUNDING_BOX_BUF_HEIGHT;
  c2dSurfaceDef.buffer = mem_info.vaddr;
  c2dSurfaceDef.phys   = gpu_addr_;
  c2dSurfaceDef.stride = BOUNDING_BOX_BUF_WIDTH * 4;

  //Create source c2d surface.
  ret = c2dCreateSurface(&c2dsurface_id_, C2D_SOURCE,
                         (C2D_SURFACE_TYPE)(C2D_SURFACE_RGB_HOST
                         |C2D_SURFACE_WITH_PHYS), &c2dSurfaceDef);
  if(ret != C2D_STATUS_OK) {
    OVDBG_ERROR("%s: c2dCreateSurface failed!", __func__);
    goto ERROR;
  }

  ion_fd_        = mem_info.fd;
  vaddr_        = mem_info.vaddr;
  size_         = mem_info.size;
  handle_data_   = mem_info.handle_data;

  OVDBG_LEVEL2("%s: Exit", __func__);
  return ret;
ERROR:
  ioctl(ion_device_, ION_IOC_FREE, &handle_data_);
  close(ion_fd_);
  ion_fd_ = -1;
  return ret;
}

OverlayItemText::OverlayItemText(int32_t ion_device)
    :OverlayItem(ion_device), text_() {
  OVDBG_LEVEL2("%s:Enter ", __func__);
  type_ = OverlayType::kUserText;
  OVDBG_LEVEL2("%s:Exit ", __func__);
}

OverlayItemText::~OverlayItemText() {
  OVDBG_LEVEL2("%s:Enter ", __func__);
  text_.clear();
  OVDBG_LEVEL2("%s:Exit ", __func__);
}

int32_t OverlayItemText::Init(OverlayParam& param) {

  OVDBG_LEVEL2("%s: Enter", __func__);

  location_type_ = param.location;
  text_color_    = param.text_color;

  text_.setTo(param.user_text, strlen(param.user_text) + 1);
  width_  = TEXT_BUF_WIDTH;
  height_ = TEXT_BUF_HEIGHT;

  auto ret = CreateSurface();
  if(ret != 0) {
    OVDBG_ERROR("%s: CreateSurface failed!", __func__);
    return ret;
  }
  OVDBG_LEVEL2("%s: Exit", __func__);
  return ret;
}

int32_t OverlayItemText::UpdateAndDraw() {

  OVDBG_LEVEL2("%s: Enter", __func__);
  int32_t ret = 0;

  if(!dirty_)
    return ret;
#if USE_SKIA

#ifndef DEBUG_BACKGROUND_SURFACE
  canvas_->clear(SK_AlphaOPAQUE);
#else
  canvas_->clear(SK_ColorDKGRAY);
#endif

  SkPaint paint;
  paint.setColor(text_color_);
  paint.setTextSize(SkIntToScalar(TEXT_SIZE));
  paint.setAntiAlias(true);

  int32_t x = 0;
  int32_t y = TEXT_BUF_HEIGHT - TEXT_SIZE/2;
  SkString skText(text_.string(), text_.length());
  canvas_->drawText(skText.c_str(), skText.size(), 0, y, paint);
  canvas_->flush();
  usleep(1000);
#endif
  dirty_ = false;
  OVDBG_LEVEL2("%s: Exit", __func__);
  return ret;
}

void OverlayItemText::GetDrawInfo(uint32_t targetWidth,
                                  uint32_t targetHeight, DrawInfo* draw_info) {

  OVDBG_LEVEL2("%s: Enter", __func__);
  draw_info->width  = targetWidth * TEXT_TARGET_WIDTH_PERCENT/100;
  draw_info->height = targetHeight * TEXT_TARGET_HEIGHT_PERCENT/100;

  int32_t xMargin = targetWidth * OVERLAYITEM_X_MARGIN_PERCENT/100;
  int32_t yMargin = targetHeight * OVERLAYITEM_Y_MARGIN_PERCENT/100;
  int32_t x = 0;
  int32_t y = 0;

  // (0,0) is at topleft corner.
  switch (location_type_) {
    case OverlayLocationType::kTopLeft:
      x = xMargin;
      y = yMargin;
      break;
    case OverlayLocationType::kTopRight:
      x = targetWidth - (draw_info->width + xMargin);
      y = yMargin;
      break;
    case OverlayLocationType::kCenter:
      x = (targetWidth - draw_info->width)/2;
      y = (targetHeight - draw_info->height)/2;
      break;
    case OverlayLocationType::kBottomLeft:
      x = xMargin;
      y = targetHeight - (draw_info->height + yMargin);
      break;
    case OverlayLocationType::kBottomRight:
      x = targetWidth - (draw_info->width + xMargin);
      y = targetHeight - (draw_info->height + yMargin);
      break;
    case OverlayLocationType::kNone:
    default:
      x = x_;
      y = y_;
      break;
  }
  draw_info->x            = x;
  draw_info->y            = y;
  draw_info->c2dSurfaceId = c2dsurface_id_;

  OVDBG_LEVEL2("%s: Exit", __func__);
}

void OverlayItemText::GetParameters(OverlayParam& param) {

  OVDBG_LEVEL2("%s:Enter ",__func__);
  param.type      = OverlayType::kUserText;
  param.location  = location_type_;
  param.text_color = text_color_;
  std::string str(text_.string());
  str.copy(param.user_text, text_.length());
  OVDBG_LEVEL2("%s:Exit ",__func__);
}

int32_t OverlayItemText::UpdateParameters(OverlayParam& param) {

  OVDBG_LEVEL2("%s:Enter ",__func__);
  int32_t ret = 0;
  location_type_ = param.location;
  text_color_    = param.text_color;
  text_.clear();
  text_.setTo(param.user_text, strlen(param.user_text) + 1);
  MarkDirty(true);
  OVDBG_LEVEL2("%s:Exit ",__func__);
  return ret;
}

int32_t OverlayItemText::CreateSurface() {

  OVDBG_LEVEL2("%s: Enter", __func__);
  int32_t size = width_ * height_ * 4;
  IonMemInfo mem_info;
  memset(&mem_info, 0x0, sizeof(IonMemInfo));

  auto ret = AllocateIonMemory(mem_info, size);
  if(0 != ret) {
    OVDBG_ERROR("%s:AllocateIonMemory failed",__func__);
    return ret;
  }
  void* pixels = mem_info.vaddr;
  OVDBG_INFO("%s: Ion memory allocated fd = %d", __func__, mem_info.fd);
#if USE_SKIA
  //Create Skia canvas outof ION memory.
  SkImageInfo imageInfo;
  memset(&imageInfo, 0x0, sizeof(imageInfo));
  imageInfo.fWidth     = width_;
  imageInfo.fHeight    = height_;
  imageInfo.fColorType = kRGBA_8888_SkColorType;
  imageInfo.fAlphaType = kPremul_SkAlphaType;

  canvas_ = SkCanvas::NewRasterDirect(imageInfo, mem_info.vaddr,
                                      width_ *4);
  if(!canvas_) {
    OVDBG_ERROR("%s: Skia Creation failed!!",__func__);
    goto ERROR;
  }
#endif
  //Draw system time on Skia canvas.
  UpdateAndDraw();

  //Setup c2d.
  ret = c2dMapAddr(mem_info.fd, mem_info.vaddr, mem_info.size, 0,
                   KGSL_USER_MEM_TYPE_ION, &gpu_addr_);
  if(ret != C2D_STATUS_OK) {
    OVDBG_ERROR("%s: c2dMapAddr failed!",__func__);
    goto ERROR;
  }

  C2D_RGB_SURFACE_DEF c2dSurfaceDef;
  c2dSurfaceDef.format = C2D_FORMAT_SWAP_ENDIANNESS| C2D_COLOR_FORMAT_8888_RGBA;
  c2dSurfaceDef.width  = width_;
  c2dSurfaceDef.height = height_;
  c2dSurfaceDef.buffer = mem_info.vaddr;
  c2dSurfaceDef.phys   = gpu_addr_;
  c2dSurfaceDef.stride = width_ * 4;

  //Create source c2d surface.
  ret = c2dCreateSurface(&c2dsurface_id_, C2D_SOURCE,
                         (C2D_SURFACE_TYPE)(C2D_SURFACE_RGB_HOST
                         |C2D_SURFACE_WITH_PHYS), &c2dSurfaceDef);
  if(ret != C2D_STATUS_OK) {
    OVDBG_ERROR("%s: c2dCreateSurface failed!",__func__);
    goto ERROR;
  }

  ion_fd_        = mem_info.fd;
  vaddr_        = mem_info.vaddr;
  size_         = mem_info.size;
  handle_data_   = mem_info.handle_data;

  OVDBG_INFO("%s: Exit", __func__);
  return ret;

ERROR:
  ioctl(ion_device_, ION_IOC_FREE, &handle_data_);
  close(ion_fd_);
  ion_fd_ = -1;
  return ret;
}


OverlayItemPrivacyMask::OverlayItemPrivacyMask(int32_t ion_device)
    :OverlayItem(ion_device) {
  OVDBG_LEVEL2("%s: Enter", __func__);
  type_ = OverlayType::kPrivacyMask;
  OVDBG_LEVEL2("%s: Exit", __func__);
}

OverlayItemPrivacyMask::~OverlayItemPrivacyMask() {
  OVDBG_LEVEL2("%s: Enter", __func__);
  OVDBG_LEVEL2("%s: Exit", __func__);
}

int32_t OverlayItemPrivacyMask::Init(OverlayParam& param) {

  OVDBG_LEVEL2("%s: Enter", __func__);

  if((param.bounding_box.width <= 0) || (param.bounding_box.height <= 0)) {
    return BAD_VALUE;
  }
  if(param.bounding_box.start_x < 0 || param.bounding_box.start_y < 0) {
    return BAD_VALUE;
  }

  x_          = param.bounding_box.start_x;
  y_          = param.bounding_box.start_y;
  width_      = param.bounding_box.width;
  height_     = param.bounding_box.height;

  auto ret = CreateSurface();
  if(ret != 0) {
    OVDBG_ERROR("%s: CreateSurface failed!", __func__);
    return NO_INIT;
  }
  OVDBG_LEVEL2("%s: Exit", __func__);
  return ret;
}

int32_t OverlayItemPrivacyMask::UpdateAndDraw() {
  //Nothing to update, contents are static.
  //Never marked as dirty.
  return OK;
}

void OverlayItemPrivacyMask::GetDrawInfo(uint32_t targetWidth,
                                         uint32_t targetHeight,
                                         DrawInfo* draw_info) {

  OVDBG_LEVEL2("%s: Enter", __func__);
  draw_info->x            = x_;
  draw_info->y            = y_;
  draw_info->width        = width_;
  draw_info->height       = height_;
  draw_info->c2dSurfaceId = c2dsurface_id_;
  OVDBG_LEVEL2("%s: Exit", __func__);
}

void OverlayItemPrivacyMask::GetParameters(OverlayParam& param) {

  OVDBG_LEVEL2("%s:Enter ",__func__);
  param.type      = OverlayType::kPrivacyMask;
  param.location  = OverlayLocationType::kNone;
  param.bounding_box.start_x = x_;
  param.bounding_box.start_y = y_;
  param.bounding_box.width   = width_;
  param.bounding_box.height  = height_;
  OVDBG_LEVEL2("%s:Exit ",__func__);
}

int32_t OverlayItemPrivacyMask::UpdateParameters(OverlayParam& param) {

  OVDBG_LEVEL2("%s:Enter ",__func__);
  int32_t ret = 0;

  if((param.bounding_box.width <= 0) || (param.bounding_box.height <= 0)) {
    return BAD_VALUE;
  }
  if(param.bounding_box.start_x < 0 || param.bounding_box.start_y < 0) {
    return BAD_VALUE;
  }
  x_      = param.bounding_box.start_x;
  y_      = param.bounding_box.start_y;
  width_  = param.bounding_box.width;
  height_ = param.bounding_box.height;

  OVDBG_LEVEL2("%s:Exit ",__func__);
  return ret;
}

int32_t OverlayItemPrivacyMask::CreateSurface() {

  OVDBG_LEVEL2("%s: Enter", __func__);

  int32_t size = width_ * height_ * 4;
  uint32_t color = PRIVACY_MASK_COLOR;
  IonMemInfo mem_info;

  memset(&mem_info, 0x0, sizeof(IonMemInfo));
  auto ret = AllocateIonMemory(mem_info, size);
  if(0 != ret) {
    OVDBG_ERROR("%s:AllocateIonMemory failed",__func__);
    return ret;
  }
  void* pixels = mem_info.vaddr;
  OVDBG_LEVEL1("%s: Ion memory allocated fd(%d)", __func__, mem_info.fd);

#if USE_SKIA
  //Create Skia canvas outof ION memory.
  SkPaint paintBox;
  SkImageInfo imageInfo;
  memset(&imageInfo, 0x0, sizeof(imageInfo));
  imageInfo.fWidth     = width_;
  imageInfo.fHeight    = height_;
  imageInfo.fColorType = kRGBA_8888_SkColorType;
  imageInfo.fAlphaType = kPremul_SkAlphaType;

  canvas_ = SkCanvas::NewRasterDirect(imageInfo, mem_info.vaddr, width_ *4);
  if(!canvas_) {
    OVDBG_ERROR("%s: Skia Creation failed!!", __func__);
    goto ERROR;
  }

#ifndef DEBUG_BACKGROUND_SURFACE
  canvas_->clear(SK_AlphaOPAQUE);
#else
  canvas_->clear(SK_ColorDKGRAY);
#endif

  paintBox.setColor(color);
  paintBox.setStyle(SkPaint::kFill_Style);
  //For blurring effect
  paintBox.setMaskFilter(SkBlurMaskFilter::Create(kNormal_SkBlurStyle,5.0f, 0));
  OVDBG_LEVEL2(" x_ %d y_ %d width_ %d height_ %d",x_,y_,width_,height_);
  canvas_->drawRect(SkRect::MakeXYWH(0,0, width_, height_), paintBox);
  canvas_->flush();
#endif
  //Setup c2d.
  ret = c2dMapAddr(mem_info.fd, mem_info.vaddr, mem_info.size, 0,
                   KGSL_USER_MEM_TYPE_ION, &gpu_addr_);
  if(ret != C2D_STATUS_OK) {
    OVDBG_ERROR("%s: c2dMapAddr failed!", __func__);
    goto ERROR;
  }

  C2D_RGB_SURFACE_DEF c2dSurfaceDef;
  c2dSurfaceDef.format = C2D_FORMAT_SWAP_ENDIANNESS | C2D_COLOR_FORMAT_8888_RGBA;
  c2dSurfaceDef.width  = width_;
  c2dSurfaceDef.height = height_;
  c2dSurfaceDef.buffer = mem_info.vaddr;
  c2dSurfaceDef.phys   = gpu_addr_;
  c2dSurfaceDef.stride = width_*4;

  //Create source c2d surface.
  ret = c2dCreateSurface(&c2dsurface_id_, C2D_SOURCE,
                         (C2D_SURFACE_TYPE)(C2D_SURFACE_RGB_HOST
                         |C2D_SURFACE_WITH_PHYS), &c2dSurfaceDef);
  if(ret != C2D_STATUS_OK) {
    OVDBG_ERROR("%s: c2dCreateSurface failed!", __func__);
    goto ERROR;
   }

  ion_fd_        = mem_info.fd;
  vaddr_        = mem_info.vaddr;
  size_         = mem_info.size;
  handle_data_   = mem_info.handle_data;

  OVDBG_LEVEL2("%s: Exit", __func__);
  return ret;

ERROR:
  ioctl(ion_device_, ION_IOC_FREE, &handle_data_);
  close(ion_fd_);
  ion_fd_ = -1;
  return ret;
}

}; // namespace overlay

}; // namespace qmmf
