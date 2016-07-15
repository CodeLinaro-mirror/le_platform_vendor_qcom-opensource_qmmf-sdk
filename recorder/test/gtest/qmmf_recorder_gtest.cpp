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
#include <assert.h>

#include "qmmf_recorder_gtest.h"
#define DUMP_FRAMES

void RecorderGtest::SetUp() {

  ALOGD("%s:%s Enter ", TAG, __func__);

  test_info_ = ::testing::UnitTest::GetInstance()->current_test_info();
  recorder_ = new Recorder;
  assert(recorder_ != nullptr);

  recorder_status_cb_.event_cb = [&] ( EventType event_type, void *event_data,
      size_t event_data_size) { RecorderCallbackHandler(event_type,
      event_data, event_data_size); };

  iteration_count_ = ITERATION_COUNT;
  // Adding only one hardcoded camera id.
  camera_ids_.push_back(0);

  memset(&camera_start_params_, 0x0, sizeof camera_start_params_);
  camera_start_params_.zsl_mode         = false;
  camera_start_params_.zsl_queue_depth  = 10;
  camera_start_params_.zsl_width        = ZSL_WIDTH;
  camera_start_params_.zsl_height       = ZSL_HEIGHT;
  camera_start_params_.frame_rate       = ZSL_QUEUE_DEPTH;
  camera_start_params_.flags            = 0x0;

  ALOGD("%s:%s Exit ", TAG, __func__);
}

void RecorderGtest::TearDown() {

  ALOGD("%s:%s Enter ", TAG, __func__);
  if (recorder_ != nullptr) {
    delete recorder_;
    recorder_ = nullptr;
  }
  ALOGD("%s:%s Exit ", TAG, __func__);
}

int32_t RecorderGtest::Init() {

  auto ret = recorder_->Connect(recorder_status_cb_);
  assert(ret == NO_ERROR);
  return ret;
}

int32_t RecorderGtest::DeInit() {

  auto ret = recorder_->Disconnect();
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
    ALOGD("%s:%s: Running Test(%s) iteration = %d ", TAG, __func__,
        test_info_->name(), i);

    auto ret = recorder_->Connect(recorder_status_cb_);
    assert(ret == NO_ERROR);
    sleep(3);

    ret = recorder_->Disconnect();
    assert(ret == NO_ERROR);
  }
  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());
}

/*
* StartStopCamera: This test case will test start & stop camera Api.
* Api test sequence:
*  - StartCamera
*  - StopCamera
*/
TEST_F(RecorderGtest, StartStopCamera) {

  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  assert(ret == NO_ERROR);
  for(uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    ALOGD("%s:%s: Running Test(%s) iteration = %d ", TAG, __func__,
        test_info_->name(), i);

    ret = recorder_->StartCamera(camera_ids_, camera_start_params_);
    assert(ret == NO_ERROR);
    sleep(3);

    ret = recorder_->StopCamera(camera_ids_);
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
*  - StartCamera
*  - CreateSession
*  - DeleteSession
*  - StopCamera
*/
TEST_F(RecorderGtest, CreateDeleteSession) {

  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  assert(ret == NO_ERROR);
  for(uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    ALOGD("%s:%s: Running Test(%s) iteration = %d ", TAG, __func__,
        test_info_->name(), i);

    ret = recorder_->StartCamera(camera_ids_, camera_start_params_);
    assert(ret == NO_ERROR);

    SessionCb session_status_cb;
    session_status_cb.event_cb = [&] ( EventType event_type, void *event_data,
        size_t event_data_size) { SessionCallbackHandler(event_type,
        event_data, event_data_size); };

    uint32_t session_id;
    ret = recorder_->CreateSession(session_status_cb, &session_id);
    assert(session_id > 0);
    assert(ret == NO_ERROR);
    sleep(2);
    ret = recorder_->DeleteSession(session_id);
    assert(ret == NO_ERROR);
    sleep(1);
    ret = recorder_->StopCamera(camera_ids_);
    assert(ret == NO_ERROR);
  }
  ret = DeInit();
  assert(ret == NO_ERROR);

  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());

}

/*
* SessionWithVideoTrackTest1: This test will test session and track Apis.
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
TEST_F(RecorderGtest, SessionWithVideoTrackTest1)
{
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  assert(ret == NO_ERROR);

  ret = recorder_->StartCamera(camera_ids_, camera_start_params_);
  assert(ret == NO_ERROR);

  for(uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    ALOGD("%s:%s: Running Test(%s) iteration = %d ", TAG, __func__,
        test_info_->name(), i);

    SessionCb session_status_cb;
    session_status_cb.event_cb = [&] ( EventType event_type, void *event_data,
        size_t event_data_size) { SessionCallbackHandler(event_type,
        event_data, event_data_size); };

    uint32_t session_id;
    ret = recorder_->CreateSession(session_status_cb, &session_id);
    assert(session_id > 0);
    assert(ret == NO_ERROR);

    VideoTrackCreateParam video_track_param;
    memset(&video_track_param, 0x0, sizeof video_track_param);

    video_track_param.camera_ids[0] = 0;
    video_track_param.num_cameras   = 1;
    video_track_param.width         = 1920;
    video_track_param.height        = 1080;
    video_track_param.frame_rate    = 30;
    video_track_param.codec_type    = VideoCodecType::kYUV;
    video_track_param.out_device    = 0x01;
    uint32_t video_track_id = 1;

    TrackCb video_track_cb;
    video_track_cb.data_cb = [&] (uint32_t track_id, std::vector<TrackBuffer>
        buffers, void *meta_param, TrackMetaParamType meta_type,
        size_t meta_size) { VideoTrackDataCb(track_id,
        buffers, meta_param, meta_type, meta_size); };

    video_track_cb.event_cb = [&] (uint32_t track_id, EventType event_type,
        void *event_data, size_t event_data_size) { VideoTrackEventCb(track_id,
        event_type, event_data, event_data_size); };

    ret = recorder_->CreateVideoTrack(session_id, video_track_id,
                                      video_track_param, video_track_cb);
    assert(ret == NO_ERROR);

    std::vector<uint32_t> track_ids;
    track_ids.push_back(video_track_id);
    sessions_.insert(std::make_pair(session_id, track_ids));

    ret = recorder_->StartSession(session_id);
    assert(ret == NO_ERROR);

    // Let session run for 3 min, during this time buffer with valid data would
    // be received in track callback (VideoTrackDataCb).
    sleep(3*60);

    ret = recorder_->StopSession(session_id, false);
    assert(ret == NO_ERROR);

    ret = recorder_->DeleteVideoTrack(session_id, video_track_id);
    assert(ret == NO_ERROR);

    ret = recorder_->DeleteSession(session_id);
    assert(ret == NO_ERROR);
  }

  ret = recorder_->StopCamera(camera_ids_);
  assert(ret == NO_ERROR);

  ClearSessions();

  ret = DeInit();
  assert(ret == NO_ERROR);

  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());

}

/*
* SessionWithVideoTrackTest2: This test will test session and track Apis.
* Api test sequence:
*  - StartCamera
*  - CreateSession
*  loop Start {
*   ------------------
*   - CreateVideoTrack
*   - StartVideoTrack
*   - StopSession
*   - DeleteVideoTrack
*   ------------------
*   } loop End
*   - DeleteSession
*   - StopCamera
*/
TEST_F(RecorderGtest, SessionWithVideoTrackTest2)
{
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  assert(ret == NO_ERROR);

  ret = recorder_->StartCamera(camera_ids_, camera_start_params_);
  assert(ret == NO_ERROR);

  SessionCb session_status_cb;
  session_status_cb.event_cb = [&] ( EventType event_type, void *event_data,
      size_t event_data_size) { SessionCallbackHandler(event_type,
      event_data, event_data_size); };

  uint32_t session_id;
  ret = recorder_->CreateSession(session_status_cb, &session_id);
  assert(session_id > 0);
  assert(ret == NO_ERROR);

  for(uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    ALOGD("%s:%s: Running Test(%s) iteration = %d ", TAG, __func__,
        test_info_->name(), i);

    VideoTrackCreateParam video_track_param;
    memset(&video_track_param, 0x0, sizeof video_track_param);

    video_track_param.camera_ids[0] = 0;
    video_track_param.num_cameras   = 1;
    video_track_param.width         = 1920;
    video_track_param.height        = 1080;
    video_track_param.frame_rate    = 30;
    video_track_param.codec_type    = VideoCodecType::kYUV;
    video_track_param.out_device    = 0x01;
    uint32_t video_track_id = 1;

    TrackCb video_track_cb;
    video_track_cb.data_cb = [&] (uint32_t track_id, std::vector<TrackBuffer>
        buffers, void *meta_param, TrackMetaParamType meta_type,
        size_t meta_size) { VideoTrackDataCb(track_id,
        buffers, meta_param, meta_type, meta_size); };

    video_track_cb.event_cb = [&] (uint32_t track_id, EventType event_type,
        void *event_data, size_t event_data_size) { VideoTrackEventCb(track_id,
        event_type, event_data, event_data_size); };

    ret = recorder_->CreateVideoTrack(session_id, video_track_id,
                                      video_track_param, video_track_cb);
    assert(ret == NO_ERROR);

    std::vector<uint32_t> track_ids;
    track_ids.push_back(video_track_id);
    sessions_.insert(std::make_pair(session_id, track_ids));

    ret = recorder_->StartSession(session_id);
    assert(ret == NO_ERROR);

    // Let session run for 3 min, during this time buffer with valid data would
    // be received in track callback (VideoTrackDataCb).
    sleep(3*60);

    ret = recorder_->StopSession(session_id, false);
    assert(ret == NO_ERROR);

    ret = recorder_->DeleteVideoTrack(session_id, video_track_id);
    assert(ret == NO_ERROR);
  }

  ret = recorder_->DeleteSession(session_id);
  assert(ret == NO_ERROR);

  ret = recorder_->StopCamera(camera_ids_);
  assert(ret == NO_ERROR);

  ClearSessions();

  ret = DeInit();
  assert(ret == NO_ERROR);

  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());

}

/*
* SessionWithVideoTrackTest3: This test will test session and track Apis.
* Api test sequence:
*  - StartCamera
*  - CreateSession
*  - CreateVideoTrack
loop Start {
*   ------------------
*   - StartVideoTrack
*   - StopSession
*   ------------------
*   } loop End
*   - DeleteVideoTrack
*   - DeleteSession
*   - StopCamera
*/
TEST_F(RecorderGtest, SessionWithVideoTrackTest3)
{
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  assert(ret == NO_ERROR);

  ret = recorder_->StartCamera(camera_ids_, camera_start_params_);
  assert(ret == NO_ERROR);

  SessionCb session_status_cb;
  session_status_cb.event_cb = [&] ( EventType event_type, void *event_data,
      size_t event_data_size) { SessionCallbackHandler(event_type,
      event_data, event_data_size); };

  uint32_t session_id;
  ret = recorder_->CreateSession(session_status_cb, &session_id);
  assert(session_id > 0);
  assert(ret == NO_ERROR);

  VideoTrackCreateParam video_track_param;
  memset(&video_track_param, 0x0, sizeof video_track_param);

  video_track_param.camera_ids[0] = 0;
  video_track_param.num_cameras   = 1;
  video_track_param.width         = 1920;
  video_track_param.height        = 1080;
  video_track_param.frame_rate    = 30;
  video_track_param.codec_type    = VideoCodecType::kYUV;
  video_track_param.out_device    = 0x01;
  uint32_t video_track_id = 1;

  TrackCb video_track_cb;
  video_track_cb.data_cb = [&] (uint32_t track_id, std::vector<TrackBuffer>
      buffers, void *meta_param, TrackMetaParamType meta_type,
      size_t meta_size) { VideoTrackDataCb(track_id,
      buffers, meta_param, meta_type, meta_size); };

  video_track_cb.event_cb = [&] (uint32_t track_id, EventType event_type,
      void *event_data, size_t event_data_size) { VideoTrackEventCb(track_id,
      event_type, event_data, event_data_size); };

  ret = recorder_->CreateVideoTrack(session_id, video_track_id,
                                    video_track_param, video_track_cb);
  assert(ret == NO_ERROR);

  std::vector<uint32_t> track_ids;
  track_ids.push_back(video_track_id);
  sessions_.insert(std::make_pair(session_id, track_ids));

  for(uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    ALOGD("%s:%s: Running Test(%s) iteration = %d ", TAG, __func__,
        test_info_->name(), i);

    ret = recorder_->StartSession(session_id);
    assert(ret == NO_ERROR);

    // Let session run for 3 min, during this time buffer with valid data would
    // be received in track callback (VideoTrackDataCb).
    sleep(3*60);

    ret = recorder_->StopSession(session_id, false);
    assert(ret == NO_ERROR);
  }

  ret = recorder_->DeleteVideoTrack(session_id, video_track_id);
  assert(ret == NO_ERROR);

  ret = recorder_->DeleteSession(session_id);
  assert(ret == NO_ERROR);

  ret = recorder_->StopCamera(camera_ids_);
  assert(ret == NO_ERROR);

  ClearSessions();

  ret = DeInit();
  assert(ret == NO_ERROR);

  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());

}

void RecorderGtest::ClearSessions() {

  ALOGD("%s:%s Enter ", TAG, __func__);
  std::map <uint32_t , std::vector<uint32_t> >::iterator it = sessions_.begin();
  for (it; it != sessions_.end(); ++it) {
    it->second.clear();
  }
  sessions_.clear();
  ALOGD("%s:%s Exit ", TAG, __func__);

}

void RecorderGtest::RecorderCallbackHandler(EventType event_type,
                                            void *event_data,
                                            size_t event_data_size) {
  ALOGD("%s:%s Enter ", TAG, __func__);
  ALOGD("%s:%s Exit ", TAG, __func__);
}

void RecorderGtest::SessionCallbackHandler(EventType event_type,
                                          void *event_data,
                                          size_t event_data_size) {
    ALOGD("%s:%s: Enter", TAG, __func__);
    ALOGD("%s:%s: Exit", TAG, __func__);
}

void RecorderGtest::VideoTrackDataCb(uint32_t track_id,
                                     std::vector<TrackBuffer> buffers,
                                     void *meta_param,
                                     TrackMetaParamType meta_type,
                                     size_t meta_size) {
  ALOGD("%s:%s: Enter", TAG, __func__);
#ifdef DUMP_FRAMES
  static uint32_t id = 0;
  ++id;
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

    written_len = fwrite(buffers[0].data, sizeof(uint8_t), buffers[0].size, file);
    ALOGD("%s:%s: written_len =%d", TAG, __func__, written_len);
    if (buffers[0].size != written_len) {
      ALOGE("%s:%s: Bad Write error (%d):(%s)\n", TAG, __func__, errno,
          strerror(errno));
      goto FAIL;
    }
    ALOGD("%s:%s: Buffer(0x%x) Size(%u) Stored@(%s)\n", TAG, __func__,
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
  auto ret = recorder_->ReturnTrackBuffer(session_id, track_id, buffers);
  if(ret != 0) {
    ALOGE("%s:%s: ReturnTrackBuffer failed!", TAG, __func__);
  }
  ALOGD("%s:%s: Exit", TAG, __func__);
}

void RecorderGtest::VideoTrackEventCb(uint32_t track_id, EventType event_type,
                                      void *event_data, size_t event_data_size)
{
    ALOGD("%s:%s: Enter", TAG, __func__);
    ALOGD("%s:%s: Exit", TAG, __func__);
}