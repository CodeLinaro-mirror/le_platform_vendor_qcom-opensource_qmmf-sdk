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

#include <map>

#include "common/audio/inc/qmmf_audio_definitions.h"
#include "common/audio/src/service/qmmf_audio_backend.h"
#include "common/audio/src/service/qmmf_audio_common.h"

namespace qmmf {
namespace common {
namespace audio {

using ::std::map;

class AudioFrontend {
 public:
  AudioFrontend();
  ~AudioFrontend();

  void RegisterErrorHandler(AudioErrorHandler handler);
  void RegisterReadCompleteHandler(AudioReadCompleteHandler handler);
  void RegisterWriteCompleteHandler(AudioWriteCompleteHandler handler);

  int Connect(AudioHandle* audio_handle);
  int Disconnect(AudioHandle audio_handle);
  int Configure(AudioHandle audio_handle, AudioEndPointType type,
                const DeviceIdList& devices, const AudioMetadata& metadata);

  int Start(AudioHandle audio_handle);
  int Stop(AudioHandle audio_handle, bool flush);
  int Pause(AudioHandle audio_handle);
  int Resume(AudioHandle audio_handle);

  int SendBuffers(AudioHandle audio_handle, const AudioBufferList& buffers);

  int GetLatency(AudioHandle audio_handle, int* latency);
  int GetBufferSize(AudioHandle audio_handle, int* buffer_size);
  int SetParam(AudioHandle audio_handle, AudioParamType type,
               const AudioParamData& data);

 private:
  static const AudioHandle kAudioHandleMax;

  AudioHandle current_handle_;
  AudioErrorHandler error_handler_;
  AudioReadCompleteHandler read_complete_handler_;
  AudioWriteCompleteHandler write_complete_handler_;
  map<AudioHandle, IAudioBackend*> backends_;

  /* disable copy, assignment, and move */
  AudioFrontend(const AudioFrontend&) = delete;
  AudioFrontend(AudioFrontend&&) = delete;
  AudioFrontend& operator=(const AudioFrontend&) = delete;
  AudioFrontend& operator=(const AudioFrontend&&) = delete;
};

}; /* namespace audio */
}; /* namespace common */
}; /* namespace qmmf */
