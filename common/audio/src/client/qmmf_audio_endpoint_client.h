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

#include <utils/Errors.h>
#include <utils/Mutex.h>
#include <utils/RefBase.h>

#include "qmmf_audio_params.h"
#include "qmmf_audio_service_intf.h"

namespace qmmf {
namespace audio {

class AudioEndPointClient : public BnAudioServiceCallback
{
public:
    AudioEndPointClient();

    ~AudioEndPointClient();

    status_t Connect(AudioEventCallback &cb);

    status_t Disconnect();

    status_t Configure(const AudioType type,
                       const std::vector<DeviceID> &devices,
                       const AudioParams &params);

    status_t Start();

    status_t Stop(const bool do_flush);

    status_t Pause();

    status_t Resume();

    status_t Read(const std::vector<AudioTrackBuffer> &buffers);

    status_t Write(const std::vector<AudioTrackBuffer> &buffers);

    status_t GetLatency(uint32_t &latency);

    status_t GetBufferSize(uint32_t &buffer_size);

    status_t SetParam(const AudioParamType type,
                      const AudioParamData &data);

private:
    // methods of BnAudioServiceCallback.
    void notifyErrorEvent(int32_t error);

    void notifyStateChangedEvent(AudioState state)

    void notifyReadCompleteEvent(std::vector<AudioTrackBuffer> &buffers);

    void notifyWriteCompleteEvent(std::vector<AudioTrackBuffer> &buffers);

    class DeathNotifier : public IBinder::DeathRecipient
    {
    public:
        DeathNotifier(sp<AudioEndPointClient> parent) : parent_(parent){}

        void binderDied(const wp<IBinder>&) override
        {
            QMMF_INFO("%s: audio service died", __func__);

            Mutex::Autolock lock(parent_->lock_);
            parent_->audio_service_.clear();
            parent_->audio_service_ = NULL;
        }

        sp<AudioEndPointClient> parent_;
    };

    friend class DeathNotifier;

    Mutex              lock_;
    sp<IAudioService>  audio_service_;
    sp<DeathNotifier>  death_notifier_;
    AudioEventCallback audio_cb_;
    std::string        client_handle_;
};

}; // namespace audio
}; // namespace qmmf
