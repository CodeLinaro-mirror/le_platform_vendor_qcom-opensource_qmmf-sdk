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

#pragma once

#include <cutils/properties.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

#include <gbm_priv.h>
#include <gbm-buffer-backend-client-protocol.h>
#include <wayland-client.h>

#include "display/src/client/qmmf_display_client_intf.h"

namespace qmmf {

namespace display {

enum class BoStates {
  kStateFree,
  kStateQueued,
  kStateCommitted,
};

typedef struct {
  struct gbm_bo *bo_ptr;
  void *bo_vaddr;
  struct wl_buffer *wl_buf;
  BoStates bo_state;
} BoBuffer;

class DisplayWestonClient : public IDisplayClient {
 public:
  DisplayWestonClient();

  ~DisplayWestonClient();

  status_t Connect() override;

  status_t Disconnect() override;

  status_t CreateDisplay(DisplayType type, DisplayCb &cb) override;

  status_t DestroyDisplay(DisplayType type) override;

  status_t CreateSurface(SurfaceConfig &surface_config,
                         uint32_t *surface_id) override;

  status_t DestroySurface(const uint32_t surface_id) override;

  status_t DequeueSurfaceBuffer(const uint32_t surface_id,
                                SurfaceBuffer &surface_buffer) override;

  status_t QueueSurfaceBuffer(const uint32_t surface_id,
                              SurfaceBuffer &surface_buffer,
                              SurfaceParam &surface_param) override;

  status_t GetDisplayParam(DisplayParamType param_type, void *param,
                           size_t param_size) override;

  status_t SetDisplayParam(DisplayParamType param_type, void *param,
                           size_t param_size) override;

  status_t DequeueWBSurfaceBuffer(const uint32_t surface_id,
                                  SurfaceBuffer &surface_buffer) override;

  status_t QueueWBSurfaceBuffer(const uint32_t surface_id,
                                const SurfaceBuffer &surface_buffer) override;

  // Weston event callbacks
  static void OnRegistryAddHandler(void *data, struct wl_registry *registry,
                                   uint32_t id, const char *interface,
                                   uint32_t version);
  static void OnRegistryRemoveHandler(void *data, struct wl_registry *registry,
                                      uint32_t id);

  static void OnGbmBufBackendCreateParamSuccess(
      void *data, struct gbm_buffer_params *params,
      struct wl_buffer *new_buffer);
  static void OnGbmBufBackendCreateParamFail(void *data,
                                             struct gbm_buffer_params *params);
  static void OnBufferRelease(void *data, struct wl_buffer *buffer);

 private:
  struct wl_display *display_;
  struct wl_registry *registry_;
  struct wl_compositor *compositor_;
  struct wl_shell *shell_;
  struct wl_surface *surface_;
  struct wl_shell_surface *shell_surface_;
  int32_t mem_dev_fd_;
  struct gbm_device *gbm_device_;
  struct gbm_buffer_backend *gbm_buf_backend_;
  std::vector<BoBuffer> bo_buffer_list_;
  std::mutex buffer_lock_;
  std::condition_variable buffer_handler_;
  std::thread *buffer_handler_thread_;
  int32_t num_buffers_;
  bool stop_;

  int32_t GetFreeBufferIndex();
  int32_t GetQueuedBufferIndex();
  bool IsBufferAvailable();
  static void BufferHandlerEntry(DisplayWestonClient *instance);
  void BufferHandler();
};

const struct wl_registry_listener register_listener = {
    DisplayWestonClient::OnRegistryAddHandler,
    DisplayWestonClient::OnRegistryRemoveHandler};

const struct gbm_buffer_params_listener gbm_buf_backend_params_listener = {
    DisplayWestonClient::OnGbmBufBackendCreateParamSuccess,
    DisplayWestonClient::OnGbmBufBackendCreateParamFail};

static const struct wl_buffer_listener buffer_listener = {
    DisplayWestonClient::OnBufferRelease};

};  // namespace display
};  // namespace qmmf
