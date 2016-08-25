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

#define TAG "RecorderGTest"


#include <utils/Log.h>
#include <utils/String8.h>
#include <utils/Errors.h>
#include <camera/CameraMetadata.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>

#include "recorder/test/gtest/qmmf_recorder_gtest.h"

#define DUMP_META_PATH "/data/param.dump"

//#define DEBUG
#define TEST_INFO(fmt, args...)  ALOGD(fmt, ##args)
#define TEST_ERROR(fmt, args...) ALOGE(fmt, ##args)
#ifdef DEBUG
#define TEST_DBG  TEST_INFO
#else
#define TEST_DBG(...) ((void)0)
#endif

// Enable this define to dump YUV data from YUV track
//#define DUMP_YUV_FRAMES

// Enable this define to dump encoded bit stream data.
#define DUMP_BITSTREAM

static const int32_t kIterationCount = 50;
static const int32_t kRecordDuration = 2*60;   // 2 min for each iteration.
static const uint32_t kZslWidth      = 1920;
static const uint32_t kZslHeight     = 1080;
static const uint32_t kZslQDepth     = 10;
static const uint32_t kYUVDumpFreq   = 3600; // dump 1 frame in 2 min

void RecorderGtest::SetUp() {

  TEST_INFO("%s:%s Enter ", TAG, __func__);

  test_info_ = ::testing::UnitTest::GetInstance()->current_test_info();

  recorder_status_cb_.event_cb = [this] (EventType event_type, void *event_data,
                                         size_t event_data_size) -> void
      { RecorderCallbackHandler(event_type, event_data, event_data_size); };

  iteration_count_ = kIterationCount;
  camera_id_ = 0;

  memset(&camera_start_params_, 0x0, sizeof camera_start_params_);
  camera_start_params_.zsl_mode         = false;
  camera_start_params_.zsl_queue_depth  = 10;
  camera_start_params_.zsl_width        = kZslWidth;
  camera_start_params_.zsl_height       = kZslHeight;
  camera_start_params_.frame_rate       = kZslQDepth;
  camera_start_params_.flags            = 0x0;

  TEST_INFO("%s:%s Exit ", TAG, __func__);
}

void RecorderGtest::TearDown() {

  TEST_INFO("%s:%s Enter ", TAG, __func__);
  TEST_INFO("%s:%s Exit ", TAG, __func__);
}

int32_t RecorderGtest::Init() {

  auto ret = recorder_.Connect(recorder_status_cb_);
  assert(ret == NO_ERROR);
  return ret;
}

int32_t RecorderGtest::DeInit() {

  auto ret = recorder_.Disconnect();
  assert(ret == NO_ERROR);
  return ret;
}


/*
* ConnectToService: This test case will test Connect/Disconnect Api.
* Api test sequence:
*  - Connect
*  - Disconnect
*/
TEST_F(RecorderGtest, ConnectToService) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  for(uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s:%s: Running Test(%s) iteration = %d ", TAG, __func__,
        test_info_->name(), i);

    auto ret = recorder_.Connect(recorder_status_cb_);
    assert(ret == NO_ERROR);
    sleep(3);

    ret = recorder_.Disconnect();
    assert(ret == NO_ERROR);
  }
  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());
}

/*
* StartStopCamera: This test case will test Start & StopCamera Api.
* Api test sequence:
*   loop Start {
*   ------------------
*  - StartCamera
*  - StopCamera
*   ------------------
*   } loop End
*/
TEST_F(RecorderGtest, StartStopCamera) {

  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  assert(ret == NO_ERROR);
  for(uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s:%s: Running Test(%s) iteration = %d ", TAG, __func__,
        test_info_->name(), i);

    ret = recorder_.StartCamera(camera_id_, camera_start_params_);
    assert(ret == NO_ERROR);
    sleep(3);

    ret = recorder_.StopCamera(camera_id_);
    assert(ret == NO_ERROR);
  }
  ret = DeInit();
  assert(ret == NO_ERROR);
  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());
}

/*
* CreateDeleteSession: This test will test Create & Delete Session Api.
* Api test sequence:
*   loop Start {
*   ------------------
*  - StartCamera
*  - CreateSession
*  - DeleteSession
*  - StopCamera
*   ------------------
*   } loop End
*/
TEST_F(RecorderGtest, CreateDeleteSession) {

  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  assert(ret == NO_ERROR);
  for(uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s:%s: Running Test(%s) iteration = %d ", TAG, __func__,
        test_info_->name(), i);

    ret = recorder_.StartCamera(camera_id_, camera_start_params_);
    assert(ret == NO_ERROR);

    SessionCb session_status_cb;
    session_status_cb.event_cb = [this] (EventType event_type, void *event_data,
                                         size_t event_data_size) -> void
        { SessionCallbackHandler(event_type, event_data, event_data_size); };

    uint32_t session_id;
    ret = recorder_.CreateSession(session_status_cb, &session_id);
    assert(session_id > 0);
    assert(ret == NO_ERROR);
    sleep(2);
    ret = recorder_.DeleteSession(session_id);
    assert(ret == NO_ERROR);
    sleep(1);
    ret = recorder_.StopCamera(camera_id_);
    assert(ret == NO_ERROR);
  }
  ret = DeInit();
  assert(ret == NO_ERROR);

  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());
}

/*
* 4KSnapshot: This test will test 4K snapshot.
* Api test sequence:
*  - StartCamera
*   loop Start {
*   ------------------
*   - CaptureImage
*   ------------------
*   } loop End
*  - StopCamera
*/
TEST_F(RecorderGtest, 4KSnapshot) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  assert(ret == NO_ERROR);

  ret = recorder_.StartCamera(camera_id_, camera_start_params_);
  assert(ret == NO_ERROR);

  ImageParam image_param;
  memset(&image_param, 0x0, sizeof image_param);
  image_param.width         = 3840;
  image_param.height        = 2160;
  image_param.image_format  = ImageFormat::kJPEG;
  image_param.image_quality = 95;

  std::vector<CameraMetadata> meta_array;
  CameraMetadata meta;
  for(uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s:%s: Running Test(%s) iteration = %d ", TAG, __func__,
        test_info_->name(), i);

    ImageCaptureCb cb = [this] (uint32_t camera_id, uint32_t image_count,
                                BufferDescriptor buffer) -> void
        { SnapshotCb(camera_id, image_count, buffer); };

    auto ret = recorder_.GetCameraParam(camera_id_, meta);
    assert(ret == NO_ERROR);

    uint8_t awb_mode = ANDROID_CONTROL_AWB_MODE_INCANDESCENT;
    ret = meta.update(ANDROID_CONTROL_AWB_MODE, &awb_mode, 1);
    assert(ret == NO_ERROR);

    meta_array.push_back(meta);
    ret = recorder_.CaptureImage(camera_id_, image_param, 1, meta_array,
                                 cb);
    assert(ret == NO_ERROR);
    // Take snapshot after every 5 sec.
    sleep(5);
  }

  ret = recorder_.StopCamera(camera_id_);
  assert(ret == NO_ERROR);

  ret = DeInit();
  assert(ret == NO_ERROR);

  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());

}

/*
* SessionWith1080pYUVTrack: This test will test session with one 1080p YUV track.
* Api test sequence:
*  - StartCamera
*   loop Start {
*   ------------------
*   - CreateSession
*   - CreateVideoTrack
*   - StartVideoTrack
*   - StopSession
*   - DeleteVideoTrack
*   - DeleteSession
*   ------------------
*   } loop End
*  - StopCamera
*/
TEST_F(RecorderGtest, SessionWith1080pYUVTrack) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  assert(ret == NO_ERROR);

  ret = recorder_.StartCamera(camera_id_, camera_start_params_);
  assert(ret == NO_ERROR);

  for(uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s:%s: Running Test(%s) iteration = %d ", TAG, __func__,
        test_info_->name(), i);

    SessionCb session_status_cb;
    session_status_cb.event_cb = [this] (EventType event_type, void *event_data,
                                         size_t event_data_size) -> void
        { SessionCallbackHandler(event_type, event_data, event_data_size); };

    uint32_t session_id;
    ret = recorder_.CreateSession(session_status_cb, &session_id);
    assert(session_id > 0);
    assert(ret == NO_ERROR);

    VideoTrackCreateParam video_track_param;
    memset(&video_track_param, 0x0, sizeof video_track_param);

    video_track_param.camera_id     = 0;
    video_track_param.width         = 1920;
    video_track_param.height        = 1080;
    video_track_param.frame_rate    = 30;
    video_track_param.format_type   = VideoFormat::kYUV;
    video_track_param.out_device    = 0x01;
    uint32_t video_track_id = 1;

    TrackCb video_track_cb;
    video_track_cb.data_cb = [&] (uint32_t track_id, std::vector<BufferDescriptor>
        buffers, void *meta_param, TrackMetaParamType meta_type,
        size_t meta_size) { VideoTrackYUVDataCb(track_id,
        buffers, meta_param, meta_type, meta_size); };

    video_track_cb.event_cb = [&] (uint32_t track_id, EventType event_type,
        void *event_data, size_t event_data_size) { VideoTrackEventCb(track_id,
        event_type, event_data, event_data_size); };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id,
                                      video_track_param, video_track_cb);
    assert(ret == NO_ERROR);

    std::vector<uint32_t> track_ids;
    track_ids.push_back(video_track_id);
    sessions_.insert(std::make_pair(session_id, track_ids));

    ret = recorder_.StartSession(session_id);
    assert(ret == NO_ERROR);

    // Let session run for time kRecordDuration, during this time buffer with
    // valid data would be received in track callback (VideoTrackYUVDataCb).
    sleep(kRecordDuration);

    ret = recorder_.StopSession(session_id, false);
    assert(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id);
    assert(ret == NO_ERROR);

    ret = recorder_.DeleteSession(session_id);
    assert(ret == NO_ERROR);

    ClearSessions();
  }

  ret = recorder_.StopCamera(camera_id_);
  assert(ret == NO_ERROR);

  ret = DeInit();
  assert(ret == NO_ERROR);

  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());

}

/*
* SessionWith1080pEncTrack: This test will test session with 1080p h264 track.
* Api test sequence:
*  - StartCamera
*   loop Start {
*   ------------------
*   - CreateSession
*   - CreateVideoTrack
*   - StartVideoTrack
*   - StopSession
*   - DeleteVideoTrack
*   - DeleteSession
*   ------------------
*   } loop End
*  - StopCamera
*/
TEST_F(RecorderGtest, SessionWith1080pEncTrack) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  assert(ret == NO_ERROR);

  VideoFormat format_type = VideoFormat::kAVC;
  int32_t width  = 1920;
  int32_t height = 1080;
#ifdef DUMP_BITSTREAM
  String8 bitstream_filepath;
  const char* type_string = (format_type ==  VideoFormat::kAVC) ?
      "h264": "h265";
  String8 extn(type_string);
  bitstream_filepath.appendFormat("/data/gtest_track_%dx%d.%s", width, height,
      extn.string());
  track1_bitstream_filefd_ = open(bitstream_filepath.string(), O_CREAT | O_WRONLY |
      O_TRUNC, 0655);
  assert(track1_bitstream_filefd_ >= 0);
#endif

  ret = recorder_.StartCamera(camera_id_, camera_start_params_);
  assert(ret == NO_ERROR);

  for(uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s:%s: Running Test(%s) iteration = %d ", TAG, __func__,
        test_info_->name(), i);

    SessionCb session_status_cb;
    session_status_cb.event_cb = [this] (EventType event_type, void *event_data,
                                         size_t event_data_size) -> void
        { SessionCallbackHandler(event_type, event_data, event_data_size); };

    uint32_t session_id;
    ret = recorder_.CreateSession(session_status_cb, &session_id);
    assert(session_id > 0);
    assert(ret == NO_ERROR);

    VideoTrackCreateParam video_track_param;
    memset(&video_track_param, 0x0, sizeof video_track_param);

    video_track_param.camera_id     = 0;
    video_track_param.width         = width;
    video_track_param.height        = height;
    video_track_param.frame_rate    = 30;
    video_track_param.format_type   = format_type;
    video_track_param.out_device    = 0x01;
    uint32_t video_track_id = 1;

    TrackCb video_track_cb;
    video_track_cb.data_cb = [&] (uint32_t track_id, std::vector<BufferDescriptor>
        buffers, void *meta_param, TrackMetaParamType meta_type,
        size_t meta_size) { VideoTrackOneEncDataCb(track_id,
        buffers, meta_param, meta_type, meta_size); };

    video_track_cb.event_cb = [&] (uint32_t track_id, EventType event_type,
        void *event_data, size_t event_data_size) { VideoTrackEventCb(track_id,
        event_type, event_data, event_data_size); };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id,
                                      video_track_param, video_track_cb);
    assert(ret == NO_ERROR);

    std::vector<uint32_t> track_ids;
    track_ids.push_back(video_track_id);
    sessions_.insert(std::make_pair(session_id, track_ids));

    ret = recorder_.StartSession(session_id);
    assert(ret == NO_ERROR);

    // Let session run for kRecordDuration, during this time buffer with valid
    // data would be received in track callback (VideoTrackDataCb).
    sleep(kRecordDuration);

    ret = recorder_.StopSession(session_id, false);
    assert(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id);
    assert(ret == NO_ERROR);

    ret = recorder_.DeleteSession(session_id);
    assert(ret == NO_ERROR);

    ClearSessions();
  }
  ret = recorder_.StopCamera(camera_id_);
  assert(ret == NO_ERROR);

  ret = DeInit();
  assert(ret == NO_ERROR);

  if (track1_bitstream_filefd_ > 0) {
    close(track1_bitstream_filefd_);
  }
  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());

}

/*
* SessionWith1080pEncTrackStartStop: This test will test session with 1080p
* h264 track.
* Api test sequence:
*  - StartCamera
*  - CreateSession
*  - CreateVideoTrack
*   loop Start {
*   ------------------
*   - StartVideoTrack
*   - StopSession
*   ------------------
*   } loop End
*  - DeleteVideoTrack
*  - DeleteSession
*  - StopCamera
*/
TEST_F(RecorderGtest, SessionWith1080pEncTrackStartStop) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  assert(ret == NO_ERROR);

  VideoFormat format_type = VideoFormat::kAVC;
  int32_t width  = 1920;
  int32_t height = 1080;
#ifdef DUMP_BITSTREAM
  String8 bitstream_filepath;
  const char* type_string = (format_type ==  VideoFormat::kAVC) ?
      "h264": "h265";
  String8 extn(type_string);
  bitstream_filepath.appendFormat("/data/gtest_track_%dx%d.%s", width, height,
      extn.string());
  track1_bitstream_filefd_ = open(bitstream_filepath.string(), O_CREAT | O_WRONLY |
      O_TRUNC, 0655);
  assert(track1_bitstream_filefd_ >= 0);
#endif

  ret = recorder_.StartCamera(camera_id_, camera_start_params_);
  assert(ret == NO_ERROR);

  SessionCb session_status_cb;
  session_status_cb.event_cb = [this] (EventType event_type, void *event_data,
                                       size_t event_data_size) -> void
      { SessionCallbackHandler(event_type, event_data, event_data_size); };

  uint32_t session_id;
  ret = recorder_.CreateSession(session_status_cb, &session_id);
  assert(session_id > 0);
  assert(ret == NO_ERROR);

  VideoTrackCreateParam video_track_param;
  memset(&video_track_param, 0x0, sizeof video_track_param);

  video_track_param.camera_id     = 0;
  video_track_param.width         = width;
  video_track_param.height        = height;
  video_track_param.frame_rate    = 30;
  video_track_param.format_type   = format_type;
  video_track_param.out_device    = 0x01;
  uint32_t video_track_id = 1;

  TrackCb video_track_cb;
  video_track_cb.data_cb =
      [this] (uint32_t track_id, std::vector<BufferDescriptor> buffers,
              void *meta_param, TrackMetaParamType meta_type,
              size_t meta_size)
              -> void { VideoTrackOneEncDataCb(track_id, buffers, meta_param,
                                               meta_type, meta_size); };

  video_track_cb.event_cb =
      [this] (uint32_t track_id, EventType event_type,
              void *event_data, size_t event_data_size) -> void {
      VideoTrackEventCb(track_id, event_type, event_data, event_data_size); };

  ret = recorder_.CreateVideoTrack(session_id, video_track_id,
                                    video_track_param, video_track_cb);
  assert(ret == NO_ERROR);

  std::vector<uint32_t> track_ids;
  track_ids.push_back(video_track_id);
  sessions_.insert(std::make_pair(session_id, track_ids));

  for(uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s:%s: Running Test(%s) iteration = %d ", TAG, __func__,
        test_info_->name(), i);

    ret = recorder_.StartSession(session_id);
    assert(ret == NO_ERROR);

    // Let session run for kRecordDuration, during this time buffer with valid
    // data would be received in track callback (VideoTrackDataCb).
    sleep(kRecordDuration);

    ret = recorder_.StopSession(session_id, false);
    assert(ret == NO_ERROR);
  }

  ret = recorder_.DeleteVideoTrack(session_id, video_track_id);
  assert(ret == NO_ERROR);

  ret = recorder_.DeleteSession(session_id);
  assert(ret == NO_ERROR);

  ClearSessions();

  ret = recorder_.StopCamera(camera_id_);
  assert(ret == NO_ERROR);

  ret = DeInit();

  if (track1_bitstream_filefd_ > 0) {
    close(track1_bitstream_filefd_);
  }

  assert(ret == NO_ERROR);

  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());

}

/*
* SessionWith4KEncTrack: This test will test session with one 4K h264 track.
* Api test sequence:
*  - StartCamera
*   loop Start {
*   ------------------
*   - CreateSession
*   - CreateVideoTrack
*   - StartVideoTrack
*   - StopSession
*   - DeleteVideoTrack
*   - DeleteSession
*   ------------------
*   } loop End
*  - StopCamera
*/
TEST_F(RecorderGtest, SessionWith4KEncTrack) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  assert(ret == NO_ERROR);

  VideoFormat format_type = VideoFormat::kAVC;
  int32_t width  = 3840;
  int32_t height = 2160;
#ifdef DUMP_BITSTREAM
  String8 bitstream_filepath;
  const char* type_string = (format_type ==  VideoFormat::kAVC) ?
      "h264": "h265";
  String8 extn(type_string);
  bitstream_filepath.appendFormat("/data/gtest_track_%dx%d.%s", width, height,
      extn.string());
  track1_bitstream_filefd_ = open(bitstream_filepath.string(), O_CREAT | O_WRONLY |
      O_TRUNC, 0655);
  assert(track1_bitstream_filefd_ >= 0);
#endif

  ret = recorder_.StartCamera(camera_id_, camera_start_params_);
  assert(ret == NO_ERROR);

  for(uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s:%s: Running Test(%s) iteration = %d ", TAG, __func__,
        test_info_->name(), i);

    SessionCb session_status_cb;
    session_status_cb.event_cb =
        [this] (EventType event_type, void *event_data,
                size_t event_data_size) -> void {
        SessionCallbackHandler(event_type,
        event_data, event_data_size); };

    uint32_t session_id;
    ret = recorder_.CreateSession(session_status_cb, &session_id);
    assert(session_id > 0);
    assert(ret == NO_ERROR);

    VideoTrackCreateParam video_track_param;
    memset(&video_track_param, 0x0, sizeof video_track_param);

    video_track_param.camera_id   = 0;
    video_track_param.width       = width;
    video_track_param.height      = height;
    video_track_param.frame_rate  = 30;
    video_track_param.format_type = format_type;
    video_track_param.out_device  = 0x01;
    uint32_t video_track_id = 1;

    TrackCb video_track_cb;
    video_track_cb.data_cb =
        [this] (uint32_t track_id, std::vector<BufferDescriptor>
               buffers, void *meta_param, TrackMetaParamType meta_type,
               size_t meta_size) -> void { VideoTrackOneEncDataCb(track_id,
        buffers, meta_param, meta_type, meta_size); };

    video_track_cb.event_cb =
        [this] (uint32_t track_id, EventType event_type,
                void *event_data, size_t event_data_size) -> void
        { VideoTrackEventCb(track_id,
        event_type, event_data, event_data_size); };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id,
                                      video_track_param, video_track_cb);
    assert(ret == NO_ERROR);

    std::vector<uint32_t> track_ids;
    track_ids.push_back(video_track_id);
    sessions_.insert(std::make_pair(session_id, track_ids));

    ret = recorder_.StartSession(session_id);
    assert(ret == NO_ERROR);

    // Let session run for kRecordDuration, during this time buffer with valid
    // data would be received in track callback (VideoTrackDataCb).
    sleep(kRecordDuration);

    ret = recorder_.StopSession(session_id, false);
    assert(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id);
    assert(ret == NO_ERROR);

    ret = recorder_.DeleteSession(session_id);
    assert(ret == NO_ERROR);

     ClearSessions();
  }
  ret = recorder_.StopCamera(camera_id_);
  assert(ret == NO_ERROR);

  ret = DeInit();
  assert(ret == NO_ERROR);
  if (track1_bitstream_filefd_ > 0) {
    close(track1_bitstream_filefd_);
  }
  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());
}

/*
* SessionWith4KEncTrackStartStop: This test will test session with one 4K h264
*                                 track.
* Api test sequence:
*  - StartCamera
*  - CreateSession
*  - CreateVideoTrack
*   loop Start {
*   ------------------
*   - StartVideoTrack
*   - StopSession
*   ------------------
*   } loop End
*  - DeleteVideoTrack
*  - DeleteSession
*  - StopCamera
*/
TEST_F(RecorderGtest, SessionWith4KEncTrackStartStop) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  assert(ret == NO_ERROR);

  VideoFormat format_type = VideoFormat::kAVC;
  int32_t width  = 3840;
  int32_t height = 2160;
#ifdef DUMP_BITSTREAM
  String8 bitstream_filepath;
  const char* type_string = (format_type ==  VideoFormat::kAVC) ?
      "h264": "h265";
  String8 extn(type_string);
  bitstream_filepath.appendFormat("/data/gtest_track_%dx%d.%s", width, height,
      extn.string());
  track1_bitstream_filefd_ = open(bitstream_filepath.string(), O_CREAT | O_WRONLY |
      O_TRUNC, 0655);
  assert(track1_bitstream_filefd_ >= 0);
#endif

  ret = recorder_.StartCamera(camera_id_, camera_start_params_);
  assert(ret == NO_ERROR);

  SessionCb session_status_cb;
  session_status_cb.event_cb = [this] (EventType event_type, void *event_data,
                                       size_t event_data_size) -> void
      { SessionCallbackHandler(event_type, event_data, event_data_size); };

  uint32_t session_id;
  ret = recorder_.CreateSession(session_status_cb, &session_id);
  assert(session_id > 0);
  assert(ret == NO_ERROR);

  VideoTrackCreateParam video_track_param;
  memset(&video_track_param, 0x0, sizeof video_track_param);

  video_track_param.camera_id   = 0;
  video_track_param.width       = width;
  video_track_param.height      = height;
  video_track_param.frame_rate  = 30;
  video_track_param.format_type = format_type;
  video_track_param.out_device  = 0x01;
  uint32_t video_track_id = 1;

  TrackCb video_track_cb;
  video_track_cb.data_cb =
      [this] (uint32_t track_id, std::vector<BufferDescriptor>
             buffers, void *meta_param, TrackMetaParamType meta_type,
             size_t meta_size) -> void { VideoTrackOneEncDataCb(track_id,
             buffers, meta_param, meta_type, meta_size); };

  video_track_cb.event_cb =
      [this] (uint32_t track_id, EventType event_type,
              void *event_data, size_t event_data_size) -> void {
      VideoTrackEventCb(track_id, event_type, event_data, event_data_size); };

  ret = recorder_.CreateVideoTrack(session_id, video_track_id,
                                    video_track_param, video_track_cb);
  assert(ret == NO_ERROR);

  std::vector<uint32_t> track_ids;
  track_ids.push_back(video_track_id);
  sessions_.insert(std::make_pair(session_id, track_ids));

  for(uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s:%s: Running Test(%s) iteration = %d ", TAG, __func__,
        test_info_->name(), i);

    ret = recorder_.StartSession(session_id);
    assert(ret == NO_ERROR);

    // Let session run for kRecordDuration, during this time buffer with valid
    // data would be received in track callback (VideoTrackDataCb).
    sleep(kRecordDuration);

    ret = recorder_.StopSession(session_id, false);
    assert(ret == NO_ERROR);
  }
  ret = recorder_.DeleteVideoTrack(session_id, video_track_id);
  assert(ret == NO_ERROR);

  ret = recorder_.DeleteSession(session_id);
  assert(ret == NO_ERROR);

  ClearSessions();

  ret = recorder_.StopCamera(camera_id_);
  assert(ret == NO_ERROR);

  ret = DeInit();
  assert(ret == NO_ERROR);

  if (track1_bitstream_filefd_ > 0) {
    close(track1_bitstream_filefd_);
  }
  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());
}

/*
* SessionWithTwo1080pEncTracks: This test will test session with two 1080p
                                h264 tracks.
* Api test sequence:
*  - StartCamera
*  - CreateSession
*   loop Start {
*   ------------------
*   - CreateVideoTrack 1
*   - CreateVideoTrack 2
*   - StartVideoTrack
*   - StopSession
*   - DeleteVideoTrack 1
*   - DeleteVideoTrack 2
*   ------------------
*   } loop End
*  - DeleteSession
*  - StopCamera
*/
TEST_F(RecorderGtest, SessionWithTwo1080pEncTracks) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  assert(ret == NO_ERROR);

  VideoFormat format_type = VideoFormat::kAVC;
  int32_t width  = 1920;
  int32_t height = 1080;
  uint32_t video_track_id1 = 1;
  uint32_t video_track_id2 = 2;

  ret = recorder_.StartCamera(camera_id_, camera_start_params_);
  assert(ret == NO_ERROR);

  SessionCb session_status_cb;
  session_status_cb.event_cb = [this] (EventType event_type, void *event_data,
                                       size_t event_data_size) -> void {
    SessionCallbackHandler(event_type, event_data, event_data_size); };

  uint32_t session_id;
  ret = recorder_.CreateSession(session_status_cb, &session_id);
  assert(session_id > 0);
  assert(ret == NO_ERROR);

  VideoTrackCreateParam video_track_param;
  memset(&video_track_param, 0x0, sizeof video_track_param);

  video_track_param.camera_id     = 0;
  video_track_param.width         = width;
  video_track_param.height        = height;
  video_track_param.frame_rate    = 30;
  video_track_param.format_type   = format_type;
  video_track_param.out_device    = 0x01;

  TrackCb video_track_cb;
  video_track_cb.event_cb =
      [this] (uint32_t track_id, EventType event_type,
              void *event_data, size_t event_data_size) -> void {
      VideoTrackEventCb(track_id, event_type, event_data, event_data_size); };

  std::vector<uint32_t> track_ids;
  for(uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s:%s: Running Test(%s) iteration = %d ", TAG, __func__,
        test_info_->name(), i);

    video_track_cb.data_cb =
      [this] (uint32_t track_id, std::vector<BufferDescriptor>
              buffers, void *meta_param, TrackMetaParamType meta_type,
              size_t meta_size) -> void { VideoTrackOneEncDataCb(track_id,
    buffers, meta_param, meta_type, meta_size); };

    // Create 1080p encode track.
    ret = recorder_.CreateVideoTrack(session_id, video_track_id1,
                                     video_track_param, video_track_cb);
    assert(ret == NO_ERROR);
#ifdef DUMP_BITSTREAM
    if (track1_bitstream_filefd_ > 0) {
      close(track1_bitstream_filefd_);
    }
    String8 bitstream_filepath;
    const char* type_string = (format_type ==  VideoFormat::kAVC) ?
      "h264": "h265";
    String8 extn(type_string);
    bitstream_filepath.appendFormat("/data/gtest_track_%d_%dx%d.%s",
      video_track_id1, width, height, extn.string());
    track1_bitstream_filefd_ = open(bitstream_filepath.string(), O_CREAT |
       O_WRONLY | O_TRUNC, 0655);
    assert(track1_bitstream_filefd_ > 0);
#endif
    track_ids.push_back(video_track_id1);

    video_track_cb.data_cb =
      [this] (uint32_t track_id, std::vector<BufferDescriptor>
              buffers, void *meta_param, TrackMetaParamType meta_type,
              size_t meta_size) -> void { VideoTrackTwoEncDataCb(track_id,
    buffers, meta_param, meta_type, meta_size); };

    // Create another 1080p encode track.
    ret = recorder_.CreateVideoTrack(session_id, video_track_id2,
                                     video_track_param, video_track_cb);
    assert(ret == NO_ERROR);
#ifdef DUMP_BITSTREAM
    if (track2_bitstream_filefd_ > 0) {
      close(track2_bitstream_filefd_);
    }
    bitstream_filepath.clear();
    bitstream_filepath.appendFormat("/data/gtest_track_%d_%dx%d.%s",
      video_track_id2, width, height, extn.string());
    track2_bitstream_filefd_ = open(bitstream_filepath.string(), O_CREAT |
       O_WRONLY | O_TRUNC, 0655);
    assert(track2_bitstream_filefd_ > 0);
#endif

    track_ids.push_back(video_track_id1);

    sessions_.insert(std::make_pair(session_id, track_ids));

    ret = recorder_.StartSession(session_id);
    assert(ret == NO_ERROR);

    // Let session run for kRecordDuration, during this time buffer with valid
    // data would be received in track callback (VideoTrackDataCb).
    sleep(kRecordDuration);

    ret = recorder_.StopSession(session_id, false);
    assert(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id1);
    assert(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id2);
    assert(ret == NO_ERROR);
  }
  ret = recorder_.DeleteSession(session_id);
  assert(ret == NO_ERROR);

  ClearSessions();

  ret = recorder_.StopCamera(camera_id_);
  assert(ret == NO_ERROR);

  ret = DeInit();
  assert(ret == NO_ERROR);

  if (track1_bitstream_filefd_ > 0) {
    close(track1_bitstream_filefd_);
  }
  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());
}

/*
* SessionWithTwo1080pEncTracksStartStop: This test will test session with two
*                                        1080p h264 tracks.
* Api test sequence:
*  - StartCamera
*  - CreateSession
*  - CreateVideoTrack 1
*  - CreateVideoTrack 2
*   loop Start {
*   ------------------
*   - StartVideoTrack
*   - StopSession
*   ------------------
*   } loop End
*  - DeleteVideoTrack 1
*  - DeleteVideoTrack 2
*  - DeleteSession
*  - StopCamera
*/
TEST_F(RecorderGtest, SessionWithTwo1080pEncTracksStartStop) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  assert(ret == NO_ERROR);

  VideoFormat format_type = VideoFormat::kAVC;
  int32_t width  = 1920;
  int32_t height = 1080;
  uint32_t video_track_id1 = 1;
  uint32_t video_track_id2 = 2;

  ret = recorder_.StartCamera(camera_id_, camera_start_params_);
  assert(ret == NO_ERROR);

  SessionCb session_status_cb;
  session_status_cb.event_cb = [this] (EventType event_type, void *event_data,
                                       size_t event_data_size) -> void {
      SessionCallbackHandler(event_type, event_data, event_data_size); };

  uint32_t session_id;
  ret = recorder_.CreateSession(session_status_cb, &session_id);
  assert(session_id > 0);
  assert(ret == NO_ERROR);

  std::vector<uint32_t> track_ids;
  VideoTrackCreateParam video_track_param;
  memset(&video_track_param, 0x0, sizeof video_track_param);

  video_track_param.camera_id     = 0;
  video_track_param.width         = width;
  video_track_param.height        = height;
  video_track_param.frame_rate    = 30;
  video_track_param.format_type   = format_type;
  video_track_param.out_device    = 0x01;

  TrackCb video_track_cb;
  video_track_cb.event_cb =
      [this] (uint32_t track_id, EventType event_type,
              void *event_data, size_t event_data_size) -> void {
      VideoTrackEventCb(track_id, event_type, event_data, event_data_size); };

  video_track_cb.data_cb =
    [this] (uint32_t track_id, std::vector<BufferDescriptor>
            buffers, void *meta_param, TrackMetaParamType meta_type,
            size_t meta_size) -> void { VideoTrackOneEncDataCb(track_id,
  buffers, meta_param, meta_type, meta_size); };

  // Create 1080p encode track.
  ret = recorder_.CreateVideoTrack(session_id, video_track_id1,
                                   video_track_param, video_track_cb);
  assert(ret == NO_ERROR);
#ifdef DUMP_BITSTREAM
    if (track1_bitstream_filefd_ > 0) {
      close(track1_bitstream_filefd_);
    }
    String8 bitstream_filepath;
    const char* type_string = (format_type ==  VideoFormat::kAVC) ?
      "h264": "h265";
    String8 extn(type_string);
    bitstream_filepath.appendFormat("/data/gtest_track_%d_%dx%d.%s",
      video_track_id1, width, height, extn.string());
    track1_bitstream_filefd_ = open(bitstream_filepath.string(), O_CREAT |
       O_WRONLY | O_TRUNC, 0655);
    assert(track1_bitstream_filefd_ > 0);
#endif
  track_ids.push_back(video_track_id1);

  video_track_cb.data_cb =
    [this] (uint32_t track_id, std::vector<BufferDescriptor>
            buffers, void *meta_param, TrackMetaParamType meta_type,
            size_t meta_size) -> void { VideoTrackTwoEncDataCb(track_id,
  buffers, meta_param, meta_type, meta_size); };

  // Create another 1080p encode track.
  ret = recorder_.CreateVideoTrack(session_id, video_track_id2,
                                   video_track_param, video_track_cb);
  assert(ret == NO_ERROR);
#ifdef DUMP_BITSTREAM
    if (track2_bitstream_filefd_ > 0) {
      close(track2_bitstream_filefd_);
    }
    bitstream_filepath.clear();
    bitstream_filepath.appendFormat("/data/gtest_track_%d_%dx%d.%s",
      video_track_id2, width, height, extn.string());
    track2_bitstream_filefd_ = open(bitstream_filepath.string(), O_CREAT |
       O_WRONLY | O_TRUNC, 0655);
    assert(track2_bitstream_filefd_ > 0);
#endif
  track_ids.push_back(video_track_id1);

  sessions_.insert(std::make_pair(session_id, track_ids));

  for(uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s:%s: Running Test(%s) iteration = %d ", TAG, __func__,
        test_info_->name(), i);

    ret = recorder_.StartSession(session_id);
    assert(ret == NO_ERROR);

    // Let session run for kRecordDuration, during this time buffer with valid
    // data would be received in track callback (VideoTrackDataCb).
    sleep(kRecordDuration);

    ret = recorder_.StopSession(session_id, false);
    assert(ret == NO_ERROR);
  }

  ret = recorder_.DeleteVideoTrack(session_id, video_track_id1);
  assert(ret == NO_ERROR);

  ret = recorder_.DeleteVideoTrack(session_id, video_track_id2);
  assert(ret == NO_ERROR);

  ret = recorder_.DeleteSession(session_id);
  assert(ret == NO_ERROR);

  ClearSessions();

  ret = recorder_.StopCamera(camera_id_);
  assert(ret == NO_ERROR);

  ret = DeInit();
  assert(ret == NO_ERROR);

  if (track1_bitstream_filefd_ > 0) {
    close(track1_bitstream_filefd_);
  }
  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());
}

/*
* SessionWith4KAnd1080pYUVTrack: This test will test session with 4k and 1080p
*                                YUV tracks.
* Api test sequence:
*  - StartCamera
*  - CreateSession
*  loop Start {
*   ------------------
*   - CreateVideoTrack 1
*   - CreateVideoTrack 2
*   - StartVideoTrack
*   - StopSession
*   - DeleteVideoTrack 1
*   - DeleteVideoTrack 2
*   ------------------
*   } loop End
*   - DeleteSession
*   - StopCamera
*/
TEST_F(RecorderGtest, SessionWith4KAnd1080pYUVTrack) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  assert(ret == NO_ERROR);

  ret = recorder_.StartCamera(camera_id_, camera_start_params_);
  assert(ret == NO_ERROR);

  SessionCb session_status_cb;
  session_status_cb.event_cb = [this] (EventType event_type, void *event_data,
                                       size_t event_data_size) -> void {
      SessionCallbackHandler(event_type, event_data, event_data_size); };

  uint32_t session_id;
  ret = recorder_.CreateSession(session_status_cb, &session_id);
  assert(session_id > 0);
  assert(ret == NO_ERROR);
  uint32_t track1_id = 1;
  uint32_t track2_id = 2;

  std::vector<uint32_t> track_ids;
  for(uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s:%s: Running Test(%s) iteration = %d ", TAG, __func__,
        test_info_->name(), i);

    VideoTrackCreateParam video_track_param;
    memset(&video_track_param, 0x0, sizeof video_track_param);

    video_track_param.camera_id   = 0;
    video_track_param.width       = 3840;
    video_track_param.height      = 2160;
    video_track_param.frame_rate  = 30;
    video_track_param.format_type = VideoFormat::kYUV;
    video_track_param.out_device  = 0x01;

    TrackCb video_track_cb;
    video_track_cb.data_cb =
        [this] (uint32_t track_id, std::vector<BufferDescriptor>
                buffers, void *meta_param, TrackMetaParamType meta_type,
                size_t meta_size) -> void { VideoTrackYUVDataCb(track_id,
        buffers, meta_param, meta_type, meta_size); };

    video_track_cb.event_cb =
        [this] (uint32_t track_id, EventType event_type,
                void *event_data, size_t event_data_size) -> void {
        VideoTrackEventCb(track_id, event_type, event_data, event_data_size); };

    ret = recorder_.CreateVideoTrack(session_id, track1_id,
                                      video_track_param, video_track_cb);
    assert(ret == NO_ERROR);
    track_ids.push_back(track1_id);

    video_track_param.width  = 1920;
    video_track_param.height = 1080;
    ret = recorder_.CreateVideoTrack(session_id, track2_id,
                                      video_track_param, video_track_cb);
    assert(ret == NO_ERROR);

    track_ids.push_back(track2_id);

    sessions_.insert(std::make_pair(session_id, track_ids));

    ret = recorder_.StartSession(session_id);
    assert(ret == NO_ERROR);

    // Let session run for kRecordDuration, during this time buffer with valid
    // data would be received in track callback (VideoTrackYUVDataCb).
    sleep(kRecordDuration);

    ret = recorder_.StopSession(session_id, false);
    assert(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, track1_id);
    assert(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, track2_id);
    assert(ret == NO_ERROR);
  }

  ret = recorder_.DeleteSession(session_id);
  assert(ret == NO_ERROR);

  ret = recorder_.StopCamera(camera_id_);
  assert(ret == NO_ERROR);

  ClearSessions();

  ret = DeInit();
  assert(ret == NO_ERROR);

  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());
}

/*
* SessionWith4KAnd1080pYUVTrackStartStop: This test will test session with 4k and 1080p
*                                YUV tracks.
* Api test sequence:
*  - StartCamera
*  - CreateSession
*  - CreateVideoTrack 1
*  - CreateVideoTrack 2
*  loop Start {
*   ------------------
*   - StartVideoTrack
*   - StopSession
*   ------------------
*   } loop End
*   - DeleteVideoTrack 1
*   - DeleteVideoTrack 2
*   - DeleteSession
*   - StopCamera
*/
TEST_F(RecorderGtest, SessionWith4KAnd1080pYUVTrackStartStop) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  assert(ret == NO_ERROR);

  ret = recorder_.StartCamera(camera_id_, camera_start_params_);
  assert(ret == NO_ERROR);

  SessionCb session_status_cb;
  session_status_cb.event_cb = [this] (EventType event_type, void *event_data,
                                       size_t event_data_size) -> void {
      SessionCallbackHandler(event_type, event_data, event_data_size); };

  uint32_t session_id;
  ret = recorder_.CreateSession(session_status_cb, &session_id);
  assert(session_id > 0);
  assert(ret == NO_ERROR);
  uint32_t track1_id = 1;
  uint32_t track2_id = 2;

  std::vector<uint32_t> track_ids;
  VideoTrackCreateParam video_track_param;
  memset(&video_track_param, 0x0, sizeof video_track_param);

  video_track_param.camera_id   = 0;
  video_track_param.width       = 3840;
  video_track_param.height      = 2160;
  video_track_param.frame_rate  = 30;
  video_track_param.format_type = VideoFormat::kYUV;
  video_track_param.out_device  = 0x01;

  TrackCb video_track_cb;
  video_track_cb.data_cb =
      [this] (uint32_t track_id, std::vector<BufferDescriptor>
              buffers, void *meta_param, TrackMetaParamType meta_type,
              size_t meta_size) -> void { VideoTrackYUVDataCb(track_id,
      buffers, meta_param, meta_type, meta_size); };

  video_track_cb.event_cb =
      [this] (uint32_t track_id, EventType event_type,
              void *event_data, size_t event_data_size) -> void {
      VideoTrackEventCb(track_id, event_type, event_data, event_data_size); };

  ret = recorder_.CreateVideoTrack(session_id, track1_id,
                                    video_track_param, video_track_cb);
  assert(ret == NO_ERROR);
  track_ids.push_back(track1_id);

  video_track_param.width  = 1920;
  video_track_param.height = 1080;
  ret = recorder_.CreateVideoTrack(session_id, track2_id,
                                    video_track_param, video_track_cb);
  assert(ret == NO_ERROR);

  track_ids.push_back(track2_id);

  sessions_.insert(std::make_pair(session_id, track_ids));

  for(uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s:%s: Running Test(%s) iteration = %d ", TAG, __func__,
        test_info_->name(), i);

    ret = recorder_.StartSession(session_id);
    assert(ret == NO_ERROR);

    // Let session run for kRecordDuration, during this time buffer with valid
    // data would be received in track callback (VideoTrackYUVDataCb).
    sleep(kRecordDuration);

    ret = recorder_.StopSession(session_id, false);
    assert(ret == NO_ERROR);
  }
  ret = recorder_.DeleteVideoTrack(session_id, track1_id);
  assert(ret == NO_ERROR);

  ret = recorder_.DeleteVideoTrack(session_id, track2_id);
  assert(ret == NO_ERROR);

  ret = recorder_.DeleteSession(session_id);
  assert(ret == NO_ERROR);

  ret = recorder_.StopCamera(camera_id_);
  assert(ret == NO_ERROR);

  ClearSessions();

  ret = DeInit();
  assert(ret == NO_ERROR);

  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());
}

/*
* CameraParamTest: This test will test camera parameter setting.
* Api test sequence:
*  - StartCamera
*  - CreateSession
*  - CreateVideoTrack
*  - StartSessiom
*  - GetCameraParam
*  - SetCameraParam - Switch AWB mode
*  - StopSession
*  - DeleteVideoTrack
*  - DeleteSession
*  - StopCamera
*/
TEST_F(RecorderGtest, CameraParamTest) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  assert(ret == NO_ERROR);

  ret = recorder_.StartCamera(camera_id_, camera_start_params_);
  assert(ret == NO_ERROR);

  SessionCb session_status_cb;
  session_status_cb.event_cb = [&] ( EventType event_type, void *event_data,
      size_t event_data_size) { SessionCallbackHandler(event_type,
      event_data, event_data_size); };

  uint32_t session_id;
  ret = recorder_.CreateSession(session_status_cb, &session_id);
  assert(session_id > 0);
  assert(ret == NO_ERROR);

  VideoTrackCreateParam video_track_param;
  memset(&video_track_param, 0x0, sizeof video_track_param);

  video_track_param.camera_id   = 0;
  video_track_param.width       = 1920;
  video_track_param.height      = 1080;
  video_track_param.frame_rate  = 30;
  video_track_param.format_type = VideoFormat::kYUV;
  video_track_param.out_device  = 0x01;
  uint32_t video_track_id = 1;

  TrackCb video_track_cb;
  video_track_cb.data_cb = [&] (uint32_t track_id, std::vector<BufferDescriptor>
      buffers, void *meta_param, TrackMetaParamType meta_type,
      size_t meta_size) { VideoTrackYUVDataCb(track_id,
      buffers, meta_param, meta_type, meta_size); };

  video_track_cb.event_cb = [&] (uint32_t track_id, EventType event_type,
      void *event_data, size_t event_data_size) { VideoTrackEventCb(track_id,
      event_type, event_data, event_data_size); };

  ret = recorder_.CreateVideoTrack(session_id, video_track_id,
                                    video_track_param, video_track_cb);
  assert(ret == NO_ERROR);

  std::vector<uint32_t> track_ids;
  track_ids.push_back(video_track_id);
  sessions_.insert(std::make_pair(session_id, track_ids));

  ret = recorder_.StartSession(session_id);
  assert(ret == NO_ERROR);

  // run session for some time
  sleep(5);

  int dump_fd = open(DUMP_META_PATH, O_WRONLY|O_CREAT);
  assert(0 <= dump_fd);

  CameraMetadata meta;
  ret = recorder_.GetCameraParam(camera_id_, meta);
  assert(ret == NO_ERROR);

  meta.dump(dump_fd, 2);
  close(dump_fd);

  // Switch AWB mode
  uint8_t awb_mode = ANDROID_CONTROL_AWB_MODE_INCANDESCENT;
  ret = meta.update(ANDROID_CONTROL_AWB_MODE, &awb_mode, 1);
  assert(ret == NO_ERROR);

  ret = recorder_.SetCameraParam(camera_id_, meta);
  assert(ret == NO_ERROR);

  // run session for some time
  sleep(5);

  ret = recorder_.StopSession(session_id, true);
  assert(ret == NO_ERROR);

  ret = recorder_.DeleteVideoTrack(session_id, video_track_id);
  assert(ret == NO_ERROR);

  ret = recorder_.DeleteSession(session_id);
  assert(ret == NO_ERROR);

  ret = recorder_.StopCamera(camera_id_);
  assert(ret == NO_ERROR);

  ClearSessions();

  ret = DeInit();
  assert(ret == NO_ERROR);

  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());

}

void RecorderGtest::ClearSessions() {

  TEST_INFO("%s:%s Enter ", TAG, __func__);
  std::map <uint32_t , std::vector<uint32_t> >::iterator it = sessions_.begin();
  for (it; it != sessions_.end(); ++it) {
    it->second.clear();
  }
  sessions_.clear();
  TEST_INFO("%s:%s Exit ", TAG, __func__);
}

void RecorderGtest::RecorderCallbackHandler(EventType event_type,
                                            void *event_data,
                                            size_t event_data_size) {
  TEST_INFO("%s:%s Enter ", TAG, __func__);
  TEST_INFO("%s:%s Exit ", TAG, __func__);
}

void RecorderGtest::SessionCallbackHandler(EventType event_type,
                                          void *event_data,
                                          size_t event_data_size) {
  TEST_INFO("%s:%s: Enter", TAG, __func__);
  TEST_INFO("%s:%s: Exit", TAG, __func__);
}

void RecorderGtest::VideoTrackYUVDataCb(uint32_t track_id,
                                        std::vector<BufferDescriptor> buffers,
                                        void *meta_param,
                                        TrackMetaParamType meta_type,
                                        size_t meta_size) {

  TEST_DBG("%s:%s: Enter", TAG, __func__);
#ifdef DUMP_YUV_FRAMES
  static uint32_t id = 0;
  ++id;
  if (id == kYUVDumpFreq) {
    String8 file_path;
    size_t written_len;
    file_path.appendFormat("/data/gtest_track_%d_%lld.yuv", track_id,
        buffers[0].timestamp);

    FILE *file = fopen(file_path.string(), "w+");
    if (!file) {
      ALOGE("%s:%s: Unable to open file(%s)", TAG, __func__,
          file_path.string());
      goto FAIL;
    }

    written_len = fwrite(buffers[0].data, sizeof(uint8_t), buffers[0].size, file);
    TEST_DBG("%s:%s: written_len =%d", TAG, __func__, written_len);
    if (buffers[0].size != written_len) {
      TEST_ERROR("%s:%s: Bad Write error (%d):(%s)\n", TAG, __func__, errno,
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
  assert(ret == NO_ERROR);
  TEST_DBG("%s:%s: Exit", TAG, __func__);
}

void RecorderGtest::VideoTrackOneEncDataCb(uint32_t track_id,
                                        std::vector<BufferDescriptor> buffers,
                                        void *meta_param,
                                        TrackMetaParamType meta_type,
                                        size_t meta_size) {

  TEST_DBG("%s:%s: Enter", TAG, __func__);
#ifdef DUMP_BITSTREAM
  assert(track1_bitstream_filefd_ > 0);
  DumpBitStream(buffers, track1_bitstream_filefd_);
#endif
  // Return buffers back to service.
  std::map <uint32_t , std::vector<uint32_t> >::iterator it = sessions_.begin();
  uint32_t session_id = it->first;
  auto ret = recorder_.ReturnTrackBuffer(session_id, track_id, buffers);
  assert(ret == NO_ERROR);

  TEST_DBG("%s:%s: Exit", TAG, __func__);
}

void RecorderGtest::VideoTrackTwoEncDataCb(uint32_t track_id,
                                          std::vector<BufferDescriptor> buffers,
                                          void *meta_param,
                                          TrackMetaParamType meta_type,
                                          size_t meta_size) {

  TEST_DBG("%s:%s: Enter", TAG, __func__);
#ifdef DUMP_BITSTREAM
  assert(track2_bitstream_filefd_ > 0);
  DumpBitStream(buffers, track2_bitstream_filefd_);
#endif
  // Return buffers back to service.
  std::map <uint32_t , std::vector<uint32_t> >::iterator it = sessions_.begin();
  uint32_t session_id = it->first;
  auto ret = recorder_.ReturnTrackBuffer(session_id, track_id, buffers);
  assert(ret == NO_ERROR);

  TEST_DBG("%s:%s: Exit", TAG, __func__);
}

#ifdef DUMP_BITSTREAM
status_t RecorderGtest::DumpBitStream(std::vector<BufferDescriptor>& buffers,
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
        return -1;
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
  return 0;
}
#endif

void RecorderGtest::VideoTrackEventCb(uint32_t track_id, EventType event_type,
                                      void *event_data, size_t data_size) {
    TEST_DBG("%s:%s: Enter", TAG, __func__);
    TEST_DBG("%s:%s: Exit", TAG, __func__);
}

void RecorderGtest::SnapshotCb(uint32_t camera_id,
                               uint32_t image_sequence_count,
                               BufferDescriptor buffer) {

  TEST_DBG("%s:%s: Enter", TAG, __func__);
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
  TEST_DBG("%s:%s: Enter", TAG, __func__);
}