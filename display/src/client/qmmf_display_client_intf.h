/* Copyright (c) 2018, 2019, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *     Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *     Neither the name of The Linux Foundation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.

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

/*! @file qmmf_display_client_intf.h
*/

#pragma once

#include "display/src/service/qmmf_display_common.h"
#include "qmmf-sdk/qmmf_display_params.h"

namespace qmmf {
namespace display {

class IDisplayClient {
 public:
  virtual ~IDisplayClient(){};

  virtual status_t Connect() = 0;

  virtual status_t Disconnect() = 0;

  virtual status_t CreateDisplay(DisplayType type, DisplayCb &cb) = 0;

  virtual status_t DestroyDisplay(DisplayType type) = 0;

  virtual status_t CreateSurface(SurfaceConfig &surface_config,
                                 uint32_t *surface_id) = 0;

  virtual status_t DestroySurface(const uint32_t surface_id) = 0;

  virtual status_t DequeueSurfaceBuffer(const uint32_t surface_id,
                                        SurfaceBuffer &surface_buffer) = 0;

  virtual status_t QueueSurfaceBuffer(const uint32_t surface_id,
                                      SurfaceBuffer &surface_buffer,
                                      SurfaceParam &surface_param) = 0;

  virtual status_t GetDisplayParam(DisplayParamType param_type, void *param,
                                   size_t param_size) = 0;

  virtual status_t SetDisplayParam(DisplayParamType param_type, void *param,
                                   size_t param_size) = 0;

  virtual status_t DequeueWBSurfaceBuffer(const uint32_t surface_id,
                                          SurfaceBuffer &surface_buffer) = 0;

  virtual status_t QueueWBSurfaceBuffer(
      const uint32_t surface_id, const SurfaceBuffer &surface_buffer) = 0;
};

};  // namespace display
};  // namespace qmmf
