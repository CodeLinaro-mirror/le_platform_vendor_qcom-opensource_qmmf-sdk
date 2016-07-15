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

#include <binder/IBinder.h>
#include <binder/IServiceManager.h>
#include <binder/Parcel.h>

#include "qmmf_audio_params.h"

namespace qmmf {
namespace audio {

using namespace android;

#define QMMF_AUDIO_SERVICE_NAME "audio.service"

enum QMMF_AUDIO_SERVICE_CMDS {
    AUDIO_CONNECT = IBinder::FIRST_CALL_TRANSACTION,
    AUDIO_DISCONNECT,
    AUDIO_CONFIGURE,
    AUDIO_START,
    AUDIO_STOP,
    AUDIO_PAUSE,
    AUDIO_RESUME,
    AUDIO_READ,
    AUDIO_WRITE,
    AUDIO_GET_LATENCY,
    AUDIO_GET_BUFFER_SIZE,
    AUDIO_SET_PARAM,
};


class IAudioServiceCallback;
class IAudioService : public IInterface
{
public:
    DECLARE_META_INTERFACE(AudioService);

    virtual status_t Connect(const sp<IAudioServiceCallback> &service_cb,
                             std::string &handle) = 0;

    virtual status_t Disconnect(const std::string &handle) = 0;

    virtual status_t Configure(const std::string &handle,
                               const AudioType type,
                               const std::vector<DeviceID> &devices,
                               const AudioParams &params);

    virtual status_t Start(const std::string &handle) = 0;

    virtual status_t Stop(const std::string &handle,
                          const bool do_flush) = 0;

    virtual status_t Pause(const std::string &handle) = 0;

    virtual status_t Resume(const std::string &handle) = 0;

    virtual status_t Read(const std::string &handle,
                          const std::vector<AudioTrackBuffer> &buffers) = 0;

    virtual status_t Write(const std::string &handle,
                           const std::vector<AudioTrackBuffer> &buffers) = 0;

    virtual status_t GetLatency(const std::string &handle,
                                uint32_t &latency) = 0;

    virtual status_t GetBufferSize(const std::string &handle,
                                   uint32_t &buffer_size) = 0;

    virtual status_t SetParam(const std::string &handle,
                              const AudioParamType type,
                              const AudioParamData &data) = 0;
};

enum AUDIO_SERVICE_CB_CMDS {
  AUDIO_NOTIFY_ERROR=IBinder::FIRST_CALL_TRANSACTION,
  AUDIO_NOTIFY_STATE_CHANGED,
  AUDIO_NOTIFY_READ_COMPLETE,
  AUDIO_NOTIFY_WRITE_COMPLETE,
};

// Binder interface for callbacks from AudioService to AudioEndPointClient
class IAudioServiceCallback : public IInterface
{
public:
    DECLARE_META_INTERFACE(AudioServiceCallback);

    virtual void notifyErrorEvent(int32_t error) = 0;

    virtual void notifyStateChangedEvent(AudioState state) = 0;

    virtual void notifyReadCompleteEvent(
                                    std::vector<AudioTrackBuffer> &buffers) = 0;

    virtual void notifyWriteCompleteEvent(
                                    std::vector<AudioTrackBuffer> &buffers) = 0;
};

// this class is responsible to provide callbacks from audio service
class BnAudioServiceCallback : public BnInterface<IAudioServiceCallback>
{
public:
    virtual status_t onTransact(uint32_t code, const Parcel &data,
                                Parcel *reply, uint32_t flags = 0);
};

}; // namespace audio
}; // namespace qmmf
