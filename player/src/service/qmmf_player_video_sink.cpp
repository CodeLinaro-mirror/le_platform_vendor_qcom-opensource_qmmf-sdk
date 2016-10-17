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

#define TAG "VideoSink"


#include "player/src/service/qmmf_player_video_sink.h"


namespace qmmf {

namespace player {


VideoSink* VideoSink::instance_ = nullptr;

VideoSink* VideoSink::CreateVideoSink()
{
  if(!instance_) {
      instance_ = new VideoSink();
      if(!instance_) {
          QMMF_ERROR("%s:%s: Can't Create VideoSink Instance!", TAG, __func__);
          return NULL;
        }
      }

  QMMF_INFO("%s:%s: VideoSink Instance Created Successfully(0x%x)", TAG,
      __func__, instance_);

  return instance_;
}

VideoSink::VideoSink()
{

}

VideoSink::~VideoSink()
{

}

status_t VideoSink::CreateTrackSink(uint32_t track_id, VideotrackParams& param)
{
  sp<VideoTrackSink> track_sink;

  if (param.out_device == VideoOutSubtype::kLCD)
        track_sink = new VideoTrackSink();

  video_track_sinks.add(track_id,track_sink);
}

const sp<VideoTrackSink>& VideoSink::GetTrackSink(uint32_t track_id)
{
  int32_t idx = video_track_sinks.indexOfKey(track_id);
  assert(idx >= 0);
  return video_track_sinks.valueFor(track_id);
}

VideoTrackSink::VideoTrackSink()
{

}


VideoTrackSink::~VideoTrackSink()
{

}

status_t VideoTrackSink::Init(VideoTrackParams& param)
{

}

status_t VideoTrackSink::GetBuffer(CodecBuffer& codec_buffer)
{

  return 0;
}


status_t VideoTrackSink::ReturnBuffer(CodecBuffer& codec_buffer)
{

  return 0;
}

};
};
