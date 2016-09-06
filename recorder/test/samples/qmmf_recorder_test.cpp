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

#include <fcntl.h>
#include <sys/mman.h>
#include <utils/Log.h>
#include <utils/String8.h>
#include <assert.h>

#include "recorder/test/samples/qmmf_recorder_test.h"
#include "recorder/test/samples/qmmf_recorder_test_wav.h"

//#define DEBUG
#define TEST_INFO(fmt, args...)  ALOGD(fmt, ##args)
#define TEST_ERROR(fmt, args...) ALOGE(fmt, ##args)
#ifdef DEBUG
#define TEST_DBG  TEST_INFO
#else
#define TEST_DBG(...) ((void)0)
#endif

static const char* kDefaultAudioFilenamePrefix =
    "/data/qmmf_recorder_test_audio";

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

  CameraStartParam camera_params;
  memset(&camera_params, 0x0, sizeof camera_params);
  camera_params.zsl_mode            = false;
  camera_params.zsl_queue_depth     = 10;
  camera_params.zsl_width           = 3840;
  camera_params.zsl_height          = 2160;
  camera_params.frame_rate          = 30;
  camera_params.flags               = 0x0;

  camera_id_ = 0;
  auto ret = recorder_.StartCamera(camera_id_, camera_params);
  if(ret != 0) {
      ALOGE("%s:%s StartCamera Failed!!", TAG, __func__);
  }

  TEST_INFO("%s:%s: Exit", TAG, __func__);
  return 0;
}

int32_t RecorderTest::StopCamera() {

  TEST_INFO("%s:%s: Enter", TAG, __func__);

  auto ret = recorder_.StopCamera(camera_id_);
  if(ret != 0) {
    ALOGE("%s:%s StopCamera Failed!!", TAG, __func__);
  }

  TEST_INFO("%s:%s: Exit", TAG, __func__);
  return 0;
}

int32_t RecorderTest::TakeSnapshot() {

  TEST_INFO("%s:%s: Enter", TAG, __func__);

  ImageParam image_param;
  memset(&image_param, 0x0, sizeof image_param);
  image_param.width         = 3840;
  image_param.height        = 2160;
  image_param.image_format  = ImageFormat::kJPEG;
  image_param.image_quality = 95;

  ImageCaptureCb cb = [&] (uint32_t camera_id_, uint32_t image_sequence_count,
      BufferDescriptor buffer) { SnapshotCb(camera_id_, image_sequence_count,
      buffer); };

  std::vector<CameraMetadata> meta_array;
  CameraMetadata meta;
  auto ret = recorder_.GetDefaultCaptureParam(camera_id_, meta);
  assert(ret == NO_ERROR);

  uint8_t awb_mode = ANDROID_CONTROL_AWB_MODE_INCANDESCENT;
  ret = meta.update(ANDROID_CONTROL_AWB_MODE, &awb_mode, 1);
  assert(ret == NO_ERROR);

  meta_array.push_back(meta);

  ret = recorder_.CaptureImage(camera_id_, image_param, 1, meta_array, cb);
  if(ret != 0) {
    ALOGE("%s:%s CaptureImage Failed!!", TAG, __func__);
  }

  TEST_INFO("%s:%s: Exit", TAG, __func__);
  return 0;
}

// This session has two YUV video tracks 4K and 1080p.
int32_t RecorderTest::Session4KAnd1080pYUVTracks() {

  TEST_INFO("%s:%s: Enter", TAG, __func__);

  std::vector<TrackInfo> tracks;

  SessionCb session_status_cb;
  session_status_cb.event_cb = [&] ( EventType event_type, void *event_data,
      size_t event_data_size) { SessionCallbackHandler(event_type,
      event_data, event_data_size); };

  uint32_t session_id;
  auto ret = recorder_.CreateSession(session_status_cb, &session_id);
  TEST_INFO("%s:%s: sessions_id = %d", TAG, __func__, session_id);

  //Create Video track.
  TrackCb video_track_cb;
  uint32_t video_track_id = 1;
  VideoTrackCreateParam video_track_param;
  memset(&video_track_param, 0x0, sizeof video_track_param);
  //TODO: change it vector.
  video_track_param.camera_id   = 0;
  video_track_param.width       = 3840;
  video_track_param.height      = 2160;
  video_track_param.frame_rate  = 30;
  video_track_param.format_type = VideoFormat::kYUV;
  video_track_param.out_device  = 0x01;

  video_track_cb.data_cb = [&] (uint32_t track_id, std::vector<BufferDescriptor>
      buffers, void *meta_param, TrackMetaParamType meta_type,
      size_t meta_size) { VideoTrack4KYUVDataCb(track_id,
      buffers, meta_param, meta_type, meta_size); };

  video_track_cb.event_cb = [&] (uint32_t track_id, EventType event_type,
      void *event_data, size_t event_data_size) {
          VideoTrack4KYUVEventCb(track_id, event_type, event_data,
                                 event_data_size); };

  ret = recorder_.CreateVideoTrack(session_id, video_track_id,
                                   video_track_param, video_track_cb);

  assert(ret == 0);

  TrackInfo track_info;
  memset(&track_info, 0x0, sizeof track_info);
  track_info.track_id = video_track_id;
  track_info.type     = TrackType::kVideoTrack;
  tracks.push_back(track_info);

  video_track_id = 2;
  memset(&video_track_param, 0x0, sizeof video_track_param);

  video_track_param.camera_id   = 0;
  video_track_param.width       = 1920;
  video_track_param.height      = 1080;
  video_track_param.frame_rate  = 30;
  video_track_param.format_type = VideoFormat::kYUV;
  video_track_param.out_device  = 0x01;

  memset(&video_track_cb, 0x0, sizeof (video_track_cb));
  video_track_cb.data_cb = [&] (uint32_t track_id, std::vector<BufferDescriptor>
      buffers, void *meta_param, TrackMetaParamType meta_type,
      size_t meta_size) { VideoTrack1080pYUVDataCb(track_id,
      buffers, meta_param, meta_type, meta_size); };

  video_track_cb.event_cb = [&] (uint32_t track_id, EventType event_type,
      void *event_data, size_t event_data_size)
      { VideoTrack1080pYUVEventCb(track_id, event_type, event_data,
        event_data_size);
      };

  ret = recorder_.CreateVideoTrack(session_id, video_track_id,
                                   video_track_param, video_track_cb);

  assert(ret == 0);

  memset(&track_info, 0x0, sizeof track_info);
  track_info.track_id = video_track_id;
  track_info.type     = TrackType::kVideoTrack;
  tracks.push_back(track_info);

  sessions_.insert(std::make_pair(session_id, tracks));

  TEST_INFO("%s:%s: Exit", TAG, __func__);
  return 0;
}

// This session has one 4K video encode track
int32_t RecorderTest::Session4KEncTrack(const VideoCodecType& type) {

  TEST_INFO("%s:%s: Enter", TAG, __func__);

  std::vector<TrackInfo> tracks;

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

  video_track_param.camera_id   = 0;
  video_track_param.width       = 3840;
  video_track_param.height      = 2160;
  video_track_param.frame_rate  = 30;
  video_track_param.format_type = (type == VideoCodecType::kTypeAVC) ?
      VideoFormat::kAVC : VideoFormat::kHEVC;
  video_track_param.out_device  = 0x01;

  switch (video_track_param.format_type) {
    case VideoFormat::kAVC:
      video_track_param.codec_param.avc.idr_interval = 1;
      video_track_param.codec_param.avc.bitrate      = 10000000;
      video_track_param.codec_param.avc.profile = AVCProfileType::kBaseline;
      video_track_param.codec_param.avc.level   = AVCLevelType::kLevel3;
      video_track_param.codec_param.avc.ratecontrol_type =
          VideoRateControlType::kConstant;
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
      break;
    case VideoFormat::kHEVC:
      video_track_param.codec_param.hevc.idr_interval = 1;
      video_track_param.codec_param.hevc.bitrate      = 10000000;
      video_track_param.codec_param.hevc.profile = HEVCProfileType::kMain;
      video_track_param.codec_param.hevc.level   = HEVCLevelType::kLevel3;
      video_track_param.codec_param.hevc.ratecontrol_type =
          VideoRateControlType::kConstant;
      video_track_param.codec_param.hevc.qp_params.enable_init_qp = true;
      video_track_param.codec_param.hevc.qp_params.init_qp.init_IQP = 56;
      video_track_param.codec_param.hevc.qp_params.init_qp.init_PQP = 56;
      video_track_param.codec_param.hevc.qp_params.init_qp.init_BQP = 56;
      video_track_param.codec_param.hevc.qp_params.init_qp.init_QP_mode = 0x7;
      video_track_param.codec_param.hevc.qp_params.enable_qp_range = true;
      video_track_param.codec_param.hevc.qp_params.qp_range.min_QP = 26;
      video_track_param.codec_param.hevc.qp_params.qp_range.max_QP = 56;
      video_track_param.codec_param.hevc.qp_params.enable_qp_IBP_range = true;
      video_track_param.codec_param.hevc.qp_params.qp_IBP_range.min_IQP = 26;
      video_track_param.codec_param.hevc.qp_params.qp_IBP_range.max_IQP = 56;
      video_track_param.codec_param.hevc.qp_params.qp_IBP_range.min_PQP = 26;
      video_track_param.codec_param.hevc.qp_params.qp_IBP_range.max_PQP = 56;
      video_track_param.codec_param.hevc.qp_params.qp_IBP_range.min_BQP = 26;
      video_track_param.codec_param.hevc.qp_params.qp_IBP_range.max_BQP = 56;
      break;
    default:
      assert(0);
      break;
  }

  TrackCb video_track_cb;
  video_track_cb.data_cb = [&] (uint32_t track_id, std::vector<BufferDescriptor>
      buffers, void *meta_param, TrackMetaParamType meta_type,
      size_t meta_size) { VideoTrack4KEncDataCb(track_id,
      buffers, meta_param, meta_type, meta_size); };

  video_track_cb.event_cb = [&] (uint32_t track_id, EventType event_type,
      void *event_data, size_t data_size) { VideoTrack4KEncEventCb(track_id,
      event_type, event_data, data_size); };

  ret = recorder_.CreateVideoTrack(session_id, video_track_id,
                                   video_track_param, video_track_cb);
#ifdef DUMP_BITSTREAM
  String8 bitstream_filepath;
  const char* type_string = (video_track_param.format_type ==
      VideoFormat::kAVC) ? "h264": "h265";
  String8 extn(type_string);
  bitstream_filepath.appendFormat("/data/track_%d_%dx%d.%s",
      video_track_id, video_track_param.width, video_track_param.height,
      extn.string());
  file_fd1_ = open(bitstream_filepath.string(), O_CREAT | O_WRONLY | O_TRUNC,
      0655);
  assert(file_fd1_ >= 0);
#endif

  TrackInfo track_info;
  memset(&track_info, 0x0, sizeof track_info);
  track_info.track_id = video_track_id;
  track_info.type     = TrackType::kVideoTrack;
  tracks.push_back(track_info);

  sessions_.insert(std::make_pair(session_id, tracks));

  TEST_INFO("%s:%s: Exit", TAG, __func__);
  return 0;
}

// This session has one 1080p video encode and one PCM Audio track.
int32_t RecorderTest::Session1080pEncTrack(const VideoCodecType& type) {

  TEST_INFO("%s:%s: Enter", TAG, __func__);
  std::vector<TrackInfo> tracks;

  SessionCb session_status_cb;
  session_status_cb.event_cb = [&] ( EventType event_type, void *event_data,
      size_t event_data_size) { SessionCallbackHandler(event_type,
      event_data, event_data_size); };

  uint32_t session_id;
  auto ret = recorder_.CreateSession(session_status_cb, &session_id);
  TEST_INFO("%s:%s: sessions_id = %d", TAG, __func__, session_id);

  //Create Video track (1080p Encode)
  uint32_t video_track_id = 1;
  VideoTrackCreateParam video_track_param;
  memset(&video_track_param, 0x0, sizeof video_track_param);

  //TODO: change it vector.
  video_track_param.camera_id   = 0;
  video_track_param.width       = 1920;
  video_track_param.height      = 1080;
  video_track_param.frame_rate  = 30;
  video_track_param.format_type = (type == VideoCodecType::kTypeAVC) ?
      VideoFormat::kAVC : VideoFormat::kHEVC;
  video_track_param.out_device  = 0x1;

  switch (video_track_param.format_type) {
    case VideoFormat::kAVC:
      video_track_param.codec_param.avc.idr_interval = 1;
      video_track_param.codec_param.avc.bitrate      = 10000000;
      video_track_param.codec_param.avc.profile = AVCProfileType::kBaseline;
      video_track_param.codec_param.avc.level   = AVCLevelType::kLevel3;
      video_track_param.codec_param.avc.ratecontrol_type =
          VideoRateControlType::kVariable;
      video_track_param.codec_param.avc.qp_params.enable_init_qp = true;
      video_track_param.codec_param.avc.qp_params.init_qp.init_IQP = 26;
      video_track_param.codec_param.avc.qp_params.init_qp.init_PQP = 26;
      video_track_param.codec_param.avc.qp_params.init_qp.init_BQP = 26;
      video_track_param.codec_param.avc.qp_params.init_qp.init_QP_mode = 0x7;
      video_track_param.codec_param.avc.qp_params.enable_qp_range = true;
      video_track_param.codec_param.avc.qp_params.qp_range.min_QP = 1;
      video_track_param.codec_param.avc.qp_params.qp_range.max_QP = 51;
      video_track_param.codec_param.avc.qp_params.enable_qp_IBP_range = true;
      video_track_param.codec_param.avc.qp_params.qp_IBP_range.min_IQP = 1;
      video_track_param.codec_param.avc.qp_params.qp_IBP_range.max_IQP = 51;
      video_track_param.codec_param.avc.qp_params.qp_IBP_range.min_PQP = 1;
      video_track_param.codec_param.avc.qp_params.qp_IBP_range.max_PQP = 51;
      video_track_param.codec_param.avc.qp_params.qp_IBP_range.min_BQP = 1;
      video_track_param.codec_param.avc.qp_params.qp_IBP_range.max_BQP = 51;
      break;
    case VideoFormat::kHEVC:
      video_track_param.codec_param.hevc.idr_interval = 1;
      video_track_param.codec_param.hevc.bitrate      = 10000000;
      video_track_param.codec_param.hevc.profile = HEVCProfileType::kMain;
      video_track_param.codec_param.hevc.level   = HEVCLevelType::kLevel3;
      video_track_param.codec_param.hevc.ratecontrol_type =
          VideoRateControlType::kVariable;
      video_track_param.codec_param.hevc.qp_params.enable_init_qp = true;
      video_track_param.codec_param.hevc.qp_params.init_qp.init_IQP = 26;
      video_track_param.codec_param.hevc.qp_params.init_qp.init_PQP = 26;
      video_track_param.codec_param.hevc.qp_params.init_qp.init_BQP = 26;
      video_track_param.codec_param.hevc.qp_params.init_qp.init_QP_mode = 0x7;
      video_track_param.codec_param.hevc.qp_params.enable_qp_range = true;
      video_track_param.codec_param.hevc.qp_params.qp_range.min_QP = 1;
      video_track_param.codec_param.hevc.qp_params.qp_range.max_QP = 51;
      video_track_param.codec_param.hevc.qp_params.enable_qp_IBP_range = true;
      video_track_param.codec_param.hevc.qp_params.qp_IBP_range.min_IQP = 1;
      video_track_param.codec_param.hevc.qp_params.qp_IBP_range.max_IQP = 51;
      video_track_param.codec_param.hevc.qp_params.qp_IBP_range.min_PQP = 1;
      video_track_param.codec_param.hevc.qp_params.qp_IBP_range.max_PQP = 51;
      video_track_param.codec_param.hevc.qp_params.qp_IBP_range.min_BQP = 1;
      video_track_param.codec_param.hevc.qp_params.qp_IBP_range.max_BQP = 51;
      break;
    default:
      assert(0);
      break;
  }
  TrackCb video_track_cb;
  video_track_cb.data_cb = [&] (uint32_t track_id, std::vector<BufferDescriptor>
      buffers, void *meta_param, TrackMetaParamType meta_type,
      size_t meta_size) { VideoTrack1080pEncDataCb1(track_id,
      buffers, meta_param, meta_type, meta_size); };

  video_track_cb.event_cb = [&] (uint32_t track_id, EventType event_type,
      void *event_data, size_t data_size) { VideoTrack1080pEncEventCb(track_id,
      event_type, event_data, data_size); };

  ret = recorder_.CreateVideoTrack(session_id, video_track_id,
                                   video_track_param, video_track_cb);
  assert(ret == 0);

#ifdef DUMP_BITSTREAM
  String8 bitstream_filepath;
  const char* type_string = (video_track_param.format_type ==
      VideoFormat::kAVC) ? "h264": "h265";
  String8 extn(type_string);
  bitstream_filepath.appendFormat("/data/track_%d_%dx%d.%s",
      video_track_id, video_track_param.width, video_track_param.height,
      extn.string());
  file_fd1_ = open(bitstream_filepath.string(), O_CREAT | O_WRONLY | O_TRUNC,
      0655);
  assert(file_fd1_ >= 0);
#endif

  TrackInfo track_info;
  memset(&track_info, 0x0, sizeof track_info);
  track_info.track_id = video_track_id;
  track_info.type     = TrackType::kVideoTrack;
  tracks.push_back(track_info);

  //Create Audio track.
  uint32_t audio_track_id = 101;
  AudioTrackCreateParam audio_track_params;
  memset(&audio_track_params, 0x0, sizeof audio_track_params);

  audio_track_params.in_device[0]   = 0;
  audio_track_params.num_in_devices = 1;
  audio_track_params.sample_rate    = 48000;
  audio_track_params.channels       = 1;
  audio_track_params.bit_depth      = 16;
  audio_track_params.format_type    = AudioFormat::kPCM;
  audio_track_params.out_device     = 0;
  audio_track_params.flags          = 0;

  TrackCb audio_track_cb;
  audio_track_cb.data_cb =
      [this] (uint32_t track_id, std::vector<BufferDescriptor> buffers,
              void* meta_param, TrackMetaParamType meta_type, size_t meta_size)
              -> void {
        AudioTrackDataCb(track_id, buffers, meta_param, meta_type, meta_size);
      };

  audio_track_cb.event_cb =
      [this] (uint32_t track_id, EventType event_type, void *event_data,
              size_t event_data_size) -> void {
        AudioTrackEventCb(track_id, event_type, event_data, event_data_size);
      };

  ret = recorder_.CreateAudioTrack(session_id, audio_track_id,
                                      audio_track_params, audio_track_cb);
  assert(ret == NO_ERROR);

  memset(&track_info, 0x0, sizeof track_info);
  track_info.track_id = audio_track_id;
  track_info.type     = TrackType::kAudioTrack;
  tracks.push_back(track_info);

  sessions_.insert({session_id, tracks});

  ret = wav_.Configure(kDefaultAudioFilenamePrefix, audio_track_params);
  assert(ret == NO_ERROR);

  TEST_INFO("%s:%s: Exit", TAG, __func__);
  return 0;
}

// This session has one 4K YUV and one 1080p video encode track
int32_t RecorderTest::Session4KYUVAnd1080pEncTracks(const VideoCodecType& type) {

  TEST_INFO("%s:%s: Enter", TAG, __func__);

  std::vector<TrackInfo> tracks;

  SessionCb session_status_cb;
  session_status_cb.event_cb = [&] ( EventType event_type, void *event_data,
      size_t event_data_size) { SessionCallbackHandler(event_type,
      event_data, event_data_size); };

  uint32_t session_id;
  auto ret = recorder_.CreateSession(session_status_cb, &session_id);
  TEST_INFO("%s:%s: sessions_id = %d", TAG, __func__, session_id);

  //Create Video track (1080p Encode)
  uint32_t video_track_id = 1;
  VideoTrackCreateParam video_track_param;
  memset(&video_track_param, 0x0, sizeof video_track_param);

  //TODO: change it vector.
  video_track_param.camera_id   = 0;
  video_track_param.width       = 1920;
  video_track_param.height      = 1080;
  video_track_param.frame_rate  = 30;
  video_track_param.format_type = (type == VideoCodecType::kTypeAVC) ?
      VideoFormat::kAVC : VideoFormat::kHEVC;
  video_track_param.out_device  = 0x01;

  switch (video_track_param.format_type) {
    case VideoFormat::kAVC:
      video_track_param.codec_param.avc.idr_interval = 1;
      video_track_param.codec_param.avc.bitrate      = 10000000;
      video_track_param.codec_param.avc.profile = AVCProfileType::kBaseline;
      video_track_param.codec_param.avc.level   = AVCLevelType::kLevel3;
      video_track_param.codec_param.avc.ratecontrol_type =
          VideoRateControlType::kVariable;
      video_track_param.codec_param.avc.qp_params.enable_init_qp = true;
      video_track_param.codec_param.avc.qp_params.init_qp.init_IQP = 26;
      video_track_param.codec_param.avc.qp_params.init_qp.init_PQP = 26;
      video_track_param.codec_param.avc.qp_params.init_qp.init_BQP = 26;
      video_track_param.codec_param.avc.qp_params.init_qp.init_QP_mode = 0x7;
      video_track_param.codec_param.avc.qp_params.enable_qp_range = true;
      video_track_param.codec_param.avc.qp_params.qp_range.min_QP = 1;
      video_track_param.codec_param.avc.qp_params.qp_range.max_QP = 51;
      video_track_param.codec_param.avc.qp_params.enable_qp_IBP_range = true;
      video_track_param.codec_param.avc.qp_params.qp_IBP_range.min_IQP = 1;
      video_track_param.codec_param.avc.qp_params.qp_IBP_range.max_IQP = 51;
      video_track_param.codec_param.avc.qp_params.qp_IBP_range.min_PQP = 1;
      video_track_param.codec_param.avc.qp_params.qp_IBP_range.max_PQP = 51;
      video_track_param.codec_param.avc.qp_params.qp_IBP_range.min_BQP = 1;
      video_track_param.codec_param.avc.qp_params.qp_IBP_range.max_BQP = 51;
      break;
    case VideoFormat::kHEVC:
      video_track_param.codec_param.hevc.idr_interval = 1;
      video_track_param.codec_param.hevc.bitrate      = 10000000;
      video_track_param.codec_param.hevc.profile = HEVCProfileType::kMain;
      video_track_param.codec_param.hevc.level   = HEVCLevelType::kLevel3;
      video_track_param.codec_param.hevc.ratecontrol_type =
          VideoRateControlType::kVariable;
      video_track_param.codec_param.hevc.qp_params.enable_init_qp = true;
      video_track_param.codec_param.hevc.qp_params.init_qp.init_IQP = 26;
      video_track_param.codec_param.hevc.qp_params.init_qp.init_PQP = 26;
      video_track_param.codec_param.hevc.qp_params.init_qp.init_BQP = 26;
      video_track_param.codec_param.hevc.qp_params.init_qp.init_QP_mode = 0x7;
      video_track_param.codec_param.hevc.qp_params.enable_qp_range = true;
      video_track_param.codec_param.hevc.qp_params.qp_range.min_QP = 1;
      video_track_param.codec_param.hevc.qp_params.qp_range.max_QP = 51;
      video_track_param.codec_param.hevc.qp_params.enable_qp_IBP_range = true;
      video_track_param.codec_param.hevc.qp_params.qp_IBP_range.min_IQP = 1;
      video_track_param.codec_param.hevc.qp_params.qp_IBP_range.max_IQP = 51;
      video_track_param.codec_param.hevc.qp_params.qp_IBP_range.min_PQP = 1;
      video_track_param.codec_param.hevc.qp_params.qp_IBP_range.max_PQP = 51;
      video_track_param.codec_param.hevc.qp_params.qp_IBP_range.min_BQP = 1;
      video_track_param.codec_param.hevc.qp_params.qp_IBP_range.max_BQP = 51;
      break;
    default:
      assert(0);
      break;
  }

  TrackCb video_track_cb;
  video_track_cb.data_cb = [&] (uint32_t track_id, std::vector<BufferDescriptor>
      buffers, void *meta_param, TrackMetaParamType meta_type,
      size_t meta_size) { VideoTrack1080pEncDataCb1(track_id,
      buffers, meta_param, meta_type, meta_size); };

  video_track_cb.event_cb = [&] (uint32_t track_id, EventType event_type,
      void *event_data, size_t data_size) { VideoTrack1080pEncEventCb(track_id,
      event_type, event_data, data_size); };

  ret = recorder_.CreateVideoTrack(session_id, video_track_id,
                                   video_track_param, video_track_cb);
  assert(ret == 0);
  TrackInfo track_info;
  memset(&track_info, 0x0, sizeof track_info);
  track_info.track_id = video_track_id;
  track_info.type     = TrackType::kVideoTrack;
  tracks.push_back(track_info);

#ifdef DUMP_BITSTREAM
  String8 bitstream_filepath;
  const char* type_string = (video_track_param.format_type ==
      VideoFormat::kAVC) ? "h264": "h265";
  String8 extn(type_string);
  bitstream_filepath.appendFormat("/data/track_%d_%dx%d.%s",
      video_track_id, video_track_param.width, video_track_param.height,
      extn.string());
  file_fd1_ = open(bitstream_filepath.string(), O_CREAT | O_WRONLY | O_TRUNC,
      0655);
  assert(file_fd1_ >= 0);
#endif
  // Create another video track - 4K YUV
  video_track_id = 2;
  memset(&video_track_param, 0x0, sizeof video_track_param);

  video_track_param.camera_id   = 0;
  video_track_param.width       = 3840;
  video_track_param.height      = 2160;
  video_track_param.frame_rate  = 30;
  video_track_param.format_type = VideoFormat::kYUV;
  video_track_param.out_device  = 0x01;

  memset(&video_track_cb, 0x0, sizeof (video_track_cb));
  video_track_cb.data_cb = [&] (uint32_t track_id, std::vector<BufferDescriptor>
      buffers, void *meta_param, TrackMetaParamType meta_type,
      size_t meta_size) { VideoTrack4KYUVDataCb(track_id,
      buffers, meta_param, meta_type, meta_size); };

  video_track_cb.event_cb = [&] (uint32_t track_id, EventType event_type,
      void *event_data, size_t event_data_size)
      { VideoTrack4KYUVEventCb(track_id, event_type, event_data,
        event_data_size);
      };

  ret = recorder_.CreateVideoTrack(session_id, video_track_id,
                                   video_track_param, video_track_cb);
  assert(ret == 0);

  memset(&track_info, 0x0, sizeof track_info);
  track_info.track_id = video_track_id;
  track_info.type     = TrackType::kVideoTrack;
  tracks.push_back(track_info);

  sessions_.insert(std::make_pair(session_id, tracks));

  TEST_INFO("%s:%s: Exit", TAG, __func__);
  return 0;
}

// This session has two 1080p video encode tracks.
int32_t RecorderTest::SessionTwo1080pEncTracks(const VideoCodecType& type) {

  TEST_INFO("%s:%s: Enter", TAG, __func__);

  std::vector<TrackInfo> tracks;

  SessionCb session_status_cb;
  session_status_cb.event_cb = [&] ( EventType event_type, void *event_data,
      size_t event_data_size) { SessionCallbackHandler(event_type,
      event_data, event_data_size); };

  uint32_t session_id;
  auto ret = recorder_.CreateSession(session_status_cb, &session_id);
  TEST_INFO("%s:%s: sessions_id = %d", TAG, __func__, session_id);

  //Create Video track - 1080p Encode
  uint32_t video_track_id = 1;
  VideoTrackCreateParam video_track_param;
  memset(&video_track_param, 0x0, sizeof video_track_param);

  //TODO: change it vector.
  video_track_param.camera_id   = 0;
  video_track_param.width       = 1920;
  video_track_param.height      = 1080;
  video_track_param.frame_rate  = 30;
  video_track_param.format_type = (type == VideoCodecType::kTypeAVC) ?
      VideoFormat::kAVC : VideoFormat::kHEVC;
  video_track_param.out_device  = 0x01;

  switch (video_track_param.format_type) {
    case VideoFormat::kAVC:
      video_track_param.codec_param.avc.idr_interval = 1;
      video_track_param.codec_param.avc.bitrate      = 10000000;
      video_track_param.codec_param.avc.profile = AVCProfileType::kBaseline;
      video_track_param.codec_param.avc.level   = AVCLevelType::kLevel3;
      video_track_param.codec_param.avc.ratecontrol_type =
          VideoRateControlType::kVariable;
      video_track_param.codec_param.avc.qp_params.enable_init_qp = true;
      video_track_param.codec_param.avc.qp_params.init_qp.init_IQP = 26;
      video_track_param.codec_param.avc.qp_params.init_qp.init_PQP = 26;
      video_track_param.codec_param.avc.qp_params.init_qp.init_BQP = 26;
      video_track_param.codec_param.avc.qp_params.init_qp.init_QP_mode = 0x7;
      video_track_param.codec_param.avc.qp_params.enable_qp_range = true;
      video_track_param.codec_param.avc.qp_params.qp_range.min_QP = 1;
      video_track_param.codec_param.avc.qp_params.qp_range.max_QP = 51;
      video_track_param.codec_param.avc.qp_params.enable_qp_IBP_range = true;
      video_track_param.codec_param.avc.qp_params.qp_IBP_range.min_IQP = 1;
      video_track_param.codec_param.avc.qp_params.qp_IBP_range.max_IQP = 51;
      video_track_param.codec_param.avc.qp_params.qp_IBP_range.min_PQP = 1;
      video_track_param.codec_param.avc.qp_params.qp_IBP_range.max_PQP = 51;
      video_track_param.codec_param.avc.qp_params.qp_IBP_range.min_BQP = 1;
      video_track_param.codec_param.avc.qp_params.qp_IBP_range.max_BQP = 51;
      break;
    case VideoFormat::kHEVC:
      video_track_param.codec_param.hevc.idr_interval = 1;
      video_track_param.codec_param.hevc.bitrate      = 10000000;
      video_track_param.codec_param.hevc.profile = HEVCProfileType::kMain;
      video_track_param.codec_param.hevc.level   = HEVCLevelType::kLevel3;
      video_track_param.codec_param.hevc.ratecontrol_type =
          VideoRateControlType::kVariable;
      video_track_param.codec_param.hevc.qp_params.enable_init_qp = true;
      video_track_param.codec_param.hevc.qp_params.init_qp.init_IQP = 26;
      video_track_param.codec_param.hevc.qp_params.init_qp.init_PQP = 26;
      video_track_param.codec_param.hevc.qp_params.init_qp.init_BQP = 26;
      video_track_param.codec_param.hevc.qp_params.init_qp.init_QP_mode = 0x7;
      video_track_param.codec_param.hevc.qp_params.enable_qp_range = true;
      video_track_param.codec_param.hevc.qp_params.qp_range.min_QP = 1;
      video_track_param.codec_param.hevc.qp_params.qp_range.max_QP = 51;
      video_track_param.codec_param.hevc.qp_params.enable_qp_IBP_range = true;
      video_track_param.codec_param.hevc.qp_params.qp_IBP_range.min_IQP = 1;
      video_track_param.codec_param.hevc.qp_params.qp_IBP_range.max_IQP = 51;
      video_track_param.codec_param.hevc.qp_params.qp_IBP_range.min_PQP = 1;
      video_track_param.codec_param.hevc.qp_params.qp_IBP_range.max_PQP = 51;
      video_track_param.codec_param.hevc.qp_params.qp_IBP_range.min_BQP = 1;
      video_track_param.codec_param.hevc.qp_params.qp_IBP_range.max_BQP = 51;
      break;
    default:
      assert(0);
      break;
  }

  TrackCb video_track_cb;
  video_track_cb.data_cb = [&] (uint32_t track_id, std::vector<BufferDescriptor>
      buffers, void *meta_param, TrackMetaParamType meta_type,
      size_t meta_size) { VideoTrack1080pEncDataCb1(track_id,
      buffers, meta_param, meta_type, meta_size); };

  video_track_cb.event_cb = [&] (uint32_t track_id, EventType event_type,
      void *event_data, size_t data_size) { VideoTrack1080pEncEventCb(track_id,
      event_type, event_data, data_size); };

  ret = recorder_.CreateVideoTrack(session_id, video_track_id,
                                   video_track_param, video_track_cb);
  assert(ret == 0);
  TrackInfo track_info;
  memset(&track_info, 0x0, sizeof track_info);
  track_info.track_id = video_track_id;
  track_info.type     = TrackType::kVideoTrack;
  tracks.push_back(track_info);

#ifdef DUMP_BITSTREAM
  String8 bitstream_filepath;
  const char* type_string = (video_track_param.format_type ==
      VideoFormat::kAVC) ? "h264": "h265";
  String8 extn(type_string);
  bitstream_filepath.appendFormat("/data/track_%d_%dx%d.%s",
      video_track_id, video_track_param.width, video_track_param.height,
      extn.string());
  file_fd1_ = open(bitstream_filepath.string(), O_CREAT | O_WRONLY | O_TRUNC,
      0655);
  assert(file_fd1_ >= 0);
#endif
  // Create another 1080p video track.
  video_track_id = 2;

  memset(&video_track_cb, 0x0, sizeof (video_track_cb));
  video_track_cb.data_cb = [&] (uint32_t track_id, std::vector<BufferDescriptor>
      buffers, void *meta_param, TrackMetaParamType meta_type,
      size_t meta_size) { VideoTrack1080pEncDataCb2(track_id,
      buffers, meta_param, meta_type, meta_size); };

  video_track_cb.event_cb = [&] (uint32_t track_id, EventType event_type,
      void *event_data, size_t event_data_size)
      { VideoTrack1080pEncEventCb(track_id, event_type, event_data,
        event_data_size);
      };

  ret = recorder_.CreateVideoTrack(session_id, video_track_id,
                                   video_track_param, video_track_cb);
  assert(ret == 0);

  memset(&track_info, 0x0, sizeof track_info);
  track_info.track_id = video_track_id;
  track_info.type     = TrackType::kVideoTrack;
  tracks.push_back(track_info);

#ifdef DUMP_BITSTREAM
  bitstream_filepath.clear();
  bitstream_filepath.appendFormat("/data/track_%d_%dx%d.%s",
      video_track_id, video_track_param.width, video_track_param.height,
      extn.string());
  file_fd2_ = open(bitstream_filepath.string(), O_CREAT | O_WRONLY | O_TRUNC,
      0655);
  assert(file_fd2_ >= 0);
#endif
  sessions_.insert(std::make_pair(session_id, tracks));

  TEST_INFO("%s:%s: Exit", TAG, __func__);
  return 0;
}

void RecorderTest::CreateAudioOnlySession() {

  TEST_INFO("%s:%s: Enter", TAG, __func__);
  std::vector<TrackInfo> tracks;

  SessionCb session_status_cb;
  session_status_cb.event_cb =
      [&] (EventType event_type, void *event_data, size_t event_data_size) {
        SessionCallbackHandler(event_type, event_data, event_data_size);
      };

  uint32_t session_id;
  auto result = recorder_.CreateSession(session_status_cb, &session_id);
  assert(result == NO_ERROR);
  TEST_INFO("%s:%s: sessions_id = %d", TAG, __func__, session_id);

  uint32_t audio_track_id = 101;
  AudioTrackCreateParam audio_track_params;
  memset(&audio_track_params, 0x0, sizeof audio_track_params);

  audio_track_params.in_device[0]   = 0;
  audio_track_params.num_in_devices = 1;
  audio_track_params.sample_rate    = 48000;
  audio_track_params.channels       = 1;
  audio_track_params.bit_depth      = 16;
  audio_track_params.format_type    = AudioFormat::kPCM;
  audio_track_params.out_device     = 0;
  audio_track_params.flags          = 0;

  TrackCb audio_track_cb;
  audio_track_cb.data_cb =
      [this] (uint32_t track_id, std::vector<BufferDescriptor> buffers,
              void* meta_param, TrackMetaParamType meta_type, size_t meta_size)
              -> void {
        AudioTrackDataCb(track_id, buffers, meta_param, meta_type, meta_size);
      };

  audio_track_cb.event_cb =
      [this] (uint32_t track_id, EventType event_type, void *event_data,
              size_t event_data_size) -> void {
        AudioTrackEventCb(track_id, event_type, event_data, event_data_size);
      };

  result = recorder_.CreateAudioTrack(session_id, audio_track_id,
                                      audio_track_params, audio_track_cb);
  assert(result == NO_ERROR);

  TrackInfo track_info;
  memset(&track_info, 0x0, sizeof track_info);
  track_info.track_id = audio_track_id;
  track_info.type     = TrackType::kAudioTrack;
  tracks.push_back(track_info);

  sessions_.insert({session_id, tracks});

  result = wav_.Configure(kDefaultAudioFilenamePrefix, audio_track_params);
  assert(result == NO_ERROR);

  TEST_INFO("%s:%s: Exit", TAG, __func__);
}

int32_t RecorderTest::StartSession() {

  TEST_INFO("%s:%s: Enter", TAG, __func__);
  session_iter_ it = sessions_.begin();
  uint32_t session_id = it->first;
  auto result = recorder_.StartSession(session_id);
  assert(result == NO_ERROR);

  bool has_audio = false;
  for (auto track_info : it->second) {
    if (track_info.type == TrackType::kAudioTrack) has_audio = true;
  }
  if (has_audio) {
    result = wav_.Open();
    assert(result == NO_ERROR);
  }

  TEST_INFO("%s:%s: Exit", TAG, __func__);
}

int32_t RecorderTest::StopSession() {

  TEST_INFO("%s:%s: Enter", TAG, __func__);
  session_iter_ it = sessions_.begin();
  uint32_t session_id = it->first;
  auto result = recorder_.StopSession(session_id, true /*flush buffers*/);
  assert(result == NO_ERROR);

  bool has_audio = false;
  for (auto track_info : it->second) {
    if (track_info.type == TrackType::kAudioTrack) has_audio = true;
  }
  if (has_audio)
    wav_.Close();

  TEST_INFO("%s:%s: Exit", TAG, __func__);
}

int32_t RecorderTest::PauseSession() {

  TEST_INFO("%s:%s: Enter", TAG, __func__);
  session_iter_ it = sessions_.begin();
  uint32_t session_id = it->first;
  auto ret = recorder_.PauseSession(session_id);
  assert(ret == 0);
  TEST_INFO("%s:%s: Exit", TAG, __func__);
}

int32_t RecorderTest::SetParams() {

  TEST_INFO("%s:%s: Enter", TAG, __func__);
  int32_t ret = 0;
  session_iter_ it = sessions_.begin();
  uint32_t session_id = it->first;
  CodecParamType param_type;

  int32_t input;
  int32_t value;
  do {
    printf("\n");
    printf("****** Set Dynamic Codec Param *******\n" );
    printf("  1. bitrate \n" );
    printf("  2. framerate \n" );
    printf("  3. insert-idr \n" );
    printf("  4. idr interval \n" );
    printf("  5. ltr mark \n" );
    printf("  6. ltr use \n" );
    printf("  0. exit \n");
    printf("\n");
    printf("Enter set param option\n");
    scanf("%d", &input);

    switch(input) {
      case 0:
        break;
      case 1:
        printf("Enter bitrate value\n");
        scanf("%d", &value);
        param_type = CodecParamType::kBitRateType;
        ret = recorder_.SetVideoTrackParam(session_id, 1, param_type, &value,
                                              sizeof(value));
        break;
      case 2:
        printf("Enter fps value\n");
        scanf("%d", &value);
        param_type = CodecParamType::kFrameRateType;
        ret = recorder_.SetVideoTrackParam(session_id, 1, param_type, &value,
                                              sizeof(value));
        break;
      case 3:
        printf("Insert idr frame\n");
        param_type = CodecParamType::kInsertIDRType;
        ret = recorder_.SetVideoTrackParam(session_id, 1, param_type, &value,
                                              sizeof(value));
        break;
      case 4:
        printf("Enter number of P frame value\n");
        scanf("%d", &value);
        param_type = CodecParamType::kIDRIntervalType;
        VideoEncIdrInterval idr_interval;
        idr_interval.num_pframes = value;
        idr_interval.num_bframes = 0;
        idr_interval.idr_period = 0;
        ret = recorder_.SetVideoTrackParam(session_id, 1, param_type,
                                           &idr_interval, sizeof(idr_interval));
        break;
      case 5:
        printf("Enter ltr mark id value\n");
        scanf("%d", &value);
        param_type = CodecParamType::kMarkLtrType;
        ret = recorder_.SetVideoTrackParam(session_id, 1, param_type, &value,
                                              sizeof(value));
        break;
      case 6:
        printf("Enter ltr use id value\n");
        scanf("%d", &value);
        param_type = CodecParamType::kUseLtrType;
        VideoEncLtrUse ltr_use;
        ltr_use.id = value;
        ltr_use.frame = 5;
        ret = recorder_.SetVideoTrackParam(session_id, 1, param_type, &ltr_use,
                                              sizeof(ltr_use));
        break;
      default:
         printf("Wrong value entered(%d)\n", input);
         input = 0;
    }
    if(input) {
      assert(ret == 0);
    }
  } while(input);

  TEST_INFO("%s:%s: Exit", TAG, __func__);
}

int32_t RecorderTest::ResumeSession() {

  TEST_INFO("%s:%s: Enter", TAG, __func__);
  session_iter_ it = sessions_.begin();
  uint32_t session_id = it->first;
  auto ret = recorder_.ResumeSession(session_id);
  assert(ret == 0);
  TEST_INFO("%s:%s: Exit", TAG, __func__);
}

int32_t RecorderTest::EnableOverlay() {

  TEST_INFO("%s:%s: Enter", TAG, __func__);
  // Enable overlay on all existing video tracks.

  // Create Overlay object
  OverlayParam object_params;
  memset(&object_params, 0x0, sizeof object_params);
  object_params.type = OverlayType::kStaticImage;
  object_params.location = OverlayLocationType::kBottomRight;
  std::string str("/etc/overlay_test.rgba");
  str.copy(object_params.image_info.image_location, str.length());
  object_params.image_info.width  = 451;
  object_params.image_info.height = 109;
  session_iter_ it = sessions_.begin();

  for (auto track_info : it->second) {

    if (track_info.type == TrackType::kVideoTrack) {
      std::vector<uint32_t> object_ids;
      uint32_t object_id;
      auto ret = recorder_.CreateOverlayObject(track_info.track_id,
                                               object_params, &object_id);
      assert(ret == 0);

      ret = recorder_.SetOverlay(track_info.track_id, object_id);
      assert(ret == 0);

      // One track can have multiple types of overlay.
      object_ids.push_back(object_id);
      overlay_ids_.insert(std::make_pair(track_info.track_id, object_ids));
    }
  }
  TEST_INFO("%s:%s: Exit", TAG, __func__);
}

int32_t RecorderTest::DisableOverlay() {

  TEST_INFO("%s:%s: Enter", TAG, __func__);
  session_iter_ it = sessions_.begin();
  for (auto track_info : it->second) {

    if (track_info.type == TrackType::kVideoTrack) {
      std::vector<uint32_t> overlay_ids;
      overlay_ids = overlay_ids_[track_info.track_id];
      for (auto overlay_id : overlay_ids) {
        TEST_INFO("%s:%s: TrackId(%d):overlayId(%d) to Disable!", TAG, __func__,
            track_info.track_id, overlay_id);
        auto ret = recorder_.RemoveOverlay(track_info.track_id, overlay_id);
        assert(ret == 0);
        ret = recorder_.DeleteOverlayObject(track_info.track_id, overlay_id);
        assert(ret == 0);
      }
    }
  }
  overlay_ids_.clear();
  TEST_INFO("%s:%s: Exit", TAG, __func__);
}

int32_t RecorderTest::DeleteSession()
{
  TEST_INFO("%s:%s: Enter", TAG, __func__);
  session_iter_ it = sessions_.begin();
  uint32_t session_id = it->first;
  // Delete all the tracks associated to session.
  status_t ret;
  for (auto track_info : it->second) {
      if (track_info.type == TrackType::kAudioTrack)
        ret = recorder_.DeleteAudioTrack(session_id, track_info.track_id);
      else
        ret = recorder_.DeleteVideoTrack(session_id, track_info.track_id);
      assert(ret == 0);
  }
  // Once all tracks are deleted successfully delete session.
  ret = recorder_.DeleteSession(session_id);

  sessions_.erase(it);
  TEST_INFO("%s:%s: Exit", TAG, __func__);
  return 0;
}

void RecorderTest::SnapshotCb(uint32_t camera_id,
                              uint32_t image_sequence_count,
                              BufferDescriptor buffer) {

  String8 file_path;
  size_t written_len;
  static uint32_t snapshot_count = 0;
  file_path.appendFormat("/data/snapshot_%u.jpg", snapshot_count);

  FILE *file = fopen(file_path.string(), "w+");
  if (!file) {
    ALOGE("%s:%s: Unable to open file(%s)", TAG, __func__,
        file_path.string());
    goto FAIL;
  }

  written_len = fwrite(buffer.data, sizeof(uint8_t), buffer.size, file);
  TEST_INFO("%s:%s: written_len =%d", TAG, __func__, written_len);
  if (buffer.size != written_len) {
    ALOGE("%s:%s: Bad Write error (%d):(%s)\n", TAG, __func__, errno,
          strerror(errno));
    goto FAIL;
  }
  TEST_INFO("%s:%s: Buffer(0x%x) Size(%u) Stored@(%s)\n", TAG, __func__,
            buffer.data, written_len, file_path.string());

  snapshot_count++;

FAIL:
  if (file != NULL) {
    fclose(file);
  }
  // Return buffer back to recorder service.
  recorder_.ReturnImageCaptureBuffer(camera_id, buffer);
}

void RecorderTest::RecorderCallbackHandler(EventType event_type,
                                           void *event_data,
                                           size_t event_data_size) {
  TEST_INFO("%s:%s: Enter", TAG, __func__);
  TEST_INFO("%s:%s: Exit", TAG, __func__);
}

void RecorderTest::SessionCallbackHandler(EventType event_type,
                                          void *event_data,
                                          size_t event_data_size) {
  TEST_INFO("%s:%s: Enter", TAG, __func__);
  TEST_INFO("%s:%s: Exit", TAG, __func__);
}

void RecorderTest::AudioTrackDataCb(uint32_t track_id,
                                    std::vector<BufferDescriptor> buffers,
                                    void *meta_param,
                                    TrackMetaParamType meta_type,
                                    size_t meta_size) {
  TEST_DBG("%s:%s: Enter", TAG, __func__);

  for (const BufferDescriptor& buffer : buffers) {
    int result = wav_.Write(buffer);
    assert(result == 0);
  }

  // Return buffers back to service.
  session_iter_ it = sessions_.begin();
  uint32_t session_id = it->first;
  auto ret = recorder_.ReturnTrackBuffer(session_id, track_id, buffers);
  assert(ret == 0);

  TEST_DBG("%s:%s: Exit", TAG, __func__);
}

void RecorderTest::AudioTrackEventCb(uint32_t track_id, EventType event_type,
                                     void *event_data,
                                     size_t event_data_size) {
  TEST_INFO("%s:%s: Enter", TAG, __func__);

  if (event_type == EventType::kError)
    assert(false);

  TEST_INFO("%s:%s: Exit", TAG, __func__);
}

void RecorderTest::VideoTrack4KYUVDataCb(uint32_t track_id,
                                         std::vector<BufferDescriptor> buffers,
                                         void *meta_param,
                                         TrackMetaParamType meta_type,
                                         size_t meta_size) {

  TEST_DBG("%s:%s: Enter", TAG, __func__);

  TEST_DBG("%s:%s: meta_type=%d", TAG, __func__, meta_type);
  MetaInfo* meta_data;
  if (meta_type == TrackMetaParamType::kCamBufMetaData) {
    meta_data = static_cast<MetaInfo*>(meta_param);
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
#ifdef DUMP_YUV_FRAMES
  for (const BufferDescriptor& buffer : buffers) {
    DumpYUVFrame(track_id, meta_data, buffer);
  }
#endif
  // Return buffers back to service.
  session_iter_ it = sessions_.begin();
  uint32_t session_id = it->first;
  auto ret = recorder_.ReturnTrackBuffer(session_id, track_id, buffers);
  assert(ret == 0);
  TEST_DBG("%s:%s: Exit", TAG, __func__);
}

void RecorderTest::VideoTrack4KYUVEventCb(uint32_t track_id,
                                          EventType event_type,
                                          void *event_data,
                                          size_t event_data_size) {
  TEST_INFO("%s:%s: Enter", TAG, __func__);
  TEST_INFO("%s:%s: Exit", TAG, __func__);
}

void RecorderTest::VideoTrack1080pYUVDataCb(uint32_t track_id,
                                            std::vector<BufferDescriptor>
                                            buffers, void *meta_param,
                                            TrackMetaParamType meta_type,
                                            size_t meta_size) {

  TEST_DBG("%s:%s: Enter", TAG, __func__);

  MetaInfo* meta_data;
  if (meta_type == TrackMetaParamType::kCamBufMetaData) {
    meta_data = static_cast<MetaInfo*>(meta_param);
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
#ifdef DUMP_YUV_FRAMES
  for (const BufferDescriptor& buffer : buffers) {
    auto ret = DumpYUVFrame(track_id, meta_data, buffer);
  }
#endif

  // Return buffers back to service.
  session_iter_ it = sessions_.begin();
  uint32_t session_id = it->first;
  auto ret = recorder_.ReturnTrackBuffer(session_id, track_id, buffers);
  assert(ret == 0);
  TEST_DBG("%s:%s: Exit", TAG, __func__);
}

void RecorderTest::VideoTrack1080pYUVEventCb(uint32_t track_id,
                                          EventType event_type,
                                          void *event_data,
                                          size_t event_data_size) {
  TEST_INFO("%s:%s: Enter", TAG, __func__);
  TEST_INFO("%s:%s: Exit", TAG, __func__);
}

void RecorderTest::VideoTrack4KEncDataCb(uint32_t track_id,
                                    std::vector<BufferDescriptor> buffers,
                                    void *meta_param,
                                    TrackMetaParamType meta_type,
                                    size_t meta_size) {

  TEST_DBG("%s:%s: Enter", TAG, __func__);

#ifdef DUMP_BITSTREAM
  DumpBitStream(buffers, file_fd1_);
#endif
  // Return buffers back to service.
  session_iter_ it = sessions_.begin();
  uint32_t session_id = it->first;
  auto ret = recorder_.ReturnTrackBuffer(session_id, track_id, buffers);
  assert(ret == 0);
  TEST_DBG("%s:%s: Exit", TAG, __func__);
}

void RecorderTest::VideoTrack4KEncEventCb(uint32_t track_id, EventType event_type,
                                          void *event_data,
                                          size_t event_data_size) {
  TEST_INFO("%s:%s: Enter", TAG, __func__);
  TEST_INFO("%s:%s: Exit", TAG, __func__);
}

void RecorderTest::VideoTrack1080pEncDataCb1(uint32_t track_id,
                                    std::vector<BufferDescriptor> buffers,
                                    void *meta_param,
                                    TrackMetaParamType meta_type,
                                    size_t meta_size) {

  TEST_DBG("%s:%s: Enter", TAG, __func__);

#ifdef DUMP_BITSTREAM
  DumpBitStream(buffers, file_fd1_);
#endif
  // Return buffers back to service.
  session_iter_ it = sessions_.begin();
  uint32_t session_id = it->first;
  auto ret = recorder_.ReturnTrackBuffer(session_id, track_id, buffers);
  assert(ret == 0);
  TEST_DBG("%s:%s: Exit", TAG, __func__);
}

void RecorderTest::VideoTrack1080pEncDataCb2(uint32_t track_id,
                                    std::vector<BufferDescriptor> buffers,
                                    void *meta_param,
                                    TrackMetaParamType meta_type,
                                    size_t meta_size) {

  TEST_DBG("%s:%s: Enter", TAG, __func__);
#ifdef DUMP_BITSTREAM
  DumpBitStream(buffers, file_fd2_);
#endif
  // Return buffers back to service.
  session_iter_ it = sessions_.begin();
  uint32_t session_id = it->first;
  auto ret = recorder_.ReturnTrackBuffer(session_id, track_id, buffers);
  assert(ret == 0);
  TEST_DBG("%s:%s: Exit", TAG, __func__);
}

void RecorderTest::VideoTrack1080pEncEventCb(uint32_t track_id,
                                             EventType event_type,
                                             void *event_data,
                                             size_t event_data_size) {
  TEST_INFO("%s:%s: Enter", TAG, __func__);
  TEST_INFO("%s:%s: Exit", TAG, __func__);
}

#ifdef DUMP_BITSTREAM
status_t RecorderTest::DumpBitStream(std::vector<BufferDescriptor>& buffers,
                                     int32_t file_fd) {

  TEST_DBG("%s:%s: Enter", TAG, __func__);
  for (auto& iter : buffers) {
    if(file_fd > 0) {
      uint32_t exp_size = iter.size;
      TEST_DBG("%s:%s BitStream buffer data(0x%x):size(%d):ts(%lld):flag(0x%x)"
        ":buf_id(%d):capacity(%d)", TAG, __func__, iter.data, iter.size,
         iter.timestamp, iter.flag, iter.buf_id, iter.capacity);

      uint32_t written_length = write(file_fd, iter.data, iter.size);
      TEST_DBG("%s:%s: written_length(%d)", TAG, __func__, written_length);
      if (written_length != exp_size) {
        TEST_ERROR("%s:%s: Bad Write error (%d) %s", TAG, __func__, errno,
        strerror(errno));
      }
    } else {
      TEST_ERROR("%s:%s File is not open fd = %d", TAG, __func__, file_fd);
      assert(0);
    }
    if(iter.flag & static_cast<uint32_t>(BufferFlags::kFlagEOS)) {
      TEST_INFO("%s:%s EOS Last buffer!", TAG, __func__);
      close(file_fd);
      file_fd = -1;
    }
  }
  TEST_DBG("%s:%s: Exit", TAG, __func__);
}
#endif

#ifdef DUMP_YUV_FRAMES
status_t RecorderTest::DumpYUVFrame(uint32_t track_id, MetaInfo* meta_data,
                                    BufferDescriptor buffer) {

  static uint32_t id = 0;
  ++id;
  // Dump every 200th Frame.
  if (id == 200) {
    String8 file_path;
    size_t written_len;
    file_path.appendFormat("/data/track_%d_%dx%d_%lld.yuv", track_id,
        meta_data->plane_info[0].width, meta_data->plane_info[0].height,
        buffer.timestamp);

    FILE *file = fopen(file_path.string(), "w+");
    if (!file) {
      ALOGE("%s:%s: Unable to open file(%s)", TAG, __func__,
          file_path.string());
      goto FAIL;
    }

    written_len = fwrite(buffer.data, sizeof(uint8_t), buffer.size,
        file);
    TEST_DBG("%s:%s: written_len =%d", TAG, __func__, written_len);
    if (buffer.size != written_len) {
      ALOGE("%s:%s: Bad Write error (%d):(%s)\n", TAG, __func__, errno,
          strerror(errno));
      goto FAIL;
    }
    TEST_DBG("%s:%s: Buffer(0x%x) Size(%u) Stored@(%s)\n", TAG, __func__,
        buffers.data, written_len, file_path.string());
FAIL:
    if (file != NULL) {
      fclose(file);
    }
    id = 0;
  }
}
#endif

void CmdMenu::PrintMenu() {

  printf("\n\n=========== QIPCAM TEST MENU ===================\n\n");

  printf(" \n\nIPCam Test Application commands \n");
  printf(" -----------------------------\n");
  printf("   %c. Connect\n", CmdMenu::CONNECT_CMD);
  printf("   %c. Disconnect\n", CmdMenu::DISCONNECT_CMD);
  printf("   %c. Start Camera\n", CmdMenu::START_CAMERA_CMD);
  printf("   %c. Stop Camera\n", CmdMenu::STOP_CAMERA_CMD);
  printf("   %c. Create Session: (4K YUV + 1080 YUV)\n",
      CmdMenu::CREATE_YUV_SESSION_CMD);
  printf("   %c. Create Session: (4K Enc AVC)\n",
      CmdMenu::CREATE_4KENC_AVC_SESSION_CMD);
  printf("   %c. Create Session: (4K Enc HEVC)\n",
      CmdMenu::CREATE_4KENC_HEVC_SESSION_CMD);
  printf("   %c. Create Session: (1080p Enc AVC)\n",
      CmdMenu::CREATE_1080pENC_AVC_SESSION_CMD);
  printf("   %c. Create Session: (1080p Enc HEVC)\n",
      CmdMenu::CREATE_1080pENC_HEVC_SESSION_CMD);
  printf("   %c. Create Session: (4K YUV + 1080p Enc AVC)\n",
    CmdMenu::CREATE_4KYUV_1080pENC_SESSION_CMD);
  printf("   %c. Create Session: (Two 1080p Enc AVC)\n",
    CmdMenu::CREATE_TWO_1080pENC_SESSION_CMD);
  printf("   %c. Create Session: (PCM mono,16,48KHz)\n",
      CmdMenu::CREATE_AUD_SESSION_CMD);
  printf("   %c. Start Session\n", CmdMenu::START_SESSION_CMD);
  printf("   %c. Stop Session\n", CmdMenu::STOP_SESSION_CMD);
  printf("   %c. Take Snapshot\n", CmdMenu::TAKE_SNAPSHOT_CMD);
  printf("   %c. Set Dynamic Codec Param \n", CmdMenu::SET_PARAM_CMD);
  printf("   %c. Pause Session\n", CmdMenu::PAUSE_SESSION_CMD);
  printf("   %c. Resume Session\n", CmdMenu::RESUME_SESSION_CMD);
  printf("   %c. Enable Overlay\n", CmdMenu::ENABLE_OVERLAY_CMD);
  printf("   %c. Disable Overlay\n", CmdMenu::DISABLE_OVERLAY_CMD);
  printf("   %c. Delete Session\n", CmdMenu::DELETE_SESSION_CMD);
  printf("   %c. Exit\n", CmdMenu::EXIT_CMD);
  printf("\n   Choice: ");
}

CmdMenu::Command CmdMenu::GetCommand() {
  PrintMenu();
  return CmdMenu::Command(static_cast<CmdMenu::CommandType>(getchar()));
}

int main(int argc,char *argv[]) {

  TEST_INFO("%s:%s: Enter", TAG, __func__);

  RecorderTest test_context;

  CmdMenu cmd_menu(test_context);

  int32_t exit_test = false;

  while (!exit_test) {

    CmdMenu::Command command = cmd_menu.GetCommand();
    switch (command.cmd) {

      case CmdMenu::CONNECT_CMD: {
        test_context.Connect();
      }
      break;
      case CmdMenu::DISCONNECT_CMD: {
        test_context.Disconnect();
      }
      break;
      case CmdMenu::START_CAMERA_CMD: {
        test_context.StartCamera();
      }
      break;
      case CmdMenu::STOP_CAMERA_CMD: {
        test_context.StopCamera();
      }
      break;
      case CmdMenu::CREATE_YUV_SESSION_CMD: {
        test_context.Session4KAnd1080pYUVTracks();
      }
      break;
      case CmdMenu::CREATE_4KENC_AVC_SESSION_CMD: {
        test_context.Session4KEncTrack(VideoCodecType::kTypeAVC);
      }
      break;
      case CmdMenu::CREATE_4KENC_HEVC_SESSION_CMD: {
        test_context.Session4KEncTrack(VideoCodecType::kTypeHEVC);
      }
      break;
      case CmdMenu::CREATE_1080pENC_AVC_SESSION_CMD: {
        test_context.Session1080pEncTrack(VideoCodecType::kTypeAVC);
      }
      break;
      case CmdMenu::CREATE_1080pENC_HEVC_SESSION_CMD: {
        test_context.Session1080pEncTrack(VideoCodecType::kTypeHEVC);
      }
      break;
      case CmdMenu::CREATE_4KYUV_1080pENC_SESSION_CMD: {
        test_context.Session4KYUVAnd1080pEncTracks(VideoCodecType::kTypeAVC);
      }
      break;
      case CmdMenu::CREATE_TWO_1080pENC_SESSION_CMD: {
        test_context.SessionTwo1080pEncTracks(VideoCodecType::kTypeAVC);
      }
      break;
      case CmdMenu::CREATE_AUD_SESSION_CMD: {
          test_context.CreateAudioOnlySession();
      }
      break;

      case CmdMenu::START_SESSION_CMD: {
        test_context.StartSession();
      }
      break;
      case CmdMenu::STOP_SESSION_CMD: {
        test_context.StopSession();
      }
      break;
      case CmdMenu::TAKE_SNAPSHOT_CMD: {
        test_context.TakeSnapshot();
      }
      break;
      case CmdMenu::SET_PARAM_CMD: {
        test_context.SetParams();
      }
      break;
      case CmdMenu::PAUSE_SESSION_CMD: {
        test_context.PauseSession();
      }
      break;
      case CmdMenu::RESUME_SESSION_CMD: {
        test_context.ResumeSession();
      }
      break;
      case CmdMenu::ENABLE_OVERLAY_CMD: {
        test_context.EnableOverlay();
      }
      break;
      case CmdMenu::DISABLE_OVERLAY_CMD: {
        test_context.DisableOverlay();
      }
      break;
      case CmdMenu::DELETE_SESSION_CMD: {
        test_context.DeleteSession();
      }
      break;
      case CmdMenu::EXIT_CMD: {
        exit_test = true;
      }
      break;
      default:
        break;
    }
  }
  return 0;
}
