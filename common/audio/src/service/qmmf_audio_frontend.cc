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

#define TAG "AudioFrontend"

#include "common/audio/src/service/qmmf_audio_frontend.h"

#include <functional>
#include <map>

#include "common/audio/inc/qmmf_audio_definitions.h"
#include "common/audio/src/service/qmmf_audio_backend.h"
#include "common/audio/src/service/qmmf_audio_backend_primary.h"
#include "common/audio/src/service/qmmf_audio_common.h"
#include "common/qmmf_log.h"

namespace qmmf {
namespace common {
namespace audio {

using ::std::function;
using ::std::map;

const AudioHandle AudioFrontend::kAudioHandleMax = 100;

AudioFrontend::AudioFrontend() : current_handle_(0) {}

AudioFrontend::~AudioFrontend() {}

void AudioFrontend::RegisterErrorHandler(AudioErrorHandler handler) {
  QMMF_DEBUG("%s: %s() TRACE", TAG, __func__);
  QMMF_VERBOSE("%s: %s() INPARAM: handler[%s]", TAG, __func__,
               handler.target_type().name());

  error_handler_ = handler;
}

void AudioFrontend::RegisterReadCompleteHandler(
    AudioReadCompleteHandler handler) {
  QMMF_DEBUG("%s: %s() TRACE", TAG, __func__);
  QMMF_VERBOSE("%s: %s() INPARAM: handler[%s]", TAG, __func__,
               handler.target_type().name());

  read_complete_handler_ = handler;
}

void AudioFrontend::RegisterWriteCompleteHandler(
    AudioWriteCompleteHandler handler) {
  QMMF_DEBUG("%s: %s() TRACE", TAG, __func__);
  QMMF_VERBOSE("%s: %s() INPARAM: handler[%s]", TAG, __func__,
               handler.target_type().name());

  write_complete_handler_ = handler;
}

int AudioFrontend::Connect(AudioHandle* audio_handle) {
  QMMF_DEBUG("%s: %s() TRACE", TAG, __func__);

  /* find an available AudioHandle */
  if (current_handle_ + 1 > kAudioHandleMax)
    current_handle_ = 0;
  ++current_handle_;
  while (backends_.find(current_handle_) != backends_.end())
    ++current_handle_;

  backends_.insert({current_handle_, nullptr});

  *audio_handle = current_handle_;
  QMMF_VERBOSE("%s: %s() OUTPARAM: audio_handle[%d]", TAG, __func__,
               *audio_handle);

  return 0;
}

int AudioFrontend::Disconnect(AudioHandle audio_handle) {
  QMMF_DEBUG("%s: %s() TRACE", TAG, __func__);
  QMMF_VERBOSE("%s: %s() INPARAM: audio_handle[%d]", TAG, __func__,
               audio_handle);

  auto backend = backends_.find(audio_handle);
  if (backend == backends_.end()) {
    QMMF_ERROR("%s: %s() no backend for key[%d]", TAG, __func__, audio_handle);
    return -EINVAL;
  }

  int result;
  if (backend->second != nullptr) {
    result = backend->second->Close();
    if (result < 0)
      QMMF_ERROR("%s: %s() backend->Close failed: %d", TAG, __func__, result);

    delete backend->second;
    backend->second = nullptr;
  }

  backends_.erase(backend);

  return result;
}

int AudioFrontend::Configure(AudioHandle audio_handle, AudioEndPointType type,
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

  auto backend = backends_.find(audio_handle);
  if (backend == backends_.end()) {
    QMMF_ERROR("%s: %s() no backend for key[%d]", TAG, __func__, audio_handle);
    return -EINVAL;
  }

  IAudioBackend* primary = new AudioBackendPrimary(audio_handle, error_handler_,
                                                   read_complete_handler_,
                                                   write_complete_handler_);
  if (primary == nullptr) {
    QMMF_ERROR("%s: %s() unable to allocate primary backend", TAG, __func__);
    return -ENOMEM;
  }
  backend->second = primary;

  int result = backend->second->Open(type, devices, metadata);
  if (result < 0)
    QMMF_ERROR("%s: %s() backend->Open failed: %d", TAG, __func__, result);

  return result;
}

int AudioFrontend::Start(AudioHandle audio_handle) {
  QMMF_DEBUG("%s: %s() TRACE", TAG, __func__);
  QMMF_VERBOSE("%s: %s() INPARAM: audio_handle[%d]", TAG, __func__,
               audio_handle);

  auto backend = backends_.find(audio_handle);
  if (backend == backends_.end()) {
    QMMF_ERROR("%s: %s() no backend for key[%d]", TAG, __func__, audio_handle);
    return -EINVAL;
  }

  if (backend->second == nullptr) {
    QMMF_ERROR("%s: %s() backend[%d] has a null object pointer", TAG, __func__,
               audio_handle);
    return -ENOSYS;
  }

  int result = backend->second->Start();
  if (result < 0)
    QMMF_ERROR("%s: %s() backend->Start failed: %d", TAG, __func__, result);

  return result;
}

int AudioFrontend::Stop(AudioHandle audio_handle, bool flush) {
  QMMF_DEBUG("%s: %s() TRACE", TAG, __func__);
  QMMF_VERBOSE("%s: %s() INPARAM: audio_handle[%d]", TAG, __func__,
               audio_handle);
  QMMF_VERBOSE("%s: %s() INPARAM: flush[%s]", TAG, __func__,
               flush ? "true" : "false");

  auto backend = backends_.find(audio_handle);
  if (backend == backends_.end()) {
    QMMF_ERROR("%s: %s() no backend for key[%d]", TAG, __func__, audio_handle);
    return -EINVAL;
  }

  if (backend->second == nullptr) {
    QMMF_ERROR("%s: %s() backend[%d] has a null object pointer", TAG, __func__,
               audio_handle);
    return -ENOSYS;
  }

  int result = backend->second->Stop(flush);
  if (result < 0)
    QMMF_ERROR("%s: %s() backend->Stop failed: %d", TAG, __func__, result);

  return result;
}

int AudioFrontend::Pause(AudioHandle audio_handle) {
  QMMF_DEBUG("%s: %s() TRACE", TAG, __func__);
  QMMF_VERBOSE("%s: %s() INPARAM: audio_handle[%d]", TAG, __func__,
               audio_handle);

  auto backend = backends_.find(audio_handle);
  if (backend == backends_.end()) {
    QMMF_ERROR("%s: %s() no backend for key[%d]", TAG, __func__, audio_handle);
    return -EINVAL;
  }

  if (backend->second == nullptr) {
    QMMF_ERROR("%s: %s() backend[%d] has a null object pointer", TAG, __func__,
               audio_handle);
    return -ENOSYS;
  }

  int result = backend->second->Pause();
  if (result < 0)
    QMMF_ERROR("%s: %s() backend->Pause failed: %d", TAG, __func__, result);

  return result;
}

int AudioFrontend::Resume(AudioHandle audio_handle) {
  QMMF_DEBUG("%s: %s() TRACE", TAG, __func__);
  QMMF_VERBOSE("%s: %s() INPARAM: audio_handle[%d]", TAG, __func__,
               audio_handle);

  auto backend = backends_.find(audio_handle);
  if (backend == backends_.end()) {
    QMMF_ERROR("%s: %s() no backend for key[%d]", TAG, __func__, audio_handle);
    return -EINVAL;
  }

  if (backend->second == nullptr) {
    QMMF_ERROR("%s: %s() backend[%d] has a null object pointer", TAG, __func__,
               audio_handle);
    return -ENOSYS;
  }

  int result = backend->second->Resume();
  if (result < 0)
    QMMF_ERROR("%s: %s() backend->Resume failed: %d", TAG, __func__, result);

  return result;
}

int AudioFrontend::SendBuffers(AudioHandle audio_handle,
                               const AudioBufferList& buffers) {
  QMMF_DEBUG("%s: %s() TRACE", TAG, __func__);
  QMMF_VERBOSE("%s: %s() INPARAM: audio_handle[%d]", TAG, __func__,
               audio_handle);
  for (const AudioBuffer& buffer : buffers.list)
    QMMF_VERBOSE("%s: %s() INPARAM: buffer[%s]", TAG, __func__,
                 buffer.ToString().c_str());

  auto backend = backends_.find(audio_handle);
  if (backend == backends_.end()) {
    QMMF_ERROR("%s: %s() no backend for key[%d]", TAG, __func__, audio_handle);
    return -EINVAL;
  }

  if (backend->second == nullptr) {
    QMMF_ERROR("%s: %s() backend[%d] has a null object pointer", TAG, __func__,
               audio_handle);
    return -ENOSYS;
  }

  int result = backend->second->SendBuffers(buffers);
  if (result < 0)
    QMMF_ERROR("%s: %s() backend->SendBuffers failed: %d", TAG, __func__,
               result);

  return result;
}

int AudioFrontend::GetLatency(AudioHandle audio_handle, int* latency) {
  QMMF_DEBUG("%s: %s() TRACE", TAG, __func__);
  QMMF_VERBOSE("%s: %s() INPARAM: audio_handle[%d]", TAG, __func__,
               audio_handle);

  auto backend = backends_.find(audio_handle);
  if (backend == backends_.end()) {
    QMMF_ERROR("%s: %s() no backend for key[%d]", TAG, __func__, audio_handle);
    return -EINVAL;
  }

  if (backend->second == nullptr) {
    QMMF_ERROR("%s: %s() backend[%d] has a null object pointer", TAG, __func__,
               audio_handle);
    return -ENOSYS;
  }

  int result = backend->second->GetLatency(latency);
  if (result < 0)
    QMMF_ERROR("%s: %s() backend->GetLatency failed: %d", TAG, __func__,
               result);

  QMMF_VERBOSE("%s: %s() OUTPARAM: latency[%d]", TAG, __func__, *latency);
  return result;
}

int AudioFrontend::GetBufferSize(AudioHandle audio_handle, int* buffer_size) {
  QMMF_DEBUG("%s: %s() TRACE", TAG, __func__);
  QMMF_VERBOSE("%s: %s() INPARAM: audio_handle[%d]", TAG, __func__,
               audio_handle);

  auto backend = backends_.find(audio_handle);
  if (backend == backends_.end()) {
    QMMF_ERROR("%s: %s() no backend for key[%d]", TAG, __func__, audio_handle);
    return -EINVAL;
  }

  if (backend->second == nullptr) {
    QMMF_ERROR("%s: %s() backend[%d] has a null object pointer", TAG, __func__,
               audio_handle);
    return -ENOSYS;
  }

  int result = backend->second->GetBufferSize(buffer_size);
  if (result < 0)
    QMMF_ERROR("%s: %s() backend->GetBufferSize failed: %d", TAG, __func__,
               result);

  QMMF_VERBOSE("%s: %s() OUTPARAM: buffer_size[%d]", TAG, __func__,
               *buffer_size);
  return result;
}

int AudioFrontend::SetParam(AudioHandle audio_handle, AudioParamType type,
                            const AudioParamData& data) {
  QMMF_DEBUG("%s: %s() TRACE", TAG, __func__);
  QMMF_VERBOSE("%s: %s() INPARAM: audio_handle[%d]", TAG, __func__,
               audio_handle);
  QMMF_VERBOSE("%s: %s() INPARAM: type[%d]", TAG, __func__,
               static_cast<int>(type));
  QMMF_VERBOSE("%s: %s() INPARAM: data[%s]", TAG, __func__,
               data.ToString(type).c_str());

  auto backend = backends_.find(audio_handle);
  if (backend == backends_.end()) {
    QMMF_ERROR("%s: %s() no backend for key[%d]", TAG, __func__, audio_handle);
    return -EINVAL;
  }

  if (backend->second == nullptr) {
    QMMF_ERROR("%s: %s() backend[%d] has a null object pointer", TAG, __func__,
               audio_handle);
    return -ENOSYS;
  }

  int result = backend->second->SetParam(type, data);
  if (result < 0)
    QMMF_ERROR("%s: %s() backend->SetParam failed: %d", TAG, __func__, result);

  return result;
}

}; /* namespace audio */
}; /* namespace common */
}; /* namespace qmmf */
