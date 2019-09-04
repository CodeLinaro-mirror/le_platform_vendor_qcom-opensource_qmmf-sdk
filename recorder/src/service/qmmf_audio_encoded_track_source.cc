/*
 * Copyright (c) 2016-2019, The Linux Foundation. All rights reserved.
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

//! @file qmmf_audio_encoded_track_source.cc

#define LOG_TAG "RecorderAudioEncodedTrackSource"

#include "recorder/src/service/qmmf_audio_track_source.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

#include "common/utils/qmmf_log.h"
#include "common/utils/qmmf_condition.h"
#include "common/codecadaptor/src/qmmf_avcodec.h"
#include "recorder/src/service/qmmf_audio_pulse_client.h"
#include "recorder/src/service/qmmf_recorder_common.h"
#include "recorder/src/service/qmmf_recorder_ion.h"

namespace qmmf {
namespace recorder {

using ::qmmf::avcodec::CodecPortStatus;
using ::qmmf::avcodec::PortEventType;
using ::qmmf::avcodec::PortreconfigData;
using ::std::chrono::seconds;
using ::std::mutex;
using ::std::queue;
using ::std::string;
using ::std::thread;
using ::std::unique_lock;
using ::std::vector;

//! Default number of audio buffers to allocate.
static const int kNumberOfBuffers = INPUT_MAX_COUNT;

AudioEncodedTrackSource::AudioEncodedTrackSource(const AudioTrackParams& params)
    : track_params_(params),
      pulse_client_(nullptr),
      buffer_size_(0),
      stop_called_(true),
      stop_notify_received_(true) {
  QMMF_DEBUG("%s() TRACE", __func__);
  QMMF_VERBOSE("%s() INPARAM: params[%s]", __func__,
               params.ToString().c_str());
}

AudioEncodedTrackSource::~AudioEncodedTrackSource() {
  QMMF_DEBUG("%s() TRACE", __func__);
}

/*!
 *  Sets up the data path between an instance of an AVCodec audio encoder and a
 *  newly created instance of an AudioPulseClient.  Connects to the audio
 *  service and configures the pulseaudio client based on the given parameters.
 *
 *  Allocates a set number of audio buffers, the size of each being determined
 *  by the AVCodec audio encoder.
 */
status_t AudioEncodedTrackSource::Init() {
  QMMF_DEBUG("%s() TRACE: track_id[%u]", __func__, track_params_.track_id);
  status_t result;

  if (pulse_client_ != nullptr) {
    QMMF_ERROR("%s() pulse client already exists", __func__);
    return ::android::ALREADY_EXISTS;
  }

  pulse_client_ = new AudioPulseClient;
  if (pulse_client_ == nullptr) {
    QMMF_ERROR("%s() could not instantiate pulse client", __func__);
    return ::android::NO_MEMORY;
  }

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

  result = pulse_client_->Connect(audio_handler);
  if (result < 0) {
    QMMF_ERROR("%s() pulseclient->Connect failed: %d[%s]", __func__,
               result, strerror(result));
    goto error_init_free;
  }

  result = pulse_client_->Configure(track_params_);
  if (result < 0) {
    QMMF_ERROR("%s() pulseclient->Configure failed: %d[%s]", __func__,
               result, strerror(result));
    goto error_init_disconnect;
  }

  if (strlen(track_params_.params.profile) > 0) {
    auto ret = SetParameter("audio_stream_profile",
                            track_params_.params.profile);
    if (ret != NO_ERROR) {
      QMMF_ERROR("%s: Failed to enable profile: %d", __func__, ret);
      goto error_init_disconnect;
    }
  }

  int32_t buffer_size;
  result = pulse_client_->GetBufferSize(&buffer_size);
  if (result < 0) {
    QMMF_ERROR("%s() pulseclient->GetBufferSize failed: %d[%s]", __func__,
               result, strerror(result));
    goto error_init_disconnect;
  }
  QMMF_INFO("%s() buffer_size is %d", __func__, buffer_size);
  buffer_size_ = buffer_size;

  result = ion_.Allocate(kNumberOfBuffers, buffer_size_);
  if (result < 0) {
    QMMF_ERROR("%s() ion->Allocate failed: %d[%s]", __func__, result,
               strerror(result));
    goto error_init_deallocate;
  }

  return ::android::NO_ERROR;

error_init_deallocate:
  ion_.Deallocate();

error_init_disconnect:
  pulse_client_->Disconnect();

error_init_free:
  delete pulse_client_;
  pulse_client_ = nullptr;

  return ::android::FAILED_TRANSACTION;
}

/*!
 *  Deallocates the audio buffers, disconnects from the pulseaudio service, and
 *  then destroys the pulseaudio client.
 */
status_t AudioEncodedTrackSource::DeInit() {
  QMMF_DEBUG("%s() TRACE: track_id[%u]", __func__, track_params_.track_id);
  status_t result;

  result = ion_.Deallocate();
  if (result < 0)
    QMMF_ERROR("%s() ion->Deallocate failed: %d[%s]", __func__,
               result, strerror(result));

  result = pulse_client_->Disconnect();
  if (result < 0)
    QMMF_ERROR("%s() pulseclient->Disconnect failed: %d[%s]", __func__,
               result, strerror(result));

  delete pulse_client_;
  pulse_client_ = nullptr;

  return ::android::NO_ERROR;
}

/*!
 *  Starts the data flow in the pulseaudio client and pushes the list of audio
 *  buffers to it. Initializes the state flags to indicate data is flowing.
 */
status_t AudioEncodedTrackSource::StartTrack() {
  QMMF_DEBUG("%s() TRACE: track_id[%u]", __func__, track_params_.track_id);
  vector<BufferDescriptor> initial_buffers;
  status_t result;

  result = pulse_client_->Start();
  if (result < 0) {
    QMMF_ERROR("%s() pulseclient->Start failed: %d[%s]", __func__,
               result, strerror(result));
    goto error_start_stop;
  }

  result = ion_.GetList(&initial_buffers);
  if (result < 0) {
    QMMF_ERROR("%s() ion->GetList failed: %d[%s]", __func__, result,
               strerror(result));
    goto error_start_stop;
  }

  result = pulse_client_->SendBuffers(initial_buffers);
  if (result < 0) {
    QMMF_ERROR("%s() pulseclient->SendBuffers failed: %d[%s]", __func__,
               result, strerror(result));
    goto error_start_stop;
  }

  stop_called_ = false;
  stop_notify_received_ = false;

  return ::android::NO_ERROR;

error_start_stop:
  StopTrack();

  return ::android::FAILED_TRANSACTION;
}

/*!
 *  Stops the data flow in the pulseaudio client and sets the state flag to
 *  indicate that stop has been called.
 */
status_t AudioEncodedTrackSource::StopTrack() {
  QMMF_DEBUG("%s() TRACE: track_id[%u]", __func__, track_params_.track_id);

  status_t result = pulse_client_->Stop();
  if (result < 0) {
    QMMF_ERROR("%s() pulseclient->Stop failed: %d[%s]", __func__,
               result, strerror(result));
    return ::android::FAILED_TRANSACTION;
  }

  stop_called_ = true;

  return ::android::NO_ERROR;
}

/*!
 *  Pauses the data flow in the pulseaudio client.
 */
status_t AudioEncodedTrackSource::PauseTrack() {
  QMMF_DEBUG("%s() TRACE: track_id[%u]", __func__, track_params_.track_id);

  status_t result = pulse_client_->Pause();
  if (result < 0)
    QMMF_ERROR("%s() pulseclient->Pause failed: %d[%s]", __func__,
               result, strerror(result));

  return result;
}

/*!
 *  Resumes the data flow in the pulseaudio client.
 */
status_t AudioEncodedTrackSource::ResumeTrack() {
  QMMF_DEBUG("%s() TRACE: track_id[%u]", __func__, track_params_.track_id);

  status_t result = pulse_client_->Resume();
  if (result < 0)
    QMMF_ERROR("%s() pulseclient->Resume failed: %d[%s]", __func__,
               result, strerror(result));

  return result;
}

/*!
 *  Passes the given parameter arguments to the pulseaudio client.
 */
status_t AudioEncodedTrackSource::SetParameter(const string& key,
                                               const string& value) {
  QMMF_VERBOSE("%s() INPARAM: key[%s]", __func__, key.c_str());
  QMMF_VERBOSE("%s() INPARAM: value[%s]", __func__, value.c_str());

  status_t result = pulse_client_->SetParam(key, value);
  if (result < 0)
    QMMF_ERROR("%s() pulseclient->SetParam failed: %d[%s]", __func__,
               result, strerror(result));

  return result;
}

/*!
 *  This method is stubbed out.
 *  @warning This should never be called.
 */
status_t AudioEncodedTrackSource::ReturnTrackBuffer(
    const std::vector<BnBuffer> &buffers) {
  QMMF_DEBUG("%s() TRACE: track_id[%u]", __func__, track_params_.track_id);
  for (const BnBuffer& buffer : buffers)
    QMMF_VERBOSE("%s() INPARAM: bn_buffer[%s]", __func__,
                 buffer.ToString().c_str());

  QMMF_ERROR("%s() unsupported operation", __func__);
  assert(0);

  return ::android::INVALID_OPERATION;
}

/*!
 *  Pops a buffer (filled with PCM audio) off of the buffer queue and send it to
 *  the audio encoder.  If this is the last buffer, then the audio encoder is
 *  notified by a return value of -1.
 *
 *  If the buffer queue is empty, waits for a buffer from the pulseaudio client
 *  to be pushed into the queue.
 */
status_t AudioEncodedTrackSource::GetBuffer(BufferDescriptor& buffer,
                                            void* client_data) {
  QMMF_DEBUG("%s() TRACE: track_id[%u]", __func__,
             track_params_.track_id);

  while (buffers_.empty() && !stop_notify_received_) {
    unique_lock<mutex> lk(mutex_);
    if (signal_.WaitFor(lk, seconds(1)) != 0)
      QMMF_WARN("%s() timed out on wait", __func__);
  }

  if (stop_notify_received_) {
    QMMF_WARN("%s() request for buffer after stop_notify", __func__);
    return ::android::NO_ERROR;
  }

  mutex_.lock();
  buffer = buffers_.front();
  buffers_.pop();
  mutex_.unlock();

  QMMF_VERBOSE("%s() OUTPARAM: buffer[%s]", __func__,
               buffer.ToString().c_str());
  if (buffer.flag & static_cast<uint32_t>(BufferFlags::kFlagEOS))
    return -1;
  else
    return ::android::NO_ERROR;
}

/*!
 *  Receives an empty buffer from the audio encoder and sends it to the
 *  pulseaudio client.
 *
 *  If stop has been called, then the buffer is discarded.
 */
status_t AudioEncodedTrackSource::ReturnBuffer(BufferDescriptor& buffer,
                                               void* client_data) {
  QMMF_DEBUG("%s() TRACE: track_id[%u]", __func__,
             track_params_.track_id);
  QMMF_VERBOSE("%s() INPARAM: buffer[%s]", __func__,
               buffer.ToString().c_str());
  int32_t result;

  if (stop_called_ == true) {
    QMMF_VERBOSE("%s() throwning away the buffer", __func__);
    return ::android::NO_ERROR;
  }

  buffer.buf_id = buffer.fd;
  buffer.capacity = buffer_size_;
  memset(buffer.data, 0x00, buffer.capacity);
  buffer.size = 0;
  buffer.offset = 0;
  buffer.timestamp = 0;

  result = pulse_client_->SendBuffers({buffer});
  if (result < 0 && !stop_called_) {
    QMMF_ERROR("%s() pulseclient->SendBuffers failed: %d[%s]",
               __func__, result, strerror(result));
    return ::android::FAILED_TRANSACTION;
  }

  return ::android::NO_ERROR;
}

/*!
 *  Receives any events from the input port of the audio encoder.  Used to
 *  synchronize with the audio encoder to ensure the last buffer is sent.
 *
 *  * The kPortStop event is used to stop sending buffers to audio encoder.
 *  * The kPortIdle event is used to safely flush the buffer queue.
 */
status_t AudioEncodedTrackSource::NotifyPortEvent(PortEventType event_type,
                                                  void* event_data) {
  QMMF_DEBUG("%s() TRACE: track_id[%u]", __func__, track_params_.track_id);
  if (event_type == PortEventType::kPortStatus) {
    CodecPortStatus status = *(static_cast<CodecPortStatus*>(event_data));
    switch (status) {
      case CodecPortStatus::kPortStop:
        if (stop_called_) {
          stop_notify_received_ = true;
        }
        break;
      case CodecPortStatus::kPortIdle:
        if (stop_notify_received_) {
          mutex_.lock();
          while (!buffers_.empty())
            buffers_.pop();
          mutex_.unlock();
          signal_.Signal();
          QMMF_VERBOSE("%s() emptied the buffer queue", __func__);
        }
        break;
      case CodecPortStatus::kPortStart:
        break;
    }
  }

  return ::android::NO_ERROR;
}

/*!
 *  @todo Currently not used.
 */
status_t AudioEncodedTrackSource::GetBufferSize(int32_t* buffer_size) {
  QMMF_DEBUG("%s() TRACE: track_id[%u]", __func__,
             track_params_.track_id);

  *buffer_size = buffer_size_;
  QMMF_VERBOSE("%s() OUTPARAM: buffer_size[%d]", __func__,
               *buffer_size);

  return ::android::NO_ERROR;
}

/*!
 *  @todo Currently not used.
 */
status_t AudioEncodedTrackSource::SetBufferSize(const int32_t buffer_size) {
  QMMF_DEBUG("%s() TRACE: track_id[%u]", __func__,
             track_params_.track_id);
  QMMF_VERBOSE("%s() INPARAM: buffer_size[%d]", __func__, buffer_size);

  buffer_size_ = buffer_size;

  return ::android::NO_ERROR;
}

/*!
 *  Logs the error from the pulseaudio client and asserts (crashing the service).
 *
 *  @todo Send notification to application instead of asserting.
 */
void AudioEncodedTrackSource::ErrorHandler(const int32_t error) {
  QMMF_DEBUG("%s() TRACE: track_id[%u]", __func__, track_params_.track_id);
  QMMF_VERBOSE("%s() INPARAM: type[%d]", __func__, error);

  QMMF_ERROR("%s() received error from pulse client: %d[%s]", __func__,
             error, strerror(error));
  assert(false);
}

/*!
 *  Receives a buffer (filled with PCM audio) from the pulseaudio client, maps
 *  it to the buffer descriptor and pushes it into the buffer queue.
 */
void AudioEncodedTrackSource::BufferHandler(const BufferDescriptor& buffer) {
  QMMF_DEBUG("%s() TRACE: track_id[%u]", __func__, track_params_.track_id);
  QMMF_VERBOSE("%s() INPARAM: buffer[%s]", __func__,
               buffer.ToString().c_str());

  mutex_.lock();
  buffers_.push(buffer);
  mutex_.unlock();
  signal_.Signal();
}

}; // namespace recorder
}; // namespace qmmf
