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

#include <cstddef>
#include <cstdlib>
#include <vector>
#include <string>

#include "qmmf_audio_params.h"

namespace qmmf {
namespace audio {

// Update Makefile if you change these
const int kMajorVersion = 1;
const int kMinorVersion = 0;

class AudioEndPointClient;

class AudioEndPoint
{
public:
    AudioEndPoint();

    ~AudioEndPoint();

    // Connect to audio service and set callback
    status_t Connect(AudioEventCallback &cb);

    // Disconnect from audio service
    status_t Disconnect();

    status_t Configure(const AudioType type,
                       const std::vector<DeviceID> &devices,
                       const AudioParams &params);

    // Asynchronous
    status_t Start();

    // Asynchronous
    status_t Stop(const bool do_flush);

    // Asynchronous
    status_t Pause();

    // Asynchronous
    status_t Resume();

    // Asynchronous
    status_t Read(const std::vector<AudioTrackBuffer> &buffers);

    // Asynchronous
    status_t Write(const std::vector<AudioTrackBuffer> &buffers);

    status_t GetLatency(uint32_t &latency);

    status_t GetBufferSize(uint32_t &buffer_size);

    status_t SetParam(const AudioParamType type,
                      const AudioParamData &data);

private:
    AudioEndPointClient *audio_endpoint_client_;
};

}; // namespace audio
}; // namespace qmmf
