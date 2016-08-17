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
#include <vector>

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
using ::std::vector;

class AudioService : public BnInterface<IAudioService>
{
 public:
  AudioService();
  ~AudioService();

 private:
  int32_t Connect(const sp<IAudioServiceCallback>& client_handler,
                  AudioHandle* audio_handle) override;
  int32_t Disconnect(const AudioHandle audio_handle) override;
  int32_t Configure(const AudioHandle audio_handle,
                    const AudioEndPointType type,
                    const vector<DeviceId>& devices,
                    const AudioMetadata& metadata) override;

  int32_t Start(const AudioHandle audio_handle) override;
  int32_t Stop(const AudioHandle audio_handle, const bool flush) override;
  int32_t Pause(const AudioHandle audio_handle) override;
  int32_t Resume(const AudioHandle audio_handle) override;

  int32_t SendBuffers(const AudioHandle audio_handle,
                      const vector<AudioBuffer>& buffers) override;

  int32_t GetLatency(const AudioHandle audio_handle, int32_t* latency) override;
  int32_t GetBufferSize(const AudioHandle audio_handle,
                        int32_t* buffer_size) override;
  int32_t SetParam(const AudioHandle audio_handle, const AudioParamType type,
                   const AudioParamData& data) override;

  // method of BnInterface<IAudioService>
  int32_t onTransact(uint32_t code, const Parcel& data, Parcel* reply,
                     uint32_t flags = 0) override;

  class DeathNotifier : public IBinder::DeathRecipient {
   public:
    DeathNotifier(AudioService* parent, const AudioHandle audio_handle)
        : parent_(parent), audio_handle_(audio_handle) {}

    void binderDied(const wp<IBinder>&) override {
      QMMF_WARN("%s() audio client died", __func__);
      lock_guard<mutex> lock(parent_->lock_);
      // TODO(kwestfie@codeaurora.org):
      // Investigate issue with the following statement:
      //   parent_->client_handlers_.find(audio_handle_)->second->clear();
      //
      parent_->client_handlers_.erase(audio_handle_);
    }

    AudioService* parent_;
    AudioHandle audio_handle_;
  };
  friend class DeathNotifier;

  typedef map<AudioHandle, sp<DeathNotifier>> DeathNotifierMap;
  typedef map<AudioHandle, sp<IAudioServiceCallback>> ClientHandlerMap;

  mutex lock_;
  AudioIon ion_;
  AudioFrontend audio_frontend_;
  DeathNotifierMap death_notifiers_;
  ClientHandlerMap client_handlers_;

  // disable copy, assignment, and move
  AudioService(const AudioService&) = delete;
  AudioService(AudioService&&) = delete;
  AudioService& operator=(const AudioService&) = delete;
  AudioService& operator=(const AudioService&&) = delete;
};

}; // namespace audio
}; // namespace common
}; // namespace qmmf
