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
#include <map>
#include <mutex>

#include <binder/Parcel.h>
#include <utils/RefBase.h>

#include "common/audio/inc/qmmf_audio_definitions.h"
#include "common/audio/src/service/qmmf_audio_service_interface.h"
#include "common/audio/src/service/qmmf_audio_common.h"
#include "common/audio/src/service/qmmf_audio_frontend.h"
#include "common/audio/src/service/qmmf_audio_ion.h"
#include "common/qmmf_log.h"

namespace qmmf {
namespace common {
namespace audio {

using ::android::BnInterface;
using ::android::IBinder;
using ::android::Parcel;
using ::android::sp;
using ::android::wp;
using ::std::lock_guard;
using ::std::map;
using ::std::mutex;

class AudioService : public BnInterface<IAudioService>
{
 public:
  AudioService();
  ~AudioService();

 private:
  int Connect(const sp<IAudioServiceCallback>& client_handler,
              AudioHandle* audio_handle) override;
  int Disconnect(AudioHandle audio_handle) override;
  int Configure(AudioHandle audio_handle, AudioEndPointType type,
                const DeviceIdList& devices,
                const AudioMetadata& metadata) override;

  int Start(AudioHandle audio_handle) override;
  int Stop(AudioHandle audio_handle, bool flush) override;
  int Pause(AudioHandle audio_handle) override;
  int Resume(AudioHandle audio_handle) override;

  int SendBuffers(AudioHandle audio_handle,
                  const AudioBufferList& buffers) override;

  int GetLatency(AudioHandle audio_handle, int* latency) override;
  int GetBufferSize(AudioHandle audio_handle, int* buffer_size) override;
  int SetParam(AudioHandle audio_handle, AudioParamType type,
               const AudioParamData& data) override;

  /* method of BnInterface<IAudioService> */
  int onTransact(uint32_t code, const Parcel& data, Parcel* reply,
                 uint32_t flags = 0) override;

  class DeathNotifier : public IBinder::DeathRecipient {
   public:
    DeathNotifier(AudioService* parent, AudioHandle audio_handle)
        : parent_(parent), audio_handle_(audio_handle) {}

    void binderDied(const wp<IBinder>&) override {
      QMMF_WARN("%s() audio client died", __func__);
      lock_guard<mutex> lock(parent_->lock_);
      /* TODO(kwestfie@codeaurora.org):
       * Investigate issue with the following statement:
       *   parent_->client_handlers_.find(audio_handle_)->second->clear();
       */
      parent_->client_handlers_.erase(audio_handle_);
    }

    AudioService* parent_;
    AudioHandle audio_handle_;
  };
  friend class DeathNotifier;

  mutex lock_;
  AudioIon ion_;
  AudioFrontend audio_frontend_;
  map<AudioHandle, sp<DeathNotifier>> death_notifiers_;
  map<AudioHandle, sp<IAudioServiceCallback>> client_handlers_;

  /* disable copy, assignment, and move */
  AudioService(const AudioService&) = delete;
  AudioService(AudioService&&) = delete;
  AudioService& operator=(const AudioService&) = delete;
  AudioService& operator=(const AudioService&&) = delete;
};

}; /* namespace audio */
}; /* namespace common */
}; /* namespace qmmf */
