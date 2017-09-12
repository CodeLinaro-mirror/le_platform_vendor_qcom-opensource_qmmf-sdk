/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
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

#define TAG "AudioBackendSink"

#include "common/audio/src/service/qmmf_audio_backend_sink.h"

#include <chrono>
#include <condition_variable>
#include <cstring>
#include <functional>
#include <map>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#include <mm-audio/qahw_api/inc/qahw_api.h>
#include <mm-audio/qahw_api/inc/qahw_defs.h>

#include "common/audio/inc/qmmf_audio_definitions.h"
#include "common/audio/src/service/qmmf_audio_common.h"
#include "common/qmmf_log.h"

namespace qmmf {
namespace common {
namespace audio {

using ::std::chrono::milliseconds;
using ::std::chrono::seconds;
using ::std::condition_variable;
using ::std::cv_status;
using ::std::function;
using ::std::map;
using ::std::mutex;
using ::std::queue;
using ::std::string;
using ::std::thread;
using ::std::unique_lock;
using ::std::vector;

const audio_io_handle_t AudioBackendSink::kIOHandleMin = 800;
const audio_io_handle_t AudioBackendSink::kIOHandleMax = 899;

AudioBackendSink::AudioBackendSink(const AudioHandle audio_handle,
                                   const AudioErrorHandler& error_handler,
                                   const AudioBufferHandler& buffer_handler)
    : audio_handle_(audio_handle),
      state_(AudioState::kNew),
      error_handler_(error_handler),
      buffer_handler_(buffer_handler),
      using_offload_(false),
      current_io_handle_(kIOHandleMin) {
  QMMF_DEBUG("%s: %s() state is now %d", TAG, __func__,
             static_cast<int>(state_));
}

AudioBackendSink::~AudioBackendSink() {}

int32_t AudioBackendSink::Open(const qahw_module_handle_t * const modules[],
                               const vector<DeviceId>& devices,
                               const AudioMetadata& metadata) {
  QMMF_DEBUG("%s: %s() TRACE", TAG, __func__);
  for (const DeviceId device : devices)
    QMMF_VERBOSE("%s: %s() INPARAM: device[%d]", TAG, __func__, device);
  QMMF_VERBOSE("%s: %s() INPARAM: metadata[%s]", TAG, __func__,
               metadata.ToString().c_str());
  int32_t result = 0;

  switch (state_) {
    case AudioState::kNew:
      // proceed
      break;
    case AudioState::kConnect:
    case AudioState::kIdle:
    case AudioState::kRunning:
    case AudioState::kPaused:
      QMMF_ERROR("%s: %s() invalid operation for current state: %d", TAG,
                 __func__, static_cast<int>(state_));
      return -ENOSYS;
      break;
    default:
      QMMF_ERROR("%s: %s() unknown state: %d", TAG, __func__,
                 static_cast<int>(state_));
      return -ENOSYS;
      break;
  }

  qahw_module_ = const_cast<qahw_module_handle_t *>
                           (modules[AudioHAL::kPrimary]);
  if (qahw_module_ == nullptr) {
    QMMF_ERROR("%s: %s() QAHW module[%s] is not currently loaded",
               TAG, __func__, QAHW_MODULE_ID_PRIMARY);
    return -ENOMEM;
  }

  if ((metadata.flags & static_cast<uint32_t>(AudioFlag::kFlagLowLatency)) == 0)
    using_offload_ = true;
  else
    using_offload_ = false;

  audio_config_t config = AUDIO_CONFIG_INITIALIZER;
  switch (metadata.format) {
    case AudioFormat::kPCM:
      switch (metadata.sample_size) {
        case 8:
          config.format = AUDIO_FORMAT_PCM_8_BIT;
          break;
        case 16:
          config.format = AUDIO_FORMAT_PCM_16_BIT;
          break;
        case 24:
          config.format = AUDIO_FORMAT_PCM_24_BIT_PACKED;
          break;
        case 243:
          config.format = AUDIO_FORMAT_PCM_24_BIT_PACKED;
          break;
        case 244:
          config.format = AUDIO_FORMAT_PCM_8_24_BIT;
          break;
        case 32:
          config.format = AUDIO_FORMAT_PCM_32_BIT;
          break;
        default:
          QMMF_ERROR("%s: %s() invalid sample size: %d", TAG, __func__,
                     metadata.sample_size);
          return -EINVAL;
      }
      break;

    case AudioFormat::kMP3:
      config.format = AUDIO_FORMAT_MP3;
      break;

    default:
      QMMF_ERROR("%s: %s() invalid format: %d", TAG, __func__,
                 static_cast<int32_t>(metadata.format));
      return -EINVAL;
  }

  config.sample_rate = metadata.sample_rate;
  config.channel_mask = audio_channel_out_mask_from_count(metadata.num_channels);
  config.frame_count = 0;

  if (using_offload_) {
    config.offload_info = AUDIO_INFO_INITIALIZER;
    config.offload_info.sample_rate = config.sample_rate;
    config.offload_info.channel_mask = config.channel_mask;
    config.offload_info.format = config.format;
    config.offload_info.stream_type = AUDIO_STREAM_MUSIC;
    switch (metadata.sample_size) {
      case 8:   config.offload_info.bit_width = 8; break;
      case 16:  config.offload_info.bit_width = 16; break;
      case 24:  config.offload_info.bit_width = 24; break;
      case 243: config.offload_info.bit_width = 24; break;
      case 244: config.offload_info.bit_width = 24; break;
      case 32:  config.offload_info.bit_width = 32; break;
      default:
        QMMF_ERROR("%s: %s() invalid sample size: %d", TAG, __func__,
                   metadata.sample_size);
        return -EINVAL;
    }
    config.offload_info.usage = AUDIO_USAGE_MEDIA;
  }

  // use the next available io_handle
  if (current_io_handle_ + 1 > kIOHandleMax)
    current_io_handle_ = kIOHandleMin;
  ++current_io_handle_;

  audio_output_flags_t flags;
  if (using_offload_)
    flags = static_cast<audio_output_flags_t>(AUDIO_OUTPUT_FLAG_DIRECT |
                                              AUDIO_OUTPUT_FLAG_COMPRESS_OFFLOAD |
                                              AUDIO_OUTPUT_FLAG_NON_BLOCKING);
  else
    flags = static_cast<audio_output_flags_t>(AUDIO_OUTPUT_FLAG_DIRECT |
                                              AUDIO_OUTPUT_FLAG_FAST);

  result = qahw_open_output_stream(qahw_module_, current_io_handle_,
                                   AUDIO_DEVICE_OUT_SPEAKER, flags, &config,
                                   &qahw_stream_, "output_stream");
  if (result != 0) {
    QMMF_ERROR("%s: %s() failed to open output stream: %d[%s]", TAG, __func__,
               result, strerror(result));
    return result;
  }

  if (using_offload_) {
    result = qahw_out_set_callback(qahw_stream_,
                                   AudioBackendSink::CallbackEntry, this);
    if (result != 0) {
      QMMF_ERROR("%s: %s() failed to set callback: %d[%s]", TAG, __func__,
                 result, strerror(result));
      return result;
    }
  }

  state_ = AudioState::kIdle;
  QMMF_DEBUG("%s: %s() state is now %d", TAG, __func__,
             static_cast<int>(state_));

  return result;
}

int32_t AudioBackendSink::Close() {
  QMMF_DEBUG("%s: %s() TRACE", TAG, __func__);
  int32_t result = 0;

  switch (state_) {
    case AudioState::kNew:
      QMMF_WARN("%s: %s() nothing to do, state is: %d", TAG, __func__,
                static_cast<int>(state_));
      return 0;
      break;
    case AudioState::kIdle:
      // proceed
      break;
    case AudioState::kConnect:
    case AudioState::kRunning:
    case AudioState::kPaused:
      QMMF_ERROR("%s: %s() invalid operation for current state: %d", TAG,
                 __func__, static_cast<int>(state_));
      return -ENOSYS;
      break;
    default:
      QMMF_ERROR("%s: %s() unknown state: %d", TAG, __func__,
                 static_cast<int>(state_));
      return -ENOSYS;
      break;
  }

  result = qahw_close_output_stream(qahw_stream_);
  if (result != 0) {
    QMMF_ERROR("%s: %s() failed to close output stream: %d[%s]",
               TAG, __func__, result, strerror(result));
    return result;
  }

  state_ = AudioState::kNew;
  QMMF_DEBUG("%s: %s() state is now %d", TAG, __func__,
             static_cast<int>(state_));

  return result;
}

int32_t AudioBackendSink::Start() {
  QMMF_DEBUG("%s: %s() TRACE", TAG, __func__);

  switch (state_) {
    case AudioState::kIdle:
      // proceed
      break;
    case AudioState::kNew:
    case AudioState::kConnect:
    case AudioState::kRunning:
    case AudioState::kPaused:
      QMMF_ERROR("%s: %s() invalid operation for current state: %d", TAG,
                 __func__, static_cast<int>(state_));
      return -ENOSYS;
      break;
    default:
      QMMF_ERROR("%s: %s() unknown state: %d", TAG, __func__,
                 static_cast<int>(state_));
      return -ENOSYS;
      break;
  }

  while (!messages_.empty())
    messages_.pop();

  thread_ = new thread(AudioBackendSink::ThreadEntry, this);
  if (thread_ == nullptr) {
    QMMF_ERROR("%s: %s() unable to allocate thread", TAG, __func__);
    return -ENOMEM;
  }

  state_ = AudioState::kRunning;
  QMMF_DEBUG("%s: %s() state is now %d", TAG, __func__,
             static_cast<int>(state_));

  return 0;
}

int32_t AudioBackendSink::Stop(const bool flush) {
  QMMF_DEBUG("%s: %s() TRACE", TAG, __func__);
  QMMF_VERBOSE("%s: %s() INPARAM: flush[%s]", TAG, __func__,
               flush ? "true" : "false");

  switch (state_) {
    case AudioState::kNew:
    case AudioState::kConnect:
    case AudioState::kIdle:
      QMMF_WARN("%s: %s() nothing to do, state is: %d", TAG, __func__,
                static_cast<int>(state_));
      return 0;
      break;
    case AudioState::kRunning:
    case AudioState::kPaused:
      // proceed
      break;
    default:
      QMMF_ERROR("%s: %s() unknown state: %d", TAG, __func__,
                 static_cast<int>(state_));
      return -ENOSYS;
      break;
  }

  AudioMessage message;
  message.type = AudioMessageType::kMessageStop;
  message.flush = flush;

  message_lock_.lock();
  messages_.push(message);
  message_lock_.unlock();
  signal_.notify_one();

  thread_->join();
  delete thread_;

  while (!messages_.empty())
    messages_.pop();

  state_ = AudioState::kIdle;
  QMMF_DEBUG("%s: %s() state is now %d", TAG, __func__,
             static_cast<int>(state_));

  return 0;
}

int32_t AudioBackendSink::Pause() {
  QMMF_DEBUG("%s: %s() TRACE", TAG, __func__);

  switch (state_) {
    case AudioState::kRunning:
      // proceed
      break;
    case AudioState::kNew:
    case AudioState::kConnect:
    case AudioState::kIdle:
    case AudioState::kPaused:
      QMMF_ERROR("%s: %s() invalid operation for current state: %d", TAG,
                 __func__, static_cast<int>(state_));
      return -ENOSYS;
      break;
    default:
      QMMF_ERROR("%s: %s() unknown state: %d", TAG, __func__,
                 static_cast<int>(state_));
      return -ENOSYS;
      break;
  }

  AudioMessage message;
  message.type = AudioMessageType::kMessagePause;

  message_lock_.lock();
  messages_.push(message);
  message_lock_.unlock();
  signal_.notify_one();

  state_ = AudioState::kPaused;
  QMMF_DEBUG("%s: %s() state is now %d", TAG, __func__,
             static_cast<int>(state_));

  return 0;
}

int32_t AudioBackendSink::Resume() {
  QMMF_DEBUG("%s: %s() TRACE", TAG, __func__);

  switch (state_) {
    case AudioState::kPaused:
      // proceed
      break;
    case AudioState::kNew:
    case AudioState::kConnect:
    case AudioState::kIdle:
    case AudioState::kRunning:
      QMMF_ERROR("%s: %s() invalid operation for current state: %d", TAG,
                 __func__, static_cast<int>(state_));
      return -ENOSYS;
      break;
    default:
      QMMF_ERROR("%s: %s() unknown state: %d", TAG, __func__,
                 static_cast<int>(state_));
      return -ENOSYS;
      break;
  }

  AudioMessage message;
  message.type = AudioMessageType::kMessageResume;

  message_lock_.lock();
  messages_.push(message);
  message_lock_.unlock();
  signal_.notify_one();

  state_ = AudioState::kRunning;
  QMMF_DEBUG("%s: %s() state is now %d", TAG, __func__,
             static_cast<int>(state_));

  return 0;
}

int32_t AudioBackendSink::SendBuffers(const vector<AudioBuffer>& buffers) {
  QMMF_DEBUG("%s: %s() TRACE", TAG, __func__);
  for (const AudioBuffer& buffer : buffers)
    QMMF_VERBOSE("%s: %s() INPARAM: buffer[%s]", TAG, __func__,
                 buffer.ToString().c_str());

  switch (state_) {
    case AudioState::kIdle:
    case AudioState::kRunning:
      // proceed
      break;
    case AudioState::kNew:
    case AudioState::kConnect:
    case AudioState::kPaused:
      QMMF_ERROR("%s: %s() invalid operation for current state: %d", TAG,
                 __func__, static_cast<int>(state_));
      return -ENOSYS;
      break;
    default:
      QMMF_ERROR("%s: %s() unknown state: %d", TAG, __func__,
                 static_cast<int>(state_));
      return -ENOSYS;
      break;
  }

  AudioMessage message;
  message.type = AudioMessageType::kMessageBuffer;
  for (const AudioBuffer& buffer : buffers)
    message.buffers.push_back(buffer);

  message_lock_.lock();
  messages_.push(message);
  message_lock_.unlock();
  signal_.notify_one();

  return 0;
}

int32_t AudioBackendSink::GetLatency(int32_t* latency) {
  QMMF_DEBUG("%s: %s() TRACE", TAG, __func__);

  switch (state_) {
    case AudioState::kIdle:
      // proceed
      break;
    case AudioState::kNew:
    case AudioState::kConnect:
    case AudioState::kRunning:
    case AudioState::kPaused:
      QMMF_ERROR("%s: %s() invalid operation for current state: %d", TAG,
                 __func__, static_cast<int>(state_));
      return -ENOSYS;
      break;
    default:
      QMMF_ERROR("%s: %s() unknown state: %d", TAG, __func__,
                 static_cast<int>(state_));
      return -ENOSYS;
      break;
  }

  *latency = qahw_out_get_latency(qahw_stream_);

  QMMF_VERBOSE("%s: %s() OUTPARAM: latency[%d]", TAG, __func__, *latency);
  return 0;
}

int32_t AudioBackendSink::GetBufferSize(int32_t* buffer_size) {
  QMMF_DEBUG("%s: %s() TRACE", TAG, __func__);

  switch (state_) {
    case AudioState::kIdle:
      // proceed
      break;
    case AudioState::kNew:
    case AudioState::kConnect:
    case AudioState::kRunning:
    case AudioState::kPaused:
      QMMF_ERROR("%s: %s() invalid operation for current state: %d", TAG,
                 __func__, static_cast<int>(state_));
      return -ENOSYS;
      break;
    default:
      QMMF_ERROR("%s: %s() unknown state: %d", TAG, __func__,
                 static_cast<int>(state_));
      return -ENOSYS;
      break;
  }

  *buffer_size = qahw_out_get_buffer_size(qahw_stream_);

  QMMF_VERBOSE("%s: %s() OUTPARAM: buffer_size[%d]", TAG, __func__,
               *buffer_size);
  return 0;
}

int32_t AudioBackendSink::SetParam(const AudioParamType type,
                                   const AudioParamData& data) {
  QMMF_DEBUG("%s: %s() TRACE", TAG, __func__);
  QMMF_VERBOSE("%s: %s() INPARAM: type[%d]", TAG, __func__,
               static_cast<int>(type));
  QMMF_VERBOSE("%s: %s() INPARAM: data[%s]", TAG, __func__,
               data.ToString(type).c_str());

  switch (state_) {
    case AudioState::kIdle:
    case AudioState::kRunning:
    case AudioState::kPaused:
      // proceed
      break;
    case AudioState::kNew:
    case AudioState::kConnect:
      QMMF_ERROR("%s: %s() invalid operation for current state: %d", TAG,
                 __func__, static_cast<int>(state_));
      return -ENOSYS;
      break;
    default:
      QMMF_ERROR("%s: %s() unknown state: %d", TAG, __func__,
                 static_cast<int>(state_));
      return -ENOSYS;
      break;
  }

  switch (type) {
    case AudioParamType::kVolume:
      {
        float volume = static_cast<float>(data.volume) / 100.0;

        int result = qahw_out_set_volume(qahw_stream_, volume, volume);
        if (result != 0) {
          QMMF_ERROR("%s: %s() failed to set volume[%f]: %d[%s]",
                     TAG, __func__, volume, result, strerror(result));
          return result;
        }
      }
      break;
    case AudioParamType::kDevice:
      QMMF_WARN("%s: %s() invalid operation", TAG, __func__);
      break;
    case AudioParamType::kCustom:
      {
        string keyvalue = data.custom.key;
        keyvalue.append("=");
        keyvalue.append(data.custom.value);

        int result = qahw_out_set_parameters(qahw_stream_, keyvalue.c_str());
        if (result != 0) {
          QMMF_ERROR("%s: %s() failed to set custom parameter[%s]: %d[%s]",
                     TAG, __func__, keyvalue.c_str(), result,
                     strerror(result));
          return result;
        }
      }
      break;
    default:
      QMMF_ERROR("%s: %s() unknown parameter: %d", TAG, __func__,
                 static_cast<int>(type));
      return -ENOSYS;
      break;
  }

  return 0;
}

int32_t AudioBackendSink::GetRenderedPosition(uint32_t* frames,
                                              uint64_t* time)
{
  QMMF_DEBUG("%s: %s() TRACE", TAG, __func__);

  switch (state_) {
    case AudioState::kIdle:
    case AudioState::kNew:
    case AudioState::kConnect:
      QMMF_ERROR("%s: %s() invalid operation for current state: %d", TAG,
          __func__, static_cast<int>(state_));
      return -ENOSYS;
      break;
    case AudioState::kRunning:
    case AudioState::kPaused:
      // proceed
      break;
    default:
      QMMF_ERROR("%s: %s() unknown state: %d", TAG, __func__,
          static_cast<int>(state_));
      return -ENOSYS;
      break;
  }

  int result = qahw_out_get_render_position(qahw_stream_, frames);
  if (result < 0) {
    QMMF_ERROR("%s: %s() Failed to get render position : %d", TAG,
        __func__, result);
  }

  QMMF_VERBOSE("%s: %s() Total Frames Rendered : %u", TAG, __func__, *frames);

  uint64_t frame;
  struct timespec tv;

  result = qahw_out_get_presentation_position(qahw_stream_, &frame, &tv);
  if (result < 0) {
    QMMF_ERROR("%s: %s() Failed to get presentation position: %d",
               TAG, __func__, result);
  }

  *time = (uint64_t)(tv.tv_sec) * 1000000 + (uint64_t)(tv.tv_nsec) / 1000;

  QMMF_VERBOSE("%s: %s() Total Frames Rendered (%llu) Time (%llu)", TAG,
      __func__, frame, *time);

  QMMF_VERBOSE("%s: %s() OUTPARAM: frames[%u] time[%llu]", TAG, __func__,
      *frames, *time);
  return 0;
}

int AudioBackendSink::CallbackEntry(qahw_stream_callback_event_t event,
                                    void* param,
                                    void* cookie) {
  QMMF_DEBUG("%s: %s() TRACE", TAG, __func__);

  if (cookie == nullptr) {
    QMMF_ERROR("%s: %s() invalid cookie given", TAG, __func__);
    return 0;
  }

  AudioBackendSink* backend = reinterpret_cast<AudioBackendSink*>(cookie);
  return backend->Callback(event, param);
}

int AudioBackendSink::Callback(qahw_stream_callback_event_t event,
                               void* param) {
  QMMF_DEBUG("%s: %s() TRACE", TAG, __func__);

  switch (event) {
    case QAHW_STREAM_CBK_EVENT_WRITE_READY:
      QMMF_DEBUG("%s: %s() received WRITE_READY event", TAG, __func__);
      {
        AudioMessage message;
        message.type = AudioMessageType::kMessageOffload;

        message_lock_.lock();
        messages_.push(message);
        message_lock_.unlock();
        signal_.notify_one();
      }
      break;

    case QAHW_STREAM_CBK_EVENT_DRAIN_READY:
      QMMF_DEBUG("%s: %s() received DRAIN_READY event", TAG, __func__);
      drain_signal_.notify_one();
      break;

    case QAHW_STREAM_CBK_EVENT_ADSP:
      QMMF_DEBUG("%s: %s() received ADSP event", TAG, __func__);
      break;

    case QAHW_STREAM_CBK_EVENT_ERROR:
      QMMF_ERROR("%s: %s() received ERROR event", TAG, __func__);
      Stop(false);
      break;

    default:
      QMMF_ERROR("%s: %s() invalid event[%d]", TAG, __func__, event);
      break;
  }

  return 0;
}

void AudioBackendSink::ThreadEntry(AudioBackendSink* backend) {
  QMMF_DEBUG("%s: %s() TRACE", TAG, __func__);

  backend->Thread();
}

void AudioBackendSink::Thread() {
  QMMF_DEBUG("%s: %s() TRACE", TAG, __func__);
  queue<AudioBuffer> buffers;
  int result;

  size_t bytes_written = 0;
  bool pending_offload = false;
  bool stop_received = false;
  bool eof_received = false;
  bool flush_requested = false;
  bool paused = false;
  bool keep_running = true;
  while (keep_running) {
    // wait until there is something to do
    if (buffers.empty() && messages_.empty()) {
      unique_lock<mutex> lk(message_lock_);
      if (signal_.wait_for(lk, seconds(1)) == cv_status::timeout)
        QMMF_WARN("%s: %s() timed out on wait", TAG, __func__);
    }

    // process the next pending message
    message_lock_.lock();
    if (!messages_.empty()) {
      AudioMessage message = messages_.front();

      switch (message.type) {
        case AudioMessageType::kMessagePause:
          QMMF_DEBUG("%s: %s-MessagePause() TRACE", TAG, __func__);
          paused = true;

          result = qahw_out_pause(qahw_stream_);
          if (result < 0) {
            QMMF_ERROR("%s: %s() failed to pause output stream: %d[%s]", TAG,
                       __func__, result, strerror(result));
            error_handler_(audio_handle_, result);
          }
          break;

        case AudioMessageType::kMessageResume:
          QMMF_DEBUG("%s: %s-MessageResume() TRACE", TAG, __func__);
          paused = false;

          result = qahw_out_resume(qahw_stream_);
          if (result < 0) {
            QMMF_ERROR("%s: %s() failed to resume output stream: %d[%s]", TAG,
                       __func__, result, strerror(result));
            error_handler_(audio_handle_, result);
          }
          break;

        case AudioMessageType::kMessageStop:
          QMMF_DEBUG("%s: %s-MessageStop() TRACE", TAG, __func__);
          paused = false;
          stop_received = true;
          flush_requested = message.flush;
          break;

        case AudioMessageType::kMessageBuffer:
          QMMF_DEBUG("%s: %s-MessageBuffer() TRACE", TAG, __func__);
          for (const AudioBuffer& buffer : message.buffers) {
            QMMF_VERBOSE("%s: %s() INPARAM: buffer[%s] to queue[%u]",
                         TAG, __func__, buffer.ToString().c_str(),
                         buffers.size());
            buffers.push(buffer);
            QMMF_VERBOSE("%s: %s() buffers queue is now %u deep",
                         TAG, __func__, buffers.size());
          }
          break;

        case AudioMessageType::kMessageOffload:
          QMMF_DEBUG("%s: %s-MessageOffload() TRACE", TAG, __func__);
          pending_offload = false;
          break;
      }

      messages_.pop();
    }
    message_lock_.unlock();

    // process the next pending buffer
    if (!buffers.empty() && !paused && !pending_offload && keep_running) {
      AudioBuffer& buffer = buffers.front();
      QMMF_VERBOSE("%s: %s() processing next buffer[%s] from queue[%u]",
                   TAG, __func__, buffer.ToString().c_str(), buffers.size());

      qahw_out_buffer_t qahw_buffer;
      memset(&qahw_buffer, 0, sizeof(qahw_out_buffer_t));
      qahw_buffer.buffer = reinterpret_cast<uint8_t*>(buffer.data) +
                           bytes_written;
      qahw_buffer.bytes = buffer.size - bytes_written;

      result = qahw_out_write(qahw_stream_, &qahw_buffer);
      if (result < 0) {
        QMMF_ERROR("%s: %s() failed to write output stream: %d[%s]", TAG,
                   __func__, result, strerror(result));
        error_handler_(audio_handle_, result);
      } else if (static_cast<size_t>(result) != qahw_buffer.bytes &&
                 using_offload_) {
        pending_offload = true;
        bytes_written += result;
      } else if (static_cast<size_t>(result) == qahw_buffer.bytes) {
        bytes_written += result;
      } else {
        QMMF_ERROR("%s: %s() incomplete write to output stream for non-offload stream",
                   TAG, __func__);
        error_handler_(audio_handle_, -1);
      }

      if (bytes_written == static_cast<size_t>(buffer.size)) {
        bytes_written = 0;

        if (buffer.flags & static_cast<uint32_t>(BufferFlags::kFlagEOS))
          eof_received = true;

        buffer.size = 0;
        buffer.timestamp = 0;

        // return empty buffer to client
        buffer_handler_(audio_handle_, buffer);
        buffers.pop();
        QMMF_VERBOSE("%s: %s() buffers queue is now %u deep",
                     TAG, __func__, buffers.size());
      }
    }

    // stop condition
    if ((stop_received && !flush_requested) ||
        (stop_received && flush_requested && eof_received))
      keep_running = false;
  }

  if (flush_requested) {
    result = qahw_out_drain(qahw_stream_, QAHW_DRAIN_ALL);
    if (result != 0) {
      QMMF_ERROR("%s: %s() failed to drain the output stream: %d[%s]",
                 TAG, __func__, result, strerror(result));
      error_handler_(audio_handle_, result);
    }

    if (using_offload_) {
      unique_lock<mutex> lk(drain_lock_);
      while (drain_signal_.wait_for(lk, seconds(1)) == cv_status::timeout)
        QMMF_WARN("%s: %s() timed out on wait for drain", TAG, __func__);
    }
  }

  result = qahw_out_standby(qahw_stream_);
  if (result != 0) {
    QMMF_ERROR("%s: %s() failed to put output stream in standby: %d[%s]",
               TAG, __func__, result, strerror(result));
    error_handler_(audio_handle_, result);
  }
}

}; // namespace audio
}; // namespace common
}; // namespace qmmf
