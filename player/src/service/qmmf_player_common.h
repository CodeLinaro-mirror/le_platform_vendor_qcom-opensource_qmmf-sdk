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


#include "include/qmmf-sdk/qmmf_codec.h"
#include "include/qmmf-sdk/qmmf_device.h"

// Enable this define to dump bitstream from demuxer
//#define DUMP_BITSTREAM

// Enable this define to dump YUV from decoder.
//#define DUMP_YUV_FRAMES

// Enable this define to dump PCM from decoder.
//#define DUMP_PCM_DATA


namespace qmmf {
namespace player {


enum class TrackType {
    kVideo,
    kAudio
};

enum PlayerState
{
     QPLAYER_STATE_ERROR = 0,
     QPLAYER_STATE_IDLE = 1 << 0,
     QPLAYER_STATE_PREPARED = 1 << 1,
     QPLAYER_STATE_STARTED = 1 << 2,
     QPLAYER_STATE_PAUSED = 1 << 3,
     QPLAYER_STATE_STOPPED =  1 << 4,
     QPLAYER_STATE_PLAYBACK_COMPLETED = 1<< 5,
};

/*
typedef std::function<void(uint32_t track_id, std::vector<BnBuffer> buffers,
    void *meta_param, MetaParamType meta_type, size_t meta_size)>
    buffer_callback;
*/

typedef struct AudioTrackParams {
  uint32_t               track_id;
  uint32_t               sample_rate;
  uint32_t               channels;
  uint32_t               bit_depth;
  AudioFormat            format;
  AudioCodecParams       codec_param;
  AudioOutSubtype        out_device;
  //buffer_callback        data_cb;
}AudioTrackParams;


typedef struct VideotrackParams {
  uint32_t               track_id;
  uint32_t               width;
  uint32_t               height;
  uint32_t               frame_rate;
  VideoFormat            format;
  VideoCodecParams       codec_param;
  VideoOutSubtype        out_device;
  //buffer_callback        data_cb;
}VideoTrackParams;


typedef struct AVCodecBuffer {
    void *data;
    size_t frame_length;
    size_t filled_length;
    int64_t time_stamp;
    uint32_t flag;
    uint32_t fd;
    uint32_t buf_id;
} AVCodecBuffer;


typedef struct Event{
   PlayerState state;
};


extern "C" void DebugCreateAudioTrackParam (const char* _func_,
                                            AudioTrackParams& params);

extern "C" void DebugCreateVideoTrackParam (const char* _func_,
                                            VideoTrackParams& params);

extern "C" void DebugAudioSinkParam (const char* _func_,
                                            AudioTrackParams& params);

extern "C" void DebugQueueInputBuffer(const char* _func_,
                                         std::vector<AVCodecBuffer>& buffers);


}; //player
}; //qmmf
