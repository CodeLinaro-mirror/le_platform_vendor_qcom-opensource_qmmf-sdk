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

#define LOG_TAG "DisplayWestonClient"

#include "display/src/client/qmmf_display_weston_client.h"

#define GBM_DEVICE       "/dev/dri/card0"
#define PROP_NUM_BUFFERS "persist.qmmf.display.num.buf"

uint32_t qmmf_log_level;

namespace qmmf {

namespace display {

using std::chrono::seconds;
using std::cv_status;

DisplayWestonClient::DisplayWestonClient()
    : display_(nullptr),
      registry_(nullptr),
      compositor_(nullptr),
      shell_(nullptr),
      surface_(nullptr),
      shell_surface_(nullptr),
      mem_dev_fd_(-1),
      gbm_device_(nullptr),
      gbm_buf_backend_(nullptr),
      buffer_handler_thread_(nullptr),
      num_buffers_(11),
      stop_(false) {
  QMMF_DEBUG("%s Enter ", __func__);
  QMMF_GET_LOG_LEVEL();
  char prop_val[PROPERTY_VALUE_MAX];
  property_get(PROP_NUM_BUFFERS, prop_val, "11");
  num_buffers_ = atoi(prop_val);
  QMMF_DEBUG("%s Exit (0x%p)", __func__, this);
}

DisplayWestonClient::~DisplayWestonClient() {
  QMMF_DEBUG("%s Enter ", __func__);

  if (!stop_) {
    QMMF_INFO("%s stop_ is not set", __func__);
    QMMF_INFO("%s Calling Destroy*, Disconnect for you", __func__);
    DestroySurface(0);
    DestroyDisplay(DisplayType::kDisplayMax);
    Disconnect();
    QMMF_INFO("%s Hope you'll call Destroy*, Disconnect next time", __func__);
  }
  QMMF_DEBUG("%s Exit 0x%p", __func__, this);
}

status_t DisplayWestonClient::Connect() {
  QMMF_DEBUG("%s Enter ", __func__);
  display_ = wl_display_connect(nullptr);
  if (!display_) {
    QMMF_ERROR("%s Can't connect to display: %d[%s]", __func__, -errno,
               strerror(errno));
    return -ENODEV;
  }
  QMMF_DEBUG("%s Connect successful!! ", __func__);
  QMMF_DEBUG("%s Exit ", __func__);
  return 0;
}

status_t DisplayWestonClient::Disconnect() {
  QMMF_DEBUG("%s Enter ", __func__);
  if (display_) {
    wl_display_disconnect(display_);
    display_ = nullptr;
    QMMF_DEBUG("%s Disconnect successful!! ", __func__);
  }
  QMMF_DEBUG("%s Exit ", __func__);
  return 0;
}

status_t DisplayWestonClient::CreateDisplay(DisplayType type, DisplayCb &cb) {
  QMMF_DEBUG("%s Enter ", __func__);
  registry_ = wl_display_get_registry(display_);
  if (!registry_) {
    QMMF_ERROR("%s Can't get registry: %d[%s]", __func__, -errno,
               strerror(errno));
    return -ENODEV;
  }
  wl_registry_add_listener(registry_, &register_listener, this);
  // Wait till we get registry objects
  wl_display_roundtrip(display_);

  if (!compositor_) {
    QMMF_ERROR("%s Compositor not found: %d[%s]", __func__, -errno,
               strerror(errno));
    goto FAIL;
  }

  if (!shell_) {
    QMMF_ERROR("%s Shell not found: %d[%s]", __func__, -errno, strerror(errno));
    goto FAIL;
  }

  if (!gbm_buf_backend_) {
    QMMF_ERROR("%s GBM buffer backend not found: %d[%s]", __func__, -errno,
               strerror(errno));
    goto FAIL;
  }

  mem_dev_fd_ = open(GBM_DEVICE, O_RDWR);
  if (mem_dev_fd_ < 0) {
    QMMF_ERROR("%s Error opening gbm device: %d[%s]", __func__, -errno,
               strerror(errno));
    goto FAIL;
  }

  gbm_device_ = gbm_create_device(mem_dev_fd_);
  if (!gbm_device_) {
    QMMF_ERROR("%s Failed to create gbm device: %d[%s]", __func__, -errno,
               strerror(errno));
    goto FAIL;
  }

  QMMF_DEBUG("%s Exit ", __func__);
  return 0;

FAIL:
  DestroyDisplay(DisplayType::kDisplayMax);
  QMMF_DEBUG("%s Exit ", __func__);
  return -ENODEV;
}

status_t DisplayWestonClient::DestroyDisplay(DisplayType type) {
  QMMF_DEBUG("%s Enter ", __func__);

  if (gbm_device_) {
    gbm_device_destroy(gbm_device_);
    gbm_device_ = nullptr;
  }

  if (mem_dev_fd_ > 0) {
    close(mem_dev_fd_);
    mem_dev_fd_ = -1;
  }

  if (gbm_buf_backend_) {
    gbm_buffer_backend_destroy(gbm_buf_backend_);
    gbm_buf_backend_ = nullptr;
  }

  if (shell_) {
    wl_shell_destroy(shell_);
    shell_ = nullptr;
  }

  if (compositor_) {
    wl_compositor_destroy(compositor_);
    compositor_ = nullptr;
  }

  if (registry_) {
    wl_registry_destroy(registry_);
    registry_ = nullptr;
  }

  QMMF_DEBUG("%s Exit ", __func__);
  return 0;
}

status_t DisplayWestonClient::CreateSurface(SurfaceConfig &surface_config,
                                            uint32_t *surface_id) {
  QMMF_DEBUG("%s Enter ", __func__);
  surface_ = wl_compositor_create_surface(compositor_);
  if (!surface_) {
    return -ENODEV;
  }

  shell_surface_ = wl_shell_get_shell_surface(shell_, surface_);
  if (!shell_surface_) {
    goto FAIL;
  }

  wl_shell_surface_set_fullscreen(
      shell_surface_, WL_SHELL_SURFACE_FULLSCREEN_METHOD_SCALE, 0, NULL);

  for (auto i = 0; i < num_buffers_; i++) {
    BoBuffer buffer;
    struct gbm_bo *bo;
    bo = gbm_bo_create(gbm_device_, surface_config.width, surface_config.height,
                       GBM_FORMAT_NV12, 0);
    if (nullptr == bo) {
      QMMF_ERROR("%s: Unable to allocate Gbm buffer object\n", __func__);
      goto FAIL;
    }
    buffer.bo_ptr = bo;
    void *bo_addr;
    if (gbm_perform(GBM_PERFORM_CPU_MAP_FOR_BO, bo, &bo_addr)) {
      QMMF_ERROR("%s: failed to map for buffer", __func__);
      goto FAIL;
    }
    buffer.bo_vaddr = bo_addr;
    buffer.wl_buf = nullptr;
    buffer.bo_state = BoStates::kStateFree;
    bo_buffer_list_.push_back(buffer);
  }

  stop_ = false;

  buffer_handler_thread_ =
      new std::thread(DisplayWestonClient::BufferHandlerEntry, this);
  if (buffer_handler_thread_ == nullptr) {
    QMMF_ERROR("%s: Unable to create BufferHandlerEntry thread\n", __func__);
    goto FAIL;
  }

  QMMF_DEBUG("%s Exit ", __func__);
  return 0;

  FAIL:
  DestroySurface(0);
  QMMF_DEBUG("%s Exit ", __func__);
  return -ENODEV;
}

status_t DisplayWestonClient::DestroySurface(const uint32_t surface_id) {
  QMMF_DEBUG("%s Enter ", __func__);

  if (!stop_) {
    stop_ = true;
    {
      std::unique_lock<std::mutex> lg(buffer_lock_);
      buffer_handler_.notify_one();
      QMMF_DEBUG("%s: Signaling BufferHandler to stop", __func__);
    }
  }

  if (buffer_handler_thread_ && buffer_handler_thread_->joinable()) {
    buffer_handler_thread_->join();
    delete buffer_handler_thread_;
    buffer_handler_thread_ = nullptr;
    QMMF_DEBUG("%s BufferHandler exited", __func__);
  }

  for (auto &it : bo_buffer_list_) {
    if (it.bo_ptr) {
    if (gbm_perform(GBM_PERFORM_CPU_UNMAP_FOR_BO, it.bo_ptr)) {
      QMMF_ERROR("%s: failed to unmap for buffer", __func__);
      return -EINVAL;
    }
      gbm_bo_destroy(it.bo_ptr);
      it.bo_ptr = nullptr;
    }
    if (it.wl_buf) {
      wl_buffer_destroy(it.wl_buf);
      it.wl_buf = nullptr;
    }
  }
  bo_buffer_list_.clear();

  if (shell_surface_) {
    wl_shell_surface_destroy(shell_surface_);
    shell_surface_ = nullptr;
  }

  if (surface_) {
    wl_surface_destroy(surface_);
    surface_ = nullptr;
  }

  QMMF_DEBUG("%s Exit ", __func__);
  return 0;
}

status_t DisplayWestonClient::DequeueSurfaceBuffer(
    const uint32_t surface_id, SurfaceBuffer &surface_buffer) {
  QMMF_DEBUG("%s: Enter", __func__);
  QMMF_DEBUG("%s Exit ", __func__);
  return 0;
}

status_t DisplayWestonClient::QueueSurfaceBuffer(const uint32_t surface_id,
                                                 SurfaceBuffer &surface_buffer,
                                                 SurfaceParam &surface_param) {
  QMMF_DEBUG("%s Enter ", __func__);

  if (stop_) {
    QMMF_DEBUG("%s Display stopped returning buffer", __func__);
    QMMF_DEBUG("%s Exit ", __func__);
    return 0;
  }

  BoBuffer *free_buffer = nullptr;
  auto idx = GetFreeBufferIndex();
  if (idx != num_buffers_) {
    free_buffer = &bo_buffer_list_[idx];
  } else {
    QMMF_ERROR("%s No free buffers for %u, returning ", __func__, surface_id);
    return 0;
  }
  QMMF_DEBUG("%s Using free buffer %p for surface_id %u ", __func__,
             free_buffer->bo_ptr, surface_id);

  void *data = surface_buffer.plane_info[0].buf;
  uint32_t size = surface_buffer.plane_info[0].size;
  memcpy(free_buffer->bo_vaddr, data, size);
  free_buffer->bo_state = BoStates::kStateQueued;
  QMMF_DEBUG("%s Buffer %p prepared", __func__, free_buffer->bo_ptr);
  {
    std::unique_lock<std::mutex> lg(buffer_lock_);
    buffer_handler_.notify_one();
    QMMF_DEBUG("%s Buffer Handler Signaled", __func__);
  }

  QMMF_DEBUG("%s Exit ", __func__);
  return 0;
}

status_t DisplayWestonClient::GetDisplayParam(DisplayParamType param_type,
                                              void *param, size_t param_size) {
  QMMF_DEBUG("%s Enter ", __func__);
  QMMF_DEBUG("%s Exit ", __func__);
  return 0;
}

status_t DisplayWestonClient::SetDisplayParam(DisplayParamType param_type,
                                              void *param, size_t param_size) {
  QMMF_DEBUG("%s Enter ", __func__);
  QMMF_DEBUG("%s Exit ", __func__);
  return 0;
}

status_t DisplayWestonClient::DequeueWBSurfaceBuffer(
    const uint32_t surface_id, SurfaceBuffer &surface_buffer) {
  QMMF_DEBUG("%s Enter ", __func__);
  QMMF_DEBUG("%s Exit ", __func__);
  return 0;
}

status_t DisplayWestonClient::QueueWBSurfaceBuffer(
    const uint32_t surface_id, const SurfaceBuffer &surface_buffer) {
  QMMF_DEBUG("%s Enter ", __func__);
  QMMF_DEBUG("%s Exit ", __func__);
  return 0;
}

void DisplayWestonClient::BufferHandlerEntry(DisplayWestonClient *instance) {
  QMMF_DEBUG("%s Enter ", __func__);
  instance->BufferHandler();
  QMMF_DEBUG("%s Exit ", __func__);
}

void DisplayWestonClient::BufferHandler() {
  bool run = true;
  int32_t idx;
  QMMF_DEBUG("%s: Enter", __func__);
  while (run) {
    {
      std::unique_lock<std::mutex> lg(buffer_lock_);
      while (!IsBufferAvailable() && !stop_) {
        QMMF_DEBUG("%s: No buffers queued, wait", __func__);
        if (buffer_handler_.wait_for(lg, seconds(1)) == cv_status::timeout) {
          QMMF_WARN("%s: Timed out on wait, continuing", __func__);
          continue;
        }
        if (stop_) {
          QMMF_DEBUG("%s: Got signal for stop", __func__);
          break;
        }
        QMMF_DEBUG("%s: Got signal from QueueSurfaceBuffer,get buffer index",
                   __func__);
        break;
      }
      idx = GetQueuedBufferIndex();
    }
    if (stop_) {
      QMMF_DEBUG("%s:Stop requested", __func__);
      break;
    }

    auto ready_buffer = &bo_buffer_list_[idx];
    struct gbm_buffer_params *params =
        gbm_buffer_backend_create_params(gbm_buf_backend_);

    gbm_buffer_params_add_listener(params, &gbm_buf_backend_params_listener,
                                   ready_buffer);
    struct gbm_bo *bo = ready_buffer->bo_ptr;
    gbm_buffer_params_create(params, bo->ion_fd, bo->ion_metadata_fd, bo->width,
                             bo->height, bo->format, 0);
    QMMF_DEBUG("%s: gbm_buffer_params_create done", __func__);
    wl_display_roundtrip(display_);
    struct wl_buffer *wl_buf = ready_buffer->wl_buf;
    if (!wl_buf) {
      QMMF_ERROR("%s failed!! No wl_buffer from server", __func__);
      continue;
    }
    wl_surface_attach(surface_, wl_buf, 0, 0);
    ready_buffer->bo_state = BoStates::kStateCommitted;
    wl_surface_commit(surface_);
    wl_display_dispatch_pending(display_);
    QMMF_DEBUG("%s buffer committed bo_ptr:%p wl_buf:%p",
        __func__, ready_buffer->bo_ptr, wl_buf);
  }
  QMMF_DEBUG("%s: Exit", __func__);
}

int32_t DisplayWestonClient::GetFreeBufferIndex() {
  QMMF_DEBUG("%s Enter ", __func__);
  uint32_t i;
  auto size = bo_buffer_list_.size();
  for (i = 0; i < size; i++) {
    if (bo_buffer_list_[i].bo_state == BoStates::kStateFree) {
      break;
    }
  }

  QMMF_DEBUG("%s Exit ", __func__);
  return i;
}

int32_t DisplayWestonClient::GetQueuedBufferIndex() {
  QMMF_DEBUG("%s Enter ", __func__);
  uint32_t i;
  auto size = bo_buffer_list_.size();
  for (i = 0; i < size; i++) {
    if (bo_buffer_list_[i].bo_state == BoStates::kStateQueued) {
      break;
    }
  }
  QMMF_DEBUG("%s Exit ", __func__);
  return i;
}

bool DisplayWestonClient::IsBufferAvailable() {
   if (GetQueuedBufferIndex() == num_buffers_) {
     return false;
   }
   return true;
}

void DisplayWestonClient::OnRegistryAddHandler(void *data,
                                               struct wl_registry *registry,
                                               uint32_t id,
                                               const char *interface,
                                               uint32_t version) {
  QMMF_DEBUG("%s Enter ", __func__);
  DisplayWestonClient *instance = reinterpret_cast<DisplayWestonClient *>(data);
  QMMF_DEBUG("%s Got a registry add event for %s id %d", __func__, interface,
             id);
  if (!strcmp(interface, wl_compositor_interface.name)) {
    instance->compositor_ = reinterpret_cast<wl_compositor *>(
        wl_registry_bind(registry, id, &wl_compositor_interface, version));
  } else if (!strcmp(interface, wl_shell_interface.name)) {
    instance->shell_ = reinterpret_cast<wl_shell *>(
        wl_registry_bind(registry, id, &wl_shell_interface, version));
  } else if (!strcmp(interface, "gbm_buffer_backend")) {
    instance->gbm_buf_backend_ = reinterpret_cast<gbm_buffer_backend *>(
        wl_registry_bind(registry, id, &gbm_buffer_backend_interface, 1));
  }
  QMMF_DEBUG("%s Exit ", __func__);
}

void DisplayWestonClient::OnRegistryRemoveHandler(void *data,
                                                  struct wl_registry *registry,
                                                  uint32_t id) {
  QMMF_DEBUG("%s Got a registry remove event fo id %d", __func__, id);
}

void DisplayWestonClient::OnGbmBufBackendCreateParamSuccess(
    void *data, struct gbm_buffer_params *params,
    struct wl_buffer *new_buffer) {
  QMMF_DEBUG("%s Enter ", __func__);
  auto buffer_ptr = reinterpret_cast<BoBuffer *>(data);
  buffer_ptr->wl_buf = new_buffer;
  wl_buffer_add_listener(new_buffer, &buffer_listener, data);
  gbm_buffer_params_destroy(params);
  QMMF_DEBUG("%s Buffer added bo_ptr:%p wl_buf:%p", __func__,
             buffer_ptr->bo_ptr, buffer_ptr->wl_buf);
  QMMF_DEBUG("%s Exit ", __func__);
}

void DisplayWestonClient::OnGbmBufBackendCreateParamFail(
    void *data, struct gbm_buffer_params *params) {
  QMMF_DEBUG("%s Enter ", __func__);
  auto buffer_ptr = reinterpret_cast<BoBuffer *>(data);
  gbm_buffer_params_destroy(params);
  QMMF_ERROR("gbm_linux_buffer_params.create failed");
  QMMF_DEBUG("%s Exit ", __func__);
}

void DisplayWestonClient::OnBufferRelease(void *data,
                                          struct wl_buffer *buffer) {
  QMMF_DEBUG("%s Enter ", __func__);
  auto buffer_ptr = reinterpret_cast<BoBuffer *>(data);
  wl_buffer_destroy(buffer);
  buffer_ptr->bo_state = BoStates::kStateFree;
  buffer_ptr->wl_buf = nullptr;
  QMMF_DEBUG("%s %p buffer released wl_buffer:%p", __func__,
                         buffer_ptr->bo_ptr, buffer);
  QMMF_DEBUG("%s Exit ", __func__);
}

};  // namespace display
};  // namespace qmmf
