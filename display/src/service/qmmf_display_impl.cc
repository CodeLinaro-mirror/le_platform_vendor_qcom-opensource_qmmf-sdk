/*
* Copyright (c) 2016-2018, The Linux Foundation. All rights reserved.
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

#define LOG_TAG "DisplayImpl"

#include <cutils/properties.h>
#include <dlfcn.h>
#include <utils/KeyedVector.h>
#include <utils/List.h>
#include <utils/RefBase.h>
#include <hardware/hardware.h>
#include <sys/prctl.h>

#include "display/src/service/qmmf_display_impl.h"
#include "display/src/service/qmmf_display_sdm_buffer_sync_handler.h"

#ifndef QMMF_DISPLAY_INTF_v1
static const uint32_t kHwVSyncTimeoutUs = 100000;
#else
static const uint32_t kHwVSyncTimeoutUs = 400000;
#endif

namespace qmmf {

namespace display {

using std::chrono::duration_cast;
using std::chrono::high_resolution_clock;
using std::chrono::microseconds;
using std::cv_status;

DisplayImpl* DisplayImpl::instance_ = nullptr;
CoreInterface* DisplayImpl::core_intf_ = nullptr;

DisplayImpl* DisplayImpl::CreateDisplayCore() {
    QMMF_DEBUG("%s: Enter", __func__);

  if(!instance_) {

    instance_ = new DisplayImpl;
    if(!instance_) {
      QMMF_ERROR("%s: Can't Create Display Instance!", __func__);
      return nullptr;
    }

#ifndef TARGET_USES_GRALLOC1
    int32_t res;
    void *handle;
    struct hw_module_t *hmi;
    handle = dlopen(GRALLOC_MODULE_PATH, RTLD_NOW);
    if (handle == nullptr) {
      char const *err_str = dlerror();
      QMMF_ERROR("load: module=%s\n%s \n", GRALLOC_MODULE_PATH,
          err_str ? err_str : "unknown");
      free(instance_);
      instance_ = nullptr;
      return nullptr;
    }

    hmi = (struct hw_module_t *)dlsym(handle, HAL_MODULE_INFO_SYM_AS_STR);
    if (hmi == nullptr) {
      QMMF_ERROR("load: couldn't find symbol %s\n", HAL_MODULE_INFO_SYM_AS_STR);
      free(instance_);
      instance_ = nullptr;
      dlclose(handle);
      return nullptr;
    }

    if (strcmp(GRALLOC_HARDWARE_MODULE_ID, hmi->id) != 0) {
      QMMF_ERROR("load: id=%s != hmi->id=%s\n", GRALLOC_HARDWARE_MODULE_ID,
          hmi->id);
      return nullptr;
    }

    hmi->dso = handle;

    res = hmi->methods->open(hmi, GRALLOC_HARDWARE_GPU0,
                             (struct hw_device_t**)&instance_->gralloc_device_);
    if (0 != res) {
      QMMF_ERROR("%s: Could not open Gralloc module: %s (%d) \n", __func__,
                 strerror(-res), res);
    }

    QMMF_DEBUG("%s: Gralloc Module author: %s, version: %d name: %s\n", __func__,
        instance_->gralloc_device_->common.module->author,
        instance_->gralloc_device_->common.module->hal_api_version,
        instance_->gralloc_device_->common.module->name);
#endif
  }

  QMMF_DEBUG("%s: Display Instance Created Successfully(0x%p)",
      __func__, instance_);
  return instance_;
}
DisplayImpl::DisplayImpl()
  : current_handle_(0),
    unique_surface_id_(0),
    layer_stack_(nullptr),
    current_display_state_(DisplayState::kStateOff) {
  QMMF_DEBUG("%s: Enter", __func__);

  if(!core_intf_) {
#ifndef QMMF_DISPLAY_INTF_v1
    DisplayError error = CoreInterface::CreateCore(DisplayDebugHandlerV1::Get(),
        &buffer_allocator_, &buffer_sync_handler_, &core_intf_);
    is_first_commit_ = false;
#else
    DisplayError error = CoreInterface::CreateCore(&buffer_allocator_,
                                                   &buffer_sync_handler_,
                                                   nullptr,
                                                   &core_intf_);
    is_first_commit_ = true;
#endif
    if (!core_intf_) {
      QMMF_ERROR("%s: Display Core Initialization Failed. Error = %d",
          __func__, error);
    }

    QMMF_DEBUG("%s: Display Core Initialized Successfully!", __func__);

  }
  QMMF_DEBUG("%s: Exit", __func__);
}

DisplayImpl::~DisplayImpl() {

  QMMF_DEBUG("%s: Enter", __func__);

  if (display_client_info_map_.size() != 0)
    return;

  DisplayError error = CoreInterface::DestroyCore();
  if (error != kErrorNone) {
    QMMF_ERROR("%s() Display core de-initialization failed. Error = %d",
        __func__, error);
  }
  instance_->display_client_info_map_.clear();
  instance_ = nullptr;
  core_intf_ = nullptr;

  QMMF_DEBUG("%s: Exit (0x%p)", __func__, this);
}

status_t DisplayImpl::Connect() {
  QMMF_DEBUG("%s: Enter", __func__);
  int32_t ret = NO_ERROR;

  QMMF_DEBUG("%s: Exit", __func__);
  return ret;
}

status_t DisplayImpl::Disconnect() {
  QMMF_DEBUG("%s: Enter", __func__);
  int32_t ret = NO_ERROR;

  QMMF_DEBUG("%s: Exit", __func__);
  return ret;
}

status_t DisplayImpl::CreateDisplay(sp<RemoteCallBack>& remote_cb,
    DisplayType display_type, DisplayHandle* display_handle) {

  QMMF_DEBUG("%s: Enter", __func__);
  int32_t ret = NO_ERROR;

  std::unique_lock<std::mutex> lock(api_lock_);
  assert(remote_cb.get() != nullptr);

  // Create a new handle value.
  ++current_handle_;
  *display_handle = current_handle_;
  display_client_info_map_[current_handle_] = new DisplayClientInfo();
  DisplayClientInfo* display_client_info = display_client_info_map_[current_handle_];
  display_client_info->display_type = display_type;
  display_client_info->remote_cb = remote_cb;
  display_client_info->num_of_client_layers = 0;

  auto display_type_info = display_type_info_map_.find(display_type);
  if (display_type_info == display_type_info_map_.end()) {
    DisplayInterface* displayintf = nullptr;
    DisplayError error = core_intf_->CreateDisplay(static_cast<sdm::DisplayType>(display_type),
        static_cast<DisplayEventHandler*>(this), &displayintf);
    if (error != kErrorNone) {
      QMMF_ERROR("%s: Display Create Failed. Error = %d", __func__, error);
      return error;
    }
    assert(displayintf != nullptr);

    DisplayTypeInfo* displaytypeinfo = new DisplayTypeInfo();
    displaytypeinfo->display_intf = displayintf;
    displaytypeinfo->num_of_clients = 1;
    display_type_info_map_.insert({display_type, displaytypeinfo});

    error =  displayintf->SetCompositionState(
        static_cast<sdm::LayerComposition>(kCompositionGPU), false);
    if (error != kErrorNone) {
      QMMF_ERROR("%s: SetCompositionState Failed. Error = %d", __func__,
          error);
      return error;
    }

#ifndef QMMF_DISPLAY_INTF_v1
    error = displayintf->SetDisplayState(kStateOn);
    if (error != kErrorNone) {
      QMMF_ERROR("%s: SetDisplayState Failed. Error = %d", __func__, error);
      return error;
    }
#else
    int release_fence = -1;
    // Set display active
    error = displayintf->SetDisplayState(kStateOn, &release_fence);
    if (error != kErrorNone) {
      QMMF_ERROR("%s: SetDisplayState On Failed. Error = %d", __func__,
          error);
      return error;
    }
    if (release_fence > -1) {
      close(release_fence);
    }
#endif

    DisplayState current_state = GetDisplayState();
    SetDisplayState(DisplayState::kStateOn);
    displayintf->SetIdleTimeoutMs(0);
    error = displayintf->SetVSyncState(true);
    if (error != kErrorNone) {
      QMMF_ERROR("%s: SetVSyncState Failed. Error = %d", __func__, error);
      SetDisplayState(current_state);
      return error;
    }

    layer_stack_ =  new LayerStack();
    running_ = true;

    handle_vsync_thread_ = new std::thread(DisplayImpl::HandleVSyncThreadEntry,
        this);
    if (handle_vsync_thread_ == nullptr) {
      QMMF_ERROR("%s: Unable to create HandleVSync thread\n", __func__);
      return error;
    }
  } else  {
    QMMF_DEBUG("%s: Display Type already exists", __func__);
    display_type_info->second->num_of_clients++;
  }

  QMMF_DEBUG("%s: Display Created Successfully! display_handle:%d",
      __func__, *display_handle);

  QMMF_DEBUG("%s: Exit", __func__);
  return ret;
}

status_t DisplayImpl::DestroyDisplay(DisplayHandle display_handle) {
  int32_t ret = NO_ERROR;
  QMMF_DEBUG("%s: Enter", __func__);

  std::map<DisplayHandle, DisplayClientInfo*>::iterator display_client_info;
  std::map<DisplayType, DisplayTypeInfo*>::iterator display_type_info;
  {
    std::unique_lock<std::mutex> lock(api_lock_);
    display_client_info = display_client_info_map_.find(display_handle);
    if (display_client_info == display_client_info_map_.end()) {
      QMMF_ERROR("%s() no displayinfo %u", __func__, display_handle);
      return -EINVAL;
    }

    display_type_info = display_type_info_map_.find(
        display_client_info->second->display_type);

    if (GetDisplayState() == DisplayState::kStateOff) {
      QMMF_ERROR("%s() Can't destroy the display handle(%u) since the display"
                 "state is OFF", __func__, display_handle);
      return ret;
    }

    if (display_type_info->second->num_of_clients > 1) {
      QMMF_ERROR("%s() Cant destroy.There are more users of this display. %d",
          __func__, static_cast<std::underlying_type<DisplayType>::type>
                               (display_client_info->second->display_type));
      display_type_info->second->num_of_clients--;
      delete display_client_info->second;
      display_client_info_map_.erase(display_handle);
      return ret;
    }

    if (handle_vsync_thread_ != nullptr) {
      running_ = false;
    }
  }

  if (handle_vsync_thread_ != nullptr) {
    handle_vsync_thread_->join();
    delete handle_vsync_thread_;
    handle_vsync_thread_ = nullptr;
  }

  {
    std::unique_lock<std::mutex> lock(api_lock_);
    assert(display_type_info->second->z_order_surface_id_map.size() == 0);

    DisplayInterface* displayintf = display_type_info->second->display_intf;
    assert(displayintf != nullptr);

    DisplayError error = displayintf->SetVSyncState(false);
    if (error != kErrorNone) {
      QMMF_ERROR("%s: SetVSyncState Failed. Error = %d", __func__,
          error);
    }

#ifndef QMMF_DISPLAY_INTF_v1
    DisplayState current_state = GetDisplayState();
    SetDisplayState(DisplayState::kStateOff);
    error = displayintf->SetDisplayState(kStateOff);
    if (error != kErrorNone) {
      QMMF_ERROR("%s: SetDisplayState Failed. Error = %d", __func__, error);
      SetDisplayState(current_state);
    }
#else
    int release_fence = -1;
    error = displayintf->SetDisplayState(kStateOff, &release_fence);
    if (error != kErrorNone) {
      QMMF_ERROR("%s: SetDisplayState Failed. Error = %d", __func__,
          error);
    }
    if(release_fence > -1) {
      close(release_fence);
    }
#endif

    assert(core_intf_ != nullptr);
    error = core_intf_->DestroyDisplay(displayintf);
    if (error != kErrorNone) {
      QMMF_ERROR("%s: Display Disconnect Failed. Error = %d", __func__,
          error);
      return error;
    }

    if(layer_stack_ != nullptr) {
      delete layer_stack_;
      layer_stack_ = nullptr;
    }
    if (display_type_info->second != nullptr) {
      delete display_type_info->second;
      display_type_info->second = nullptr;
    }
    display_type_info_map_.erase(display_client_info->second->display_type);
    if (display_client_info->second != nullptr) {
      delete display_client_info->second;
      display_client_info->second = nullptr;
    }
    display_client_info_map_.erase(display_handle);
  }
  QMMF_DEBUG("%s: Exit", __func__);
  return ret;
}

status_t DisplayImpl::CreateSurface(DisplayHandle display_handle,
    SurfaceConfig &surface_config, uint32_t* surface_id) {
  QMMF_DEBUG("%s: Enter", __func__);
  std::unique_lock<std::mutex> lock(api_lock_);

  int32_t ret = NO_ERROR;
  SurfaceParam surface_param;
  DisplayError error;

  surface_param.src_rect = { 0.0, 0.0, static_cast<float>(surface_config.width),
      static_cast<float>(surface_config.height) };
  if (surface_config.surface_transform.rotation == 90.0f ||
      surface_config.surface_transform.rotation == 270.0f) {
    surface_param.dst_rect = { 0.0, 0.0,
        static_cast<float>(surface_config.height),
        static_cast<float>(surface_config.width) };
  } else {
    surface_param.dst_rect = { 0.0, 0.0,
        static_cast<float>(surface_config.width),
        static_cast<float>(surface_config.height) };
  }

  surface_param.surface_blending = SurfaceBlending::kBlendingCoverage;
  surface_param.surface_flags.cursor = 0;
  surface_param.frame_rate = 30;
  surface_param.solid_fill_color = 0;

  auto display_client_info = display_client_info_map_.find(display_handle);
  if (display_client_info == display_client_info_map_.end()) {
    QMMF_ERROR("%s() no display_handle::%u", __func__, display_handle);
    return -EINVAL;
  }

  if (GetDisplayState() == DisplayState::kStateOff) {
    QMMF_ERROR("%s() Can't create surface for the display handle(%u) since the"
               " display state is OFF", __func__, display_handle);
    return ret;
  }

  auto display_type_info =
      display_type_info_map_.find(display_client_info->second->display_type);
  auto displayintf = display_type_info->second->display_intf;
  assert(displayintf != nullptr);

  SurfaceInfo* surfaceinfo = new SurfaceInfo();
  assert(surfaceinfo != nullptr);

  Layer* layer = AllocateLayer(display_handle, surface_id, surface_config.z_order);
  assert(layer != nullptr);

  surfaceinfo->layer=layer;
  {
    std::lock_guard<std::mutex> lock(surface_lock_);
    surface_info_map_.insert({*surface_id, surfaceinfo});
  }

  BufferInfo buffer_info;
  int32_t aligned_width, aligned_height;

  aligned_width = surface_config.width;
  aligned_height = surface_config.height;
  buffer_info.buffer_config.width = surface_config.width;
  buffer_info.buffer_config.height = surface_config.height;
  buffer_info.buffer_config.format =
      static_cast<LayerBufferFormat>(surface_config.format);
  buffer_info.buffer_config.buffer_count = 1;
  buffer_info.buffer_config.cache = surface_config.cache;
  buffer_info.alloc_buffer_info.fd = -1;
  buffer_info.alloc_buffer_info.stride = 0;
  buffer_info.alloc_buffer_info.size = 0;
  error = buffer_allocator_.GetBufferInfo(&buffer_info, aligned_width, aligned_height);
  if (error != kErrorNone) {
    QMMF_ERROR("%s: GetBufferInfo Failed. Error = %d",
        __func__, error);
  }

  layer->input_buffer.width = aligned_width;
  layer->input_buffer.height = aligned_height;
  layer->input_buffer.unaligned_width = surface_config.width;
  layer->input_buffer.unaligned_height = surface_config.height;
  layer->input_buffer.format =
      static_cast<LayerBufferFormat>(surface_config.format);
  SetRect(surface_param.dst_rect, &layer->dst_rect);
  SetRect(surface_param.src_rect, &layer->src_rect);
  layer->frame_rate = surface_param.frame_rate;
  layer->solid_fill_color = surface_param.solid_fill_color;
  layer->flags.solid_fill = 0;
  layer->flags.cursor = 0;
  layer->transform.rotation = surface_config.surface_transform.rotation;
  layer->transform.flip_horizontal =
      surface_config.surface_transform.flip_horizontal;
  layer->transform.flip_vertical =
      surface_config.surface_transform.flip_vertical;
  layer->plane_alpha = 0xFF;

  QMMF_DEBUG("%s: Buffer format: %d", __func__, layer->input_buffer.format);
  QMMF_DEBUG("%s: Color Primaries value: %d", __func__,
      layer->input_buffer.color_metadata.colorPrimaries);
  if(layer->input_buffer.format == kFormatYCbCr420SemiPlanarVenus ||
      layer->input_buffer.format == kFormatYCbCr420SPVenusUbwc) {
    layer->input_buffer.color_metadata.colorPrimaries = ColorPrimaries_BT601_6_525;
    layer->input_buffer.color_metadata.range = Range_Limited;
  }
  LayerStack* layer_stack = GetLayerStack(display_client_info->second->display_type,
      false);
  if (layer_stack == nullptr) {
    QMMF_ERROR("Layer Stack is null");
    delete surfaceinfo;
    FreeLayer(display_handle, *surface_id);
    return -ENODATA;
  }

  layer_stack->flags.flags=0;
  error = displayintf->Prepare(layer_stack);
  if (error != kErrorNone) {
    if (error == kErrorShutDown) {
    } else if (error != kErrorPermission) {
      QMMF_ERROR("%s: Prepare failed. Error = %d", __func__, error);
    }

    QMMF_DEBUG("%s: display_handle::%d surface_id::%u", __func__, display_handle,
        *surface_id);
    ret = FreeLayer(display_handle, *surface_id);
    if (ret != NO_ERROR) {
      QMMF_ERROR("%s: Failed to free surface_id::%u", __func__, *surface_id);
    }
    {
      std::lock_guard<std::mutex> lock(surface_lock_);
      surface_info_map_.erase(*surface_id);
    }
    delete surfaceinfo;
    return error;
  }

  if (!surface_config.use_buffer) {
    for(uint32_t i=0; i<surface_config.buffer_count;i++) {
      BufferInfo* buf_info = new BufferInfo();
      BufferState* buf_state = new BufferState(BufferStates::kStateFree);

      buf_info->buffer_config.width = surface_config.width;
      buf_info->buffer_config.height = surface_config.height;
      buf_info->buffer_config.format =
          static_cast<LayerBufferFormat>(surface_config.format);
      buf_info->buffer_config.buffer_count = 1;
      buf_info->buffer_config.cache = surface_config.cache;
      buf_info->alloc_buffer_info.fd = -1;
      buf_info->alloc_buffer_info.stride = 0;
      buf_info->alloc_buffer_info.size = 0;
      surfaceinfo->allocate_buffer_mode = !surface_config.use_buffer;
      error = buffer_allocator_.AllocateBuffer(buf_info);
      surfaceinfo->buffer_info.insert({buf_info->alloc_buffer_info.fd,
          buf_info});
      surfaceinfo->buffer_state.insert({buf_info->alloc_buffer_info.fd,
          buf_state});
      if (error != kErrorNone) {
        QMMF_ERROR("%s: AllocateBuffer Failed. Error = %d",
            __func__, error);
        return -ENOMEM;
      }
      QMMF_DEBUG("%s Surface id::%u Number of buffers to allocate::%u "
          "Service Allocated Buffer Ion_Fd::%d", __func__, *surface_id,
          surface_config.buffer_count, buf_info->alloc_buffer_info.fd);
    }
  }

  QMMF_DEBUG("%s: Exit", __func__);
  return ret;
}

status_t DisplayImpl::DestroySurface(DisplayHandle display_handle,
    const uint32_t surface_id) {

  QMMF_DEBUG("%s: Enter", __func__);
  std::unique_lock<std::mutex> lock(api_lock_);

  int32_t ret = NO_ERROR;
  auto display_client_info = display_client_info_map_.find(display_handle);
  if (display_client_info == display_client_info_map_.end()) {
    QMMF_ERROR("%s() no displayinfo %u", __func__, display_handle);
    return -EINVAL;
  }

  if (GetDisplayState() == DisplayState::kStateOff) {
    QMMF_ERROR("%s() Can't destroy surface for the display handle(%u)"
               " since the display state is OFF", __func__, display_handle);
    return ret;
  }

  assert(display_client_info->second != nullptr);

  if (display_client_info->second->num_of_client_layers == 0) {
    QMMF_ERROR("%s() All surfaces are already destoryed for display. %d",
        __func__, static_cast<std::underlying_type<DisplayType>::type>
                             (display_client_info->second->display_type));
  } else {
    std::map<uint32_t, SurfaceInfo*>::iterator surfaceinfo;
    {
      std::lock_guard<std::mutex> lock(surface_lock_);
      surfaceinfo = surface_info_map_.find(surface_id);
      if (surfaceinfo == surface_info_map_.end()) {
        QMMF_DEBUG("%s: surfaceinfo not found!", __func__);
        return -EINVAL;
      }
    }
    if (surfaceinfo->second->allocate_buffer_mode) {
      for (auto& buf_state: surfaceinfo->second->buffer_state) {
        if(buf_state.second->GetState() == BufferStates::kStateDequeued) {
          QMMF_ERROR("%s: Buffer is dequeued by client", __func__);
          return -EINVAL;
        }
      }
    }

    QMMF_DEBUG("%s: display_handle::%d surface_id::%u", __func__, display_handle,
        surface_id);
    ret = FreeLayer(display_handle, surface_id);
    if (ret != NO_ERROR) {
      QMMF_ERROR("%s: Failed to free surface_id::%u", __func__, surface_id);
      return ret;
    }

    for (auto& it : surfaceinfo->second->buffer_info) {
      if (surfaceinfo->second->allocate_buffer_mode) {
        QMMF_DEBUG("%s: Free alloc_buffer_info.fd ::%d", __func__, it.first);
        buffer_allocator_.FreeBuffer(it.second);
      }
      if (it.second != nullptr) {
        delete it.second;
        it.second = nullptr;
      }
    }

    for (auto& it : surfaceinfo->second->buffer_state) {
      if (it.second != nullptr) {
        delete it.second;
        it.second = nullptr;
      }
    }

    surfaceinfo->second->buffer_info.clear();
    surfaceinfo->second->buffer_state.clear();

    if (surfaceinfo->second != nullptr) {
      delete surfaceinfo->second;
      surfaceinfo->second = nullptr;
    }
    {
      std::lock_guard<std::mutex> lock(surface_lock_);
      surface_info_map_.erase(surfaceinfo);
    }
    latest_queued_buffer_info_map_.erase(surface_id);
  }

  QMMF_DEBUG("%s: Exit", __func__);
  return ret;
}

status_t DisplayImpl::DequeueSurfaceBuffer(DisplayHandle display_handle,
    const uint32_t surface_id, SurfaceBuffer &surface_buffer) {
  QMMF_DEBUG("%s: Enter for surface id: %u", __func__, surface_id);

  PrintBuffersState(surface_id);
  int32_t ret = NO_ERROR;
  std::unique_lock<std::mutex> lock(api_lock_);

  auto display_client_info = display_client_info_map_.find(display_handle);
  if (display_client_info == display_client_info_map_.end()) {
    QMMF_ERROR("%s() no displayinfo %u", __func__, display_handle);
    return -EINVAL;
  }
  surface_buffer.buf_id = -1;
  if (GetDisplayState() == DisplayState::kStateOff) {
    QMMF_ERROR("%s() Can't Dequeue Surface Buffer for the display handle(%u) "
               "since the display state is OFF for surface:%u", __func__,
               display_handle, surface_id);
    return ret;
  }

  assert(display_client_info->second->num_of_client_layers > 0);
  std::map<uint32_t, SurfaceInfo*>::iterator surfaceinfo;
  {
   std::lock_guard<std::mutex> lock(surface_lock_);
   surfaceinfo = surface_info_map_.find(surface_id);
  }
  assert(surfaceinfo->second != nullptr);

  for (auto it = surfaceinfo->second->buffer_state.begin();
          it != surfaceinfo->second->buffer_state.end(); ++it) {
      if (it->second->GetState() == BufferStates::kStateFree) {
      BufferStates state = it->second->SetState(BufferStates::kStateDequeued);
      if (state != BufferStates::kStateDequeued) {
        QMMF_ERROR("%s Could not Set state::%u of Buffer Ion_Fd::%d ", __func__,
            static_cast<std::underlying_type<BufferStates>::type>
                       (BufferStates::kStateDequeued), it->first);
        return -EPERM;
      }
      auto buffer_info = surfaceinfo->second->buffer_info.find(it->first);
      if (buffer_info != surfaceinfo->second->buffer_info.end()) {
        BufferInfo* bufferinfo = buffer_info->second;
        assert(bufferinfo != nullptr);
        //TBD for plane_info other than 0.
        surface_buffer.plane_info[0].width = bufferinfo->buffer_config.width;
        surface_buffer.plane_info[0].height = bufferinfo->buffer_config.height;
        surface_buffer.format =
            static_cast<SurfaceFormat>(bufferinfo->buffer_config.format);
        surface_buffer.plane_info[0].ion_fd = bufferinfo->alloc_buffer_info.fd;
        QMMF_DEBUG("%s State of Buffer Ion_Fd::%d, state::%u ",  __func__,
            surface_buffer.plane_info[0].ion_fd,
            static_cast<std::underlying_type<BufferStates>::type>
                       (it->second->GetState()));
        surface_buffer.buf_id = it->first;
        surface_buffer.plane_info[0].offset = 0;
        surface_buffer.plane_info[0].stride = bufferinfo->alloc_buffer_info.stride;
        surface_buffer.plane_info[0].size = bufferinfo->alloc_buffer_info.size/
            bufferinfo->buffer_config.buffer_count;
        surface_buffer.capacity = bufferinfo->alloc_buffer_info.size;
        break;
      }
    }
  }

  for (auto it = surfaceinfo->second->buffer_state.begin();
       it != surfaceinfo->second->buffer_state.end(); ++it) {
    QMMF_DEBUG("%s buf_id_use map buf_id::%u buf_id_use->second::0x%p ",
        __func__, it->first, it->second);
  }

  QMMF_DEBUG("%s: Exit for surface id: %u", __func__, surface_id);
  return ret;
}

status_t DisplayImpl::QueueSurfaceBuffer(DisplayHandle display_handle,
    const uint32_t surface_id, SurfaceBuffer &surface_buffer,
    SurfaceParam &surface_param) {

  QMMF_DEBUG("%s: Enter for surface id: %u", __func__, surface_id);
  PrintBuffersState(surface_id);
  std::unique_lock<std::mutex> lock(api_lock_);

  int32_t ret = NO_ERROR;
  auto display_client_info = display_client_info_map_.find(display_handle);
  if (display_client_info == display_client_info_map_.end()) {
    QMMF_ERROR("%s() no displayinfo %u", __func__, display_handle);
    return -EINVAL;
  }

  std::map<uint32_t, SurfaceInfo*>::iterator surfaceinfo;
  {
    std::lock_guard<std::mutex> lock(surface_lock_);
    surfaceinfo = surface_info_map_.find(surface_id);
    assert(surfaceinfo->second != nullptr);
  }

  if (GetDisplayState() == DisplayState::kStateOff) {
    auto buf_state =
        surfaceinfo->second->buffer_state.find(surface_buffer.buf_id);
    assert(buf_state != surfaceinfo->second->buffer_state.end());
    assert(buf_state->second != nullptr);
    if (buf_state->second->GetState() == BufferStates::kStateDequeued) {
      BufferStates state =
          buf_state->second->SetState(BufferStates::kStateFree);
      if (state != BufferStates::kStateFree) {
        QMMF_ERROR("%s Could not Set state::%u of Buffer Ion_Fd::%d", __func__,
                   static_cast<std::underlying_type<BufferStates>::type>(
                       BufferStates::kStateFree),
                   surface_buffer.plane_info[0].ion_fd);
        return -EPERM;
      }
      QMMF_DEBUG("%s: The Buffer ION_FD:%d has been set to state:%u", __func__,
                 surface_buffer.buf_id,
                 static_cast<std::underlying_type<BufferStates>::type>(
                     BufferStates::kStateFree));
    }
    for (auto prev_queued_buffer = surfaceinfo->second->buffer_state.begin();
         prev_queued_buffer != surfaceinfo->second->buffer_state.end();
         ++prev_queued_buffer) {
      if (prev_queued_buffer->second->GetState() ==
          BufferStates::kStateQueued) {
        BufferStates state =
            prev_queued_buffer->second->SetState(BufferStates::kStateFree);
        if (state != BufferStates::kStateFree) {
          QMMF_ERROR("%s Could not Set state::%u of Buffer Ion_Fd::%d",
                     __func__,
                     static_cast<std::underlying_type<BufferStates>::type>(
                         BufferStates::kStateFree),
                     prev_queued_buffer->first);
          return -EPERM;
        }
        QMMF_DEBUG("%s: The Buffer ION_FD:%d has been set to state:%u",
                   __func__, prev_queued_buffer->first,
                   static_cast<std::underlying_type<BufferStates>::type>(
                       BufferStates::kStateFree));
      }
    }
    QMMF_ERROR("%s() Can't Queue Surface Buffer for the display handle(%u)"
               " since the display state is OFF for surface:%u", __func__,
               display_handle, surface_id);
    return ret;
  }

  QueuedBufferInfo queued_buffer_info;
  memcpy(&queued_buffer_info.surface_buffer, &surface_buffer, sizeof(SurfaceBuffer));
  memcpy(&queued_buffer_info.surface_param, &surface_param, sizeof(SurfaceParam));

  if (surfaceinfo->second->allocate_buffer_mode) {
    auto buf_state = surfaceinfo->second->buffer_state.find(surface_buffer.buf_id);
    assert(buf_state != surfaceinfo->second->buffer_state.end());
    assert(buf_state->second != nullptr);
    if (buf_state->second->GetState() == BufferStates::kStateDequeued) {
      for (auto prev_queued_buffer = surfaceinfo->second->buffer_state.begin();
           prev_queued_buffer != surfaceinfo->second->buffer_state.end();
           ++prev_queued_buffer) {
        if (prev_queued_buffer->second->GetState() == BufferStates::kStateQueued) {
          BufferStates state =
              prev_queued_buffer->second->SetState(BufferStates::kStateFree);
          if (state != BufferStates::kStateFree) {
            QMMF_ERROR("%s Could not Set state::%u of Buffer Ion_Fd::%d", __func__,
                static_cast<std::underlying_type<BufferStates>::type>
                (BufferStates::kStateFree), prev_queued_buffer->first);
            return -EPERM;
          }
          QMMF_DEBUG("%s: The Buffer ION_FD:%d has been set to state:%u", __func__,
              prev_queued_buffer->first,
              static_cast<std::underlying_type<BufferStates>::type>
              (BufferStates::kStateFree));
          break;
        }
      }
      BufferStates state = buf_state->second->SetState(BufferStates::kStateQueued);
      if (state != BufferStates::kStateQueued) {
        QMMF_ERROR("%s Could not Set state::%u of Buffer Ion_Fd::%d", __func__,
            static_cast<std::underlying_type<BufferStates>::type>
            (BufferStates::kStateQueued), surface_buffer.plane_info[0].ion_fd);
        return -EPERM;
      } else {
        auto it_last_queued = latest_queued_buffer_info_map_.find(surface_id);
        if(it_last_queued != latest_queued_buffer_info_map_.end()) {
          latest_queued_buffer_info_map_.erase(surface_id);
        }
        latest_queued_buffer_info_map_.insert({surface_id, queued_buffer_info});
        QMMF_DEBUG("%s The Buffer ION_FD:%d has been set to state:%u",
            __func__, surface_buffer.plane_info[0].ion_fd,
            static_cast<std::underlying_type<BufferStates>::type>
            (BufferStates::kStateQueued));
      }
    }
  } else {
    std::map<int32_t, BufferState*>::iterator it =
        surfaceinfo->second->buffer_state.find(surface_buffer.buf_id);
    if (it != surfaceinfo->second->buffer_state.end()) {
      auto buffer_info = surfaceinfo->second->buffer_info.find(it->first);
      if (buffer_info != surfaceinfo->second->buffer_info.end()) {
        QMMF_DEBUG("%s Client Allocated it->first::%u surface_buffer.buf_id::%u ",
            __func__, it->first, surface_buffer.buf_id);
        BufferInfo* bufferinfo = buffer_info->second;
        if (bufferinfo) {
          bufferinfo->buffer_config.width =
              surface_buffer.plane_info[0].width;
          bufferinfo->buffer_config.height =
              surface_buffer.plane_info[0].height;
          bufferinfo->buffer_config.format =
              static_cast<LayerBufferFormat>(surface_buffer.format);;
          bufferinfo->alloc_buffer_info.stride =
              surface_buffer.plane_info[0].stride;
          bufferinfo->alloc_buffer_info.size =
              surface_buffer.plane_info[0].size;
          bufferinfo->alloc_buffer_info.fd =
              surface_buffer.plane_info[0].ion_fd;
          if (it->second->GetState() == BufferStates::kStateDequeued) {
            for (auto prev_queued_buffer = surfaceinfo->second->buffer_state.begin();
                 prev_queued_buffer != surfaceinfo->second->buffer_state.end();
                 ++prev_queued_buffer) {
              if (prev_queued_buffer->second->GetState() == BufferStates::kStateQueued) {
                BufferStates state =
                    prev_queued_buffer->second->SetState(BufferStates::kStateFree);
                if (state != BufferStates::kStateFree) {
                  QMMF_ERROR("%s Could not Set state::%u of Buffer Ion_Fd::%d", __func__,
                      static_cast<std::underlying_type<BufferStates>::type>
                      (BufferStates::kStateFree), prev_queued_buffer->first);
                  return -EPERM;
                }
                QMMF_DEBUG("%s: The Buffer ION_FD:%d has been set to state:%u", __func__,
                    prev_queued_buffer->first,
                    static_cast<std::underlying_type<BufferStates>::type>
                    (BufferStates::kStateFree));
                break;
              }
            }
            BufferStates state = it->second->SetState(BufferStates::kStateQueued);
            if (state != BufferStates::kStateQueued) {
              QMMF_ERROR("%s Could not Set state::%u of Buffer Ion_Fd::%d",
                  __func__, static_cast<std::underlying_type<BufferStates>::type>
                  (BufferStates::kStateQueued), surface_buffer.plane_info[0].ion_fd);
              return -EPERM;
            } else {
             auto it_last_queued = latest_queued_buffer_info_map_.find(surface_id);
              if(it_last_queued != latest_queued_buffer_info_map_.end()) {
                latest_queued_buffer_info_map_.erase(surface_id);
              }
              latest_queued_buffer_info_map_.insert({surface_id, queued_buffer_info});
              QMMF_DEBUG("%s The Buffer ION_FD:%d has been set to state:%u",
                __func__, surface_buffer.plane_info[0].ion_fd,
                static_cast<std::underlying_type<BufferStates>::type>
                (BufferStates::kStateQueued));
            }
          }
        }
      }
    }
    if (it == surfaceinfo->second->buffer_state.end()) {
      for (auto prev_queued_buffer = surfaceinfo->second->buffer_state.begin();
           prev_queued_buffer != surfaceinfo->second->buffer_state.end();
           ++prev_queued_buffer) {
        if (prev_queued_buffer->second->GetState() == BufferStates::kStateQueued) {
          BufferStates state =
              prev_queued_buffer->second->SetState(BufferStates::kStateFree);
          if (state != BufferStates::kStateFree) {
            QMMF_ERROR("%s Could not Set state::%u of Buffer Ion_Fd::%d", __func__,
                static_cast<std::underlying_type<BufferStates>::type>
                (BufferStates::kStateFree), prev_queued_buffer->first);
            return -EPERM;
          }
          QMMF_DEBUG("%s: The Buffer ION_FD:%d has been set to state:%u", __func__,
              prev_queued_buffer->first,
              static_cast<std::underlying_type<BufferStates>::type>
              (BufferStates::kStateFree));
          break;
        }
      }
      BufferInfo* bufferinfo = new BufferInfo();
      BufferState* bufferstate = new BufferState(BufferStates::kStateQueued);
      int32_t buf_id = surface_buffer.buf_id;
      bufferinfo->buffer_config.width =surface_buffer.plane_info[0].width;
      bufferinfo->buffer_config.height = surface_buffer.plane_info[0].height;
      bufferinfo->buffer_config.format =
          static_cast<LayerBufferFormat>(surface_buffer.format);
      bufferinfo->buffer_config.buffer_count = 1;
      bufferinfo->alloc_buffer_info.fd = surface_buffer.plane_info[0].ion_fd;
      QMMF_DEBUG("%s: State of Buffer Ion_Fd::%d has been set to state:%u",
          __func__, bufferinfo->alloc_buffer_info.fd,
          static_cast<std::underlying_type<BufferStates>::type>
         (BufferStates::kStateQueued));
      bufferinfo->alloc_buffer_info.stride =
          surface_buffer.plane_info[0].stride;
      bufferinfo->alloc_buffer_info.size = surface_buffer.plane_info[0].size;
      surfaceinfo->second->buffer_info.insert({buf_id, bufferinfo});
      surfaceinfo->second->buffer_state.insert({buf_id, bufferstate});
      auto it_last_queued = latest_queued_buffer_info_map_.find(surface_id);
      if(it_last_queued != latest_queued_buffer_info_map_.end()) {
        latest_queued_buffer_info_map_.erase(surface_id);
      }
      latest_queued_buffer_info_map_.insert({surface_id, queued_buffer_info});
      for (auto it = surfaceinfo->second->buffer_state.begin();
          it != surfaceinfo->second->buffer_state.end(); ++it) {
        QMMF_DEBUG("%s Client Allocated buf_id_use map buf_id::%u "
            "buf_id_use->second::0x%p ", __func__, it->first, it->second);
      }
    }
  }

  QMMF_DEBUG("%s: Exit for surface id: %u", __func__, surface_id);
  return ret;
}

status_t DisplayImpl::GetDisplayParam(DisplayHandle display_handle,
    DisplayParamType param_type, void *param, size_t param_size) {

  QMMF_DEBUG("%s: Enter", __func__);

  uint32_t ret = NO_ERROR;

  auto display_client_info = display_client_info_map_.find(display_handle);
  if (display_client_info == display_client_info_map_.end()) {
    QMMF_ERROR("%s() no displayinfo %d", __func__, display_handle);
    return -EINVAL;
  }

  DisplayInterface* displayintf =
      display_type_info_map_.find(
      display_client_info->second->display_type)->second->display_intf;
  assert(displayintf != nullptr);
  if (param_type == DisplayParamType::kBrightness){
    DisplayError error = displayintf->GetPanelBrightness(reinterpret_cast<int*>(param));
    if (error != kErrorNone) {
      QMMF_ERROR("%s: GetPanelBrightness Failed. Error = %d", __func__,
          error);
      return error;
    }
  } else if (param_type == DisplayParamType::kDisplayState) {
    DisplayError error = displayintf->GetDisplayState(
        reinterpret_cast<sdm::DisplayState*>(param));
    if (error != kErrorNone) {
      QMMF_ERROR("%s: GetDisplayState Failed. Error = %d", __func__, error);
      return error;
    }
  }

  QMMF_DEBUG("%s: Exit", __func__);
  return ret;
}

status_t DisplayImpl::SetDisplayParam(DisplayHandle display_handle,
    DisplayParamType param_type, void *param, size_t param_size) {

  QMMF_DEBUG("%s: Enter", __func__);

  std::unique_lock<std::mutex> lock(layer_lock_);

  uint32_t ret = NO_ERROR;

  auto display_client_info = display_client_info_map_.find(display_handle);
  if (display_client_info == display_client_info_map_.end()) {
    QMMF_ERROR("%s() no displayinfo %d", __func__, display_handle);
    return -EINVAL;
  }

  DisplayInterface* displayintf =
      display_type_info_map_.find(display_client_info->second->display_type)->second->display_intf;
  assert(displayintf != nullptr);
  if (param_type == DisplayParamType::kBrightness){
    int* level = reinterpret_cast<int*>(param);
    DisplayError error = displayintf->SetPanelBrightness(*level);
    if (error != kErrorNone) {
      QMMF_ERROR("%s: SetPanelBrightness Failed. Error = %d", __func__,
          error);
      return error;
    }
  } else if (param_type == DisplayParamType::kDisplayState) {
    sdm::DisplayState current_display_state;
    DisplayError error = displayintf->GetDisplayState(&current_display_state);
    if (error != kErrorNone) {
      QMMF_ERROR("%s: GetDisplayState Failed. Error = %d", __func__, error);
      return error;
    }
    sdm::DisplayState* new_display_state =
        reinterpret_cast<sdm::DisplayState*>(param);
    if (*new_display_state == current_display_state) {
      QMMF_DEBUG("%s: Expected State is already set, nothing to change", __func__);
      return kErrorNone;
    }
    if (*new_display_state == sdm::DisplayState::kStateOn) {
#ifndef QMMF_DISPLAY_INTF_v1
      DisplayError error = displayintf->SetDisplayState(*new_display_state);
      if (error != kErrorNone) {
        QMMF_ERROR("%s: SetDisplayState Failed. Error = %d", __func__, error);
        return error;
      }
#else
      int release_fence = -1;
      DisplayError error = displayintf->SetDisplayState(*new_display_state, &release_fence);
      if (error != kErrorNone) {
        QMMF_ERROR("%s: SetDisplayState Failed. Error = %d", __func__, error);
        return error;
      }
      if (release_fence > -1) {
        close(release_fence);
      }
#endif
      SetDisplayState(DisplayState::kStateOn);
      error = displayintf->SetVSyncState(true);
      if (error != kErrorNone) {
        QMMF_ERROR("%s: SetVSyncState Failed. Error = %d", __func__, error);
        SetDisplayState(DisplayState::kStateOff);
        return error;
      } else {
        std::unique_lock<std::mutex> lg(display_on_lock_);
        display_on_cond_.notify_one();
      }
    } else if (*new_display_state == sdm::DisplayState::kStateOff){
      error = displayintf->SetVSyncState(false);
      if (error != kErrorNone) {
        QMMF_ERROR("%s: SetVSyncState Failed. Error = %d", __func__, error);
        return error;
      }
      SetDisplayState(DisplayState::kStateOff);
#ifndef QMMF_DISPLAY_INTF_v1
      DisplayError error = displayintf->SetDisplayState(*new_display_state);
      if (error != kErrorNone) {
        QMMF_ERROR("%s: SetDisplayState Failed. Error = %d", __func__, error);
        SetDisplayState(DisplayState::kStateOn);
        return error;
      }
#else
      int release_fence = -1;
      DisplayError error = displayintf->SetDisplayState(*new_display_state, &release_fence);
      if (error != kErrorNone) {
        QMMF_ERROR("%s: SetDisplayState Failed. Error = %d", __func__, error);
        SetDisplayState(DisplayState::kStateOn);
        return error;
      }
      if (release_fence > -1) {
        close(release_fence);
      }
#endif
    }
  }
  QMMF_DEBUG("%s: Exit", __func__);
  return ret;
}

status_t DisplayImpl::DequeueWBSurfaceBuffer(DisplayHandle display_handle,
    const uint32_t surface_id, SurfaceBuffer &surface_buffer) {

  QMMF_DEBUG("%s: Enter", __func__);
  //TBD
  int32_t ret = NO_ERROR;

  QMMF_DEBUG("%s: Exit", __func__);
  return ret;
}

status_t DisplayImpl::QueueWBSurfaceBuffer(DisplayHandle display_handle,
    const uint32_t surface_id, const SurfaceBuffer &surface_buffer) {

  QMMF_DEBUG("%s: Enter", __func__);
  //TBD
  int32_t ret = NO_ERROR;
  QMMF_DEBUG("%s: Exit", __func__);
  return ret;
}

void DisplayImpl::HandleVSyncThreadEntry(DisplayImpl* display_impl) {
  QMMF_DEBUG("%s: Enter", __func__);

  prctl(PR_SET_NAME, "DisImplHVSync", 0, 0, 0);
  display_impl->HandleVSync();

  QMMF_DEBUG("%s: Exit", __func__);
}

void DisplayImpl::HandleVSync() {
  bool run = true;

  DisplayInterface* displayintf;
  QMMF_DEBUG("%s: Start HandleVSync thread while loop", __func__);
  int32_t wait_iter;
  while (run) {
    wait_iter = 0;
    api_lock_.lock();
    if (!running_) {
      QMMF_DEBUG("%s: Break HandleVSync thread while loop", __func__);
      api_lock_.unlock();
      break;
    } else {
      api_lock_.unlock();
    }

    if (GetDisplayState() == DisplayState::kStateOff) {
      QMMF_WARN("%s: Display State is OFF", __func__);
      std::unique_lock<std::mutex> lg(display_on_lock_);
      if (display_on_cond_.wait_for(lg, microseconds(100000)) ==
          cv_status::timeout) {
        continue;
      }
    }

    if (!is_first_commit_) {
      std::unique_lock<std::mutex> lg(vsync_callback_locker_);
      while (wait_iter < kNumberOfAttempts) {
        if (vsync_callback_.wait_for(lg, microseconds(kHwVSyncTimeoutUs / 5)) ==
            cv_status::timeout) {
          if (GetDisplayState() == DisplayState::kStateOff) {
            QMMF_WARN("%s: Display State is OFF", __func__);
            break;
          }
          ++wait_iter;
          if (wait_iter < kNumberOfAttempts) {
            continue;
          }
        } else {
          break;
        }

        QMMF_ERROR("%s: Timed out since HW Vsync not received", __func__);
        wait_iter = 0;
        for (auto& client_info_map_it : display_client_info_map_) {
          assert(client_info_map_it.second->remote_cb.get() != nullptr);
          DisplayErrorType error = DisplayErrorType::kVSyncError;
          client_info_map_it.second->remote_cb->notifyDisplayEvent(
              DisplayEventType::kError, &error, sizeof(error));
        }
        continue;
      }
      if (GetDisplayState() == DisplayState::kStateOff) {
        QMMF_WARN("%s: Display State set to OFF", __func__);
        continue;
      }
      QMMF_DEBUG("%s: HW VSync Signal Received", __func__);
    }
    api_lock_.lock();
    for (auto it = display_type_info_map_.begin();
         it != display_type_info_map_.end(); ++it) {
      QMMF_DEBUG("%s:display_type_info_map_.size()::%d", __func__,
                 display_type_info_map_.size());

      int64_t timestamp = 0;
      displayintf = it->second->display_intf;
      if (displayintf == nullptr) {
        continue;
      }

      {
        std::unique_lock<std::mutex> lock(layer_lock_);
        if (it->second->z_order_surface_id_map.size() == 0) {
          continue;
        }
      }

      LayerStack* layer_stack = GetLayerStack(it->first, true);
      if (layer_stack == nullptr) {
        QMMF_ERROR("Layer Stack is null");
        continue;
      }
      if (layer_stack->layers.size()) {
        layer_stack->flags.flags = 0;

        for(uint32_t i = 0 ; i< layer_stack->layers.size(); i++) {
          Layer * layer = layer_stack->layers[i];
          LayerBuffer buffer = layer->input_buffer;
          if(buffer.acquire_fence_fd >= 0) {
            QMMF_DEBUG("%s: Set acquire_fence_fd::%d", __func__,
                buffer.acquire_fence_fd);
            close(buffer.acquire_fence_fd);
            buffer.acquire_fence_fd = -1;
          }
        }
        api_lock_.unlock();
        layer_lock_.lock();

        if (GetDisplayState() == DisplayState::kStateOff) {
          QMMF_ERROR("%s: Cant Prepare/Commit since the Display state is OFF",
                     __func__);
          layer_lock_.unlock();
          continue;
        }

        auto start_time = high_resolution_clock::now();
        DisplayError error = displayintf->Prepare(layer_stack);
        auto end_time = high_resolution_clock::now();
        auto diff = duration_cast<microseconds>(end_time - start_time).count();
        QMMF_DEBUG("%s: Prepare time:%llu", __func__, diff);
        if (error != kErrorNone) {
          QMMF_WARN("%s: Prepare failed. Error = %d", __func__, error);
        } else {
          start_time = high_resolution_clock::now();
          error = displayintf->Commit(layer_stack);
          end_time = high_resolution_clock::now();
          diff = duration_cast<microseconds>(end_time - start_time).count();
          QMMF_DEBUG("%s: Commit time:%llu", __func__, diff);
          if (error != kErrorNone) {
            QMMF_WARN("%s: Commit failed. Error = %d", __func__, error);
          }
          is_first_commit_ = false;

          for(uint32_t i = 0 ; i< layer_stack->layers.size(); i++) {
            Layer * layer = layer_stack->layers[i];
            LayerBuffer buffer = layer->input_buffer;
            QMMF_DEBUG("%s: Release_fence_fd::%d", __func__,
                 buffer.release_fence_fd);
            close(buffer.release_fence_fd);
          }
        }
        if (layer_stack->retire_fence_fd >= 0) {
          QMMF_DEBUG("%s: close retire_fence_fd::%d", __func__,
              layer_stack->retire_fence_fd);
          close(layer_stack->retire_fence_fd);
          layer_stack->retire_fence_fd = -1;
        }
        layer_lock_.unlock();
      } else {
        api_lock_.unlock();
      }
      api_lock_.lock();
      for (auto& client_info_map_it : display_client_info_map_) {
        if (client_info_map_it.second->display_type == it->first) {
          assert(client_info_map_it.second->remote_cb.get() != nullptr);
          client_info_map_it.second->remote_cb->notifyVSyncEvent(timestamp);
          QMMF_DEBUG("%s: Notifying vsync to client", __func__);
        }
      }
    }
    api_lock_.unlock();
    usleep(1000);
  }
}

void DisplayImpl::SetRect(const SurfaceRect &source, LayerRect *target) {
  target->left = FLOAT(source.left);
  target->top = FLOAT(source.top);
  target->right = FLOAT(source.right);
  target->bottom = FLOAT(source.bottom);
}

Layer* DisplayImpl::AllocateLayer(DisplayHandle display_handle,
    uint32_t* surface_id, uint32_t z_order) {
  QMMF_DEBUG("%s: Enter", __func__);
  std::unique_lock<std::mutex> lock(layer_lock_);

   auto display_client_info = display_client_info_map_.find(display_handle);
  if (display_client_info == display_client_info_map_.end()) {
    QMMF_ERROR("%s() no display_handle::%u", __func__, display_handle);
    return nullptr;
  }

  Layer* layer = new Layer();
  assert(layer != nullptr);

  display_client_info->second->num_of_client_layers++;
  *surface_id = ++unique_surface_id_;

  auto display_type_info = display_type_info_map_.find(display_client_info->second->display_type);
  display_type_info->second->z_order_surface_id_map.insert(
      std::pair<uint32_t, uint32_t>(z_order, *surface_id));

  QMMF_DEBUG("%s: Exit", __func__);

  return layer;
}

status_t DisplayImpl::FreeLayer(DisplayHandle display_handle,
    const uint32_t surface_id) {
  QMMF_DEBUG("%s: Enter", __func__);
  std::unique_lock<std::mutex> lock(layer_lock_);

  auto display_client_info = display_client_info_map_.find(display_handle);
  if (display_client_info == display_client_info_map_.end()) {
    QMMF_ERROR("%s() no display_handle::%u", __func__, display_handle);
    return -EINVAL;
  }


  std::map<uint32_t, SurfaceInfo*>::iterator surfaceinfo;
  {
    std::lock_guard<std::mutex> lock(surface_lock_);
    surfaceinfo = surface_info_map_.find(surface_id);
    if (surfaceinfo == surface_info_map_.end()) {
      QMMF_ERROR("%s() no surface_id::%u", __func__, surface_id);
      return -EINVAL;
    }
  }

  Layer *layer = surfaceinfo->second->layer;
  assert(layer != nullptr);

  LayerBuffer buffer = layer->input_buffer;
  QMMF_DEBUG("%s: close buffer.release_fence_fd::%d, "
      "buffer.acquire_fence_fd: %d", __func__,
      buffer.release_fence_fd, buffer.acquire_fence_fd);
  close(buffer.release_fence_fd);
  buffer.release_fence_fd = -1;
  close(buffer.acquire_fence_fd);
  buffer.acquire_fence_fd = -1;

  delete layer;
  surfaceinfo->second->layer = nullptr;
  display_client_info->second->num_of_client_layers--;
  auto display_type_info = display_type_info_map_.find(display_client_info->second->display_type);

  uint32_t surface_id_key;
  for(auto &it : display_type_info->second->z_order_surface_id_map) {
    if (it.second == surface_id) {
      surface_id_key = it.first;
      break;
    }
  }

  display_type_info->second->z_order_surface_id_map.erase(surface_id_key);

  QMMF_DEBUG("%s: Exit", __func__);
  return NO_ERROR;
}

Layer* DisplayImpl::GetLayer(DisplayHandle display_handle,
    const uint32_t surface_id) {
  QMMF_DEBUG("%s: Enter", __func__);
  std::unique_lock<std::mutex> lock(layer_lock_);

  auto display_client_info = display_client_info_map_.find(display_handle);
  if (display_client_info == display_client_info_map_.end()) {
    QMMF_ERROR("%s() no display_handle::%u", __func__, display_handle);
    return nullptr;
  }

  std::map<uint32_t, SurfaceInfo*>::iterator surfaceinfo;
  {
    std::lock_guard<std::mutex> lock(surface_lock_);
    surfaceinfo = surface_info_map_.find(surface_id);
    if (surfaceinfo == surface_info_map_.end()) {
      QMMF_ERROR("%s() no surface_id::%u", __func__, surface_id);
      return nullptr;
    }
  }
  assert(surfaceinfo->second->layer != nullptr);

  QMMF_DEBUG("%s: Exit", __func__);
  return surfaceinfo->second->layer;
}

LayerStack* DisplayImpl::GetLayerStack(DisplayType display_type,
                                       bool queued_buffers_only) {
  QMMF_VERBOSE("%s: Enter", __func__);
  auto display_type_info = display_type_info_map_.find(display_type);
  assert(display_type_info != display_type_info_map_.end());
  layer_stack_->layers.clear();
  Layer* layer;
  bool push_layers = false;
  for (auto it = display_type_info->second->z_order_surface_id_map.begin();
       it != display_type_info->second->z_order_surface_id_map.end(); it++) {
    std::map<uint32_t, SurfaceInfo*>::iterator surfaceinfo;
    {
      std::lock_guard<std::mutex> lock(surface_lock_);
      surfaceinfo = surface_info_map_.find(it->second);
      if (surfaceinfo == surface_info_map_.end()) {
        QMMF_ERROR("%s() no display_type::%d", __func__, display_type);
        continue;
      }
    }
    layer = surfaceinfo->second->layer;
    if (layer != nullptr) {
      if (!queued_buffers_only) {
        push_layers = true;
        layer_stack_->layers.push_back(layer);
      } else {
        std::map<int32_t, BufferState*>::iterator it_queued_buffer;
        std::map<int32_t, BufferState*>::iterator it_committed_buffer;
        for (it_queued_buffer = surfaceinfo->second->buffer_state.begin();
             it_queued_buffer != surfaceinfo->second->buffer_state.end();
             ++it_queued_buffer) {
          if (it_queued_buffer->second->GetState() ==
              BufferStates::kStateQueued) {
            break;
          }
        }
        for (it_committed_buffer = surfaceinfo->second->buffer_state.begin();
             it_committed_buffer != surfaceinfo->second->buffer_state.end();
             ++it_committed_buffer) {
          if (it_committed_buffer->second->GetState() ==
              BufferStates::kStateCommitted) {
            break;
          }
        }
        if (it_queued_buffer != surfaceinfo->second->buffer_state.end()) {
          // Free the committed buffer
          if (it_committed_buffer != surfaceinfo->second->buffer_state.end()) {
            BufferStates state =
                it_committed_buffer->second->SetState(BufferStates::kStateFree);
            if (state != BufferStates::kStateFree) {
              QMMF_ERROR("%s Could not Set state::%u of Buffer Ion_Fd::%d",
                         __func__,
                         static_cast<std::underlying_type<BufferStates>::type>(
                         BufferStates::kStateFree),
                         it_committed_buffer->first);
            } else {
              QMMF_DEBUG("%s The Buffer ION_FD:%d has been set to state:%u",
                         __func__, it_committed_buffer->first,
                         static_cast<std::underlying_type<BufferStates>::type>(
                         BufferStates::kStateFree));
            }
          }
          // Fill layer params for queued buffer
          auto queued_buffer_info =
              latest_queued_buffer_info_map_.find(it->second);
          if (queued_buffer_info == latest_queued_buffer_info_map_.end()) {
            return nullptr;
          }
          BufferInfo buffer_info;
          int32_t aligned_width, aligned_height;
          aligned_width =
              queued_buffer_info->second.surface_buffer.plane_info[0].width;
          aligned_height =
              queued_buffer_info->second.surface_buffer.plane_info[0].height;
          buffer_info.buffer_config.width =
              queued_buffer_info->second.surface_buffer.plane_info[0].width;
          buffer_info.buffer_config.height =
              queued_buffer_info->second.surface_buffer.plane_info[0].height;
          buffer_info.buffer_config.format = static_cast<LayerBufferFormat>(
              queued_buffer_info->second.surface_buffer.format);
          buffer_info.buffer_config.buffer_count = 1;
          buffer_info.buffer_config.cache = 0;
          buffer_info.alloc_buffer_info.fd = -1;
          buffer_info.alloc_buffer_info.stride = 0;
          buffer_info.alloc_buffer_info.size = 0;
          auto ret = buffer_allocator_.GetBufferInfo(
                     &buffer_info, aligned_width, aligned_height);
          if (ret != kErrorNone)
            QMMF_ERROR("%s: GetBufferInfo Failed. Error = %d", __func__, ret);
          layer->input_buffer.width = aligned_width;
          layer->input_buffer.height = aligned_height;
          layer->input_buffer.unaligned_width =
              queued_buffer_info->second.surface_buffer.plane_info[0].width;
          layer->input_buffer.unaligned_height =
              queued_buffer_info->second.surface_buffer.plane_info[0].height;
          layer->input_buffer.size =
              queued_buffer_info->second.surface_buffer.plane_info[0].size;
          layer->input_buffer.planes[0].offset =
              queued_buffer_info->second.surface_buffer.plane_info[0].offset;
          layer->input_buffer.planes[0].stride =
              queued_buffer_info->second.surface_buffer.plane_info[0].stride;
          if (layer->input_buffer.format == kFormatYCbCr420SemiPlanarVenus ||
              layer->input_buffer.format == kFormatYCbCr420SPVenusUbwc) {
            layer->input_buffer.color_metadata.colorPrimaries =
                ColorPrimaries_BT601_6_525;
            layer->input_buffer.color_metadata.range = Range_Limited;
          }
          SetRect(queued_buffer_info->second.surface_param.dst_rect,
                  &layer->dst_rect);
          SetRect(queued_buffer_info->second.surface_param.src_rect,
                  &layer->src_rect);
          layer->blending = static_cast<LayerBlending>(
              queued_buffer_info->second.surface_param.surface_blending);
          layer->transform.flip_horizontal =
              queued_buffer_info->second.surface_param.surface_transform
                  .flip_horizontal;
          layer->transform.flip_vertical =
              queued_buffer_info->second.surface_param.surface_transform
                  .flip_vertical;
          layer->transform.rotation = queued_buffer_info->second.surface_param
                                          .surface_transform.rotation;
          layer->plane_alpha = 0xFF;
          layer->frame_rate =
              queued_buffer_info->second.surface_param.frame_rate;
          layer->solid_fill_color =
              queued_buffer_info->second.surface_param.solid_fill_color;
          layer->flags.solid_fill =
              queued_buffer_info->second.surface_param.surface_flags.solid_fill;
          layer->flags.cursor =
              queued_buffer_info->second.surface_param.surface_flags.cursor;
          layer->input_buffer.planes[0].fd =
              queued_buffer_info->second.surface_buffer.plane_info[0].ion_fd;
          QMMF_DEBUG("%s: Buffer Ion_Fd = %u", __func__,
               queued_buffer_info->second.surface_buffer.plane_info[0].ion_fd);
          layer->input_buffer.buffer_id =
              queued_buffer_info->second.surface_buffer.buf_id;
          layer->flags.updating = true;

          // Commit the queued buffer
          BufferStates state =
              it_queued_buffer->second->SetState(BufferStates::kStateCommitted);
          if (state != BufferStates::kStateCommitted) {
            QMMF_ERROR("%s Could not Set state::%u of Buffer Ion_Fd::%d",
                       __func__,
                       static_cast<std::underlying_type<BufferStates>::type>(
                       BufferStates::kStateCommitted),
                       it_queued_buffer->first);
          } else {
            QMMF_DEBUG("%s The Buffer ION_FD:%d has been set to state:%u",
                       __func__, it_queued_buffer->first,
                       static_cast<std::underlying_type<BufferStates>::type>(
                       BufferStates::kStateCommitted));
          }
          push_layers = true;
          layer_stack_->layers.push_back(layer);
        } else if (it_committed_buffer !=
                   surfaceinfo->second->buffer_state.end()) {
          layer_stack_->layers.push_back(layer);
        }
      }
    }
  }

  if (!push_layers)
    layer_stack_->layers.clear();

  QMMF_VERBOSE("%s: Exit", __func__);
  return layer_stack_;
}

DisplayError DisplayImpl::VSync(const DisplayEventVSync &vsync) {

  QMMF_DEBUG("%s: Enter", __func__);

  std::unique_lock<std::mutex> lg(vsync_callback_locker_);
  vsync_callback_.notify_one();

  QMMF_DEBUG("%s: Exit", __func__);
  return kErrorNone;
}

DisplayError DisplayImpl::VSync(int fd, unsigned int sequence,
                                unsigned int tv_sec, unsigned int tv_usec,
                                void *data) {
  return kErrorNotSupported;
}

DisplayError DisplayImpl::PFlip(int fd, unsigned int sequence,
                                unsigned int tv_sec, unsigned int tv_usec,
                                void *data) {
  return kErrorNotSupported;
}

DisplayError DisplayImpl::Refresh() {
  return kErrorNotSupported;
}

DisplayError DisplayImpl::CECMessage(char *message) {
  return kErrorNotSupported;
}

DisplayError DisplayImpl::HandleEvent(DISPLAY_EVENT event) {
  return kErrorNotSupported;
}

void DisplayImpl::PrintBuffersState(const uint32_t surface_id) {
  std::map<uint32_t, SurfaceInfo*>::iterator surfaceinfo;
  {
    std::lock_guard<std::mutex> lock(surface_lock_);
    surfaceinfo = surface_info_map_.find(surface_id);
    assert(surfaceinfo->second != nullptr);
  }
  for (auto buffer_state = surfaceinfo->second->buffer_state.begin();
       buffer_state != surfaceinfo->second->buffer_state.end();
       ++buffer_state) {
    QMMF_DEBUG("%s State of Buffer Ion_Fd::%d is %u", __func__,
               buffer_state->first,
               static_cast<std::underlying_type<BufferStates>::type>(
                   buffer_state->second->GetState()));
  }
}

}; // namespace display

}; //namespace qmmf
