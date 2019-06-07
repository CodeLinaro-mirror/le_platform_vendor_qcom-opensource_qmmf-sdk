/*
* Copyright (c) 2016, 2018, 2019, The Linux Foundation. All rights reserved.
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

/*! @file qmmf_display_client.h
*/

#pragma once

#include <utils/Errors.h>
#include <utils/Mutex.h>
#include <utils/RefBase.h>
#include <utils/KeyedVector.h>
#include <map>
#include <mutex>
#include <ion/ion.h>
#include <linux/dma-buf.h>
#include <linux/msm_ion.h>

#include "display/src/client/qmmf_display_client_intf.h"
#include "display/src/client/qmmf_display_service_intf.h"

namespace qmmf {

/// @namespace qmmf::display
namespace display {

/**
 * @brief Delegation to binder proxy <IDisplayService>
 * and implementation of binder CB.
 */
class DisplayClient: public IDisplayClient
{
public:
  DisplayClient();

  ~DisplayClient();

  /**
   * @brief Connect to display service.
   */
  status_t Connect() override;

  /**
   * @brief Disconnect from display service.
   *
   * All the surfaces should be deleted before calling
   * Disconnet Api.
   */
  status_t Disconnect() override;

  /**
   * @brief Create a Display based on Display type
   */
  status_t CreateDisplay(DisplayType type, DisplayCb& cb) override;

  /**
   * @brief Destroy a Display based on Display type
   */
  status_t DestroyDisplay(DisplayType type) override;

  /**
   * @brief This API internally calls prepare() which checks surface properties
   * and check whether one of the available pipe's can be assigned to this surface.
   *
   * If surface properties meet the requirement of available pipe capabilities,
   * one of the available pipe is assigned to this layer
   * Surface represents the layer (YUV or RGB) associated with a display.
   */
  status_t CreateSurface(SurfaceConfig &surface_config,
      uint32_t* surface_id) override;

  /**
   * @brief Destroy the Surface based on Surface id
   */
  status_t DestroySurface(const uint32_t surface_id) override;

  /**
   * @brief This API gets the empty buffer to be used by the client for rendering.
   */
  status_t DequeueSurfaceBuffer(const uint32_t surface_id,
      SurfaceBuffer &surface_buffer) override;

  /**
   * @brief The client renders the data into the empty buffer and calls this API to
   * push this data for composition and display.
   */
  status_t QueueSurfaceBuffer(const uint32_t surface_id,
      SurfaceBuffer &surface_buffer, SurfaceParam &surface_param) override;

  /**
   * @brief Gets display params values
   */
  status_t GetDisplayParam(DisplayParamType param_type, void *param,
      size_t param_size) override;

  /**
   * @brief Sets Dynamic display params
   */
  status_t SetDisplayParam(DisplayParamType param_type, void *param,
      size_t param_size) override;

  /**
   * @brief This API gets the composed layers data for WFD usecase
   */
  status_t DequeueWBSurfaceBuffer(const uint32_t surface_id,
      SurfaceBuffer &surface_buffer) override;

  /**
   * @brief The client provides the empty writeback buffers to display.
   */
  status_t QueueWBSurfaceBuffer(const uint32_t surface_id,
      const SurfaceBuffer &surface_buffer) override;

  /**
   * @brief Display Callback from service.
   */
  void notifyDisplayEvent(DisplayEventType event_type, void *event_data,
      size_t event_data_size);

  /**
   * @brief Session Callback from service.
   */
  void notifySessionEvent(DisplayEventType event_type, void *event_data,
      size_t event_data_size);

  /**
   * @brief VSync Callback from service.
   */
  void notifyVSyncEvent(int64_t time_stamp);

 private:

  bool checkServiceStatus();

  class DeathNotifier : public IBinder::DeathRecipient
  {
   public:
    DeathNotifier(DisplayClient* parent) : parent_(parent) {}

    void binderDied(const wp<IBinder>&) override {
        ALOGD("DisplayClient:%s: Display service died", __func__);

          std::lock_guard<std::mutex> lock(parent_->lock_);
          parent_->display_service_.clear();
          parent_->display_service_ = NULL;
    }
    DisplayClient* parent_;
  };
  friend class DeathNotifier;

  std::mutex           lock_;
  sp<IDisplayService>  display_service_;
  sp<DeathNotifier>    death_notifier_;
  DisplayCb            display_cb_;
  int32_t              ion_device_;
  DisplayHandle        display_handle_;
  DisplayType          display_type_;

  typedef struct BufInfo {
    int32_t ion_fd; /**< Transferred ION Id */
    void    *pointer; /**< Memory mapped buffer */
    size_t  frame_len; /**< Size */
    ion_user_handle_t ion_handle; /**< ION handle */
    uint32_t surface_id; /**< surface id */
  } BufInfo;

  typedef std::map<int32_t, BufInfo*> buf_info_map; /**< map <buffer index, buffer_info> */
  buf_info_map buf_info_map_;

  use_buffer_map surface_id_buffer_allocation_mode_map_;
};

class ServiceCallbackHandler : public BnDisplayServiceCallback {
 public:

  ServiceCallbackHandler(DisplayClient* client);

  ~ServiceCallbackHandler();

 private:
  /**
   * @brief BnDisplayServiceCallback Display callback
   */
  void notifyDisplayEvent(DisplayEventType event_type, void *event_data,
      size_t event_data_size) override;

  /**
   * @brief BnDisplayServiceCallback Session callback
   */
  void notifySessionEvent(DisplayEventType event_type, void *event_data,
      size_t event_data_size) override;

  /**
   * @brief BnDisplayServiceCallback VSync callback
   */
  void notifyVSyncEvent(int64_t time_stamp) override;

  DisplayClient *client_;
};


}; // namespace qmmf

}; // namespace display.
