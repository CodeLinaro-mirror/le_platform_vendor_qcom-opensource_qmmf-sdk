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

#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>

#include <hardware/audio.h>

#include "common/audio/inc/qmmf_audio_definitions.h"
#include "common/audio/src/service/qmmf_audio_backend.h"
#include "common/audio/src/service/qmmf_audio_common.h"

namespace qmmf {
namespace common {
namespace audio {

using ::std::condition_variable;
using ::std::mutex;
using ::std::queue;
using ::std::thread;

class AudioBackendPrimary : public IAudioBackend {
 public:
  AudioBackendPrimary(AudioHandle audio_handle, AudioErrorHandler error_handler,
                      AudioReadCompleteHandler read_complete_handler,
                      AudioWriteCompleteHandler write_complete_handler);
  ~AudioBackendPrimary();

  int Open(AudioEndPointType type, const DeviceIdList& devices,
           const AudioMetadata& metadata) override;
  int Close() override;

  int Start() override;
  int Stop(bool flush) override;
  int Pause() override;
  int Resume() override;

  int SendBuffers(const AudioBufferList& buffers) override;

  int GetLatency(int* latency) override;
  int GetBufferSize(int* buffer_size) override;
  int SetParam(AudioParamType type, const AudioParamData& data) override;

 private:
  enum class AudioMessageType {
    kMessageStop,
    kMessagePause,
    kMessageResume,
    kMessageBuffer,
  };

  struct AudioMessage {
    AudioMessageType type;
    AudioBufferList buffers;
    bool flush;
  };

  static void StaticThreadEntry(AudioBackendPrimary* backend);
  void ThreadEntry();
  void SourceThread();
  void SinkThread();

  AudioHandle audio_handle_;
  AudioEndPointType type_;
  AudioState state_;

  AudioErrorHandler error_handler_;
  AudioReadCompleteHandler read_complete_handler_;
  AudioWriteCompleteHandler write_complete_handler_;

  thread* thread_;
  mutex message_lock_;
  queue<AudioMessage> messages_;
  condition_variable signal_;

  const hw_module_t* hal_module_;
  audio_hw_device_t* hal_device_;
  audio_stream_in_t* hal_input_stream_;
  audio_stream_out_t* hal_output_stream_;

  /* disable default, copy, assignment, and move */
  AudioBackendPrimary() = delete;
  AudioBackendPrimary(const AudioBackendPrimary&) = delete;
  AudioBackendPrimary(AudioBackendPrimary&&) = delete;
  AudioBackendPrimary& operator=(const AudioBackendPrimary&) = delete;
  AudioBackendPrimary& operator=(const AudioBackendPrimary&&) = delete;
};

}; /* namespace audio */
}; /* namespace common */
}; /* namespace qmmf */
