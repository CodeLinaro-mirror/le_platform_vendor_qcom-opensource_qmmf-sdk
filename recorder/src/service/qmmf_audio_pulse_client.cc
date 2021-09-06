/*
 * Copyright (c) 2019-2021, The Linux Foundation. All rights reserved.
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

//! @file qmmf_audio_pulse_client.cc

#define LOG_TAG "RecorderAudioPulseClient"

#include "recorder/src/service/qmmf_audio_pulse_client.h"

#include <cerrno>
#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

#include <sys/prctl.h>

#include "common/utils/qmmf_condition.h"
#include "common/utils/qmmf_device_internal.h"
#include "common/utils/qmmf_log.h"
#include "recorder/src/service/qmmf_recorder_common.h"

namespace qmmf {
namespace recorder {

using ::qmmf::BufferDescriptor;
using ::std::chrono::seconds;
using ::std::cv_status;
using ::std::function;
using ::std::lock_guard;
using ::std::mutex;
using ::std::queue;
using ::std::string;
using ::std::thread;
using ::std::to_string;
using ::std::unique_lock;
using ::std::vector;

const string AudioPulseClient::kTSAdjustName("persist.qmmf.timestamp.adjust");
const string AudioPulseClient::kPaClientName("QMMF-PA");
const string AudioPulseClient::kPaClientVersion("1.0.0");
const string AudioPulseClient::kPaStreamName("QMMF-PA:Capture");
const string AudioPulseClient::kPaMediaRole("music");
const size_t AudioPulseClient::kPaBinaryNameSize(256);
const size_t AudioPulseClient::kPaUserNameSize(256);
const size_t AudioPulseClient::kPaHostNameSize(256);
const size_t AudioPulseClient::kPaBufferLength(100); // ms
const size_t AudioPulseClient::kLatency(25000);

AudioPulseClient::AudioPulseClient()
    : pa_mainloop_(nullptr),
      pa_mainloop_api_(nullptr),
      pa_context_props_(nullptr),
      pa_context_(nullptr),
      pa_format_info_(nullptr),
      pa_stream_props_(nullptr),
      pa_stream_(nullptr),
      thread_(nullptr) {
  QMMF_DEBUG("%s() TRACE", __func__);
  QMMF_INFO("%s() client instantiated", __func__);
}

AudioPulseClient::~AudioPulseClient() {
  QMMF_DEBUG("%s() TRACE", __func__);
  QMMF_INFO("%s() client destroyed", __func__);
}

status_t AudioPulseClient::Connect(const AudioEventHandler& handler) {
  QMMF_DEBUG("%s() TRACE", __func__);
  char* string_ptr;
  int result;

  if (pa_mainloop_ != nullptr ||
      pa_mainloop_api_ != nullptr ||
      pa_context_ != nullptr) {
    QMMF_ERROR("%s() already connected to service", __func__);
    return -1;
  }

  event_handler_ = handler;

  // context state callback
  pa_context_notify_cb_t pa_context_state_cb =
      [](pa_context* ctx, void* userdata) -> void {
    QMMF_DEBUG("%s() TRACE", "pa_context_state_cb");
    assert(ctx != nullptr);
    assert(userdata != nullptr);
    pa_threaded_mainloop* ml = reinterpret_cast<pa_threaded_mainloop*>
                                               (userdata);

    switch (pa_context_get_state(ctx)) {
    case PA_CONTEXT_UNCONNECTED:
    case PA_CONTEXT_CONNECTING:
    case PA_CONTEXT_AUTHORIZING:
    case PA_CONTEXT_SETTING_NAME:
      // do nothing
      break;
    case PA_CONTEXT_READY:
    case PA_CONTEXT_FAILED:
    case PA_CONTEXT_TERMINATED:
      QMMF_DEBUG("%s() signalling back to mainloop", "pa_context_state_cb");
      pa_threaded_mainloop_signal(ml, 0);
      break;
    default:
      assert(false);
      break;
    }
  };

  // initialize pulseaudio mainloop
  pa_mainloop_ = pa_threaded_mainloop_new();
  if (pa_mainloop_ == nullptr) {
    QMMF_ERROR("%s() PA mainloop allocation failed", __func__);
    return -1;
  }
  pa_threaded_mainloop_set_name(pa_mainloop_,
                                (kPaClientName + "-Thread").c_str());
  QMMF_DEBUG("%s() PA mainloop[%s] allocated",
      __func__, (kPaClientName + "-Thread").c_str());

  pa_mainloop_api_ = pa_threaded_mainloop_get_api(pa_mainloop_);
  if (pa_mainloop_api_ == nullptr) {
    QMMF_ERROR("%s() PA mainloop API allocation failed", __func__);
    goto exit_ml;
  }
  QMMF_DEBUG("%s() PA mainloop API allocated", __func__);

  // create properties for pulseaudio context
  pa_context_props_ = pa_proplist_new();
  if (pa_context_props_ == nullptr) {
    QMMF_ERROR("%s() PA context proplist allocation failed", __func__);
    goto exit_ml;
  }

  result = pa_proplist_sets(pa_context_props_, PA_PROP_APPLICATION_NAME,
                            kPaClientName.c_str());
  if (result != 0) {
    QMMF_ERROR("%s() setting PA context property [%s=%s] failed: [%d]%s",
               __func__, PA_PROP_APPLICATION_NAME, kPaClientName.c_str(),
               result, pa_strerror(result));
    goto exit_ctx_prop;
  }

  result = pa_proplist_sets(pa_context_props_, PA_PROP_APPLICATION_VERSION,
                            kPaClientVersion.c_str());
  if (result != 0) {
    QMMF_ERROR("%s() setting PA context property [%s=%s] failed: [%d]%s",
               __func__, PA_PROP_APPLICATION_VERSION, kPaClientVersion.c_str(),
               result, pa_strerror(result));
    goto exit_ctx_prop;
  }

  result = pa_proplist_setf(pa_context_props_, PA_PROP_APPLICATION_PROCESS_ID,
                            "%d", getpid());
  if (result != 0) {
    QMMF_ERROR("%s() setting PA context property [%s=%s] failed: [%d]%s",
               __func__, PA_PROP_APPLICATION_PROCESS_ID, string_ptr, result,
               pa_strerror(result));
    goto exit_ctx_prop;
  }

  string_ptr = reinterpret_cast<char*>(pa_xmalloc0(kPaBinaryNameSize));
  string_ptr = pa_get_binary_name(string_ptr, kPaBinaryNameSize);
  result = pa_proplist_sets(pa_context_props_,
                            PA_PROP_APPLICATION_PROCESS_BINARY, string_ptr);
  if (result != 0) {
    QMMF_ERROR("%s() setting PA context property [%s=%s] failed: [%d]%s",
               __func__, PA_PROP_APPLICATION_PROCESS_BINARY, string_ptr, result,
               pa_strerror(result));
    pa_xfree(string_ptr);
    goto exit_ctx_prop;
  }
  pa_xfree(string_ptr);

  string_ptr = reinterpret_cast<char*>(pa_xmalloc0(kPaUserNameSize));
  string_ptr = pa_get_user_name(string_ptr, kPaUserNameSize);
  result = pa_proplist_sets(pa_context_props_, PA_PROP_APPLICATION_PROCESS_USER,
                            string_ptr);
  if (result != 0) {
    QMMF_ERROR("%s() setting PA context property [%s=%s] failed: [%d]%s",
               __func__, PA_PROP_APPLICATION_PROCESS_USER, string_ptr, result,
               pa_strerror(result));
    pa_xfree(string_ptr);
    goto exit_ctx_prop;
  }
  pa_xfree(string_ptr);

  string_ptr = reinterpret_cast<char*>(pa_xmalloc0(kPaHostNameSize));
  string_ptr = pa_get_host_name(string_ptr, kPaHostNameSize);
  result = pa_proplist_sets(pa_context_props_, PA_PROP_APPLICATION_PROCESS_HOST,
                            string_ptr);
  if (result != 0) {
    QMMF_ERROR("%s() setting PA context property [%s=%s] failed: [%d]%s",
               __func__, PA_PROP_APPLICATION_PROCESS_HOST, string_ptr, result,
               pa_strerror(result));
    pa_xfree(string_ptr);
    goto exit_ctx_prop;
  }
  pa_xfree(string_ptr);

  string_ptr = pa_proplist_to_string_sep(pa_context_props_, " | ");
  QMMF_DEBUG("%s() PA context proplist allocated: %s", __func__, string_ptr);
  pa_xfree(string_ptr);

  // initialize pulseaudio context
  pa_context_ = pa_context_new_with_proplist(pa_mainloop_api_,
                                             kPaClientName.c_str(),
                                             pa_context_props_);
  if (pa_context_ == nullptr) {
    QMMF_ERROR("%s() PA context allocation failed", __func__);
    goto exit_ctx_prop;
  }
  pa_context_set_state_callback(pa_context_, pa_context_state_cb, pa_mainloop_);
  QMMF_DEBUG("%s() PA context allocated", __func__);

  // initiate connection of pulseaudio context to server
  result = pa_context_connect(pa_context_, nullptr, PA_CONTEXT_NOAUTOSPAWN,
                              nullptr);
  if (result < 0) {
    result = pa_context_errno(pa_context_);
    QMMF_ERROR("%s() PA context failed to connect to server: [%d]%s",
               __func__, result, pa_strerror(result));
    goto exit_ctx;
  }
  QMMF_DEBUG("%s() PA context initiating connection to server", __func__);

  pa_threaded_mainloop_lock(pa_mainloop_);

  // start the pulseaudio mainloop
  result = pa_threaded_mainloop_start(pa_mainloop_);
  if (result < 0) {
    QMMF_ERROR("%s() PA mainloop thread failed to start: [%d]%s",
               __func__, result, pa_strerror(result));
    pa_threaded_mainloop_unlock(pa_mainloop_);
    goto exit_ctx_conn;
  }
  QMMF_DEBUG("%s() PA mainloop has been started", __func__);

  // wait until the pulseaudio context is connected to server
  while (true) {
    pa_context_state_t state = pa_context_get_state(pa_context_);

    if (state == PA_CONTEXT_READY)
      break;
    if (!PA_CONTEXT_IS_GOOD(state)) {
      result = pa_context_errno(pa_context_);
      QMMF_ERROR("%s() PA context failed to connect: [%d]%s",
                 __func__, result, pa_strerror(result));
      goto exit_ml_start;
    }

    pa_threaded_mainloop_wait(pa_mainloop_);
  }
  QMMF_DEBUG("%s() PA context connected to server", __func__);

  pa_threaded_mainloop_unlock(pa_mainloop_);

  return 0;

exit_ml_start:
  if (pa_mainloop_ != nullptr)
    pa_threaded_mainloop_stop(pa_mainloop_);
exit_ctx_conn:
  if (pa_context_ != nullptr)
    pa_context_disconnect(pa_context_);
  pa_threaded_mainloop_unlock(pa_mainloop_);
exit_ctx:
  if (pa_context_ != nullptr) {
    pa_context_unref(pa_context_);
    pa_context_ = nullptr;
  }
exit_ctx_prop:
  if (pa_context_props_ != nullptr) {
    pa_proplist_free(pa_context_props_);
    pa_context_props_ = nullptr;
  }
exit_ml:
  if (pa_mainloop_ != nullptr) {
    pa_threaded_mainloop_free(pa_mainloop_);
    pa_mainloop_api_ = nullptr;
    pa_mainloop_ = nullptr;
  }

  return -1;
}

status_t AudioPulseClient::Disconnect() {
  QMMF_DEBUG("%s() TRACE", __func__);

  if (pa_mainloop_ == nullptr && pa_mainloop_api_ == nullptr &&
      pa_context_ == nullptr) {
    QMMF_ERROR("%s() not connected to service", __func__);
    return -1;
  }

  if (pa_mainloop_ != nullptr)
    pa_threaded_mainloop_stop(pa_mainloop_);

  if (pa_stream_ != nullptr) {
    pa_stream_unref(pa_stream_);
    pa_stream_ = nullptr;
  }

  if (pa_stream_props_ != nullptr) {
    pa_proplist_free(pa_stream_props_);
    pa_stream_props_ = nullptr;
  }

  if (pa_format_info_ != nullptr) {
    pa_format_info_free(pa_format_info_);
    pa_format_info_ = nullptr;
  }

  if (pa_context_ != nullptr)
    pa_context_disconnect(pa_context_);

  if (pa_context_ != nullptr) {
    pa_context_unref(pa_context_);
    pa_context_ = nullptr;
  }

  if (pa_context_props_ != nullptr) {
    pa_proplist_free(pa_context_props_);
    pa_context_props_ = nullptr;
  }

  if (pa_mainloop_ != nullptr) {
    pa_threaded_mainloop_free(pa_mainloop_);
    pa_mainloop_api_ = nullptr;
    pa_mainloop_ = nullptr;
  }

  return 0;
}

status_t AudioPulseClient::Configure(const uint32_t track_id,
                                     const AudioTrackParam& params,
                                     const BnBufferCallback& cb) {
  QMMF_DEBUG("%s() TRACE", __func__);
  QMMF_VERBOSE("%s() INPARAM: params[%s]", __func__, params.ToString().c_str());
  PaStreamSuccessCbData cb_data;
  const pa_buffer_attr* conf_attr;
  pa_operation *op;
  char* string_ptr;
  string stream_name;
  int result;

  if (pa_mainloop_ == nullptr || pa_mainloop_api_ == nullptr ||
      pa_context_ == nullptr) {
    QMMF_ERROR("%s() not connected to service", __func__);
    return -1;
  }

  // stream state callback
  pa_stream_notify_cb_t pa_stream_state_cb =
      [](pa_stream* stream, void* userdata) -> void {
    assert(stream != NULL);
    assert(userdata != NULL);
    pa_threaded_mainloop* ml = reinterpret_cast<pa_threaded_mainloop*>
                                               (userdata);

    switch (pa_stream_get_state(stream)) {
    case PA_STREAM_UNCONNECTED:
    case PA_STREAM_CREATING:
        // do nothing
        break;
    case PA_STREAM_READY:
    case PA_STREAM_FAILED:
    case PA_STREAM_TERMINATED:
        pa_threaded_mainloop_signal(ml, 0);
        break;
    default:
        assert(false);
        break;
    }
  };

  // stream read callback
  pa_stream_request_cb_t pa_stream_read_cb =
      [](pa_stream* stream, size_t nbytes, void* userdata) -> void {
    assert(stream != NULL);
    assert(userdata != NULL);
    pa_threaded_mainloop* ml = reinterpret_cast<pa_threaded_mainloop*>
                                               (userdata);

    pa_threaded_mainloop_signal(ml, 0);
  };

  // stream latency update callback
  pa_stream_notify_cb_t pa_stream_latency_update_cb =
      [](pa_stream* stream, void* userdata) -> void {
    assert(stream != NULL);
    assert(userdata != NULL);
    PaStreamLatencyCbData* data = reinterpret_cast<PaStreamLatencyCbData*>
                                                  (userdata);

    // save the timing info from pulseaudio
    lock_guard<mutex> lock(data->timing_info_mutex);
    data->timing_info = *pa_stream_get_timing_info(stream);
    data->timing_info_valid = true;
  };

  // stream success callback
  pa_stream_success_cb_t pa_stream_success_cb =
      [](pa_stream* stream, int success, void* userdata) -> void {
    assert(stream != NULL);
    assert(userdata != NULL);
    PaStreamSuccessCbData* data = reinterpret_cast<PaStreamSuccessCbData*>
                                                  (userdata);

    if (success != 0)
      data->success = true;
    else
      data->success = false;
    pa_threaded_mainloop_signal(data->mainloop, 0);
  };

  // initialize the pulseaudio format info for the stream
  pa_format_info_ = pa_format_info_new();
  pa_format_info_->encoding = PA_ENCODING_PCM;
  switch (params.bit_depth) {
    case 16:
      pa_format_info_set_sample_format(pa_format_info_, PA_SAMPLE_S16LE);
      break;
    case 24:
      pa_format_info_set_sample_format(pa_format_info_, PA_SAMPLE_S24LE);
      break;
    case 32:
      pa_format_info_set_sample_format(pa_format_info_, PA_SAMPLE_S32LE);
      break;
    default:
      QMMF_ERROR("%s() invalid sample size: %d", __func__, params.bit_depth);
      goto exit_format;
  }
  pa_format_info_set_rate(pa_format_info_, params.sample_rate);
  pa_format_info_set_channels(pa_format_info_, params.channels);

  if (!pa_format_info_valid(pa_format_info_) ||
          !pa_format_info_is_pcm(pa_format_info_)) {
    QMMF_ERROR("%s() created PA format info is not valid PCM: size[%d] rate[%d] channels[%d]",
               __func__, params.bit_depth, params.sample_rate, params.channels);
    goto exit_format;
  }

  string_ptr = reinterpret_cast<char*>(pa_xmalloc0(PA_FORMAT_INFO_SNPRINT_MAX));
  QMMF_DEBUG("%s() PA format info set to [%s]", __func__,
             pa_format_info_snprint(string_ptr, PA_FORMAT_INFO_SNPRINT_MAX,
                                    pa_format_info_));
  pa_xfree(string_ptr);

  // initialize the pulseaudio sample spec for the stream
  pa_sample_spec_init(&pa_sample_spec_);
  result = pa_format_info_to_sample_spec(pa_format_info_, &pa_sample_spec_,
                                         nullptr);
  if (result != 0) {
    QMMF_ERROR("%s() PA stream failed to convert to spec: [%d]%s",
               __func__, result, pa_strerror(result));
    goto exit_format;
  }

  string_ptr = reinterpret_cast<char*>(pa_xmalloc0(PA_SAMPLE_SPEC_SNPRINT_MAX));
  QMMF_DEBUG("%s() PA sample spec set to [%s]", __func__,
             pa_sample_spec_snprint(string_ptr, PA_SAMPLE_SPEC_SNPRINT_MAX,
                                    &pa_sample_spec_));
  pa_xfree(string_ptr);

  // create properties for pulseaudio stream
  pa_stream_props_ = pa_proplist_new();
  if (pa_stream_props_ == nullptr) {
    QMMF_ERROR("%s() PA stream proplist allocation failed", __func__);
    goto exit_format;
  }

  pa_threaded_mainloop_lock(pa_mainloop_);

  result = pa_proplist_sets(pa_stream_props_, PA_PROP_MEDIA_ROLE,
                            kPaMediaRole.c_str());
  if (result != 0) {
    QMMF_ERROR("%s() setting PA stream property failed: %s=%s",
               __func__, PA_PROP_MEDIA_ROLE, kPaMediaRole.c_str());
    goto exit_stream_prop;
  }

  string_ptr = pa_proplist_to_string_sep(pa_stream_props_, " | ");
  QMMF_DEBUG("%s() PA stream proplist allocated: %s", __func__, string_ptr);
  pa_xfree(string_ptr);

  // initialize pulseaudio stream
  stream_name = kPaStreamName + "(";
  stream_name += "Client " + to_string((track_id >> 24) & 0xFF) + ",";
  stream_name += "Session " + to_string((track_id >> 16) & 0xFF) + ",";
  stream_name += "Track " + to_string(track_id & 0xFFFF) + ")";
  pa_stream_ = pa_stream_new_extended(pa_context_, stream_name.c_str(),
                                      &pa_format_info_, 1, pa_stream_props_);
  if (pa_stream_ == nullptr) {
    result = pa_context_errno(pa_context_);
    QMMF_ERROR("%s() failed to create new stream: [%d]%s",
               __func__, result, pa_strerror(result));
    goto exit_stream_prop;
  }
  pa_stream_set_state_callback(pa_stream_, pa_stream_state_cb, pa_mainloop_);
  pa_stream_set_read_callback(pa_stream_, pa_stream_read_cb, pa_mainloop_);
  pa_latency_data_.timing_info_valid = false;
  pa_stream_set_latency_update_callback(pa_stream_,
                                        pa_stream_latency_update_cb,
                                        &pa_latency_data_);
  QMMF_DEBUG("%s() PA stream [%s] created", __func__, kPaStreamName.c_str());

  // set the pulseaudio buffering attributes
  pa_buffer_attr req_attr;
  req_attr.maxlength = pa_usec_to_bytes(kPaBufferLength * PA_USEC_PER_MSEC,
                                        &pa_sample_spec_);
  req_attr.tlength = static_cast<uint32_t>(-1);
  req_attr.prebuf = static_cast<uint32_t>(-1);
  req_attr.minreq = static_cast<uint32_t>(-1);
  req_attr.fragsize = static_cast<uint32_t>(-1);
  QMMF_DEBUG("%s() PA stream requested buffer attr: maxlength[%u][%" PRIu64 " usec]",
             __func__, req_attr.maxlength,
             pa_bytes_to_usec(req_attr.maxlength, &pa_sample_spec_));
  QMMF_DEBUG("%s() PA stream requested buffer attr: tlength[%u][%" PRIu64 " usec]",
             __func__, req_attr.tlength,
             pa_bytes_to_usec(req_attr.tlength, &pa_sample_spec_));
  QMMF_DEBUG("%s() PA stream requested buffer attr: prebuf[%u][%" PRIu64 " usec]",
             __func__, req_attr.prebuf,
             pa_bytes_to_usec(req_attr.prebuf, &pa_sample_spec_));
  QMMF_DEBUG("%s() PA stream requested buffer attr: minreq[%u][%" PRIu64 " usec]",
             __func__, req_attr.minreq,
             pa_bytes_to_usec(req_attr.minreq, &pa_sample_spec_));
  QMMF_DEBUG("%s() PA stream requested buffer attr: fragsize[%u][%" PRIu64 " usec]",
             __func__, req_attr.fragsize,
             pa_bytes_to_usec(req_attr.fragsize, &pa_sample_spec_));

  // initiate connection of pulseaudio stream to server
  result = pa_stream_connect_record(pa_stream_, nullptr, &req_attr,
                                    static_cast<pa_stream_flags_t>
                                               (PA_STREAM_AUTO_TIMING_UPDATE));
  if (result != 0) {
    result = pa_context_errno(pa_context_);
    QMMF_ERROR("%s() PA stream failed to connect: [%d]%s",
               __func__, result, pa_strerror(result));
    goto exit_stream;
  }

  // wait until the pulseaudio stream is connected to server
  while (true) {
    pa_stream_state_t state = pa_stream_get_state(pa_stream_);

    if (state == PA_STREAM_READY)
      break;
    if (!PA_STREAM_IS_GOOD(state)) {
      result = pa_context_errno(pa_context_);
      QMMF_ERROR("%s() PA stream failed to connect: [%d]%s",
                 __func__, result, pa_strerror(result));
      goto exit_stream;
    }

    pa_threaded_mainloop_wait(pa_mainloop_);
  }
  QMMF_DEBUG("%s() PA stream connected to server", __func__);

  // get the configured pulseaudio buffering attributes
  conf_attr = pa_stream_get_buffer_attr(pa_stream_);
  QMMF_DEBUG("%s() PA stream configured buffer attr: maxlength[%u][%" PRIu64 " usec]",
             __func__, conf_attr->maxlength,
             pa_bytes_to_usec(conf_attr->maxlength, &pa_sample_spec_));
  QMMF_DEBUG("%s() PA stream configured buffer attr: tlength[%u][%" PRIu64 " usec]",
             __func__, conf_attr->tlength,
             pa_bytes_to_usec(conf_attr->tlength, &pa_sample_spec_));
  QMMF_DEBUG("%s() PA stream configured buffer attr: prebuf[%u][%" PRIu64 " usec]",
             __func__, conf_attr->prebuf,
             pa_bytes_to_usec(conf_attr->prebuf, &pa_sample_spec_));
  QMMF_DEBUG("%s() PA stream configured buffer attr: minreq[%u][%" PRIu64 " usec]",
             __func__, conf_attr->minreq,
             pa_bytes_to_usec(conf_attr->minreq, &pa_sample_spec_));
  QMMF_DEBUG("%s() PA stream configured buffer attr: fragsize[%u][%" PRIu64 " usec]",
             __func__, conf_attr->fragsize,
             pa_bytes_to_usec(conf_attr->fragsize, &pa_sample_spec_));

  // register the buffer size
  buffer_size_ = conf_attr->fragsize / 2;
  QMMF_DEBUG("%s() PA stream buffer size set to %d bytes",
             __func__, buffer_size_);

  // initiate the request for the initial timing info
  cb_data = { false, pa_mainloop_ };
  op = pa_stream_update_timing_info(pa_stream_, pa_stream_success_cb,
                                    &cb_data);
  if (op == nullptr) {
    result = pa_context_errno(pa_context_);
    QMMF_ERROR("%s() PA stream failed to request timing info: [%d]%s",
               __func__, result, pa_strerror(result));
    goto exit_stream;
  }

  // wait for the initial timing info request to finish
  while (pa_operation_get_state(op) == PA_OPERATION_RUNNING) {
    pa_threaded_mainloop_wait(pa_mainloop_);
    result = ValidatePaStream();
    if (result != 0) {
      QMMF_ERROR("%s() PA stream died during timing info request: [%d]%s",
                 __func__, result, pa_strerror(result));
      goto exit_timing_info;
    }
  }

  // check the result of the initial timing info request
  if (!cb_data.success) {
    result = pa_context_errno(pa_context_);
    QMMF_ERROR("%s() PA stream failed to complete timing info request: [%d]%s",
               __func__, result, pa_strerror(result));
    goto exit_timing_info;
  }
  pa_operation_unref(op);
  QMMF_DEBUG("%s() PA stream initial timing info has been received", __func__);

  pa_threaded_mainloop_unlock(pa_mainloop_);

  return 0;

exit_timing_info:
  if (op != nullptr) {
    pa_operation_unref(op);
    op = nullptr;
  }
exit_stream:
  if (pa_stream_ != nullptr) {
    pa_stream_unref(pa_stream_);
    pa_stream_ = nullptr;
  }
exit_stream_prop:
  if (pa_stream_props_ != nullptr) {
    pa_proplist_free(pa_stream_props_);
    pa_stream_props_ = nullptr;
  }
  pa_threaded_mainloop_unlock(pa_mainloop_);
exit_format:
  if (pa_format_info_ != nullptr) {
    pa_format_info_free(pa_format_info_);
    pa_format_info_ = nullptr;
  }

  return -1;
}

status_t AudioPulseClient::Start() {
  QMMF_DEBUG("%s() TRACE", __func__);

  if (pa_mainloop_ == nullptr || pa_mainloop_api_ == nullptr ||
      pa_context_ == nullptr) {
    QMMF_ERROR("%s() not connected to service", __func__);
    return -1;
  }

  while (!messages_.empty())
    messages_.pop();

  thread_ = new thread(AudioPulseClient::ThreadEntry, this);
  if (thread_ == nullptr) {
    QMMF_ERROR("%s() unable to allocate thread", __func__);
    return -1;
  }

  return 0;
}

status_t AudioPulseClient::Stop() {
  QMMF_DEBUG("%s() TRACE", __func__);

  if (pa_mainloop_ == nullptr || pa_mainloop_api_ == nullptr ||
      pa_context_ == nullptr) {
    QMMF_ERROR("%s() not connected to service", __func__);
    return -1;
  }

  AudioMessage message;
  message.type = AudioMessageType::kMessageStop;

  message_lock_.lock();
  messages_.push(message);
  message_lock_.unlock();
  signal_.Signal();

  thread_->join();
  delete thread_;

  while (!messages_.empty())
    messages_.pop();

  return 0;
}

status_t AudioPulseClient::Pause() {
  QMMF_DEBUG("%s() TRACE", __func__);

  if (pa_mainloop_ == nullptr || pa_mainloop_api_ == nullptr ||
      pa_context_ == nullptr) {
    QMMF_ERROR("%s() not connected to service", __func__);
    return -1;
  }

  AudioMessage message;
  message.type = AudioMessageType::kMessagePause;

  message_lock_.lock();
  messages_.push(message);
  message_lock_.unlock();
  signal_.Signal();

  return 0;
}

status_t AudioPulseClient::Resume() {
  QMMF_DEBUG("%s() TRACE", __func__);

  if (pa_mainloop_ == nullptr || pa_mainloop_api_ == nullptr ||
      pa_context_ == nullptr) {
    QMMF_ERROR("%s() not connected to service", __func__);
    return -1;
  }

  AudioMessage message;
  message.type = AudioMessageType::kMessageResume;

  message_lock_.lock();
  messages_.push(message);
  message_lock_.unlock();
  signal_.Signal();

  return 0;
}

status_t AudioPulseClient::SendBuffers(const vector<BufferDescriptor>& buffers) {
  QMMF_DEBUG("%s() TRACE", __func__);
  for (const BufferDescriptor& buffer : buffers)
    QMMF_VERBOSE("%s() INPARAM: buffer[%s]", __func__,
                 buffer.ToString().c_str());

  if (pa_mainloop_ == nullptr || pa_mainloop_api_ == nullptr ||
      pa_context_ == nullptr) {
    QMMF_ERROR("%s() not connected to service", __func__);
    return -1;
  }

  AudioMessage message;
  message.type = AudioMessageType::kMessageBuffer;
  for (const BufferDescriptor& buffer : buffers)
    message.buffers.push_back(buffer);

  message_lock_.lock();
  messages_.push(message);
  message_lock_.unlock();
  signal_.Signal();

  return 0;
}

status_t AudioPulseClient::GetBufferSize(int32_t* buffer_size) {
  QMMF_DEBUG("%s() TRACE", __func__);

  if (pa_mainloop_ == nullptr || pa_mainloop_api_ == nullptr ||
      pa_context_ == nullptr) {
    QMMF_ERROR("%s() not connected to service", __func__);
    return -1;
  }

  *buffer_size = buffer_size_;

  QMMF_VERBOSE("%s() OUTPARAM: buffer_size[%d]", __func__, *buffer_size);
  return 0;
}

status_t AudioPulseClient::SetParam(const string& key, const string& value) {
  QMMF_DEBUG("%s() TRACE", __func__);
  QMMF_VERBOSE("%s() INPARAM: key[%s]", __func__, key.c_str());
  QMMF_VERBOSE("%s() INPARAM: value[%s]", __func__, value.c_str());

  if (pa_mainloop_ == nullptr || pa_mainloop_api_ == nullptr ||
      pa_context_ == nullptr) {
    QMMF_ERROR("%s() not connected to service", __func__);
    return -1;
  }

  // TODO: implement SetParam feature
  QMMF_WARN("%s() operation not yet implemented", __func__);
  return 0;
}

int AudioPulseClient::ValidatePaStream() {
  QMMF_DEBUG("%s() TRACE", __func__);

  if (pa_context_ == nullptr || pa_stream_ == nullptr) {
    return PA_ERR_BADSTATE;
  } else {
    if (!PA_CONTEXT_IS_GOOD(pa_context_get_state(pa_context_)) ||
        !PA_STREAM_IS_GOOD(pa_stream_get_state(pa_stream_))) {
      if (pa_context_get_state(pa_context_) == PA_CONTEXT_FAILED ||
          pa_stream_get_state(pa_stream_) == PA_STREAM_FAILED)
        return pa_context_errno(pa_context_);
      else
        return PA_ERR_BADSTATE;
    } else {
      return 0;
    }
  }
}

void AudioPulseClient::NotifyErrorEvent(const int32_t error) {
  QMMF_DEBUG("%s() TRACE", __func__);
  QMMF_VERBOSE("%s() INPARAM: error[%d]", __func__, error);

  event_handler_(AudioEventType::kError, AudioEventData(error));
}

void AudioPulseClient::NotifyBufferEvent(const BufferDescriptor& buffer) {
  QMMF_DEBUG("%s() TRACE", __func__);
  QMMF_VERBOSE("%s() INPARAM: buffer[%s]", __func__, buffer.ToString().c_str());

  event_handler_(AudioEventType::kBuffer, AudioEventData(buffer));
}

void AudioPulseClient::ThreadEntry(AudioPulseClient* pulse_client) {
  QMMF_DEBUG("%s() TRACE", __func__);
  prctl(PR_SET_NAME, "AudioPulseCTh", 0, 0, 0);
  pulse_client->Thread();
}

void AudioPulseClient::Thread() {
  QMMF_DEBUG("%s() TRACE", __func__);
  queue<BufferDescriptor> buffers;
  bool first_buffer_read = true;
  bool keep_running = true;
  bool stop_received = false;
  bool paused = false;
  const void* pa_data = nullptr;
  size_t pa_index = 0;
  size_t pa_length = 0;
  size_t length;
  size_t b_length;
  void* b_data;
  int result;
  uint64_t tracking_clock;

  char adjust_string[PROPERTY_VALUE_MAX];
  property_get(kTSAdjustName.c_str(), adjust_string, "0");
  int64_t adjustment_timestamp = atol(adjust_string);
  QMMF_VERBOSE("%s() value of timestamp adjustment property[%lld]",
               __func__, adjustment_timestamp);
  adjustment_timestamp -= kLatency;
  QMMF_VERBOSE("%s() value of timestamp adjustment[%lld] after subtracting latency[%lld]",
               __func__, adjustment_timestamp, static_cast<int64_t>(kLatency));

  while (keep_running) {
    // wait until there is something to do
    if (buffers.empty() && messages_.empty()) {
      unique_lock<mutex> lk(message_lock_);
      signal_.Wait(lk);
    }

    // process the next pending message
    message_lock_.lock();
    if (!messages_.empty()) {
      AudioMessage message = messages_.front();

      switch (message.type) {
        case AudioMessageType::kMessagePause:
          QMMF_DEBUG("%s-MessagePause() TRACE", __func__);
          paused = true;
          break;

        case AudioMessageType::kMessageResume:
          QMMF_DEBUG("%s-MessageResume() TRACE", __func__);
          paused = false;
          break;

        case AudioMessageType::kMessageStop:
          QMMF_DEBUG("%s-MessageStop() TRACE", __func__);
          paused = false;
          stop_received = true;
          break;

        case AudioMessageType::kMessageBuffer:
          QMMF_DEBUG("%s-MessageBuffer() TRACE", __func__);
          for (const BufferDescriptor& buffer : message.buffers) {
            QMMF_VERBOSE("%s() INPARAM: buffer[%s] to queue[%u]",
                         __func__, buffer.ToString().c_str(),
                         buffers.size());
            buffers.push(buffer);
            QMMF_VERBOSE("%s() buffers queue is now %u deep",
                         __func__, buffers.size());
          }
          break;
      }

      messages_.pop();
    }
    message_lock_.unlock();

    // process the next pending buffer
    if (!buffers.empty() && !paused && keep_running) {
      BufferDescriptor& buffer = buffers.front();
      QMMF_VERBOSE("%s() processing next buffer[%s] from queue[%u]",
                   __func__, buffer.ToString().c_str(), buffers.size());

      pa_threaded_mainloop_lock(pa_mainloop_);
      result = ValidatePaStream();
      if (result != 0) {
          QMMF_ERROR("%s() PA stream died during lock: [%d]%s",
                     __func__, result, pa_strerror(result));
          goto exit_thread;
      }

      size_t b_length = buffer.capacity;
      void* b_data = buffer.data;
      while (b_length > 0) {
        while (pa_data == nullptr) {
          result = pa_stream_peek(pa_stream_, &pa_data, &pa_length);
          if (result != 0) {
              result = pa_context_errno(pa_context_);
              QMMF_ERROR("%s() write to PA stream failed: [%d]%s",
                         __func__, result, pa_strerror(result));
              goto exit_thread;
          }

          if (pa_length <= 0) {
            // wait for data to become available
            pa_threaded_mainloop_wait(pa_mainloop_);
            result = ValidatePaStream();
            if (result != 0) {
                QMMF_ERROR("%s() PA stream died during read: [%d]%s",
                           __func__, result, pa_strerror(result));
                goto exit_thread;
            }
          } else if (pa_data == nullptr) {
            // there's a hole in the stream, skip it
            result = pa_stream_drop(pa_stream_);
            if (result != 0) {
              result = pa_context_errno(pa_context_);
              QMMF_ERROR("%s() data drop from PA stream failed: [%d]%s",
                         __func__, result, pa_strerror(result));
              goto exit_thread;
            }
          } else {
            pa_index = 0;
          }
        }

        length = pa_length < b_length ? pa_length : b_length;
        memcpy(b_data, reinterpret_cast<const uint8_t*>(pa_data) + pa_index,
               length);

        b_data = reinterpret_cast<uint8_t*>(b_data) + length;
        b_length -= length;
        buffer.size += length;

        pa_index += length;
        pa_length -= length;

        if (pa_length == 0) {
          result = pa_stream_drop(pa_stream_);
          pa_data = nullptr;
          pa_length = 0;
          pa_index = 0;
          if (result != 0) {
            result = pa_context_errno(pa_context_);
            QMMF_ERROR("%s() data drop from PA stream failed: [%d]%s",
                       __func__, result, pa_strerror(result));
            goto exit_thread;
          }
        }
      }

      pa_threaded_mainloop_unlock(pa_mainloop_);

      // return timestamped buffer to client
      if (first_buffer_read) {
        struct timespec tv;
        clock_gettime(CLOCK_BOOTTIME, &tv);
        int64_t boottime = (int64_t)(tv.tv_sec) * 1000000 +
                           (int64_t)(tv.tv_nsec) / 1000;
        QMMF_VERBOSE("%s() adding current boot time[%lld] to timestamp adjustment",
                     __func__, boottime);
        adjustment_timestamp += boottime;
        QMMF_VERBOSE("%s() final timestamp adjustment[%lld]", __func__,
                     adjustment_timestamp);
        tracking_clock = 0;
        first_buffer_read = false;
      } else {
        tracking_clock += pa_bytes_to_usec(buffer.size, &pa_sample_spec_);
      }
      buffer.timestamp = tracking_clock + adjustment_timestamp;

      if (stop_received) {
        QMMF_DEBUG("%s() setting EOS flag", __func__);
        buffer.flags |= static_cast<uint32_t>(BufferFlags::kFlagEOS);
        keep_running = false;
      } else {
        buffer.flags = 0;
      }

      NotifyBufferEvent(buffer);
      buffers.pop();
      QMMF_VERBOSE("%s() buffers queue is now %u deep",
                   __func__, buffers.size());
    }
  }
  return;

exit_thread:
  NotifyErrorEvent(result);
  pa_threaded_mainloop_unlock(pa_mainloop_);
}

}; // namespace recorder
}; // namespace qmmf
