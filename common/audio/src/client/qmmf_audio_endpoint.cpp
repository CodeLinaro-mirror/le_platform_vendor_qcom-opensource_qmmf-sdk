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

#define LOG_TAG "AudioEndPoint"

#include <binder/IPCThreadState.h>

#include "qmmf_audio_endpoint.h"
#include "qmmf_audio_params.h"
#include "qmmf_audio_endpoint_client.h"
#include "qmmf_audio_common.h"

namespace qmmf {
namespace audio {

AudioEndPoint::AudioEndPoint()
    : audio_endpoint_client_(nullptr)
{
}

AudioEndPoint::~AudioEndPoint()
{
    if(audio_endpoint_client_) {
        delete audio_endpoint_client_;
        audio_endpoint_client_ = NULL;
    }
}

status_t AudioEndPoint::Connect(AudioEventCallback &cb)
{
    audio_endpoint_client_ = new AudioEndPointClient();
    if(!audio_endpoint_client_) {
        return NO_MEMORY;
    }

    auto ret = audio_endpoint_client_->Connect(cb);
    if(ret != NO_ERROR) {
        QMMF_ERROR("%s: client->Connect failed: %d", __func__, ret);
    }

    return ret;
}

status_t AudioEndPoint::Disconnect()
{
    assert(audio_endpoint_client_ != NULL);

    auto ret = audio_endpoint_client_->Disconnect();
    if(ret != NO_ERROR) {
        QMMF_ERROR("%s: client->Disconnect failed: %d", __func__, ret);
    }

    if(audio_endpoint_client_) {
        delete audio_endpoint_client_;
        audio_endpoint_client_ = NULL;
    }

    return ret;
}

status_t AudioEndPoint::Configure(const AudioType type,
                                  const std::vector<DeviceID> &devices,
                                  const AudioParams &params)
{
    assert(audio_endpoint_client_ != NULL);

    auto ret = audio_endpoint_client_->Configure(type, devices, params);
    if(ret != NO_ERROR) {
        QMMF_ERROR("%s: client->Create failed: %d", __func__, ret);
    }

    return ret;
}

status_t AudioEndPoint::Start()
{
    assert(audio_endpoint_client_ != NULL);

    auto ret = audio_endpoint_client_->Start();
    if(ret != NO_ERROR) {
        QMMF_ERROR("%s: client->Start failed: %d", __func__, ret);
    }

    return ret;
}

status_t AudioEndPoint::Stop(const bool do_flush)
{
    assert(audio_endpoint_client_ != NULL);

    auto ret = audio_endpoint_client_->Stop(do_flush);
    if(ret != NO_ERROR) {
        QMMF_ERROR("%s: client->Stop failed: %d", __func__, ret);
    }

    return ret;
}

status_t AudioEndPoint::Pause()
{
    assert(audio_endpoint_client_ != NULL);

    auto ret = audio_endpoint_client_->Pause();
    if(ret != NO_ERROR) {
        QMMF_ERROR("%s: client->Pause failed: %d", __func__, ret);
    }

    return ret;
}

status_t AudioEndPoint::Resume()
{
    assert(audio_endpoint_client_ != NULL);

    auto ret = audio_endpoint_client_->Resume();
    if(ret != NO_ERROR) {
        QMMF_ERROR("%s: client->Resume failed: %d", __func__, ret);
    }

    return ret;
}

status_t AudioEndPoint::Read(const std::vector<AudioTrackBuffer> &buffers)
{
    assert(audio_endpoint_client_ != NULL);

    auto ret = audio_endpoint_client_->Read(buffers);
    if(ret != NO_ERROR) {
        QMMF_ERROR("%s: client->Read failed: %d", __func__, ret);
    }

    return ret;
}

status_t AudioEndPoint::Write(const std::vector<AudioTrackBuffer> &buffers)
{
    assert(audio_endpoint_client_ != NULL);

    auto ret = audio_endpoint_client_->Write(buffers);
    if(ret != NO_ERROR) {
        QMMF_ERROR("%s: client->Write failed: %d", __func__, ret);
    }

    return ret;
}

status_t AudioEndPoint::GetLatency(uint32_t &latency)
{
    assert(audio_endpoint_client_ != NULL);

    auto ret = audio_endpoint_client_->GetLatency(latency);
    if(ret != NO_ERROR) {
        QMMF_ERROR("%s: client->GetLatency failed: %d", __func__, ret);
    }

    return ret;
}

status_t AudioEndPoint::GetBufferSize(uint32_t &buffer_size)
{
    assert(audio_endpoint_client_ != NULL);

    auto ret = audio_endpoint_client_->GetBufferSize(buffer_size);
    if(ret != NO_ERROR) {
        QMMF_ERROR("%s: client->GetBufferSize failed: %d", __func__, ret);
    }

    return ret;
}

status_t AudioEndPoint::SetParam(const AudioParamType type,
                             const AudioParamData &data)
{
    assert(audio_endpoint_client_ != NULL);

    auto ret = audio_endpoint_client_->SetParam(type, data);
    if(ret != NO_ERROR) {
        QMMF_ERROR("%s: client->SetParam failed: %d", __func__, ret);
    }

    return ret;
}

}; // namespace audio
}; // namespace qmmf
