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

#include "common/audio/inc/qmmf_audio_definitions.h"
#include "common/audio/inc/qmmf_audio_endpoint.h"
#include "recorder/src/service/qmmf_recorder_common.h"
#include "recorder/src/service/qmmf_recorder_ion.h"

namespace qmmf {
namespace recorder {

using ::qmmf::common::audio::AudioBuffer;
using ::qmmf::common::audio::AudioEndPoint;
using ::std::condition_variable;
using ::std::mutex;
using ::std::queue;
using ::std::thread;

class AudioSource {
 public:
  static AudioSource* CreateAudioSource();

  ~AudioSource();

  status_t CreateTrackSource(const uint32_t track_id, AudioTrackParams& param);
  status_t DeleteTrackSource(const uint32_t track_id);

  status_t StartTrackSource(const uint32_t track_id);
  status_t StopTrackSource(const uint32_t track_id);
  status_t PauseTrackSource(const uint32_t track_id);
  status_t ResumeTrackSource(const uint32_t track_id);

  status_t ReturnTrackBuffer(const uint32_t track_id,
                             const std::vector<BnBuffer> &buffers);

 private:
  enum class AudioMessageType {
    kMessageStop,
    kMessagePause,
    kMessageResume,
    kMessageBuffer,
    kMessageBnBuffer,
  };

  struct AudioMessage {
    AudioMessageType type;
    AudioBuffer      buffer;
    BnBuffer         bn_buffer;
  };

  static AudioSource* instance_;
  AudioSource();

  static void ThreadEntry(AudioSource* source);
  void Thread();

  void ErrorHandler(const int32_t error);
  void BufferHandler(const AudioBuffer& buffer);

  uint32_t track_id_;
  buffer_callback data_cb_;
  AudioEndPoint* end_point_;
  RecorderIon ion_;

  thread* thread_;
  mutex message_lock_;
  queue<AudioMessage> messages_;
  condition_variable signal_;

  // disable copy, assignment, and move
  AudioSource(const AudioSource&) = delete;
  AudioSource(AudioSource&&) = delete;
  AudioSource& operator=(const AudioSource&) = delete;
  AudioSource& operator=(const AudioSource&&) = delete;
};

}; // namespace recorder
}; // namespace qmmf
