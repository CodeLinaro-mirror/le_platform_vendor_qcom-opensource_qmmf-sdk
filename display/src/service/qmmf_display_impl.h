/*
* Copyright (c) 2016-2019, The Linux Foundation. All rights reserved.
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

/*! @file qmmf_display_impl.h
*/

#pragma once

#include <map>
#include <vector>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <set>
#include <chrono>

#include <hardware/gralloc.h>
#include <utils/KeyedVector.h>
#include <sdm/core/core_interface.h>
#include <sdm/utils/locker.h>

#include "qmmf-sdk/qmmf_display_params.h"
#include "common/utils/qmmf_log.h"
#include "qmmf_memory_interface.h"
#include "display/src/service/qmmf_display_common.h"
#include "display/src/service/qmmf_remote_cb.h"
#include "display/src/service/qmmf_display_sdm_buffer_allocator.h"
#include "display/src/service/qmmf_display_sdm_buffer_sync_handler.h"
#include "display/src/service/qmmf_display_sdm_debugger.h"

namespace qmmf {

/// @namespace qmmf::display
namespace display {

using ::sdm::DisplayEventHandler;
using ::sdm::DisplayError;
using ::sdm::LayerRect;
using ::sdm::CoreInterface;
using ::sdm::Layer;
using ::sdm::LayerBuffer;
using ::sdm::DisplayInterface;
using ::sdm::LayerStack;
using ::sdm::DisplayEventVSync;
using ::sdm::Locker;
using ::sdm::BufferInfo;
using ::sdm::LayerBlending;
using ::sdm::LayerBufferFormat;


#define NUM_DISPLAY_ALLOWED 3
#define FLOAT(exp) static_cast<float>(exp)

#ifndef QMMF_DISPLAY_INTF_v1
#define DISPLAY_EVENT DisplayEvent
#else
#define DISPLAY_EVENT ::DisplayEvent
#endif

class DisplayImpl : public DisplayEventHandler
{
 public:

  static DisplayImpl* CreateDisplayCore();

  ~DisplayImpl();

  status_t Connect();

  status_t Disconnect();

  /**
   * @brief Call sdm's CoreInterface CreateDisplay API to Create a Display with
   *
   * type display_type. CoreInterface takes an object of DisplayInterface as an
   * out param which is used to configure or submit layers for composition on
   * the display device.
   *
   * The API also set the Composition State, Display State, creates LayerStack,
   * set VSync event state, and start the HandleVSync thread.
   *
   * If Display type already exists then simply increase
   * num_of_clients for that Display
   *
   * @param[in] display_type : Create display of type display_type
   * @param[out] display_handle : handle to a specific client for this display
   */
  status_t CreateDisplay(sp<RemoteCallBack>& remote_cb,
      DisplayType display_type, DisplayHandle* display_handle);

  /**
   * @brief If Display State is ON, do following:
   *
   * - Delete HandleVsync Thread
   * - SetVSyncState to false
   * - Set Display State to Off
   * - Call sdm's CoreInterface DestroySurface API for display_handle
   */
  status_t DestroyDisplay(DisplayHandle display_handle);

  /**
   * @brief If Display State is ON, do following:
   *
   * - Create a new Layer and assigns surface_id by Calling AllocateLayer API
   * for display_handle
   * - Assigns the Layer params based on surface_config
   * - Add the Layer to LayerStack by calling GetLayerStack API
   * - Check hardware capability to compose Layers by calling sdm's Prepare API
   * - Allocate Buffers if Allocate Buffer of buffer allocation is used
   *
   * @param[out] surface_id : id of the created surface
   */
  status_t CreateSurface(DisplayHandle display_handle,
      SurfaceConfig &surface_config, uint32_t* surface_id);

  /**
   * @brief If Display State is ON, do following:
   *
   * - Free the Layer associated with display_handle by calling FreeLayer API
   * - Free the buffers, if Allocate Mode is used
   * - Erase the SurfaceInfo associated with display_handle
   */
  status_t DestroySurface(DisplayHandle display_handle,
      const uint32_t surface_id);

  /**
   * @brief If Display State is ON, do following:
   *
   * - If Buffer State if kStateFree, change it to kStateDequeued
   * - Fill surface_buffer params based on BufferInfo
   */
  status_t DequeueSurfaceBuffer(DisplayHandle display_handle,
      const uint32_t surface_id, SurfaceBuffer &surface_buffer);

  /**
   * @brief If Display State is ON, do following:
   *
   * - Update QueuedBufferInfo based on surface_buffer and surface_param
   * - If the Buffer State is kStateDequeued, do following:
   *  - Free the previously queued buffer, if present
   *  - Update the new buffer state to kStateQueued
   * - For Use Buffer Mode, no BufferInfo is present for display_handle:
   *  - Create new BufferInfo and set the state to kStateQueued
   *  - Update BufferInfo based on surface_buffer
   */
  status_t QueueSurfaceBuffer(DisplayHandle display_handle,
      const uint32_t surface_id, SurfaceBuffer &surface_buffer,
      SurfaceParam &surface_param);

  /**
   * @brief Gets the Display Param by calling DisplayInterface methods
   * based on param_type
   *
   * @param[in] param_type : DisplayParamType for display_handle
   * @param[out] param : Get current state of Display Device or
   * brightness level based on param_type
   */
  status_t GetDisplayParam(DisplayHandle display_handle,
      DisplayParamType param_type, void *param, size_t param_size);

  /**
   * @brief Sets the Display Param by calling DisplayInterface methods
   * based on param_type and param value
   */
  status_t SetDisplayParam(DisplayHandle display_handle,
      DisplayParamType param_type, void *param, size_t param_size);

  status_t DequeueWBSurfaceBuffer(DisplayHandle display_handle,
      const uint32_t surface_id, SurfaceBuffer &surface_buffer);

  status_t QueueWBSurfaceBuffer(DisplayHandle display_handle,
      const uint32_t surface_id, const SurfaceBuffer &surface_buffer);

 protected:
  inline void SetRect(const SurfaceRect &source, LayerRect *target);

  /**
   * @brief Create a new Layer and assigns a unique surface_id for Client
   * with handle display_handle
   *
   * @param[out] surface_id : unique surface_id
   */
  Layer* AllocateLayer(DisplayHandle display_handle, uint32_t* surface_id,
      uint32_t z_order);

  /**
   * @brief Delete the Layer associated with display_handle
   */
  status_t FreeLayer(DisplayHandle display_handle, const uint32_t surface_id);

  /**
   * @brief Get the Layer for SurfaceInfo associated with surface_id of
   * display_handle
   */
  Layer* GetLayer(DisplayHandle display_handle, const uint32_t surface_id);

  /**
   * @brief If a buffer with state kStateQueued is present then do following:
   *
   * - Free the committed buffer
   * - Fill layer params for the queued buffer
   * - Commit the queued buffer
   *
   * if queued_buffers_only if false then insert the Layer to Layerstack
   * for surface associated with display_type, if present
   */
  LayerStack* GetLayerStack(DisplayType display_type,
      bool queued_buffers_only);

  // DisplayEventHandler methods
  virtual DisplayError VSync(const DisplayEventVSync &vsync);
  DisplayError VSync(int fd, unsigned int sequence,
                             unsigned int tv_sec, unsigned int tv_usec,
                             void *data);

  virtual DisplayError PFlip(int fd, unsigned int sequence,
                             unsigned int tv_sec, unsigned int tv_usec,
                             void *data);
  virtual DisplayError Refresh();
  virtual DisplayError CECMessage(char *message);

  DisplayError HandleEvent(DISPLAY_EVENT event);

 private:

  std::mutex               thread_lock_;
  ::std::thread*           handle_vsync_thread_;
  std::mutex               vsync_callback_locker_;
  std::condition_variable  vsync_callback_;
  bool                     running_;
  bool                     is_first_commit_;

  /**Not allowed */
  DisplayImpl();
  DisplayImpl(const DisplayImpl&);
  DisplayImpl& operator=(const DisplayImpl&);

  static void HandleVSyncThreadEntry(DisplayImpl* display_impl);

  /**
   * @brief If Display State is ON, do following:
   *
   * - Call GetLayerstack to do following:
   *  - Free the committed buffer
   *  - Fill layer params for the queued buffer
   *  - Commit the queued buffer
   * - Check hardware capability to compose Layers by calling DisplayInterface
   * Prepare API
   * - Commit the layerstack, if Prepare is successful
   * - notifyVSyncEvent to client
   */
  void HandleVSync();
  static DisplayImpl* instance_;
  static const int32_t kNumberOfAttempts = 5;
  std::mutex display_on_lock_;
  std::condition_variable display_on_cond_;
  DisplayBufferSyncHandler buffer_sync_handler_;
  static CoreInterface* core_intf_;
  /**< It contains methods which client shall use
to create/destroy different display devices */
  IAllocDevice* alloc_device_interface_;
#ifndef TARGET_USES_GRALLOC1
  DisplayBufferAllocatorGralloc buffer_allocator_;
#else
  DisplayBufferAllocatorGralloc1 buffer_allocator_;
#endif // TARGET_USES_GRALLOC1

  enum class BufferStates {
    kStateFree      = 1, /**< x1 = 0, x2 = 0, x3 = 0 */
    kStateDequeued  = 2, /**< x1 = 1, x2 = 0, x3 = 0 */
    kStateQueued    = 3, /**< x1 = 0, x2 = 1, x3 = 0 */
    kStateCommitted = 4, /**< x1 = 0, x2 = 1, x3 = 1 */
    kInvalid        = 0x7FFFFFFF, /**< (!x1 & x2) | (!x2 & !x3) = 1 */
  };

  class BufferState {
   public:
    BufferState(const BufferStates state) {
      current_state_ = state;
      dequeued_  = GetDequeuedState(state);
      queued_    = GetQueuedState(state);
      committed_ = GetCommitedState(state);
    }

    inline BufferStates GetState() {return current_state_;}
    BufferStates SetState(const BufferStates state) {
      if (IsStateTransitionValid(current_state_, state)) {
        dequeued_  = GetDequeuedState(state);
        queued_    = GetQueuedState(state);
        committed_ = GetCommitedState(state);
        current_state_ = state;
      }
      return current_state_;
    }
   private:
    bool  dequeued_;   /**< x1 */
    bool  queued_;     /**< x2 */
    bool  committed_;  /**< x3 */
    BufferStates current_state_;

    /**
     *@brief  Returns true if Buffer state is kStateDequeued
     */
    inline bool GetDequeuedState(const BufferStates state) {
      if (state == BufferStates::kStateDequeued)
        return true;
      else
        return false;
    }

    /**
     * @brief Returns true if Buffer state is kStateQueued
     */
    inline bool GetQueuedState(const BufferStates state) {
      if (state == BufferStates::kStateQueued ||
          state == BufferStates::kStateCommitted)
        return true;
      else
        return false;
    }

    /**
     * @brief Returns true if Buffer state is kStateCommitted
     */
    inline bool GetCommitedState(const BufferStates state) {
      if (state == BufferStates::kStateCommitted)
        return true;
      else
        return false;
    }

    /**
     * @brief Returns true if Buffer state change from old_state to
     * new_state is valid
     */
    bool IsStateTransitionValid(const BufferStates old_state,
                                const BufferStates new_state) {
      if (old_state == BufferStates::kInvalid)
        return false;
      else if (old_state == BufferStates::kStateDequeued
               && (new_state == BufferStates::kStateQueued
               || new_state == BufferStates::kStateFree))
        return true;
      else if (old_state == BufferStates::kStateQueued
               && (new_state == BufferStates::kStateCommitted
               || new_state == BufferStates::kStateFree))
        return true;
      else if (old_state == BufferStates::kStateCommitted
               && new_state == BufferStates::kStateFree)
        return true;
      else if (old_state == BufferStates::kStateFree
               && new_state == BufferStates::kStateDequeued)
        return true;
      else
        return false;
    }
  };

  /**
   * @brief Print Buffer State for all buffers of surface with id surface_id
   */
  void PrintBuffersState (const uint32_t surface_id);

  typedef struct SurfaceInfo {
    Layer*                           layer;
    /**< Layer Allocated to a Surface */
    std::map<int32_t, BufferInfo*>   buffer_info;
    /**< map of buffer id and BufferInfo */
    std::map<int32_t, BufferState*>  buffer_state;
    /**< map of buffer id and BufferState */
    bool                             allocate_buffer_mode;
  } SurfaceInfo;

  /**
   * @brief Holds the information for a Client associated with a Display
   */
  typedef struct DisplayClientInfo {
    DisplayType                      display_type;
    uint32_t                         num_of_client_layers;
    /**< Number of surface layers of a display for a client */
    sp<RemoteCallBack>               remote_cb;
  } DisplayClientInfo;

  /**
   * @brief Holds the information for a Display
   */
  typedef struct DisplayTypeInfo {
    std::map<uint32_t, uint32_t>     z_order_surface_id_map;
    /**< Map for z_order and surface_id */
    uint32_t                         num_of_clients;
    /**< Number of clients for a display type */
    DisplayInterface*                display_intf;
    /**< Display device interface */
  } DisplayTypeInfo;


  /**
   * @brief Holds the information about buffer whose state is kStateQueued
   */
  typedef struct QueuedBufferInfo {
    SurfaceBuffer  surface_buffer;
    SurfaceParam   surface_param;
  } QueuedBufferInfo;

  DisplayHandle                                current_handle_;
  uint32_t                                     unique_surface_id_;
  std::mutex                                   api_lock_;
  std::mutex                                   surface_lock_;
  std::mutex                                   layer_lock_;
  std::map<DisplayHandle, DisplayClientInfo*>  display_client_info_map_;
  /**< Map for DisplayType and DisplayTypeInfo */
  std::map<DisplayType, DisplayTypeInfo*>      display_type_info_map_;
  /**< Map for surface_id and SurfaceInfo */
  std::map<uint32_t, SurfaceInfo*>             surface_info_map_;
  /**< Map for surface_id and SurfaceInfo */
  std::map<uint32_t, QueuedBufferInfo>         latest_queued_buffer_info_map_;
  /**< Map for surface_id and Latest QueuedBufferInfo */
  LayerStack*                                  layer_stack_;
  /**< layer_stack contains layers which need to be composed and
rendered onto the target */

  // Get/Set functions for Display State
  std::mutex display_state_lock_;
  DisplayState current_display_state_;

  inline DisplayState GetDisplayState() {
    std::lock_guard<std::mutex> lock(display_state_lock_);
    return current_display_state_;
  }

  inline void SetDisplayState(DisplayState state) {
    std::lock_guard<std::mutex> lock(display_state_lock_);
    current_display_state_ = state;
  }
};

}; // namespace display

}; //namespace qmmf
