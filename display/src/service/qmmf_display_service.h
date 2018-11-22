/*
* Copyright (c) 2016, 2019, The Linux Foundation. All rights reserved.
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

/*! @file qmmf_display_service.h
*/

#pragma once

#include <map>
#include <mutex>
#include <linux/msm_ion.h>

#include "display/src/client/qmmf_display_service_intf.h"
#include "display/src/service/qmmf_display_common.h"
#include "display/src/service/qmmf_display_impl.h"

namespace qmmf {

namespace display {

using namespace android;

/**
 * @brief Delegation to binder native < IDisplayService >
 * and implementation of binder CB.
 */
class DisplayService : public BnInterface<IDisplayService> {
 public:
  DisplayService();

  ~DisplayService();

 private:

  /**
   * @brief The class is responsible for sending the notification back to
   * the client,
   *
   * if this binder object unexpectedly goes away, typically because Service
   * process has been killed.
   */
  class DeathNotifier : public IBinder::DeathRecipient {
  public:
    DeathNotifier(sp<DisplayService> parent) :
        parent_(parent) {}

    /**
     * @brief Method called when binder object unexpectedly goes away
     */
    void binderDied(const wp<IBinder>&) override {
      QMMF_WARN("DisplaySerive:%s: Client Exited or died!", __func__);
      assert(parent_.get() != nullptr);
      parent_->Disconnect();
    }
  sp<DisplayService> parent_;
  DisplayHandle display_handle_;
  };

  friend class DeathNotifier;

  /**
   * @brief Native < IDisplayService > onTransact method
   * 
   * This method is responsible for handling incoming messages from clients.
   * Based on the incoming call from client, it calls corresponding service method
   * and returns the status back to the client.
   *
   * @param[in] code : Mapping of the method calls from client based
   *                  on enum ::QMMF_DISPLAY_SERVICE_CMDS
   * @param data[in] : Data sent by the Proxy implementation of < IDisplayService >
   *                  This data may be used by the service method.
   * @param reply[out] : Data sent in response from the native/service
   *                    to the proxy/client
   */
  status_t onTransact(uint32_t code, const Parcel& data,
      Parcel* reply, uint32_t flags = 0) override;

  /**
   * @brief Connect API does the following:
   *
   *  - Open ion_device_
   *  - Call CreateDisplayCore of DisplayImpl to create instance of DisplayImpl
   * and create Allocator Device for buffer management
   *  - Connect to DisplayImpl
   *  - Allocate DeathNotifier
   */
  status_t Connect() override;

  /**
   * @brief Disconnect API does following:
   *
   * - Close ion_device_
   * - Delete DeathNotifier
   * - Disconnect from DisplayImpl
   * - Delete DisplayImpl instance
   */
  status_t Disconnect() override;

  /**
   * @brief CreateDisplay API does following:
   *
   * - Create RemoteCallBack based on service_cb
   * - Call DisplayImpl CreateDisplay
   * - For RemoteCallBack, register a recipient for a notification if this binder
   * goes away by calling linkToDeath of DeathRecipient
   *
   * @param[out] service_cb : Callbacks from DisplayService to DisplayClient
   * @param[in] display_type : Client specify the type of display from enum ::DisplayType
   * @param[out] display_handle : allocated by DisplayImpl
   */
  status_t CreateDisplay(const sp<IDisplayServiceCallback>&
    service_cb, DisplayType display_type, DisplayHandle* display_handle)
    override;

  /**
   * @brief DestroyDisplay APT has following functionality:
   *
   * - For RemoteCallBack, remove the previously registered death notification
   * by calling unlinkToDeath of DeathRecipient.
   * - Call DisplayImpl DestroyDisplay
   *
   * @param[in] display_handle : Destroy display corresponding to display_handle
   */
  status_t DestroyDisplay(DisplayHandle display_handle) override;

  /**
   * @brief CreateSurface API has following functionality:

   * - CreateSurface based om surface_id
   * - Call DisplayImpl CreateSurface API
   *
   * @param[in] surface_config : params for buffer allocation specified by struct ::SurfaceConfig
   * @param[in] display_handle : Specify display for which surface is created
   * @param[out] surface_id : unique id for each surface
   */
  status_t CreateSurface(DisplayHandle display_handle,
      SurfaceConfig &surface_config, uint32_t* surface_id) override;

  /**
   * @brief DestroySurface API has following functionality:
   *
   * - DestroySurface based om surface_id
   * - Call DisplayImpl DestroySurface API
   *
   * @param[in] display_handle : Specify display for which surface is destroyed
   * @param[in] surface_id : unique surface_id to be destroyed
   */
  status_t DestroySurface(DisplayHandle display_handle,
      const uint32_t surface_id) override;

  /**
   * @brief Call DisplayImpl DequeueSurfaceBuffer
   *
   * @param[in] display_handle : Specify display
   * @param[in] surface_id : Specify display for which buffer is to be dequeued
   * @param[out] surface_buffer : Dequeued buffer specified by struct ::SurfaceBuffer
   */
  status_t DequeueSurfaceBuffer(DisplayHandle display_handle,
      const uint32_t surface_id, SurfaceBuffer &surface_buffer) override;

  /**
   * @brief Call DisplayImpl QueueSurfaceBuffer
   *
   * @param[in] display_handle : Specify display
   * @param[in] surface_id : Specify display for which buffer is to be queued
   * @param[in] surface_buffer : Queued buffer specified by struct ::SurfaceBuffer
   */
  status_t QueueSurfaceBuffer(DisplayHandle display_handle,
      const uint32_t surface_id, SurfaceBuffer &surface_buffer,
      SurfaceParam &surface_param) override;

  /**
   * @brief Call DisplayImpl GetDisplayParam to get the Display Params
   *
   * @param[in] param_type : Specify Display param type such as Brightness or State
   * @param[out] param : Display param value
   */
  status_t GetDisplayParam(DisplayHandle display_handle,
      DisplayParamType param_type, void *param, size_t param_size) override;

  /**
   * @brief Call DisplayImpl SetDisplayParam to set the display params
   * based on param_type and param value
   */
  status_t SetDisplayParam(DisplayHandle display_handle,
      DisplayParamType param_type, void *param, size_t param_size) override;

  status_t DequeueWBSurfaceBuffer(DisplayHandle display_handle,
      const uint32_t surface_id, SurfaceBuffer &surface_buffer) override;

  status_t QueueWBSurfaceBuffer(DisplayHandle display_handle,
      const uint32_t surface_id, const SurfaceBuffer &surface_buffer)
      override;


  DisplayImpl*                                          display_;
  std::map<DisplayHandle, sp<RemoteCallBack>>           remote_callback_;
  sp<DeathNotifier>                                     death_notifier_;
  std::map<DisplayHandle, sp<IDisplayServiceCallback>>  client_handlers_;
  bool                                                  connected_;
  ion_fd_map                                            ion_fd_mapping_;
  use_buffer_map                                        use_buffer_mapping_;
  ion_surface_map                                       ion_surface_mapping_;
  int32_t                                               ion_device_;
  std::mutex                                            fd_map_lock_;
  std::mutex                                            use_buffer_map_lock_;
  std::mutex                                            remote_callback_lock_;
  std::mutex                                            client_handlers_lock_;

  typedef struct BufInfo {
    int32_t ion_fd; /**< Transferred ION Id */
    void    *pointer; /**< Memory mapped buffer */
    size_t  frame_len; /**< Size */
    ion_user_handle_t ion_handle; /**< ION handle */
    uint32_t surface_id; /**< surface_id */
  } BufInfo;

  typedef std::map<int32_t, BufInfo*> buf_info_map;
  /**< map <buffer index, BufInfo> */
  buf_info_map buf_info_map_;

};

}; //namespace display

}; //namespace qmmf
