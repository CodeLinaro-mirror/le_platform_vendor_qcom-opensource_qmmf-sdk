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
#include <string>
#include <thread>

#include "common/audio/inc/qmmf_audio_definitions.h"
#include "common/audio/inc/qmmf_audio_endpoint.h"
#include "common/audio/test/samples/qmmf_audio_test_ion.h"
#include "common/audio/test/samples/qmmf_audio_test_wav.h"

namespace qmmf_test {
namespace common {
namespace audio {

using ::qmmf::common::audio::AudioBuffer;
using ::qmmf::common::audio::AudioEndPoint;
using ::qmmf::common::audio::AudioEndPointType;
using ::std::condition_variable;
using ::std::mutex;
using ::std::queue;
using ::std::string;
using ::std::thread;

class AudioTest
{
 public:
  AudioTest();
  ~AudioTest();

  void Connect();
  void Disconnect();

  void ConfigureSource();
  void ConfigureSink();

  void Start();
  void Stop();
  void Pause();
  void Resume();

  void ErrorHandler(int error);
  void BufferHandler(const AudioBuffer& buffer);

 private:
  enum class AudioMessageType {
    kMessageStop,
    kMessagePause,
    kMessageResume,
    kMessageBuffer,
  };

  struct AudioMessage {
    AudioMessageType type;
    AudioBuffer buffer;
  };

  static void StaticThreadEntry(AudioTest* test);
  void ThreadEntry();
  void SourceThread();
  void SinkThread();

  AudioEndPoint end_point_;
  AudioEndPointType type_;

  AudioTestIon ion_;
  int buffer_size_;
  int buffer_number_;

  AudioTestWav wav_;
  string filename_prefix_;

  thread* thread_;
  mutex message_lock_;
  queue<AudioMessage> messages_;
  condition_variable signal_;

  /* disable copy, assignment, and move */
  AudioTest(const AudioTest&) = delete;
  AudioTest(AudioTest&&) = delete;
  AudioTest& operator=(const AudioTest&) = delete;
  AudioTest& operator=(const AudioTest&&) = delete;
};

class CommandMenu {
 public:
  enum class Command {
    kConnect         = '1',
    kDisconnect      = '2',
    kConfigureSource = '3',
    kConfigureSink   = '4',
    kStart           = '5',
    kStop            = '6',
    kPause           = '7',
    kResume          = '8',
    kExit            = 'X',
    kInvalid         = '0'
  };

  CommandMenu() {};
  ~CommandMenu() {};
  Command GetCommand();
  void PrintMenu();

  /* disable copy, assignment, and move */
  CommandMenu(const CommandMenu&) = delete;
  CommandMenu(CommandMenu&&) = delete;
  CommandMenu& operator=(const CommandMenu&) = delete;
  CommandMenu& operator=(const CommandMenu&&) = delete;
};

}; /* namespace audio */
}; /* namespace common */
}; /* namespace qmmf_test */
