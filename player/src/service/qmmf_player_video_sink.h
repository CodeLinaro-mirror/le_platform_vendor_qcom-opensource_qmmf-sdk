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

#include "common/codecadaptor/src/qmmf_avcodec.h"
#include "player/src/service/qmmf_player_common.h"

namespace qmmf {
namespace player {

class VideoTrackSink;

class VideoSink
{

public:

static VideoSink* CreateVideoSink();

~VideoSink();

 status_t CreateTrackSink(uint32_t track_id, VideotrackParams& param);

  const sp<VideoTrackSink>& GetTrackSink(uint32_t track_id);

private:
   VideoSink();

 static VideoSink* instance_;

  // Map of track it and TrackSinks.
  DefaultKeyedVector<uint32_t, sp<VideoTrackSink> > video_track_sinks;
};


class VideoTrackSink : public IOutputCodecSource
{

public:

VideoTrackSink();

~VideoTrackSink();

status_t Init(VideoTrackParams& param);

status_t GetBuffer(CodecBuffer& codec_buffer);

status_t ReturnBuffer(CodecBuffer& codec_buffer);

};


}; //player
}; //qmmf
