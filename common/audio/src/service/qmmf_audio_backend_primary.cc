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

#define TAG "AudioBackendPrimary"

#include "common/audio/src/service/qmmf_audio_backend_primary.h"

#include <chrono>
#include <condition_variable>
#include <cstring>
#include <functional>
#include <map>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>
#include <type_traits>

#include <hardware/audio.h>

#include "common/audio/inc/qmmf_audio_definitions.h"
#include "common/audio/src/service/qmmf_audio_common.h"
#include "common/qmmf_log.h"

/* remove comment marker to mimic the AHAL instead of using it */
//#define AUDIO_BACKEND_PRIMARY_DEBUG_DATAFLOW

namespace qmmf {
namespace common {
namespace audio {

using ::std::chrono::duration_cast;
using ::std::chrono::milliseconds;
using ::std::chrono::system_clock;
using ::std::condition_variable;
using ::std::function;
using ::std::map;
using ::std::mutex;
using ::std::queue;
using ::std::thread;
using ::std::unique_lock;
using ::std::vector;
using ::std::underlying_type;

AudioBackendPrimary::AudioBackendPrimary(const AudioHandle audio_handle,
    const AudioErrorHandler& error_handler,
    const AudioBufferHandler& buffer_handler)
    : audio_handle_(audio_handle), error_handler_(error_handler),
      buffer_handler_(buffer_handler), state_(AudioState::kNew) {
  QMMF_DEBUG("%s: %s() state is now %d", TAG, __func__,
             static_cast<underlying_type<AudioState>::type>(state_));
}

AudioBackendPrimary::~AudioBackendPrimary() {}

int32_t AudioBackendPrimary::Open(const AudioEndPointType type,
                                  const vector<DeviceId>& devices,
                                  const AudioMetadata& metadata) {
  QMMF_DEBUG("%s: %s() TRACE", TAG, __func__);
  QMMF_VERBOSE("%s: %s() INPARAM: type[%d]", TAG, __func__,
               static_cast<underlying_type<AudioEndPointType>::type>(type));
  for (const DeviceId device : devices)
    QMMF_VERBOSE("%s: %s() INPARAM: device[%d]", TAG, __func__, device);
  QMMF_VERBOSE("%s: %s() INPARAM: metadata[%s]", TAG, __func__,
               metadata.ToString().c_str());
#ifndef AUDIO_BACKEND_PRIMARY_DEBUG_DATAFLOW
  int result;
#endif

  switch (state_) {
    case AudioState::kNew:
      /* proceed */
      break;
    case AudioState::kConnect:
    case AudioState::kIdle:
    case AudioState::kRunning:
    case AudioState::kPaused:
      QMMF_ERROR("%s: %s() invalid operation for current state: %d", TAG,
                 __func__,
                 static_cast<underlying_type<AudioState>::type>(state_));
      return -ENOSYS;
      break;
    default:
      QMMF_ERROR("%s: %s() unknown state: %d", TAG, __func__,
                 static_cast<underlying_type<AudioState>::type>(state_));
      return -ENOSYS;
      break;
  }

  type_ = type;

#ifndef AUDIO_BACKEND_PRIMARY_DEBUG_DATAFLOW
  result = hw_get_module_by_class(AUDIO_HARDWARE_MODULE_ID,
                                  AUDIO_HARDWARE_MODULE_ID_PRIMARY,
                                  &hal_module_);
  if (result != 0) {
    QMMF_ERROR("%s: %s() failed to get HAL module: %d[%s]", TAG, __func__,
               result, strerror(-result));
    return -result;
  }

  result = audio_hw_device_open(hal_module_, &hal_device_);
  if (result != 0) {
    QMMF_ERROR("%s: %s() failed to get HAL device: %d[%s]", TAG, __func__,
               result, strerror(-result));
    return -result;
  }

  if (hal_device_->common.version < AUDIO_DEVICE_API_VERSION_MIN) {
    QMMF_ERROR("%s: %s() incorrect HAL device version[%d]", TAG, __func__,
               hal_device_->common.version);
    return -EPERM;
  }
  QMMF_INFO("%s: %s() HAL device version[%d]", TAG, __func__,
            hal_device_->common.version);

  if (type_ == AudioEndPointType::kSource) {
    unsigned int channel_bits;
    switch (metadata.num_channels) {
      case 1:
        channel_bits = AUDIO_CHANNEL_IN_MONO;
        break;
      case 2:
        channel_bits = AUDIO_CHANNEL_IN_STEREO;
        break;
      default:
        QMMF_ERROR("%s: %s() invalid number of channels: %d", TAG, __func__,
                   metadata.num_channels);
        return -EINVAL;
    }

    audio_config_t config = AUDIO_CONFIG_INITIALIZER;
    switch (metadata.sample_size) {
      case 16:
        config.format = AUDIO_FORMAT_PCM_16_BIT;
        break;
      case 32:
        config.format = AUDIO_FORMAT_PCM_32_BIT;
        break;
      default:
        QMMF_ERROR("%s: %s() invalid sample size: %d", TAG, __func__,
                   metadata.sample_size);
        return -EINVAL;
    }

    config.sample_rate = metadata.sample_rate;
    config.channel_mask = audio_channel_mask_from_representation_and_bits(
                          AUDIO_CHANNEL_REPRESENTATION_POSITION, channel_bits);
    config.frame_count = 0;

    result = hal_device_->open_input_stream(hal_device_, 0x999,
                                            AUDIO_DEVICE_IN_BUILTIN_MIC,
                                            &config, &hal_input_stream_,
                                            AUDIO_INPUT_FLAG_NONE,
                                            "input_stream",
                                            AUDIO_SOURCE_DEFAULT);
    if (result != 0) {
      QMMF_ERROR("%s: %s() failed to open input stream: %d", TAG, __func__,
                 result);
      return -result;
    }

    result = hal_input_stream_->set_gain(hal_input_stream_, 1.0);
    if (result != 0) {
      QMMF_ERROR("%s: %s() failed to set stream gain: %d[%s]", TAG, __func__,
                 result, strerror(-result));
      return -result;
    }

    /* set the input source to AUDIO_SOURCE_MIC */
    hal_input_stream_->common.set_parameters(&hal_input_stream_->common,
                                             "input_source=1");
  } else if (type_ == AudioEndPointType::kSink) {
    unsigned int channel_bits;
    switch (metadata.num_channels) {
      case 1:
        channel_bits = AUDIO_CHANNEL_OUT_MONO;
        break;
      case 2:
        channel_bits = AUDIO_CHANNEL_OUT_STEREO;
        break;
      default:
        QMMF_ERROR("%s: %s() invalid number of channels: %d", TAG, __func__,
                   metadata.num_channels);
        return -EINVAL;
    }

    audio_config_t config = AUDIO_CONFIG_INITIALIZER;
    switch (metadata.sample_size) {
      case 16:
        config.offload_info.format = AUDIO_FORMAT_PCM_16_BIT;
        break;
      case 32:
        config.offload_info.format = AUDIO_FORMAT_PCM_32_BIT;
        break;
      default:
        QMMF_ERROR("%s: %s() invalid sample size: %d", TAG, __func__,
                   metadata.sample_size);
        return -EINVAL;
    }

    config.channel_mask = audio_channel_mask_from_representation_and_bits(
                          AUDIO_CHANNEL_REPRESENTATION_POSITION, channel_bits);
    config.offload_info.version = AUDIO_OFFLOAD_INFO_VERSION_CURRENT;
    config.offload_info.size = sizeof(audio_offload_info_t);
    config.offload_info.sample_rate = metadata.sample_rate;
    config.frame_count = 0;

    result = hal_device_->open_output_stream(hal_device_, 0x999,
                                             AUDIO_DEVICE_OUT_SPEAKER,
                                             AUDIO_OUTPUT_FLAG_COMPRESS_OFFLOAD,
                                             &config, &hal_output_stream_,
                                             "output_stream");
    if (result != 0) {
      QMMF_ERROR("%s: %s() failed to open input stream: %d", TAG, __func__,
                 result);
      return -result;
    }
  } else {
    QMMF_ERROR("%s: %s() backend has invalid type: %d", TAG, __func__,
               static_cast<underlying_type<AudioEndPointType>::type>(type_));
    return -ENOSYS;
  }
#endif

  state_ = AudioState::kIdle;
  QMMF_DEBUG("%s: %s() state is now %d", TAG, __func__,
             static_cast<underlying_type<AudioState>::type>(state_));

  return 0;
}

int32_t AudioBackendPrimary::Close() {
  QMMF_DEBUG("%s: %s() TRACE", TAG, __func__);

  switch (state_) {
    case AudioState::kNew:
      QMMF_WARN("%s: %s() nothing to do, state is: %d", TAG, __func__,
                static_cast<underlying_type<AudioState>::type>(state_));
      return 0;
      break;
    case AudioState::kIdle:
      /* proceed */
      break;
    case AudioState::kConnect:
    case AudioState::kRunning:
    case AudioState::kPaused:
      QMMF_ERROR("%s: %s() invalid operation for current state: %d", TAG,
                 __func__,
                 static_cast<underlying_type<AudioState>::type>(state_));
      return -ENOSYS;
      break;
    default:
      QMMF_ERROR("%s: %s() unknown state: %d", TAG, __func__,
                 static_cast<underlying_type<AudioState>::type>(state_));
      return -ENOSYS;
      break;
  }

#ifndef AUDIO_BACKEND_PRIMARY_DEBUG_DATAFLOW
  if (type_ == AudioEndPointType::kSource) {
    hal_device_->close_input_stream(hal_device_, hal_input_stream_);
  } else if (type_ == AudioEndPointType::kSink) {
    hal_device_->close_output_stream(hal_device_, hal_output_stream_);
  } else {
    QMMF_ERROR("%s: %s() backend has invalid type: %d", TAG, __func__,
               static_cast<underlying_type<AudioEndPointType>::type>(type_));
    return -ENOSYS;
  }

  int result = audio_hw_device_close(hal_device_);
  if (result != 0)
    QMMF_ERROR("%s: %s() failed to close HAL device: %d", TAG, __func__,
               result);
#endif

  state_ = AudioState::kNew;
  QMMF_DEBUG("%s: %s() state is now %d", TAG, __func__,
             static_cast<underlying_type<AudioState>::type>(state_));

#ifndef AUDIO_BACKEND_PRIMARY_DEBUG_DATAFLOW
  return result;
#else
  return 0;
#endif
}

int32_t AudioBackendPrimary::Start() {
  QMMF_DEBUG("%s: %s() TRACE", TAG, __func__);

  switch (state_) {
    case AudioState::kIdle:
      /* proceed */
      break;
    case AudioState::kNew:
    case AudioState::kConnect:
    case AudioState::kRunning:
    case AudioState::kPaused:
      QMMF_ERROR("%s: %s() invalid operation for current state: %d", TAG,
                 __func__,
                 static_cast<underlying_type<AudioState>::type>(state_));
      return -ENOSYS;
      break;
    default:
      QMMF_ERROR("%s: %s() unknown state: %d", TAG, __func__,
                 static_cast<underlying_type<AudioState>::type>(state_));
      return -ENOSYS;
      break;
  }

  thread_ = new thread(AudioBackendPrimary::StaticThreadEntry, this);
  if (thread_ == nullptr) {
    QMMF_ERROR("%s: %s() unable to allocate thread", TAG, __func__);
    return -ENOMEM;
  }

  state_ = AudioState::kRunning;
  QMMF_DEBUG("%s: %s() state is now %d", TAG, __func__,
             static_cast<underlying_type<AudioState>::type>(state_));

  return 0;
}

int32_t AudioBackendPrimary::Stop(const bool flush) {
  QMMF_DEBUG("%s: %s() TRACE", TAG, __func__);
  QMMF_VERBOSE("%s: %s() INPARAM: flush[%s]", TAG, __func__,
               flush ? "true" : "false");

  switch (state_) {
    case AudioState::kNew:
    case AudioState::kConnect:
    case AudioState::kIdle:
      QMMF_WARN("%s: %s() nothing to do, state is: %d", TAG, __func__,
                static_cast<underlying_type<AudioState>::type>(state_));
      return 0;
      break;
    case AudioState::kRunning:
    case AudioState::kPaused:
      /* proceed */
      break;
    default:
      QMMF_ERROR("%s: %s() unknown state: %d", TAG, __func__,
                 static_cast<underlying_type<AudioState>::type>(state_));
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

  state_ = AudioState::kIdle;
  QMMF_DEBUG("%s: %s() state is now %d", TAG, __func__,
             static_cast<underlying_type<AudioState>::type>(state_));

  return 0;
}

int32_t AudioBackendPrimary::Pause() {
  QMMF_DEBUG("%s: %s() TRACE", TAG, __func__);

  switch (state_) {
    case AudioState::kRunning:
      /* proceed */
      break;
    case AudioState::kNew:
    case AudioState::kConnect:
    case AudioState::kIdle:
    case AudioState::kPaused:
      QMMF_ERROR("%s: %s() invalid operation for current state: %d", TAG,
                 __func__,
                 static_cast<underlying_type<AudioState>::type>(state_));
      return -ENOSYS;
      break;
    default:
      QMMF_ERROR("%s: %s() unknown state: %d", TAG, __func__,
                 static_cast<underlying_type<AudioState>::type>(state_));
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
             static_cast<underlying_type<AudioState>::type>(state_));

  return 0;
}

int32_t AudioBackendPrimary::Resume() {
  QMMF_DEBUG("%s: %s() TRACE", TAG, __func__);

  switch (state_) {
    case AudioState::kPaused:
      /* proceed */
      break;
    case AudioState::kNew:
    case AudioState::kConnect:
    case AudioState::kIdle:
    case AudioState::kRunning:
      QMMF_ERROR("%s: %s() invalid operation for current state: %d", TAG,
                 __func__,
                 static_cast<underlying_type<AudioState>::type>(state_));
      return -ENOSYS;
      break;
    default:
      QMMF_ERROR("%s: %s() unknown state: %d", TAG, __func__,
                 static_cast<underlying_type<AudioState>::type>(state_));
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
             static_cast<underlying_type<AudioState>::type>(state_));

  return 0;
}

int32_t AudioBackendPrimary::SendBuffers(const vector<AudioBuffer>& buffers) {
  QMMF_DEBUG("%s: %s() TRACE", TAG, __func__);
  for (const AudioBuffer& buffer : buffers)
    QMMF_VERBOSE("%s: %s() INPARAM: buffer[%s]", TAG, __func__,
                 buffer.ToString().c_str());

  switch (state_) {
    case AudioState::kRunning:
      /* proceed */
      break;
    case AudioState::kNew:
    case AudioState::kConnect:
    case AudioState::kIdle:
    case AudioState::kPaused:
      QMMF_ERROR("%s: %s() invalid operation for current state: %d", TAG,
                 __func__,
                 static_cast<underlying_type<AudioState>::type>(state_));
      return -ENOSYS;
      break;
    default:
      QMMF_ERROR("%s: %s() unknown state: %d", TAG, __func__,
                 static_cast<underlying_type<AudioState>::type>(state_));
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

int32_t AudioBackendPrimary::GetLatency(int32_t* latency) {
  QMMF_DEBUG("%s: %s() TRACE", TAG, __func__);

  switch (state_) {
    case AudioState::kIdle:
      /* proceed */
      break;
    case AudioState::kNew:
    case AudioState::kConnect:
    case AudioState::kRunning:
    case AudioState::kPaused:
      QMMF_ERROR("%s: %s() invalid operation for current state: %d", TAG,
                 __func__,
                 static_cast<underlying_type<AudioState>::type>(state_));
      return -ENOSYS;
      break;
    default:
      QMMF_ERROR("%s: %s() unknown state: %d", TAG, __func__,
                 static_cast<underlying_type<AudioState>::type>(state_));
      return -ENOSYS;
      break;
  }

  *latency = 11;
  QMMF_VERBOSE("%s: %s() OUTPARAM: latency[%d]", TAG, __func__, *latency);

  return 0;
}

int32_t AudioBackendPrimary::GetBufferSize(int32_t* buffer_size) {
  QMMF_DEBUG("%s: %s() TRACE", TAG, __func__);

  switch (state_) {
    case AudioState::kIdle:
      /* proceed */
      break;
    case AudioState::kNew:
    case AudioState::kConnect:
    case AudioState::kRunning:
    case AudioState::kPaused:
      QMMF_ERROR("%s: %s() invalid operation for current state: %d", TAG,
                 __func__,
                 static_cast<underlying_type<AudioState>::type>(state_));
      return -ENOSYS;
      break;
    default:
      QMMF_ERROR("%s: %s() unknown state: %d", TAG, __func__,
                 static_cast<underlying_type<AudioState>::type>(state_));
      return -ENOSYS;
      break;
  }

#ifndef AUDIO_BACKEND_PRIMARY_DEBUG_DATAFLOW
  if (type_ == AudioEndPointType::kSource) {
    *buffer_size = hal_input_stream_->common.get_buffer_size(
        &hal_input_stream_->common);
  } else if (type_ == AudioEndPointType::kSink) {
    *buffer_size = hal_output_stream_->common.get_buffer_size(
        &hal_output_stream_->common);
  } else {
    QMMF_ERROR("%s: %s() backend has invalid type: %d", TAG, __func__,
               static_cast<underlying_type<AudioEndPointType>::type>(type_));
    return -ENOSYS;
  }
#else
  *buffer_size = 16;
#endif

  QMMF_VERBOSE("%s: %s() OUTPARAM: buffer_size[%d]", TAG, __func__,
               *buffer_size);
  return 0;
}

int32_t AudioBackendPrimary::SetParam(const AudioParamType type,
                                      const AudioParamData& data) {
  QMMF_DEBUG("%s: %s() TRACE", TAG, __func__);
  QMMF_VERBOSE("%s: %s() INPARAM: type[%d]", TAG, __func__,
               static_cast<underlying_type<AudioParamType>::type>(type));
  QMMF_VERBOSE("%s: %s() INPARAM: data[%s]", TAG, __func__,
               data.ToString(type).c_str());

  switch (state_) {
    case AudioState::kIdle:
    case AudioState::kRunning:
    case AudioState::kPaused:
      /* proceed */
      break;
    case AudioState::kNew:
    case AudioState::kConnect:
      QMMF_ERROR("%s: %s() invalid operation for current state: %d", TAG,
                 __func__,
                 static_cast<underlying_type<AudioState>::type>(state_));
      return -ENOSYS;
      break;
    default:
      QMMF_ERROR("%s: %s() unknown state: %d", TAG, __func__,
                 static_cast<underlying_type<AudioState>::type>(state_));
      return -ENOSYS;
      break;
  }

  return 0;
}

void AudioBackendPrimary::StaticThreadEntry(AudioBackendPrimary* backend) {
  QMMF_DEBUG("%s: %s() TRACE", TAG, __func__);

  backend->ThreadEntry();
}

void AudioBackendPrimary::ThreadEntry() {
  QMMF_DEBUG("%s: %s() TRACE", TAG, __func__);

  switch (type_) {
    case AudioEndPointType::kSource: SourceThread(); break;
    case AudioEndPointType::kSink: SinkThread(); break;
    default:
      QMMF_ERROR("%s: %s() backend has invalid type: %d", TAG, __func__,
                 static_cast<underlying_type<AudioEndPointType>::type>(type_));
  }
}

void AudioBackendPrimary::SourceThread() {
  QMMF_DEBUG("%s: %s() TRACE", TAG, __func__);
  queue<AudioBuffer> buffers;
  bool paused = false;
  bool flushing = false;

  /* clear the message queue of expired messages */
  while (!messages_.empty())
    messages_.pop();

  bool keep_running = true;
  while (keep_running) {
    /* wait until there is something to do */
    if (buffers.empty() && messages_.empty()) {
      unique_lock<mutex> lk(message_lock_);
      signal_.wait(lk);
    }

    /* process the next pending message */
    message_lock_.lock();
    if (!messages_.empty()) {
      AudioMessage message = messages_.front();

      switch (message.type) {
        case AudioMessageType::kMessagePause:
          QMMF_DEBUG("%s: %s-MessagePause() TRACE", TAG, __func__);
          paused = true;
          break;

        case AudioMessageType::kMessageResume:
          QMMF_DEBUG("%s: %s-MessageResume() TRACE", TAG, __func__);
          paused = false;
          break;

        case AudioMessageType::kMessageStop:
          QMMF_DEBUG("%s: %s-MessageStop() TRACE", TAG, __func__);
          paused = false;
          flushing = message.flush;
          keep_running = false;
          break;

        case AudioMessageType::kMessageBuffer:
          QMMF_DEBUG("%s: %s-MessageBuffer() TRACE", TAG, __func__);
          for (const AudioBuffer& buffer : message.buffers) {
            QMMF_VERBOSE("%s: %s() INPARAM: buffer[%s]", TAG, __func__,
                         buffer.ToString().c_str());
            buffers.push(buffer);
          }
          break;
      }

      messages_.pop();
    }
    message_lock_.unlock();

    /* process the next pending buffer */
    do {
      if (!buffers.empty() && !paused) {
        AudioBuffer& buffer = buffers.front();
        QMMF_VERBOSE("%s: %s() processing next buffer[%s]", TAG, __func__,
                     buffer.ToString().c_str());

#ifndef AUDIO_BACKEND_PRIMARY_DEBUG_DATAFLOW
        int result = hal_input_stream_->read(hal_input_stream_, buffer.data,
                                             buffer.capacity);
        if (result < 0) {
          QMMF_ERROR("%s: %s() failed to read input stream: %d", TAG,
                     __func__, result);
          error_handler_(audio_handle_, result);
          buffer.size = 0;
        } else {
          buffer.size = result;
        }
#else
        memset(buffer.data, 0xFF, buffer.capacity);
        memset(buffer.data, 0x11, 1);
        buffer.size = buffer.capacity;
        ::std::this_thread::sleep_for(::std::chrono::seconds(1));
#endif

        /* if filled, return timestamped buffer to client */
        if (buffer.size > 0) {
          milliseconds timestamp = duration_cast<milliseconds>(
              system_clock::now().time_since_epoch());
          buffer.timestamp = timestamp.count();

          buffer_handler_(audio_handle_, buffer);
          buffers.pop();
        }
      }

      if (buffers.empty() || paused)
        flushing = false;
    } while (flushing == true);
  }
}

void AudioBackendPrimary::SinkThread() {
  QMMF_DEBUG("%s: %s() TRACE", TAG, __func__);
  queue<AudioBuffer> buffers;
  bool paused = false;
  bool flushing = false;

  /* clear the message queue of expired messages */
  while (!messages_.empty())
    messages_.pop();

  bool keep_running = true;
  while (keep_running) {
    /* wait until there is something to do */
    if (buffers.empty() && messages_.empty()) {
      unique_lock<mutex> lk(message_lock_);
      signal_.wait(lk);
    }

    /* process the next pending message */
    message_lock_.lock();
    if (!messages_.empty()) {
      AudioMessage message = messages_.front();

      switch (message.type) {
        case AudioMessageType::kMessagePause:
          QMMF_DEBUG("%s: %s-MessagePause() TRACE", TAG, __func__);
          paused = true;
          break;

        case AudioMessageType::kMessageResume:
          QMMF_DEBUG("%s: %s-MessageResume() TRACE", TAG, __func__);
          paused = false;
          break;

        case AudioMessageType::kMessageStop:
          QMMF_DEBUG("%s: %s-MessageStop() TRACE", TAG, __func__);
          paused = false;
          flushing = message.flush;
          keep_running = false;
          break;

        case AudioMessageType::kMessageBuffer:
          QMMF_DEBUG("%s: %s-MessageBuffer() TRACE", TAG, __func__);
          for (const AudioBuffer& buffer : message.buffers) {
            QMMF_VERBOSE("%s: %s() INPARAM: buffer[%s]", TAG, __func__,
                         buffer.ToString().c_str());
            buffers.push(buffer);
          }
          break;
      }

      messages_.pop();
    }
    message_lock_.unlock();

    /* process the next pending buffer */
    do {
      if (!buffers.empty() && !paused) {
        AudioBuffer& buffer = buffers.front();
        QMMF_VERBOSE("%s: %s() processing next buffer[%s]", TAG, __func__,
                     buffer.ToString().c_str());

#ifndef AUDIO_BACKEND_PRIMARY_DEBUG_DATAFLOW
        int result = hal_output_stream_->write(hal_output_stream_,
                                               buffer.data, buffer.size);
        if (result < 0) {
          QMMF_ERROR("%s: %s() failed to write output stream: %d", TAG,
                     __func__, result);
          error_handler_(audio_handle_, result);
        } else {
          buffer.size = 0;
          buffer.timestamp = 0;
        }
#else
        for (size_t index = 0; index < buffer.size; ++index) {
          unsigned char* charbuf = static_cast<unsigned char*>(buffer.data);
          QMMF_INFO("%s: %s() consumed buffer[%d][0x%x]", TAG, __func__, index,
                    charbuf[index]);
        }
        buffer.size = 0;
        buffer.timestamp = 0;
        ::std::this_thread::sleep_for(::std::chrono::seconds(1));
#endif

        if (buffer.size == 0) {
          /* return empty buffer to client */
          buffer_handler_(audio_handle_, buffer);
          buffers.pop();
        }
      }

      if (buffers.empty() || paused)
        flushing = false;
    } while (flushing == true);
  }
}

}; /* namespace audio */
}; /* namespace common */
}; /* namespace qmmf */
