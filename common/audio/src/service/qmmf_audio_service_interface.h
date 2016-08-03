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

#include <cstdint>

#include <binder/IBinder.h>
#include <binder/IInterface.h>
#include <binder/Parcel.h>
#include <utils/Errors.h>
#include <utils/RefBase.h>

#include "common/audio/inc/qmmf_audio_definitions.h"
#include "common/audio/src/service/qmmf_audio_common.h"

namespace qmmf {
namespace common {
namespace audio {

using ::android::BnInterface;
using ::android::IBinder;
using ::android::IInterface;
using ::android::Parcel;
using ::android::sp;
using ::android::status_t;

enum class AudioServiceCommand {
  kAudioConnect = IBinder::FIRST_CALL_TRANSACTION,
  kAudioDisconnect,
  kAudioConfigure,
  kAudioStart,
  kAudioStop,
  kAudioPause,
  kAudioResume,
  kAudioSendBuffers,
  kAudioGetLatency,
  kAudioGetBufferSize,
  kAudioSetParam,
};

enum class AudioServiceCallbackCommand {
  kAudioNotifyError = IBinder::FIRST_CALL_TRANSACTION,
  kAudioNotifyBuffer,
};

static const char* kAudioServiceName = "audio.service";

/* Binder interface for callbacks from AudioService to AudioEndPointClient */
class IAudioServiceCallback : public IInterface {
 public:
  DECLARE_META_INTERFACE(AudioServiceCallback);

  virtual void NotifyErrorEvent(int error) = 0;
  virtual void NotifyBufferEvent(const AudioBuffer& buffer) = 0;
};

class IAudioService : public IInterface {
 public:
  DECLARE_META_INTERFACE(AudioService);

  virtual int Connect(const sp<IAudioServiceCallback>& client_handler,
                      AudioHandle* audio_handle) = 0;
  virtual int Disconnect(AudioHandle audio_handle) = 0;
  virtual int Configure(AudioHandle audio_handle, AudioEndPointType type,
                        const DeviceIdList& devices,
                        const AudioMetadata& metadata) = 0;

  virtual int Start(AudioHandle audio_handle) = 0;
  virtual int Stop(AudioHandle audio_handle, bool flush) = 0;
  virtual int Pause(AudioHandle audio_handle) = 0;
  virtual int Resume(AudioHandle audio_handle) = 0;

  virtual int SendBuffers(AudioHandle audio_handle,
                          const AudioBufferList& buffers) = 0;

  virtual int GetLatency(AudioHandle audio_handle, int* latency) = 0;
  virtual int GetBufferSize(AudioHandle audio_handle, int* buffer_size) = 0;
  virtual int SetParam(AudioHandle audio_handle, AudioParamType type,
                       const AudioParamData& data) = 0;
};

/* this class is responsible to provide callbacks from audio service */
class BnAudioServiceCallback : public BnInterface<IAudioServiceCallback> {
 public:
  virtual status_t onTransact(uint32_t code, const Parcel &data, Parcel *reply,
                              uint32_t flags = 0);
};

}; /* namespace audio */
}; /* namespace common */
}; /* namespace qmmf */
