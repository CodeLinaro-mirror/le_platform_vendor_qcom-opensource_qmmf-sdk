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

#define TAG "RecorderEncoderCore"

#include "qmmf_encoder_core.h"

namespace qmmf {

namespace recorder {

EncoderCore* EncoderCore::instance_ = NULL;

EncoderCore* EncoderCore::CreateEncoderCore() {

  if(!instance_) {
    instance_ = new EncoderCore;
    if(!instance_) {
      QMMF_ERROR("%s:%s: Can't Create EncoderCore Instance", TAG, __func__);
      return nullptr;
    }
  }
  QMMF_INFO("%s:%s: EncoderCore Instance Created Successfully(0x%x)", TAG,
      __func__, instance_);
  return instance_;
}

EncoderCore::EncoderCore() {

  QMMF_INFO("%s:%s: Enter", TAG, __func__);
  QMMF_INFO("%s:%s: Exit", TAG, __func__);
}

EncoderCore::~EncoderCore() {

  QMMF_INFO("%s:%s: Enter", TAG, __func__);
  if (!track_encoders_.isEmpty()) {
    track_encoders_.clear();
  }
  instance_ = NULL;
  QMMF_INFO("%s:%s: Exit", TAG, __func__);
}

status_t EncoderCore::AddSource(const sp<TrackSource>& track_source,
                                const VideoTrackParams& params) {

  QMMF_LEVEL1("%s:%s: Enter", TAG, __func__);
  assert(track_source.get() != nullptr);

  sp<TrackEncoder> track_encoder = new TrackEncoder();
  if (!track_encoder.get()) {
    QMMF_ERROR("%s:%s: track_id(%d) Can't instantiate TrackEncoder", TAG,
        __func__, params.track_id);
    return NO_MEMORY;
  }

  auto ret = track_encoder->Init(track_source, params);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s:%s: track_id(%d) TrackEncoder Init failed!", TAG, __func__,
        params.track_id);
    return BAD_VALUE;
  }

  track_encoders_.add(params.track_id, track_encoder);
  QMMF_INFO("%s:%s: TrackEncoder(0x%x) for track_id(%d) Instantiated!",
      track_encoder.get(), params.track_id);

  QMMF_LEVEL1("%s:%s: Exit", TAG, __func__);
  return ret;
}

status_t EncoderCore::StartTrackEncoder(uint32_t track_id) {

  QMMF_LEVEL1("%s:%s: Enter track_id(%d)", TAG, __func__, track_id);
  if (!isTrackValid(track_id)) {
    QMMF_ERROR("%s:%s: Invalid track_id(%d)", TAG, __func__, track_id);
    return BAD_VALUE;
  }
  sp<TrackEncoder> track_encoder = track_encoders_.valueFor(track_id);
  assert(track_encoder.get() != NULL);

  auto ret = track_encoder->Start();
  if (ret != NO_ERROR) {
    QMMF_INFO("%s:%s: track_id(%d) TrackEncoder Start failed!", TAG, __func__,
      track_id);
    return ret;
  }
  // Initial debug purpose.
  assert(ret == NO_ERROR);
  QMMF_INFO("%s:%s: track_id(%d) TrackEncoder Started Successfully!", TAG,
      __func__);
  QMMF_LEVEL1("%s:%s: Exit", TAG, __func__);
  return ret;
}

status_t EncoderCore::StopTrackEncoder(uint32_t track_id) {

  QMMF_LEVEL1("%s:%s: Enter track_id(%d)", TAG, __func__, track_id);
  if (!isTrackValid(track_id)) {
    QMMF_ERROR("%s:%s: Invalid track_id(%d)", TAG, __func__, track_id);
    return BAD_VALUE;
  }
  sp<TrackEncoder> track_encoder = track_encoders_.valueFor(track_id);
  assert(track_encoder.get() != NULL);

  auto ret = track_encoder->Stop();
  if (ret != NO_ERROR) {
    QMMF_INFO("%s:%s: track_id(%d) TrackEncoder Stop failed!", TAG, __func__,
      track_id);
    return ret;
  }
  // Initial debug purpose.
  assert(ret == NO_ERROR);

  QMMF_INFO("%s:%s: track_id(%d) TrackEncoder Stopped Successfully!", TAG,
      __func__);
  QMMF_LEVEL1("%s:%s: Exit", TAG, __func__);
  return ret;
}

status_t EncoderCore::DeleteTrackEncoder(uint32_t track_id) {

  QMMF_LEVEL1("%s:%s: Enter track_id(%d)", TAG, __func__, track_id);
  if (!isTrackValid(track_id)) {
    QMMF_ERROR("%s:%s: Invalid track_id(%d)", TAG, __func__, track_id);
    return BAD_VALUE;
  }
  track_encoders_.removeItem(track_id);

  QMMF_INFO("%s:%s: track_id(%d) TrackEncoder Deleted Successfully!", TAG,
      __func__);
  QMMF_LEVEL1("%s:%s: Exit", TAG, __func__);
  return NO_ERROR;

}

status_t EncoderCore::SetTrackEncoderParams(uint32_t track_id,
                                            VideoTrackParamType param_type,
                                            void* param, uint32_t param_size) {

  QMMF_LEVEL1("%s:%s: Enter track_id(%d)", TAG, __func__, track_id);
  if (!isTrackValid(track_id)) {
    QMMF_ERROR("%s:%s: Invalid track_id(%d)", TAG, __func__, track_id);
    return BAD_VALUE;
  }
  sp<TrackEncoder> track_encoder = track_encoders_.valueFor(track_id);
  assert(track_encoder.get() != NULL);

  auto ret = track_encoder->SetParams(param_type, param, param_size);
  if (ret != NO_ERROR) {
    QMMF_INFO("%s:%s: track_id(%d) TrackEncoder SetParams failed!", TAG,
        __func__, track_id);
    return ret;
  }
  // Initial debug purpose.
  assert(ret == NO_ERROR);

  QMMF_LEVEL1("%s:%s: Exit", TAG, __func__);
  return ret;
}

bool EncoderCore::isTrackValid(uint32_t track_id) {

  QMMF_INFO("%s: Number of Tracks exist = %d",__func__, track_encoders_.size());
  assert(track_encoders_.size() > 0);
  return track_encoders_.indexOfKey(track_id) >= 0 ? true : false;
}

TrackEncoder::TrackEncoder() {

  QMMF_INFO("%s:%s: Enter", TAG, __func__);
  memset(&track_params_, 0x0, sizeof track_params_);
  QMMF_INFO("%s:%s: Exit (0x%x)", TAG, __func__, this);
}

TrackEncoder::~TrackEncoder() {

  QMMF_INFO("%s:%s: Enter", TAG, __func__);
  QMMF_INFO("%s:%s: Exit (0x%x)", TAG, __func__, this);
}

status_t TrackEncoder::Init(const sp<TrackSource>& track_source,
                            const VideoTrackParams& params) {

  QMMF_INFO("%s:%s: Enter track_id(%d)", TAG, __func__, params.track_id);
  track_params_ = params;
  QMMF_INFO("%s:%s: Exit", TAG, __func__);
}

status_t TrackEncoder::Start() {

  QMMF_INFO("%s:%s: Enter track_id(%d)", TAG, __func__, track_params_.track_id);

  QMMF_INFO("%s:%s: Exit", TAG, __func__);
}

status_t TrackEncoder::Stop() {

  QMMF_INFO("%s:%s: Enter track_id(%d)", TAG, __func__, track_params_.track_id);

  QMMF_INFO("%s:%s: Exit", TAG, __func__);
}

status_t TrackEncoder::SetParams(VideoTrackParamType param_type, void* param,
                                 uint32_t param_size) {

  QMMF_INFO("%s:%s: Enter track_id(%d)", TAG, __func__, track_params_.track_id);

  QMMF_INFO("%s:%s: Exit", TAG, __func__);
}

status_t TrackEncoder::AllocOutputPortBufs() {

  QMMF_INFO("%s:%s: Enter track_id(%d)", TAG, __func__, track_params_.track_id);

  QMMF_INFO("%s:%s: Exit", TAG, __func__);
}

};  // namespace recorder

};  // namespace qmmf