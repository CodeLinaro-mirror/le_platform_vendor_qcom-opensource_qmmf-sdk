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

#pragma once

#include <utils/KeyedVector.h>

#include "qmmf_recorder_params.h"
#include "qmmf_recorder_common.h"
#include "qmmf_camera_source.h"

namespace qmmf {

namespace recorder {

class TrackEncoder;

class EncoderCore {
 public:

  static EncoderCore* CreateEncoderCore();

  ~EncoderCore();

  status_t AddSource(const sp<TrackSource>& track_source,
                     const VideoTrackParams& params);

  status_t StartTrackEncoder(uint32_t track_id);

  status_t StopTrackEncoder(uint32_t track_id);

  status_t SetTrackEncoderParams(uint32_t track_id,
                                 VideoTrackParamType param_type, void* param,
                                 uint32_t param_size);

  status_t DeleteTrackEncoder(uint32_t track_id);
 private:

  bool isTrackValid(uint32_t track_id);

  // vector <track_id, sp<TrackEncoder> >
  DefaultKeyedVector<uint32_t, sp<TrackEncoder> > track_encoders_;

  // Not allowed
  EncoderCore();
  EncoderCore(const EncoderCore&);
  EncoderCore& operator=(const EncoderCore&);
  static EncoderCore* instance_;
};

class TrackEncoder : public RefBase {
 public:

  TrackEncoder();

  ~TrackEncoder();

  status_t Init(const sp<TrackSource>& track_source,
                const VideoTrackParams& params);

  status_t Start();

  status_t Stop();

  status_t SetParams(VideoTrackParamType param_type, void* param,
                     uint32_t param_size);

 private:

  status_t AllocOutputPortBufs();

  sp<TrackSource> track_source_;

  VideoTrackParams track_params_;
};

}; // namespace recorder

}; // name space qmmf