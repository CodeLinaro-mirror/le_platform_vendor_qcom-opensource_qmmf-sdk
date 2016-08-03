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

#define TAG "RecorderAudioSource"

#include "recorder/src/service/qmmf_audio_source.h"

#include <condition_variable>
#include <cstring>
#include <mutex>
#include <queue>
#include <thread>

#include "common/audio/inc/qmmf_audio_definitions.h"
#include "common/audio/inc/qmmf_audio_endpoint.h"
#include "common/qmmf_log.h"
#include "recorder/src/service/qmmf_recorder_common.h"
#include "recorder/src/service/qmmf_recorder_ion.h"

namespace qmmf {
namespace recorder {

using namespace ::android;

using ::qmmf::AudioFormat;
using ::qmmf::DeviceIdList;
using ::qmmf::common::audio::AudioBuffer;
using ::qmmf::common::audio::AudioBufferList;
using ::qmmf::common::audio::AudioEndPoint;
using ::qmmf::common::audio::AudioEndPointType;
using ::qmmf::common::audio::AudioEventHandler;
using ::qmmf::common::audio::AudioMetadata;
using ::qmmf::common::audio::AudioEventType;
using ::qmmf::common::audio::AudioEventData;
using ::std::condition_variable;
using ::std::mutex;
using ::std::queue;
using ::std::thread;
using ::std::unique_lock;

static const int kNumberOfBuffers = 4;

AudioSource* AudioSource::instance_ = NULL;

AudioSource* AudioSource::CreateAudioSource() {
  if(!instance_) {
    instance_ = new AudioSource;
    if(!instance_) {
      QMMF_ERROR("%s:%s: Can't Create AudioSource Instance", TAG, __func__);
      //return NULL;
    }
  }
  QMMF_INFO("%s:%s: AudioSource Instance Created Successfully(0x%x)", TAG,
      __func__, instance_);

  return instance_;
}

AudioSource::AudioSource() : end_point_(nullptr), thread_(nullptr) {
  QMMF_INFO("%s:%s: Enter", TAG, __func__);
  QMMF_INFO("%s:%s: Exit", TAG, __func__);
}

AudioSource::~AudioSource() {
  QMMF_INFO("%s:%s: Enter", TAG, __func__);
  instance_ = nullptr;
  QMMF_INFO("%s:%s: Exit (0x%x)", TAG, __func__, this);
}

status_t AudioSource::CreateTrackSource(const uint32_t track_id,
                                        AudioTrackParams& param) {
  QMMF_DEBUG("%s:%s: Enter", TAG, __func__);
  QMMF_VERBOSE("%s:%s INPARAM: track_id[%u]", TAG, __func__, track_id);
  QMMF_VERBOSE("%s:%s INPARAM: param[%s]", TAG, __func__,
               param.ToString().c_str());
  assert(track_id >= 100);
  assert(end_point_ == nullptr);

  end_point_ = new AudioEndPoint;

  AudioEventHandler audio_handler =
    [this] (AudioEventType event_type, const AudioEventData& event_data)
           -> void {
      switch (event_type) {
        case AudioEventType::kError:
          ErrorHandler(event_data.error);
          break;
        case AudioEventType::kBuffer:
          BufferHandler(event_data.buffer);
          break;
      }
    };

  int result = end_point_->Connect(audio_handler);
  assert(result == 0);

  DeviceIdList devices;
  devices.ids.push_back(0);

  AudioMetadata metadata;
  memset(&metadata, 0x0, sizeof metadata);
  metadata.format = AudioFormat::kPCM;
  metadata.num_channels = param.channels;
  metadata.sample_rate = param.sample_rate;
  metadata.sample_size = param.bit_depth;

  result = end_point_->Configure(AudioEndPointType::kSource, devices,
                                     metadata);
  assert(result == 0);

  int buffer_size;
  result = end_point_->GetBufferSize(&buffer_size);
  assert(result == 0);
  QMMF_INFO("%s: %s() buffer_size is %d", TAG, __func__, buffer_size);

  result = ion_.Allocate(kNumberOfBuffers, buffer_size);
  if (result < 0) {
    result = ion_.Deallocate();
    assert(false);
  }

  data_cb_ = param.data_cb;
  track_id_ = track_id;

  QMMF_INFO("%s:%s: track_id(%d) Created Successfully!", TAG, __func__);
  return NO_ERROR;
}

status_t AudioSource::DeleteTrackSource(const uint32_t track_id) {
  QMMF_DEBUG("%s:%s: Enter", TAG, __func__);
  QMMF_VERBOSE("%s:%s INPARAM: track_id[%u]", TAG, __func__, track_id);
  assert(track_id == track_id_);

  int result = end_point_->Disconnect();
  assert(result == 0);

  result = ion_.Deallocate();
  assert(result == 0);

  delete end_point_;
  end_point_ = nullptr;

  QMMF_INFO("%s:%s: track_id(%d) Deleted Successfully!", TAG, __func__);
  return NO_ERROR;
}

status_t AudioSource::StartTrackSource(const uint32_t track_id) {
  QMMF_DEBUG("%s:%s: Enter", TAG, __func__);
  QMMF_VERBOSE("%s:%s INPARAM: track_id[%u]", TAG, __func__, track_id);
  assert(track_id == track_id_);

  int result = end_point_->Start();
  assert(result == 0);

  assert(thread_ == nullptr);
  thread_ = new thread(AudioSource::ThreadEntry, this);
  assert(thread_ != nullptr);

  QMMF_VERBOSE("%s:%s: TrackSource id(%d) Started Succesffuly!", TAG, __func__,
      track_id);
  return NO_ERROR;
}

status_t AudioSource::StopTrackSource(const uint32_t track_id) {
  QMMF_DEBUG("%s:%s: Enter", TAG, __func__);
  QMMF_VERBOSE("%s:%s INPARAM: track_id[%u]", TAG, __func__, track_id);
  assert(track_id == track_id_);

  AudioMessage message;
  message.type = AudioMessageType::kMessageStop;

  message_lock_.lock();
  messages_.push(message);
  message_lock_.unlock();
  signal_.notify_one();

  if (thread_ != nullptr) {
    thread_->join();
    delete thread_;
    thread_ = nullptr;
  }

  int result = end_point_->Stop(false);
  assert(result == 0);

  QMMF_VERBOSE("%s:%s: TrackSource id(%d) Stopped Successfully!", TAG, __func__,
      track_id);
  return NO_ERROR;
}

status_t AudioSource::PauseTrackSource(const uint32_t track_id) {
  QMMF_DEBUG("%s:%s: Enter", TAG, __func__);
  QMMF_VERBOSE("%s:%s INPARAM: track_id[%u]", TAG, __func__, track_id);
  assert(track_id == track_id_);

  AudioMessage message;
  message.type = AudioMessageType::kMessagePause;

  message_lock_.lock();
  messages_.push(message);
  message_lock_.unlock();
  signal_.notify_one();

  int result = end_point_->Pause();
  assert(result == 0);

  QMMF_VERBOSE("%s:%s: TrackSource id(%d) Paused Successfully!", TAG, __func__,
      track_id);
  return NO_ERROR;
}

status_t AudioSource::ResumeTrackSource(const uint32_t track_id) {
  QMMF_DEBUG("%s:%s: Enter", TAG, __func__);
  QMMF_VERBOSE("%s:%s INPARAM: track_id[%u]", TAG, __func__, track_id);
  assert(track_id == track_id_);

  int result = end_point_->Resume();
  assert(result == 0);

  AudioMessage message;
  message.type = AudioMessageType::kMessageResume;

  message_lock_.lock();
  messages_.push(message);
  message_lock_.unlock();
  signal_.notify_one();

  QMMF_VERBOSE("%s:%s: TrackSource id(%d) Resumed Successfully!", TAG, __func__,
      track_id);
  return NO_ERROR;
}

status_t AudioSource::ReturnTrackBuffer(const uint32_t track_id,
    const std::vector<BnTrackBuffer> &buffers) {
  QMMF_DEBUG("%s:%s: Enter", TAG, __func__);
  QMMF_VERBOSE("%s:%s INPARAM: track_id[%u]", TAG, __func__, track_id);
  for (const BnTrackBuffer& buffer : buffers)
    QMMF_VERBOSE("%s: %s() INPARAM: bn_buffer[%s]", TAG, __func__,
                 buffer.ToString().c_str());
  assert(track_id == track_id_);

  for (const BnTrackBuffer& buffer : buffers) {
    AudioMessage message;
    message.type = AudioMessageType::kMessageBnBuffer;
    message.bn_buffer = buffer;

    message_lock_.lock();
    messages_.push(message);
    message_lock_.unlock();
    signal_.notify_one();
  }

  return NO_ERROR;
}

void AudioSource::ErrorHandler(int error) {
  QMMF_DEBUG("%s: %s() TRACE", TAG, __func__);
  QMMF_VERBOSE("%s: %s() INPARAM: type[%d]", TAG, __func__, error);

  assert(false);
}

void AudioSource::BufferHandler(const AudioBuffer& buffer) {
  QMMF_DEBUG("%s: %s() TRACE", TAG, __func__);
  QMMF_VERBOSE("%s: %s() INPARAM: buffer[%s]", TAG, __func__,
               buffer.ToString().c_str());

  AudioMessage message;
  message.type = AudioMessageType::kMessageBuffer;
  message.buffer = buffer;

  message_lock_.lock();
  messages_.push(message);
  message_lock_.unlock();
  signal_.notify_one();
}

void AudioSource::ThreadEntry(AudioSource* source) {
  QMMF_DEBUG("%s: %s() TRACE", TAG, __func__);

  source->Thread();
}

void AudioSource::Thread() {
  QMMF_DEBUG("%s: %s() TRACE", TAG, __func__);
  queue<AudioBuffer> buffers;
  queue<BnTrackBuffer> bn_buffers;
  bool paused = false;

  /* clear the message queue of expired messages */
  while (!messages_.empty())
    messages_.pop();

  /* send the initial list of buffers */
  AudioBufferList initial_buffers;
  ion_.GetList(&initial_buffers);
  int result = end_point_->SendBuffers(initial_buffers);
  assert(result == 0);
  initial_buffers.list.clear();

  bool keep_running = true;
  while (keep_running) {
    /* wait until there is something to do */
    if (bn_buffers.empty() && buffers.empty() && messages_.empty()) {
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
          keep_running = false;
          break;

        case AudioMessageType::kMessageBuffer:
          QMMF_DEBUG("%s: %s-MessageBuffer() TRACE", TAG, __func__);
          QMMF_VERBOSE("%s: %s() INPARAM: buffer[%s]", TAG, __func__,
                       message.buffer.ToString().c_str());
          buffers.push(message.buffer);
          break;

        case AudioMessageType::kMessageBnBuffer:
          QMMF_DEBUG("%s: %s-MessageBnBuffer() TRACE", TAG, __func__);
          QMMF_VERBOSE("%s: %s() INPARAM: bn_buffer[%s]", TAG, __func__,
                       message.bn_buffer.ToString().c_str());
          bn_buffers.push(message.bn_buffer);
          break;
      }
      messages_.pop();
    }
    message_lock_.unlock();

    /* process buffers from endpoint */
    if (!buffers.empty() && !paused && keep_running) {
      AudioBuffer buffer = buffers.front();
      QMMF_VERBOSE("%s: %s() processing next buffer[%s]", TAG, __func__,
                   buffer.ToString().c_str());

      BnTrackBuffer bn_buffer;
      ion_.Export(buffer, &bn_buffer);
      vector<BnTrackBuffer> send_buffers;
      send_buffers.push_back(bn_buffer);
      data_cb_(track_id_, send_buffers, nullptr, TrackMetaParamType::kNone, 0);

      buffers.pop();
    }

    /* process buffers from client */
    if (!bn_buffers.empty() && !paused && keep_running) {
      BnTrackBuffer bn_buffer = bn_buffers.front();
      QMMF_VERBOSE("%s: %s() processing next bn_buffer[%s]", TAG, __func__,
                   bn_buffer.ToString().c_str());

      AudioBuffer buffer;
      ion_.Import(bn_buffer, &buffer);

      memset(buffer.data, 0x00, buffer.capacity);
      buffer.size = 0;
      buffer.timestamp = 0;

      AudioBufferList send_buffers;
      send_buffers.list.push_back(buffer);
      int result = end_point_->SendBuffers(send_buffers);
      assert(result == 0);

      bn_buffers.pop();
    }
  }
}

}; //namespace recorder
}; //namespace qmmf
