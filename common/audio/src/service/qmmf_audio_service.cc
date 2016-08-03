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

#define TAG "AudioService"

#include "common/audio/src/service/qmmf_audio_service.h"

#include <cerrno>
#include <cstdint>
#include <map>
#include <mutex>

#include <binder/IInterface.h>
#include <binder/Parcel.h>
#include <utils/RefBase.h>

#include "common/audio/inc/qmmf_audio_definitions.h"
#include "common/audio/src/service/qmmf_audio_common.h"
#include "common/audio/src/service/qmmf_audio_frontend.h"
#include "common/audio/src/service/qmmf_audio_ion.h"
#include "common/qmmf_log.h"

namespace qmmf {
namespace common {
namespace audio {

using ::android::IInterface;
using ::android::interface_cast;
using ::android::Parcel;
using ::android::sp;
using ::std::lock_guard;
using ::std::map;
using ::std::mutex;

AudioService::AudioService() {
  QMMF_DEBUG("%s: %s() TRACE", TAG, __func__);

  auto error_handler =
    [this](AudioHandle audio_handle, int error) -> void {
      auto client_handler = client_handlers_.find(audio_handle);
      if (client_handler == client_handlers_.end()) {
        QMMF_ERROR("%s: %s() no client handler for key[%d]", TAG, __func__,
                   audio_handle);
        return;
      }

      client_handler->second->NotifyErrorEvent(error);
    };

  auto read_complete_handler =
    [this](AudioHandle audio_handle, const AudioBuffer& buffer) -> void {
      auto client_handler = client_handlers_.find(audio_handle);
      if (client_handler == client_handlers_.end()) {
        QMMF_ERROR("%s: %s() no client handler for key[%d]", TAG, __func__,
                   audio_handle);
        return;
      }

      client_handler->second->NotifyBufferEvent(buffer);
    };

  auto write_complete_handler =
    [this](AudioHandle audio_handle, const AudioBuffer& buffer) -> void {
      auto client_handler = client_handlers_.find(audio_handle);
      if (client_handler == client_handlers_.end()) {
        QMMF_ERROR("%s: %s() no client handler for key[%d]", TAG, __func__,
                   audio_handle);
        return;
      }

      client_handler->second->NotifyBufferEvent(buffer);
    };

  audio_frontend_.RegisterErrorHandler(error_handler);
  audio_frontend_.RegisterReadCompleteHandler(read_complete_handler);
  audio_frontend_.RegisterWriteCompleteHandler(write_complete_handler);

  QMMF_INFO("%s: %s() service instantiated", TAG, __func__);
}

AudioService::~AudioService()
{
  QMMF_DEBUG("%s: %s() TRACE", TAG, __func__);
  death_notifiers_.clear();
  client_handlers_.clear();
  QMMF_INFO("%s: %s: service destroyed", TAG, __func__);
}

int AudioService::Connect(const sp<IAudioServiceCallback>& client_handler,
                          AudioHandle* audio_handle) {
  QMMF_DEBUG("%s: %s() TRACE", TAG, __func__);
  lock_guard<mutex> lock(lock_);

  int result = audio_frontend_.Connect(audio_handle);
  if (result < 0) {
    QMMF_ERROR("%s: %s() frontend->Connect failed: %d", TAG, __func__, result);
    return result;
  }

  sp<DeathNotifier> death_notifier = new DeathNotifier(this, *audio_handle);
  if (death_notifier.get() == nullptr) {
    QMMF_ERROR("%s: %s() unable to allocate death notifier", TAG, __func__);
    return -ENOMEM;
  }
  IInterface::asBinder(client_handler)->linkToDeath(death_notifier);

  death_notifiers_.insert({*audio_handle, death_notifier});
  client_handlers_.insert({*audio_handle, client_handler});

  QMMF_VERBOSE("%s: %s() OUTPARAM: audio_handle[%d]", TAG, __func__,
               *audio_handle);
  return result;
}

int AudioService::Disconnect(AudioHandle audio_handle) {
  QMMF_DEBUG("%s: %s() TRACE", TAG, __func__);
  QMMF_VERBOSE("%s: %s() INPARAM: audio_handle[%d]", TAG, __func__,
               audio_handle);
  lock_guard<mutex> lock(lock_);

  int result = audio_frontend_.Disconnect(audio_handle);
  if (result < 0) {
    QMMF_ERROR("%s: %s() frontend->Disconnect failed: %d", TAG, __func__,
               result);
    return result;
  }

  auto client_handler = client_handlers_.find(audio_handle);
  if (client_handler == client_handlers_.end()) {
    QMMF_ERROR("%s: %s() no client handler for key[%d]", TAG, __func__,
               audio_handle);
    return -EINVAL;
  }

  auto death_notifier = death_notifiers_.find(audio_handle);
  if (death_notifier == death_notifiers_.end()) {
    QMMF_ERROR("%s: %s() no death notifier for key[%d]", TAG, __func__,
               audio_handle);
    return -EINVAL;
  }

  IInterface::asBinder(
      client_handler->second)->unlinkToDeath(death_notifier->second);
  client_handler->second.clear();
  death_notifier->second.clear();
  client_handlers_.erase(client_handler);
  death_notifiers_.erase(death_notifier);

  ion_.Release(audio_handle);

  return 0;
}

int AudioService::Configure(AudioHandle audio_handle, AudioEndPointType type,
                            const DeviceIdList& devices,
                            const AudioMetadata& metadata) {
  QMMF_DEBUG("%s: %s() TRACE", TAG, __func__);
  QMMF_VERBOSE("%s: %s() INPARAM: audio_handle[%d]", TAG, __func__,
               audio_handle);
  QMMF_VERBOSE("%s: %s() INPARAM: type[%d]", TAG, __func__,
               static_cast<int>(type));
  QMMF_VERBOSE("%s: %s() INPARAM: devices[%s]", TAG, __func__,
               devices.ToString().c_str());
  QMMF_VERBOSE("%s: %s() INPARAM: metadata[%s]", TAG, __func__,
               metadata.ToString().c_str());
  lock_guard<mutex> lock(lock_);

  int result = audio_frontend_.Configure(audio_handle, type, devices, metadata);
  if (result < 0)
    QMMF_ERROR("%s: %s() frontend->Configure failed: %d", TAG, __func__,
               result);

  return result;
}

int AudioService::Start(AudioHandle audio_handle) {
  QMMF_DEBUG("%s: %s() TRACE", TAG, __func__);
  QMMF_VERBOSE("%s: %s() INPARAM: audio_handle[%d]", TAG, __func__,
               audio_handle);
  lock_guard<mutex> lock(lock_);

  int result = audio_frontend_.Start(audio_handle);
  if (result < 0)
    QMMF_ERROR("%s: %s() frontend->Start failed: %d", TAG, __func__, result);

  return result;
}

int AudioService::Stop(AudioHandle audio_handle, bool flush) {
  QMMF_DEBUG("%s: %s() TRACE", TAG, __func__);
  QMMF_VERBOSE("%s: %s() INPARAM: audio_handle[%d]", TAG, __func__,
               audio_handle);
  QMMF_VERBOSE("%s: %s() INPARAM: flush[%s]", TAG, __func__,
               flush ? "true" : "false");
  lock_guard<mutex> lock(lock_);

  int result = audio_frontend_.Stop(audio_handle, flush);
  if (result < 0)
    QMMF_ERROR("%s: %s() frontend->Stop failed: %d", TAG, __func__, result);

  return result;
}

int AudioService::Pause(AudioHandle audio_handle) {
  QMMF_DEBUG("%s: %s() TRACE", TAG, __func__);
  QMMF_VERBOSE("%s: %s() INPARAM: audio_handle[%d]", TAG, __func__,
               audio_handle);
  lock_guard<mutex> lock(lock_);

  int result = audio_frontend_.Pause(audio_handle);
  if (result < 0)
    QMMF_ERROR("%s: %s() frontend->Pause failed: %d", TAG, __func__, result);

  return result;
}

int AudioService::Resume(AudioHandle audio_handle) {
  QMMF_DEBUG("%s: %s() TRACE", TAG, __func__);
  QMMF_VERBOSE("%s: %s() INPARAM: audio_handle[%d]", TAG, __func__,
               audio_handle);
  lock_guard<mutex> lock(lock_);

  int result = audio_frontend_.Resume(audio_handle);
  if (result < 0)
    QMMF_ERROR("%s: %s() frontend->Resume failed: %d", TAG, __func__, result);

  return result;
}

int AudioService::SendBuffers(AudioHandle audio_handle,
                              const AudioBufferList& buffers) {
  QMMF_DEBUG("%s: %s() TRACE", TAG, __func__);
  QMMF_VERBOSE("%s: %s() INPARAM: audio_handle[%d]", TAG, __func__,
               audio_handle);
  for (const AudioBuffer& buffer : buffers.list)
    QMMF_VERBOSE("%s: %s() INPARAM: buffer[%s]", TAG, __func__,
                 buffer.ToString().c_str());
  lock_guard<mutex> lock(lock_);

  int result = audio_frontend_.SendBuffers(audio_handle, buffers);
  if (result < 0)
    QMMF_ERROR("%s: %s() frontend->SendBuffers failed: %d", TAG, __func__,
               result);

  return result;
}

int AudioService::GetLatency(AudioHandle audio_handle, int* latency) {
  QMMF_DEBUG("%s: %s() TRACE", TAG, __func__);
  QMMF_VERBOSE("%s: %s() INPARAM: audio_handle[%d]", TAG, __func__,
               audio_handle);
  lock_guard<mutex> lock(lock_);

  int result = audio_frontend_.GetLatency(audio_handle, latency);
  if (result < 0)
    QMMF_ERROR("%s: %s() frontend->GetLatency failed: %d", TAG, __func__,
               result);

  QMMF_VERBOSE("%s: %s() OUTPARAM: latency[%d]", TAG, __func__, *latency);
  return result;
}

int AudioService::GetBufferSize(AudioHandle audio_handle, int* buffer_size) {
  QMMF_DEBUG("%s: %s() TRACE", TAG, __func__);
  QMMF_VERBOSE("%s: %s() INPARAM: audio_handle[%d]", TAG, __func__,
               audio_handle);
  lock_guard<mutex> lock(lock_);

  int result = audio_frontend_.GetBufferSize(audio_handle, buffer_size);
  if (result < 0)
    QMMF_ERROR("%s: %s() frontend->GetBufferSize failed: %d", TAG, __func__,
               result);

  QMMF_VERBOSE("%s: %s() OUTPARAM: buffer_size[%d]", TAG, __func__,
               *buffer_size);
  return result;
}

int AudioService::SetParam(AudioHandle audio_handle, AudioParamType type,
                           const AudioParamData& data) {
  QMMF_DEBUG("%s: %s() TRACE", TAG, __func__);
  QMMF_VERBOSE("%s: %s() INPARAM: audio_handle[%d]", TAG, __func__,
               audio_handle);
  QMMF_VERBOSE("%s: %s() INPARAM: type[%d]", TAG, __func__,
               static_cast<int>(type));
  QMMF_VERBOSE("%s: %s() INPARAM: data[%s]", TAG, __func__,
               data.ToString(type).c_str());
  lock_guard<mutex> lock(lock_);

  int result = audio_frontend_.SetParam(audio_handle, type, data);
  if (result < 0)
    QMMF_ERROR("%s: %s() frontend->SetParam failed: %d", TAG, __func__, result);

  return result;
}

int AudioService::onTransact(uint32_t code, const Parcel& input, Parcel* output,
                             uint32_t flags) {
  QMMF_DEBUG("%s: %s() TRACE", TAG, __func__);
  QMMF_VERBOSE("%s: %s() INPARAM: code[%u]", TAG, __func__, code);
  QMMF_VERBOSE("%s: %s() INPARAM: flags[%u]", TAG, __func__, flags);

  if (!input.checkInterface(this))
    return -EPERM;

  switch (static_cast<AudioServiceCommand>(code)) {
    case AudioServiceCommand::kAudioConnect: {
      sp<IAudioServiceCallback> client_handler =
          interface_cast<IAudioServiceCallback>(input.readStrongBinder());

      QMMF_DEBUG("%s: %s-AudioConnect() TRACE", TAG, __func__);
      AudioHandle audio_handle;
      int result = Connect(client_handler, &audio_handle);
      QMMF_VERBOSE("%s: %s-AudioConnect() OUTPARAM: audio_handle[%d]", TAG,
                   __func__, audio_handle);

      output->writeInt32(static_cast<int32_t>(audio_handle));
      output->writeInt32(static_cast<int32_t>(result));
      break;
    }

    case AudioServiceCommand::kAudioDisconnect: {
      AudioHandle audio_handle = static_cast<AudioHandle>(input.readInt32());

      QMMF_DEBUG("%s: %s-AudioDisconnect() TRACE", TAG, __func__);
      QMMF_VERBOSE("%s: %s-AudioDisconnect() INPARAM: audio_handle[%d]", TAG,
                   __func__, audio_handle);
      int result = Disconnect(audio_handle);

      output->writeInt32(static_cast<int32_t>(result));
      break;
    }

    case AudioServiceCommand::kAudioConfigure: {
      AudioHandle audio_handle = static_cast<AudioHandle>(input.readInt32());
      AudioEndPointType type =
          static_cast<AudioEndPointType>(input.readInt32());

      DeviceIdList devices;
      devices.FromParcel(input);

      AudioMetadata metadata;
      metadata.FromParcel(input);

      QMMF_DEBUG("%s: %s-AudioConfigure() TRACE", TAG, __func__);
      QMMF_VERBOSE("%s: %s-AudioConfigure() INPARAM: audio_handle[%d]", TAG,
                   __func__, audio_handle);
      QMMF_VERBOSE("%s: %s-AudioConfigure() INPARAM: type[%d]", TAG, __func__,
                   static_cast<int>(type));
      QMMF_VERBOSE("%s: %s-AudioConfigure() INPARAM: devices[%s]", TAG,
                   __func__, devices.ToString().c_str());
      QMMF_VERBOSE("%s: %s-AudioConfigure() INPARAM: metadata[%s]", TAG,
                   __func__, metadata.ToString().c_str());
      int result = Configure(audio_handle, type, devices, metadata);

      output->writeInt32(static_cast<int32_t>(result));
      break;
    }

    case AudioServiceCommand::kAudioStart: {
      AudioHandle audio_handle = static_cast<AudioHandle>(input.readInt32());

      QMMF_DEBUG("%s: %s-AudioStart() TRACE", TAG, __func__);
      QMMF_VERBOSE("%s: %s-AudioStart() INPARAM: audio_handle[%d]", TAG,
                   __func__, audio_handle);
      int result = Start(audio_handle);

      output->writeInt32(static_cast<int32_t>(result));
      break;
    }

    case AudioServiceCommand::kAudioStop: {
      AudioHandle audio_handle = static_cast<AudioHandle>(input.readInt32());
      bool flush = static_cast<bool>(input.readInt32());

      QMMF_DEBUG("%s: %s-AudioStop() TRACE", TAG, __func__);
      QMMF_VERBOSE("%s: %s-AudioStop() INPARAM: audio_handle[%d]", TAG,
                   __func__, audio_handle);
      QMMF_VERBOSE("%s: %s-AudioStop() INPARAM: flush[%s]", TAG, __func__,
                   flush ? "true" : "false");
      int result = Stop(audio_handle, flush);

      output->writeInt32(static_cast<int32_t>(result));
      break;
    }

    case AudioServiceCommand::kAudioPause: {
      AudioHandle audio_handle = static_cast<AudioHandle>(input.readInt32());

      QMMF_DEBUG("%s: %s-AudioPause() TRACE", TAG, __func__);
      QMMF_VERBOSE("%s: %s-AudioPause() INPARAM: audio_handle[%d]", TAG,
                   __func__, audio_handle);
      int result = Pause(audio_handle);

      output->writeInt32(static_cast<int32_t>(result));
      break;
    }

    case AudioServiceCommand::kAudioResume: {
      AudioHandle audio_handle = static_cast<AudioHandle>(input.readInt32());

      QMMF_DEBUG("%s: %s-AudioResume() TRACE", TAG, __func__);
      QMMF_VERBOSE("%s: %s-AudioResume() INPARAM: audio_handle[%d]", TAG,
                   __func__, audio_handle);
      int result = Resume(audio_handle);

      output->writeInt32(static_cast<int32_t>(result));
      break;
    }

    case AudioServiceCommand::kAudioSendBuffers: {
      AudioHandle audio_handle = static_cast<AudioHandle>(input.readInt32());

      AudioBufferList buffers;
      buffers.FromParcel(input, true);

      for (AudioBuffer& buffer : buffers.list) {
        if (buffer.ion_fd != -1)
          ion_.Associate(audio_handle, &buffer);
      }

      QMMF_DEBUG("%s: %s-AudioSendBuffers() TRACE", TAG, __func__);
      QMMF_VERBOSE("%s: %s-AudioSendBuffers() INPARAM: audio_handle[%d]", TAG,
                   __func__, audio_handle);
      for (const AudioBuffer& buffer : buffers.list)
        QMMF_VERBOSE("%s: %s-AudioSendBuffers() INPARAM: buffer[%s]", TAG,
                     __func__, buffer.ToString().c_str());
      int result = SendBuffers(audio_handle, buffers);

      output->writeInt32(static_cast<int32_t>(result));
      break;
    }

    case AudioServiceCommand::kAudioGetLatency: {
      AudioHandle audio_handle = static_cast<AudioHandle>(input.readInt32());

      QMMF_DEBUG("%s: %s-AudioGetLatency() TRACE", TAG, __func__);
      QMMF_VERBOSE("%s: %s-AudioGetLatency() INPARAM: audio_handle[%d]", TAG,
                   __func__, audio_handle);
      int latency;
      int result = GetLatency(audio_handle, &latency);
      QMMF_VERBOSE("%s: %s-AudioGetLatency() OUTPARAM: latency[%d]", TAG,
                   __func__, latency);

      output->writeInt32(static_cast<int32_t>(latency));
      output->writeInt32(static_cast<int32_t>(result));
      break;
    }

    case AudioServiceCommand::kAudioGetBufferSize: {
      AudioHandle audio_handle = static_cast<AudioHandle>(input.readInt32());

      QMMF_DEBUG("%s: %s-AudioGetBufferSize() TRACE", TAG, __func__);
      QMMF_VERBOSE("%s: %s-AudioGetBufferSize() INPARAM: audio_handle[%d]", TAG,
                   __func__, audio_handle);
      int buffer_size;
      int result = GetBufferSize(audio_handle, &buffer_size);
      QMMF_VERBOSE("%s: %s-AudioGetBufferSize() OUTPARAM: buffer_size[%d]", TAG,
                   __func__, buffer_size);

      output->writeInt32(static_cast<int32_t>(buffer_size));
      output->writeInt32(static_cast<int32_t>(result));
      break;
    }

    case AudioServiceCommand::kAudioSetParam: {
      AudioHandle audio_handle = static_cast<AudioHandle>(input.readInt32());

      AudioParamType type = static_cast<AudioParamType>(input.readInt32());

      AudioParamData data;
      data.FromParcel(input);

      QMMF_DEBUG("%s: %s-AudioSetParam() TRACE", TAG, __func__);
      QMMF_VERBOSE("%s: %s-AudioSetParam() INPARAM: audio_handle[%d]", TAG,
                   __func__, audio_handle);
      QMMF_VERBOSE("%s: %s-AudioSetParam() INPARAM: type[%d]", TAG, __func__,
                   static_cast<int>(type));
      QMMF_VERBOSE("%s: %s-AudioSetParam() INPARAM: data[%s]", TAG, __func__,
                   data.ToString(type).c_str());
      int result = SetParam(audio_handle, type, data);

      output->writeInt32(static_cast<int32_t>(result));
      break;
    }

    default:
      QMMF_ERROR("%s: %s() code %u not supported ", TAG, __func__, code);
      output->writeInt32(static_cast<int32_t>(-EINVAL));
      break;
  }
  return 0;
}

}; /* namespace audio */
}; /* namespace common */
}; /* namespace qmmf */
