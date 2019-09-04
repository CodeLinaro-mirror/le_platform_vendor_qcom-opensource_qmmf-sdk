/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
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
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#include <pulse/pulseaudio.h>

#include "common/utils/qmmf_condition.h"
#include "recorder/src/service/qmmf_recorder_common.h"

namespace qmmf {
namespace recorder {

enum class AudioEventType {
  kError,
  kBuffer,
};

union AudioEventData {
  int32_t error; // kError
  BufferDescriptor buffer; // kBuffer

  ::std::string ToString(const AudioEventType key) const {
    ::std::stringstream stream;
    switch (key) {
      case AudioEventType::kError:
        stream << "error[" << error << "]";
        break;
      case AudioEventType::kBuffer:
        stream << "buffer[" << buffer.ToString() << "]";
        break;
      default:
        stream << "Invalid Key["
               << static_cast<::std::underlying_type<AudioEventType>::type>(key)
               << "]";
        break;
    }
    return stream.str();
  }

  // needed for unions with non-trivial members
  AudioEventData() : error(0) {}
  AudioEventData(int _error) : error(_error) {}
  AudioEventData(const BufferDescriptor& _buffer) : buffer(_buffer) {}
  ~AudioEventData() {}
};

typedef ::std::function<void(const AudioEventType type,
                             const AudioEventData& data)> AudioEventHandler;

class AudioPulseClient {
 public:
  AudioPulseClient();
  ~AudioPulseClient();

  status_t Connect(const AudioEventHandler& handler);
  status_t Disconnect();
  status_t Configure(const AudioTrackParams& params);

  status_t Start();
  status_t Stop();
  status_t Pause();
  status_t Resume();

  status_t SendBuffers(const ::std::vector<BufferDescriptor>& buffers);

  status_t GetBufferSize(int32_t* buffer_size);
  status_t SetParam(const ::std::string& key, const ::std::string& value);

  // callbacks from service
  void NotifyErrorEvent(const int32_t error);
  void NotifyBufferEvent(const BufferDescriptor& buffer);

 private:
  struct PaStreamSuccessCbData {
    bool success;
    pa_threaded_mainloop* mainloop;
  };

  struct PaStreamLatencyCbData {
    pa_timing_info timing_info;
    bool timing_info_valid;
    ::std::mutex timing_info_mutex;
  };

  enum class AudioMessageType {
    kMessageStop,
    kMessagePause,
    kMessageResume,
    kMessageBuffer,
  };

  struct AudioMessage {
    AudioMessageType type;
    ::std::vector<BufferDescriptor> buffers;
  };

  static const std::string kTSAdjustName;
  static const std::string kPaClientName;
  static const std::string kPaClientVersion;
  static const std::string kPaStreamName;
  static const std::string kPaMediaRole;
  static const size_t kPaBinaryNameSize;
  static const size_t kPaUserNameSize;
  static const size_t kPaHostNameSize;
  static const size_t kPaBufferLength;
  static const size_t kLatency;

  static void ThreadEntry(AudioPulseClient* pulse_client);
  void Thread();
  int ValidatePaStream();


  AudioEventHandler event_handler_;
  int32_t buffer_size_;

  pa_threaded_mainloop* pa_mainloop_;
  pa_mainloop_api* pa_mainloop_api_;
  pa_proplist* pa_context_props_;
  pa_context* pa_context_;
  pa_format_info* pa_format_info_;
  pa_sample_spec pa_sample_spec_;
  pa_proplist* pa_stream_props_;
  pa_stream* pa_stream_;
  PaStreamLatencyCbData pa_latency_data_;

  ::std::thread* thread_;
  ::std::mutex message_lock_;
  ::std::queue<AudioMessage> messages_;
  QCondition signal_;

  // Disable copy, assignment, and move
  AudioPulseClient(const AudioPulseClient&) = delete;
  AudioPulseClient(AudioPulseClient&&) = delete;
  AudioPulseClient& operator=(const AudioPulseClient&) = delete;
  AudioPulseClient& operator=(const AudioPulseClient&&) = delete;
};

}; // namespace recorder
}; // namespace qmmf
