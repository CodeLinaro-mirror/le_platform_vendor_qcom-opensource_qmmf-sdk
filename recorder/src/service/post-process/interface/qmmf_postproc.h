/*
 * Copyright (c) 2017, 2019, The Linux Foundation. All rights reserved.
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

/*! @file qmmf_postproc.h
*/

#pragma once

#include <functional>
#include "common/utils/qmmf_common_utils.h"
#include "common/cameraadaptor/qmmf_camera3_types.h"

namespace qmmf {

using namespace cameraadaptor;

namespace recorder {

/// Interface to Camera Device for re-process
class IPostProc {
public:

  /// Return buffer to Camera
  virtual status_t ReturnStreamBuffer(StreamBuffer buffer) = 0;

  /// Create Camera stream
  virtual status_t CreateDeviceStream(CameraStreamParameters& params,
                                      uint32_t frame_rate,
                                      int32_t* stream_id,
                                      bool cache) = 0;

  /// Create input Camera stream for re-process
  virtual status_t CreateDeviceInputStream(CameraInputStreamParameters& params,
                                           int32_t* stream_id,
                                           bool cache) = 0;

  /// Submit Capture request to Camera
  virtual status_t SubmitRequest(Camera3Request request,
                                 bool is_streaming,
                                 int64_t *lastFrameNumber) = 0;

  /// Delete Camera stream
  virtual status_t DeleteDeviceStream(int32_t stream_id, bool cache) = 0;

  /// Get default camera parameters for given use case
  virtual status_t CreateCaptureRequest(Camera3Request& request,
                                  camera3_request_template_t template_type) = 0;

  /// Return supported camera parameters
  virtual CameraMetadata GetCameraStaticMeta() = 0;

  /// IPostProc Destructor
  virtual ~IPostProc() {};
};

}; //namespace recorder

}; //namespace qmmf
