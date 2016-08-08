/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
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


#pragma once

#include <functional>

#include "qmmf-sdk/qmmf_codec.h"
#include "qmmf-sdk/qmmf_recorder_params.h"
#include "common/qmmf_common_utils.h"

namespace qmmf {
using namespace recorder;

static const OMX_U32 kPortIndexInput = 0;
static const OMX_U32 kPortIndexOutput = 1;

//Codec will notify input port status to Track source.
enum class CodecInputPortStatus {
  kInputPortStart,
  //Notify when codec received EOS from track source.
  kInputPortStop,
  //Notify when codec returned all buffer to track source.
  kInputPortIdle,
};

typedef std::function<void(OMX_EVENTTYPE event, OMX_U32 data1, OMX_U32 data2)>
            AVCodecEventCb;

typedef struct CodecCreateParam {
  VideoTrackCreateParam video_param;
  AVCodecEventCb        event_cb;
} CodecCreateParam;

typedef union CodecSetParam {
  VideoTrackCreateParam video_param;
  AudioTrackCreateParam audio_param;
} CodecSetParam;

}; //namespace qmmf
