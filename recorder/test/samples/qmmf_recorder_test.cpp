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

#define TAG "RecorderTest"

#include <utils/Log.h>
#include <utils/String8.h>
#include <assert.h>

#include <qmmf_camera3_types.h>
#include "qmmf_recorder_test.h"

using namespace qmmf::cameraadaptor;

//#define DEBUG
#define TEST_INFO(fmt, args...)  ALOGD(fmt, ##args)
#define TEST_ERROR(fmt, args...) ALOGE(fmt, ##args)
#ifdef DEBUG
#define TEST_DBG  TEST_INFO
#else
#define TEST_DBG(...) ((void)0)
#endif

//#define DUMP_FRAMES

RecorderTest::RecorderTest() {
  TEST_INFO("%s:%s: Enter", TAG, __func__);
  TEST_INFO("%s:%s: Exit", TAG, __func__);
}

RecorderTest::~RecorderTest() {
  TEST_INFO("%s:%s: Enter", TAG, __func__);
  TEST_INFO("%s:%s: Exit", TAG, __func__);
}

int32_t RecorderTest::Connect() {

  TEST_INFO("%s:%s: Enter", TAG, __func__);

  RecorderCb recorder_status_cb;
  recorder_status_cb.event_cb = [&] ( EventType event_type, void *event_data,
      size_t event_data_size) { RecorderCallbackHandler(event_type, event_data,
      event_data_size); };

  auto ret = recorder_.Connect(recorder_status_cb);
  TEST_INFO("%s:%s: Exit", TAG, __func__);

  return ret;
}

int32_t RecorderTest::Disconnect() {

  TEST_INFO("%s:%s: Enter", TAG, __func__);
  auto ret = recorder_.Disconnect();
  TEST_INFO("%s:%s: Exit", TAG, __func__);
  return ret;
}

int32_t RecorderTest::StartCamera() {

  TEST_INFO("%s:%s: Enter", TAG, __func__);
  std::vector<uint32_t> camera_ids;
  camera_ids.push_back(0);

  CameraStartParam camera_params;
  memset(&camera_params, 0x0, sizeof camera_params);
  camera_params.zsl_mode            = false;
  camera_params.zsl_queue_depth     = 10;
  camera_params.zsl_width           = 3840;
  camera_params.zsl_height          = 2160;
  camera_params.frame_rate          = 30;
  camera_params.flags               = 0x0;

  auto ret = recorder_.StartCamera(camera_ids, camera_params);
  if(ret != 0) {
      ALOGE("%s:%s StartCamera Failed!!", TAG, __func__);
  }

  TEST_INFO("%s:%s: Exit", TAG, __func__);
  return 0;
}

int32_t RecorderTest::StopCamera() {

  TEST_INFO("%s:%s: Enter", TAG, __func__);

  std::vector<uint32_t> camera_ids;
  camera_ids.push_back(0);

  auto ret = recorder_.StopCamera(camera_ids);
  if(ret != 0) {
    ALOGE("%s:%s StopCamera Failed!!", TAG, __func__);
  }

  TEST_INFO("%s:%s: Exit", TAG, __func__);
  return 0;
}

// This session has two video tracks 4K and 1080p
int32_t RecorderTest::SessionWithTwoVideoTrack() {

  TEST_INFO("%s:%s: Enter", TAG, __func__);

  std::vector<uint32_t> track_ids;

  SessionCb session_status_cb;
  session_status_cb.event_cb = [&] ( EventType event_type, void *event_data,
      size_t event_data_size) { SessionCallbackHandler(event_type,
      event_data, event_data_size); };

  uint32_t session_id;
  auto ret = recorder_.CreateSession(session_status_cb, &session_id);
  TEST_INFO("%s:%s: sessions_id = %d", TAG, __func__, session_id);

  //Create Video track.
  uint32_t video_track_id = 1;
  VideoTrackCreateParam video_track_param;
  memset(&video_track_param, 0x0, sizeof video_track_param);

  //TODO: change it vector.
  video_track_param.camera_ids[0] = 0;
  video_track_param.num_cameras   = 1;
  video_track_param.width         = 3840;
  video_track_param.height        = 2160;
  video_track_param.frame_rate    = 30;
  video_track_param.codec_type    = VideoCodecType::kYUV;
  video_track_param.out_device    = 0x01;
  #if 0
  // AV codec is not functional yet.
  video_track_param.codec_param.avc.idr_interval = 120;
  video_track_param.codec_param.avc.bitrate      = 6000000;
  video_track_param.codec_param.avc.profile = AVCProfileType::kMain;
  video_track_param.codec_param.avc.level   = AVCLevelType::kLevel4;
  video_track_param.codec_param.avc.ratecontrol_type =
      VideoRateControlType::kVariableSkipFrames;
  video_track_param.codec_param.avc.qp_params.enable_init_qp = true;
  video_track_param.codec_param.avc.qp_params.init_qp.init_IQP = 56;
  video_track_param.codec_param.avc.qp_params.init_qp.init_PQP = 56;
  video_track_param.codec_param.avc.qp_params.init_qp.init_BQP = 56;
  video_track_param.codec_param.avc.qp_params.init_qp.init_QP_mode = 0x7;
  video_track_param.codec_param.avc.qp_params.enable_qp_range = true;
  video_track_param.codec_param.avc.qp_params.qp_range.min_QP = 26;
  video_track_param.codec_param.avc.qp_params.qp_range.max_QP = 56;
  video_track_param.codec_param.avc.qp_params.enable_qp_IBP_range = true;
  video_track_param.codec_param.avc.qp_params.qp_IBP_range.min_IQP = 26;
  video_track_param.codec_param.avc.qp_params.qp_IBP_range.max_IQP = 56;
  video_track_param.codec_param.avc.qp_params.qp_IBP_range.min_PQP = 26;
  video_track_param.codec_param.avc.qp_params.qp_IBP_range.max_PQP = 56;
  video_track_param.codec_param.avc.qp_params.qp_IBP_range.min_BQP = 26;
  video_track_param.codec_param.avc.qp_params.qp_IBP_range.max_BQP = 56;
#endif
  TrackCb video_track_cb;
  video_track_cb.data_cb = [&] (uint32_t track_id, std::vector<TrackBuffer>
      buffers, void *meta_param, TrackMetaParamType meta_type,
      size_t meta_size) { VideoTrack4KDataCb(track_id,
      buffers, meta_param, meta_type, meta_size); };

  video_track_cb.event_cb = [&] (uint32_t track_id, EventType event_type,
      void *event_data, size_t event_data_size) { VideoTrack4KEventCb(track_id,
      event_type, event_data, event_data_size); };

  ret = recorder_.CreateVideoTrack(session_id, video_track_id,
                                   video_track_param, video_track_cb);

  assert(ret == 0);
  track_ids.push_back(video_track_id);

  video_track_id = 2;
  memset(&video_track_param, 0x0, sizeof video_track_param);

  video_track_param.camera_ids[0] = 0;
  video_track_param.num_cameras   = 1;
  video_track_param.width         = 1920;
  video_track_param.height        = 1080;
  video_track_param.frame_rate    = 30;
  video_track_param.codec_type    = VideoCodecType::kYUV;
  video_track_param.out_device    = 0x01;

  memset(&video_track_cb, 0x0, sizeof (video_track_cb));
  video_track_cb.data_cb = [&] (uint32_t track_id, std::vector<TrackBuffer>
      buffers, void *meta_param, TrackMetaParamType meta_type,
      size_t meta_size) { VideoTrack1080pDataCb(track_id,
      buffers, meta_param, meta_type, meta_size); };

  video_track_cb.event_cb = [&] (uint32_t track_id, EventType event_type,
      void *event_data, size_t event_data_size)
      { VideoTrack1080pEventCb(track_id, event_type, event_data,
        event_data_size);
      };

  ret = recorder_.CreateVideoTrack(session_id, video_track_id,
                                   video_track_param, video_track_cb);

  assert(ret == 0);
  track_ids.push_back(video_track_id);

  sessions_.insert(std::make_pair(session_id, track_ids));

  TEST_INFO("%s:%s: Exit", TAG, __func__);
  return 0;
}

//#define NOT_FUNCTIONAL
#ifdef NOT_FUNCTIONAL
int32_t RecorderTest::CreateAudioOnlySession() {

  TEST_INFO("%s:%s: Enter", TAG, __func__);

  SessionCb session_status_cb;
  session_status_cb.event_cb = [&] ( EventType event_type, void *event_data,
      size_t event_data_size) { SessionCallbackHandler(event_type,
      event_data, event_data_size); };


  auto ret = recorder_.CreateSession(session_status_cb, &session_id_);
  TEST_INFO("%s:%s: SessionId = %d", TAG, __func__, session_id_);

  //Create Audio track with dummy values.
  uint32_t audio_track_id = 1;
  AudioTrackCreateParam audio_track_params;
  memset(&audio_track_params, 0x0, sizeof audio_track_params);

  audio_track_params.in_device[0]   = 10;
  audio_track_params.in_device[1]   = 20;
  audio_track_params.in_device[2]   = 30;
  audio_track_params.in_device[3]   = 40;

  audio_track_params.num_in_devices = 4;
  audio_track_params.sample_rate    = 32000;
  audio_track_params.channels       = 2;
  audio_track_params.bit_depth      = 16;
  audio_track_params.codec_type     = AudioCodecType::kAAC;
  audio_track_params.codec_param.aac.format = AACFormat::kADTS;
  audio_track_params.codec_param.aac.mode   = AACMode::kAALC;
  audio_track_params.codec_param.aac.frame_length = 32;
  audio_track_params.codec_param.aac.bit_rate     = 42000;
  audio_track_params.out_device     = 5;
  audio_track_params.flags          = 132;

  TrackCb audio_track_cb;
  audio_track_cb.data_cb = [&] (uint32_t track_id, std::vector<TrackBuffer>
      buffers,  void *meta_param, TrackMetaParamType meta_type,
      size_t meta_size, uint32_t buffer_pool_id) { AudioTrackDataCb(track_id,
      buffers, meta_param, meta_type, meta_size, buffer_pool_id); };

  audio_track_cb.event_cb = [&] (uint32_t track_id, EventType event_type,
      void *event_data, size_t event_data_size) {
      AudioTrackEventCb(track_id, event_type, event_data, event_data_size); };

  ret = recorder_.CreateAudioTrack(session_id_, audio_track_id,
                                   audio_track_params, audio_track_cb);
  if(ret != 0) {
      ALOGE("%s:%s CreateAudioTrack failed!", TAG, __func__);
      return -1;
  }

  //Create Video track with dummy values.
  uint32_t video_track_id = 1;
  VideoTrackCreateParam video_track_param;
  memset(&video_track_param, 0x0, sizeof video_track_param);

  video_track_param.camera_ids[0] = 11;
  video_track_param.camera_ids[1] = 22;
  video_track_param.num_cameras   = 2;
  video_track_param.width         = 1920;
  video_track_param.height        = 1080;
  video_track_param.frame_rate    = 30;
  video_track_param.codec_type    = VideoCodecType::kAVC;
  video_track_param.out_device    = 0x01;
  video_track_param.codec_param.avc.idr_interval = 120;
  video_track_param.codec_param.avc.bitrate      = 6000000;
  video_track_param.codec_param.avc.profile = AVCProfileType::kMain;
  video_track_param.codec_param.avc.level   = AVCLevelType::kLevel4;
  video_track_param.codec_param.avc.ratecontrol_type = VideoRateControlType::
        kVariableSkipFrames;
  video_track_param.codec_param.avc.qp_params.enable_init_qp = true;
  video_track_param.codec_param.avc.qp_params.init_qp.init_IQP = 56;
  video_track_param.codec_param.avc.qp_params.init_qp.init_PQP = 56;
  video_track_param.codec_param.avc.qp_params.init_qp.init_BQP = 56;
  video_track_param.codec_param.avc.qp_params.init_qp.init_QP_mode = 0x7;
  video_track_param.codec_param.avc.qp_params.enable_qp_range = true;
  video_track_param.codec_param.avc.qp_params.qp_range.min_QP = 26;
  video_track_param.codec_param.avc.qp_params.qp_range.max_QP = 56;
  video_track_param.codec_param.avc.qp_params.enable_qp_IBP_range = true;
  video_track_param.codec_param.avc.qp_params.qp_IBP_range.min_IQP = 26;
  video_track_param.codec_param.avc.qp_params.qp_IBP_range.max_IQP = 56;
  video_track_param.codec_param.avc.qp_params.qp_IBP_range.min_PQP = 26;
  video_track_param.codec_param.avc.qp_params.qp_IBP_range.max_PQP = 56;
  video_track_param.codec_param.avc.qp_params.qp_IBP_range.min_BQP = 26;
  video_track_param.codec_param.avc.qp_params.qp_IBP_range.max_BQP = 56;

  TrackCb video_track_cb;
  video_track_cb.data_cb = [&] (uint32_t track_id, std::vector<TrackBuffer>
      buffers, void *meta_param, TrackMetaParamType meta_type,
      size_t meta_size) { VideoTrackDataCb(track_id,
      buffers, meta_param, meta_type, meta_size); };

  video_track_cb.event_cb = [&] (uint32_t track_id, EventType event_type,
      void *event_data, size_t event_data_size) {
      VideoTrackEventCb(track_id, event_type, event_data, event_data_size); };

  ret = recorder_.CreateVideoTrack(session_id_, video_track_id,
                                   video_track_param, video_track_cb);

  sleep(2);
  int32_t volume = 6;
  ret = recorder_.SetAudioTrackParam(session_id_, audio_track_id,
                                     AudioTrackParamType::kAudioVolumeParamType,
                                     static_cast<void *>(&volume),
                                     sizeof volume);
  if(ret != 0) {
    ALOGE("%s:%s SetAudioTrackParam failed!", TAG, __func__);
    return -1;
  }

  TEST_INFO("%s:%s: Exit", TAG, __func__);
  return 0;
}

// Audio is not functional yet.
int32_t RecorderTest::CreateAudioVideoSession() {

  TEST_INFO("%s:%s: Enter", TAG, __func__);

  SessionCb session_status_cb;
  session_status_cb.event_cb = [&] ( EventType event_type, void *event_data,
          size_t event_data_size) { SessionCallbackHandler(event_type,
          event_data, event_data_size); };

  auto ret = recorder_.CreateSession(session_status_cb, &session_id_);
  TEST_INFO("%s:%s: SessionId = %d", TAG, __func__, session_id_);

  //Create Audio track with dummy values.
  uint32_t audio_track_id = 1;
  AudioTrackCreateParam audio_track_params;
  memset(&audio_track_params, 0x0, sizeof audio_track_params);

  audio_track_params.in_device[0]   = 10;
  audio_track_params.in_device[1]   = 20;
  audio_track_params.in_device[2]   = 30;
  audio_track_params.in_device[3]   = 40;

  audio_track_params.num_in_devices = 4;
  audio_track_params.sample_rate    = 32000;
  audio_track_params.channels       = 2;
  audio_track_params.bit_depth      = 16;
  audio_track_params.codec_type     = AudioCodecType::kAAC;
  audio_track_params.codec_param.aac.format = AACFormat::kADTS;
  audio_track_params.codec_param.aac.mode   = AACMode::kAALC;
  audio_track_params.codec_param.aac.frame_length = 32;
  audio_track_params.codec_param.aac.bit_rate     = 42000;
  audio_track_params.out_device     = 5;
  audio_track_params.flags          = 132;

  TrackCb audio_track_cb;
  audio_track_cb.data_cb = [&] (uint32_t track_id, std::vector<TrackBuffer>
      buffers,  void *meta_param, TrackMetaParamType meta_type,
      size_t meta_size, uint32_t buffer_pool_id) { AudioTrackDataCb(track_id,
      buffers, meta_param, meta_type, meta_size, buffer_pool_id); };

  audio_track_cb.event_cb = [&] (uint32_t track_id, EventType event_type,
      void *event_data, size_t event_data_size) {
      AudioTrackEventCb(track_id, event_type, event_data, event_data_size); };

  ret = recorder_.CreateAudioTrack(session_id_, audio_track_id,
                                   audio_track_params, audio_track_cb);
  if(ret != 0) {
      ALOGE("%s:%s CreateAudioTrack failed!", TAG, __func__);
      return -1;
  }

  //Create Video track with dummy values.
  uint32_t video_track_id = 1;
  VideoTrackCreateParam video_track_param;
  memset(&video_track_param, 0x0, sizeof video_track_param);

  video_track_param.camera_ids[0] = 11;
  video_track_param.camera_ids[1] = 22;
  video_track_param.num_cameras   = 2;
  video_track_param.width         = 1920;
  video_track_param.height        = 1080;
  video_track_param.frame_rate    = 30;
  video_track_param.codec_type    = VideoCodecType::kAVC;
  video_track_param.out_device    = 0x01;
  video_track_param.codec_param.avc.idr_interval = 120;
  video_track_param.codec_param.avc.bitrate      = 6000000;
  video_track_param.codec_param.avc.profile = AVCProfileType::kMain;
  video_track_param.codec_param.avc.level   = AVCLevelType::kLevel4;
  video_track_param.codec_param.avc.ratecontrol_type =
                                  VideoRateControlType::kVariableSkipFrames;
  video_track_param.codec_param.avc.qp_params.enable_init_qp = true;
  video_track_param.codec_param.avc.qp_params.init_qp.init_IQP = 56;
  video_track_param.codec_param.avc.qp_params.init_qp.init_PQP = 56;
  video_track_param.codec_param.avc.qp_params.init_qp.init_BQP = 56;
  video_track_param.codec_param.avc.qp_params.init_qp.init_QP_mode = 0x7;
  video_track_param.codec_param.avc.qp_params.enable_qp_range = true;
  video_track_param.codec_param.avc.qp_params.qp_range.min_QP = 26;
  video_track_param.codec_param.avc.qp_params.qp_range.max_QP = 56;
  video_track_param.codec_param.avc.qp_params.enable_qp_IBP_range = true;
  video_track_param.codec_param.avc.qp_params.qp_IBP_range.min_IQP = 26;
  video_track_param.codec_param.avc.qp_params.qp_IBP_range.max_IQP = 56;
  video_track_param.codec_param.avc.qp_params.qp_IBP_range.min_PQP = 26;
  video_track_param.codec_param.avc.qp_params.qp_IBP_range.max_PQP = 56;
  video_track_param.codec_param.avc.qp_params.qp_IBP_range.min_BQP = 26;
  video_track_param.codec_param.avc.qp_params.qp_IBP_range.max_BQP = 56;

  TrackCb video_track_cb;
  video_track_cb.data_cb = [&] (uint32_t track_id, std::vector<TrackBuffer>
      buffers, void *meta_param, TrackMetaParamType meta_type,
      size_t meta_size) { VideoTrackDataCb(track_id,
      buffers, meta_param, meta_type, meta_size); };

  video_track_cb.event_cb = [&] (uint32_t track_id, EventType event_type,
      void *event_data, size_t event_data_size) {
      VideoTrackEventCb(track_id, event_type, event_data, event_data_size); };

  ret = recorder_.CreateVideoTrack(session_id_, video_track_id,
                                   video_track_param, video_track_cb);

  sleep(2);
  int32_t volume = 6;
  ret = recorder_.SetAudioTrackParam(session_id_, audio_track_id,
                                     AudioTrackParamType::kAudioVolumeParamType,
                                     static_cast<void *>(&volume),
                                     sizeof volume);
  if(ret != 0) {
    ALOGE("%s:%s SetAudioTrackParam failed!", TAG, __func__);
    return -1;
  }

  //Only test purpose.
  recorder_.StartSession(session_id_);

  recorder_.PauseSession(session_id_);

  recorder_.ResumeSession(session_id_);

  recorder_.StopSession(session_id_, true);

  TEST_INFO("%s:%s: Exit", TAG, __func__);
  return 0;
}
#endif // NOT_FUNCTIONAL

int32_t RecorderTest::StartSession() {

  TEST_INFO("%s:%s: Enter", TAG, __func__);
  std::map <uint32_t , std::vector<uint32_t> >::iterator it = sessions_.begin();
  uint32_t session_id = it->first;
  auto ret = recorder_.StartSession(session_id);
  assert(ret == 0);
  TEST_INFO("%s:%s: Exit", TAG, __func__);
}

int32_t RecorderTest::StopSession() {

  TEST_INFO("%s:%s: Enter", TAG, __func__);
  std::map <uint32_t , std::vector<uint32_t> >::iterator it = sessions_.begin();
  uint32_t session_id = it->first;
  auto ret = recorder_.StopSession(session_id, true /*flush buffers*/);
  assert(ret == 0);
  TEST_INFO("%s:%s: Exit", TAG, __func__);
}

int32_t RecorderTest::PauseSession() {

  TEST_INFO("%s:%s: Enter", TAG, __func__);
  std::map <uint32_t , std::vector<uint32_t> >::iterator it = sessions_.begin();
  uint32_t session_id = it->first;
  auto ret = recorder_.PauseSession(session_id);
  assert(ret == 0);
  TEST_INFO("%s:%s: Exit", TAG, __func__);
}

int32_t RecorderTest::ResumeSession() {

  TEST_INFO("%s:%s: Enter", TAG, __func__);
  std::map <uint32_t , std::vector<uint32_t> >::iterator it = sessions_.begin();
  uint32_t session_id = it->first;
  auto ret = recorder_.ResumeSession(session_id);
  assert(ret == 0);
  TEST_INFO("%s:%s: Exit", TAG, __func__);
}

int32_t RecorderTest::DeleteSession()
{
  TEST_INFO("%s:%s: Enter", TAG, __func__);
  std::map <uint32_t , std::vector<uint32_t> >::iterator iter =
      sessions_.begin();
  uint32_t session_id = iter->first;
  std::vector<uint32_t> tracks;
  tracks = iter->second;
  /*
  * Delete all the tracks associated to session.
  */
  for (size_t i = 0; i < tracks.size(); i++) {
    auto ret = recorder_.DeleteVideoTrack(session_id, tracks[i]);
    assert(ret == 0);
  }
  /*
  * Once all tracks are deleted successfully delete session.
  */
  auto ret = recorder_.DeleteSession(session_id);

  sessions_.erase(iter);
  TEST_INFO("%s:%s: Exit", TAG, __func__);
  return 0;
}

void RecorderTest::RecorderCallbackHandler(EventType event_type,
                                           void *event_data,
                                           size_t event_data_size)
{
    TEST_INFO("%s:%s: Enter", TAG, __func__);
    TEST_INFO("%s:%s: Exit", TAG, __func__);
}

void RecorderTest::SessionCallbackHandler(EventType event_type,
                                          void *event_data,
                                          size_t event_data_size)
{
    TEST_INFO("%s:%s: Enter", TAG, __func__);
    TEST_INFO("%s:%s: Exit", TAG, __func__);
}

void RecorderTest::AudioTrackDataCb(uint32_t track_id,
                                    std::vector<TrackBuffer> buffers,
                                    void *meta_param,
                                    TrackMetaParamType meta_type,
                                    size_t meta_size,
                                    uint32_t buffer_pool_id)
{
    TEST_INFO("%s:%s: Enter", TAG, __func__);
    TEST_INFO("%s:%s: Exit", TAG, __func__);
}

void RecorderTest::AudioTrackEventCb(uint32_t track_id, EventType event_type,
                                     void *event_data,
                                     size_t event_data_size)
{
    TEST_INFO("%s:%s: Enter", TAG, __func__);
    TEST_INFO("%s:%s: Exit", TAG, __func__);
}

void RecorderTest::VideoTrack4KDataCb(uint32_t track_id,
                                    std::vector<TrackBuffer> buffers,
                                    void *meta_param,
                                    TrackMetaParamType meta_type,
                                    size_t meta_size) {

  TEST_DBG("%s:%s: Enter", TAG, __func__);

  TEST_DBG("%s:%s: meta_type=%d", TAG, __func__, meta_type);
  if (meta_type == TrackMetaParamType::kCamBufMetaData) {
    MetaInfo* meta_data = static_cast<MetaInfo*>(meta_param);
    TEST_DBG("%s:%s: format=%d", TAG, __func__, meta_data->format);
    TEST_DBG("%s:%s: num_planes=%d", TAG, __func__, meta_data->num_planes);
    for (uint8_t i = 0; i < meta_data->num_planes; ++i) {
      TEST_DBG("%s:%s: plane[%d]:stride(%d)", TAG, __func__, i,
          meta_data->plane_info[i].stride);
      TEST_DBG("%s:%s: plane[%d]:scanline(%d)", TAG, __func__, i,
          meta_data->plane_info[i].scanline);
      TEST_DBG("%s:%s: plane[%d]:width(%d)", TAG, __func__, i,
          meta_data->plane_info[i].width);
      TEST_DBG("%s:%s: plane[%d]:height(%d)", TAG, __func__, i,
          meta_data->plane_info[i].height);
    }
  }
#ifdef DUMP_FRAMES
  static uint32_t id = 0;
  ++id;
  // Dump every 100th Frame.
  if (id == 100) {
    String8 file_path;
    size_t written_len;
    file_path.appendFormat("/usr/test/track_%d_%lld.yuv", track_id,
        buffers[0].timestamp);

    FILE *file = fopen(file_path.string(), "w+");
    if (!file) {
      ALOGE("%s:%s: Unable to open file(%s)", TAG, __func__,
          file_path.string());
      goto FAIL;
    }

    written_len = fwrite(buffers[0].data, sizeof(uint8_t), buffers[0].size,
        file);
    TEST_INFO("%s:%s: written_len =%d", TAG, __func__, written_len);
    if (buffers[0].size != written_len) {
      ALOGE("%s:%s: Bad Write error (%d):(%s)\n", TAG, __func__, errno,
          strerror(errno));
      goto FAIL;
    }
    TEST_INFO("%s:%s: Buffer(0x%x) Size(%u) Stored@(%s)\n", TAG, __func__,
      buffers[0].data, written_len, file_path.string());

FAIL:
    if (file != NULL) {
      fclose(file);
    }
    id = 0;
  }
#endif
  // Return buffers back to service.
  std::map <uint32_t , std::vector<uint32_t> >::iterator it = sessions_.begin();
  uint32_t session_id = it->first;
  auto ret = recorder_.ReturnTrackBuffer(session_id, track_id, buffers);
  if(ret != 0) {
    ALOGE("%s:%s: ReturnTrackBuffer failed!", TAG, __func__);
  }
  TEST_DBG("%s:%s: Exit", TAG, __func__);
}

void RecorderTest::VideoTrack4KEventCb(uint32_t track_id, EventType event_type,
                                        void *event_data,
                                        size_t event_data_size)
{
    TEST_INFO("%s:%s: Enter", TAG, __func__);
    TEST_INFO("%s:%s: Exit", TAG, __func__);
}

void RecorderTest::VideoTrack1080pDataCb(uint32_t track_id,
                                       std::vector<TrackBuffer> buffers,
                                       void *meta_param,
                                       TrackMetaParamType meta_type,
                                       size_t meta_size) {

  TEST_DBG("%s:%s: Enter", TAG, __func__);

  TEST_DBG("%s:%s: meta_type=%d", TAG, __func__, meta_type);
  if (meta_type == TrackMetaParamType::kCamBufMetaData) {
    MetaInfo* meta_data = static_cast<MetaInfo*>(meta_param);
    TEST_DBG("%s:%s: format=%d", TAG, __func__, meta_data->format);
    TEST_DBG("%s:%s: num_planes=%d", TAG, __func__, meta_data->num_planes);
    for (uint8_t i = 0; i < meta_data->num_planes; ++i) {
      TEST_DBG("%s:%s: plane[%d]:stride(%d)", TAG, __func__, i,
          meta_data->plane_info[i].stride);
      TEST_DBG("%s:%s: plane[%d]:scanline(%d)", TAG, __func__, i,
          meta_data->plane_info[i].scanline);
      TEST_DBG("%s:%s: plane[%d]:width(%d)", TAG, __func__, i,
          meta_data->plane_info[i].width);
      TEST_DBG("%s:%s: plane[%d]:height(%d)", TAG, __func__, i,
          meta_data->plane_info[i].height);
    }
  }
#ifdef DUMP_FRAMES
  static uint32_t id = 0;
  ++id;
  // Dump every 100th Frame.
  if (id == 100) {
    String8 file_path;
    size_t written_len;
    file_path.appendFormat("/usr/test/track_%d_%lld.yuv", track_id,
        buffers[0].timestamp);

    FILE *file = fopen(file_path.string(), "w+");
    if (!file) {
      ALOGE("%s:%s: Unable to open file(%s)", TAG, __func__,
          file_path.string());
      goto FAIL;
    }

    written_len = fwrite(buffers[0].data, sizeof(uint8_t), buffers[0].size,
        file);
    TEST_INFO("%s:%s: written_len =%d", TAG, __func__, written_len);
    if (buffers[0].size != written_len) {
      ALOGE("%s:%s: Bad Write error (%d):(%s)\n", TAG, __func__, errno,
          strerror(errno));
      goto FAIL;
    }
    TEST_INFO("%s:%s: Buffer(0x%x) Size(%u) Stored@(%s)\n", TAG, __func__,
      buffers[0].data, written_len, file_path.string());

FAIL:
    if (file != NULL) {
      fclose(file);
    }
    id = 0;
  }
#endif
  // Return buffers back to service.
  std::map <uint32_t , std::vector<uint32_t> >::iterator it = sessions_.begin();
  uint32_t session_id = it->first;
  auto ret = recorder_.ReturnTrackBuffer(session_id, track_id, buffers);
  if(ret != 0) {
    ALOGE("%s:%s: ReturnTrackBuffer failed!", TAG, __func__);
  }
  TEST_DBG("%s:%s: Exit", TAG, __func__);
}

void RecorderTest::VideoTrack1080pEventCb(uint32_t track_id,
                                          EventType event_type,
                                          void *event_data,
                                          size_t event_data_size) {
    TEST_INFO("%s:%s: Enter", TAG, __func__);
    TEST_INFO("%s:%s: Exit", TAG, __func__);
}

void CmdMenu::PrintMenu()
{
    printf("\n\n=========== QIPCAM TEST MENU ===================\n\n");

    printf(" \n\nIPCam Test Application commands \n");
    printf(" -----------------------------\n");
    printf("   %c. Connect\n", CmdMenu::CONNECT_CMD);
    printf("   %c. Disconnect\n", CmdMenu::DISCONNECT_CMD);
    printf("   %c. Start Camera\n", CmdMenu::START_CAMERA_CMD);
    printf("   %c. Stop Camera\n", CmdMenu::STOP_CAMERA_CMD);
    printf("   %c. Create Video Only Session\n", CmdMenu::CREATE_SESSION_CMD);
    printf("   %c. Start Session\n", CmdMenu::START_SESSION_CMD);
    printf("   %c. Stop Session\n", CmdMenu::STOP_SESSION_CMD);
    printf("   %c. Pause Session\n", CmdMenu::PAUSE_SESSION_CMD);
    printf("   %c. Resume Session\n", CmdMenu::RESUME_SESSION_CMD);
    printf("   %c. Delete Session\n", CmdMenu::DELETE_SESSION_CMD);
    printf("   %c. Exit\n", CmdMenu::EXIT_CMD);
    printf("\n   Choice: ");
}

CmdMenu::Command CmdMenu::GetCommand()
{
    PrintMenu();
    return CmdMenu::Command(
            static_cast<CmdMenu::CommandType>(getchar()));
}

int main(int argc,char *argv[])
{
    TEST_INFO("%s:%s: Enter", TAG, __func__);

    RecorderTest test_context;

    CmdMenu cmd_menu(test_context);

    int32_t testRunning = true;

    while (testRunning) {
        CmdMenu::Command command = cmd_menu.GetCommand();

        switch (command.cmd) {

            case CmdMenu::CONNECT_CMD:
            {
                test_context.Connect();
            }
            break;

            case CmdMenu::DISCONNECT_CMD:
            {
                test_context.Disconnect();
            }
            break;

            case CmdMenu::START_CAMERA_CMD:
            {
                test_context.StartCamera();
            }
            break;

            case CmdMenu::STOP_CAMERA_CMD:
            {
                test_context.StopCamera();
            }
            break;
            case CmdMenu::CREATE_SESSION_CMD:
            {
                test_context.SessionWithTwoVideoTrack();
            }
            break;
            case CmdMenu::START_SESSION_CMD:
            {
                test_context.StartSession();
            }
            break;
            case CmdMenu::STOP_SESSION_CMD:
            {
                test_context.StopSession();
            }
            break;
            case CmdMenu::PAUSE_SESSION_CMD:
            {
                test_context.PauseSession();
            }
            break;
            case CmdMenu::RESUME_SESSION_CMD:
            {
                test_context.ResumeSession();
            }
            break;
            case CmdMenu::DELETE_SESSION_CMD:
            {
                test_context.DeleteSession();
            }
            break;
            default:
                break;
        }
    }
    return 0;
}
