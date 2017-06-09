/*
 * Copyright (c) 2017, The Linux Foundation. All rights reserved.
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

#include <functional>
#include <utils/RefBase.h>
#include "common/qmmf_common_utils.h"
#include "common/cameraadaptor/qmmf_camera3_types.h"

#include "recorder/src/service/camera-reprocess/common/qmmf_camera_base_types.h"
#include "recorder/src/service/camera-reprocess/interface/qmmf_camera_callback.h"

namespace qmmf {

using namespace cameraadaptor;

namespace recorder {

class IPostProcCameraContext {
public:
  virtual status_t ReturnStreamBuffer(StreamBuffer buffer) = 0;
  virtual status_t CreateDeviceStream(CameraStreamParameters& params,
                                      uint32_t frame_rate,
                                      int32_t* stream_id) = 0;
  virtual status_t CreateDeviceInputStream(CameraInputStreamParameters& params,
                                           int32_t* stream_id) = 0;
  virtual status_t SubmitRequest(Camera3Request request,
                                 bool is_streaming,
                                 int64_t *lastFrameNumber) = 0;
  virtual status_t DeleteDeviceStream(int32_t stream_id, bool cache) = 0;
  virtual ~IPostProcCameraContext() {};
};

class ICameraReprocess : public virtual IReprocessCallbacks,
                         public virtual RefBase {

 public:

  virtual ~ICameraReprocess() {};

  virtual int32_t Create(const int32_t stream_id,
                         const ReprocParam& input,
                         const ReprocParam& output,
                         const uint32_t frame_rate,
                         const uint32_t num_images,
                         const void* static_data,
                         const void* context) = 0;

  virtual status_t Delete() = 0;

  virtual void AddBuff(StreamBuffer in_buff) = 0;

  virtual status_t ReturnBuff(StreamBuffer buffer) = 0;

  virtual void AddResult(const void* result) = 0;

  virtual status_t Start() = 0;

  virtual status_t Stop() = 0;

};

}; //namespace recorder

}; //namespace qmmf
