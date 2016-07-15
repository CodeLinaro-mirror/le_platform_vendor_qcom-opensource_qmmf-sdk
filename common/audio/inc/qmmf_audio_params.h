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

#include <sys/types.h>
#include <cstdint>
#include <functional>
#include <vector>
#include <string>

#include "qmmf_codec.h"
#include "qmmf_device.h"

namespace qmmf {

namespace audio {

typedef int32_t status_t;

enum class AudioState {
    kNew,
    kIdle,
    kRunning,
    kPaused,
};

typedef struct AudioTrackBuffer {
    void    *data;
    size_t   capacity;
    size_t   size;
    int64_t  timestamp;
    uint32_t flags;
} AudioTrackBuffer;

// Audio event callbacks

enum class EventType {
    kError,
    kStateChanged,
    kWriteComplete,
    kReadComplete,
};

typedef union EventData {
    int32_t error;
    AudioState state;
    std::vector<AudioTrackBuffer> &buffers;
} EventData;

typedef std::function<void(EventType event_type,
                           EventData event_data)> AudioEventCallback;

enum class AudioType {
    kSource,
    kSink,
};

typedef struct AudioParams {
    uint32_t sample_rate;
    uint32_t channels;
    uint32_t bit_depth;
    CodecID codec_type;
    AudioCodecParam codec_param;
    uint32_t flags;
} AudioParams;

// Audio parameter types

enum class AudioParamType {
    kVolume,
    kDevice,
};

typedef union AudioParamData {
    uint32_t volume;
    struct {
        bool enable;
        DeviceID id;
    } device;
} AudioParamData;

}; // namespace audio
}; // namespace qmmf
