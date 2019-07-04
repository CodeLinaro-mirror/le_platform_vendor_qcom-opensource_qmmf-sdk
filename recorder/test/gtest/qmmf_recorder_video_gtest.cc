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

#define LOG_TAG "VideoGTest"

#include "recorder/test/gtest/qmmf_recorder_video_gtest.h"

using namespace qcamera;

/*
 * FaceDetectionFor1080pYUVPreview: This test will test FD at 1080p preview stream.
 * Api test sequence:
 *  - StartCamera
 *   ------------------
 *   - CreateSession
 *   - CreateVideoTrack
 *   - StartVideoTrack
 *   - SetCameraParam
 *   - StopSession
 *   - Delete Overlay object
 *   - DeleteVideoTrack
 *   - DeleteSession
 *   ------------------
 *  - StopCamera
 */
TEST_F(VideoGtest, FaceDetectionFor1080pYUVPreview) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
    test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  uint32_t stream_width  = FHD_1080p_STREAM_WIDTH;
  uint32_t stream_height = FHD_1080p_STREAM_HEIGHT;
  face_info_.fd_stream_width = stream_width;
  face_info_.fd_stream_height = stream_height;
  CameraResultCb result_cb = [this] (uint32_t camera_id,
      const CameraMetadata &result) {
           ParseFaceInfo(result, face_info_);
           ApplyFaceOveralyOnStream(face_info_);};

  CameraExtraParam empty_extra_params;
  ret = recorder_.StartCamera(camera_id_, 30, empty_extra_params, result_cb);
  ASSERT_TRUE(ret == NO_ERROR);

  SessionCb session_status_cb = CreateSessionStatusCb();
  uint32_t session_id;
  ret = recorder_.CreateSession(session_status_cb, &session_id);
  ASSERT_TRUE(session_id > 0);
  ASSERT_TRUE(ret == NO_ERROR);
  VideoTrackCreateParam video_track_param{camera_id_, VideoFormat::kYUV,
                                          stream_width, stream_height, 30};
  video_track_param.low_power_mode = true;
  uint32_t video_track_id = 1;

  TrackCb video_track_cb;
  video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
    std::vector<BufferDescriptor> buffers,
    std::vector<MetaData> meta_buffers) {
    VideoTrackYUVDataCb(session_id, track_id, buffers, meta_buffers); };

  video_track_cb.event_cb = [&] (uint32_t track_id, EventType event_type,
    void *event_data, size_t event_data_size) { VideoTrackEventCb(track_id,
    event_type, event_data, event_data_size); };

  ret = recorder_.CreateVideoTrack(session_id, video_track_id,
  video_track_param, video_track_cb);
  ASSERT_TRUE(ret == NO_ERROR);

  std::vector<uint32_t> track_ids;
  track_ids.push_back(video_track_id);
  sessions_.insert(std::make_pair(session_id, track_ids));

  ret = recorder_.StartSession(session_id);
  ASSERT_TRUE(ret == NO_ERROR);

  CameraMetadata meta;
  uint8_t fd_mode = ANDROID_STATISTICS_FACE_DETECT_MODE_SIMPLE;

  ret = recorder_.GetCameraParam(camera_id_, meta);
  ASSERT_TRUE(ret == NO_ERROR);
  CameraMetadata static_meta;
  ret = recorder_.GetCameraCharacteristics(camera_id_, static_meta);
  ASSERT_TRUE(ret == NO_ERROR);
  EXPECT_TRUE(static_meta.exists(ANDROID_STATISTICS_FACE_DETECT_MODE));

  meta.update(ANDROID_STATISTICS_FACE_DETECT_MODE, &fd_mode, 1);
  ret = recorder_.SetCameraParam(camera_id_, meta);
  ASSERT_TRUE(ret == NO_ERROR);

  face_bbox_active_ = true;
  face_track_id_ = video_track_id;
  TEST_INFO("Enable Face Detection");

  sleep(record_duration_);

  ret = recorder_.StopSession(session_id, false);
  ASSERT_TRUE(ret == NO_ERROR);
  face_overlay_lock_.lock();
  for (uint32_t i = 0; i < face_bbox_id_.size(); i++) {
    // Delete overlay object.
    ret = recorder_.DeleteOverlayObject(face_track_id_,
                                        face_bbox_id_[i]);
    ASSERT_TRUE(ret == NO_ERROR);
  }
  face_bbox_active_ = false;
  face_bbox_id_.clear();
  face_overlay_lock_.unlock();

  ret = recorder_.DeleteVideoTrack(session_id, video_track_id);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteSession(session_id);
  ASSERT_TRUE(ret == NO_ERROR);

  ClearSessions();

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());
}

/*
 * FaceDetectionFor1080pAVCVideo: This test will test FD at 1080p video stream.
 * Api test sequence:
 *  - StartCamera
 *   ------------------
 *   - CreateSession
 *   - CreateVideoTrack
 *   - StartVideoTrack
 *   - SetCameraParam
 *   - StopSession
 *   - Delete Overlay object
 *   - DeleteVideoTrack
 *   - DeleteSession
 *   ------------------
 *  - StopCamera
 */
TEST_F(VideoGtest, FaceDetectionFor1080pAVCVideo) {

  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  VideoFormat format_type = VideoFormat::kAVC;
  uint32_t stream_width  = FHD_1080p_STREAM_WIDTH;
  uint32_t stream_height = FHD_1080p_STREAM_HEIGHT;

  face_info_.fd_stream_width = stream_width;
  face_info_.fd_stream_height = stream_height;
  CameraResultCb result_cb = [this] (uint32_t camera_id,
      const CameraMetadata &result) {
           ParseFaceInfo(result, face_info_);
           ApplyFaceOveralyOnStream(face_info_);};

  CameraExtraParam empty_extra_params;
  ret = recorder_.StartCamera(camera_id_, 30, empty_extra_params, result_cb);
  ASSERT_TRUE(ret == NO_ERROR);

  SessionCb session_status_cb = CreateSessionStatusCb();
  uint32_t session_id;
  ret = recorder_.CreateSession(session_status_cb, &session_id);
  ASSERT_TRUE(session_id > 0);
  ASSERT_TRUE(ret == NO_ERROR);
  VideoTrackCreateParam video_track_param{camera_id_, format_type,
                                          stream_width, stream_height, 30};

  uint32_t video_track_id = 1;
  if (dump_bitstream_.IsEnabled()) {
    StreamDumpInfo dumpinfo = { video_track_param.format_type, session_id,
                                video_track_id, stream_width, stream_height };
    ret = dump_bitstream_.SetUp(dumpinfo);
    ASSERT_TRUE(ret == NO_ERROR);
  }

  TrackCb video_track_cb;
  video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
    std::vector<BufferDescriptor> buffers,
    std::vector<MetaData> meta_buffers) {
      VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
    };

  video_track_cb.event_cb =
    [this] (uint32_t track_id, EventType event_type,void *event_data,
           size_t event_data_size) -> void { VideoTrackEventCb(track_id,
             event_type, event_data, event_data_size); };

  ret = recorder_.CreateVideoTrack(session_id, video_track_id,
                                   video_track_param, video_track_cb);
  ASSERT_TRUE(ret == NO_ERROR);

  std::vector<uint32_t> track_ids;
  track_ids.push_back(video_track_id);
  sessions_.insert(std::make_pair(session_id, track_ids));

  ret = recorder_.StartSession(session_id);
  ASSERT_TRUE(ret == NO_ERROR);

  CameraMetadata meta;
  uint8_t fd_mode = ANDROID_STATISTICS_FACE_DETECT_MODE_SIMPLE;

  ret = recorder_.GetCameraParam(camera_id_, meta);
  ASSERT_TRUE(ret == NO_ERROR);
  CameraMetadata static_meta;
  ret = recorder_.GetCameraCharacteristics(camera_id_, static_meta);
  ASSERT_TRUE(ret == NO_ERROR);
  EXPECT_TRUE(static_meta.exists(ANDROID_STATISTICS_FACE_DETECT_MODE));

  meta.update(ANDROID_STATISTICS_FACE_DETECT_MODE, &fd_mode, 1);
  ret = recorder_.SetCameraParam(camera_id_, meta);
  ASSERT_TRUE(ret == NO_ERROR);

  face_bbox_active_ = true;
  face_track_id_ = video_track_id;
  TEST_INFO("Enable Face Detection");

  sleep(record_duration_);

  ret = recorder_.StopSession(session_id, false);
  ASSERT_TRUE(ret == NO_ERROR);

  face_overlay_lock_.lock();
  for (uint32_t i = 0; i < face_bbox_id_.size(); i ++) {
    // Delete overlay object.
    ret = recorder_.DeleteOverlayObject(face_track_id_,
                                        face_bbox_id_[i]);
    ASSERT_TRUE(ret == NO_ERROR);
  }
  face_bbox_active_ = false;
  face_bbox_id_.clear();
  face_overlay_lock_.unlock();

  ret = recorder_.DeleteVideoTrack(session_id, video_track_id);
  ASSERT_TRUE(ret == NO_ERROR);
  ret = recorder_.DeleteSession(session_id);
  ASSERT_TRUE(ret == NO_ERROR);
  ClearSessions();
  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);
  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  dump_bitstream_.CloseAll();
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
TEST_F(VideoGtest, SessionWith1080pYUVTrack) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.StartCamera(camera_id_, 30);
  ASSERT_TRUE(ret == NO_ERROR);

  for(uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
        test_info_->name(), i);

    SessionCb session_status_cb = CreateSessionStatusCb();
    uint32_t session_id;
    ret = recorder_.CreateSession(session_status_cb, &session_id);
    ASSERT_TRUE(session_id > 0);
    ASSERT_TRUE(ret == NO_ERROR);
    VideoTrackCreateParam video_track_param{camera_id_, VideoFormat::kYUV,
                                            1920, 1080, 30};
    uint32_t video_track_id = 1;
    if (ubwc_stream_enable_) {
      video_track_param.low_power_mode = true;
    }
    TrackCb video_track_cb;
    video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
        std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
          VideoTrackYUVDataCb(session_id, track_id, buffers, meta_buffers); };

    video_track_cb.event_cb = [&] (uint32_t track_id, EventType event_type,
        void *event_data, size_t event_data_size) { VideoTrackEventCb(track_id,
        event_type, event_data, event_data_size); };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id,
                                     video_track_param, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    std::vector<uint32_t> track_ids;
    track_ids.push_back(video_track_id);
    sessions_.insert(std::make_pair(session_id, track_ids));

    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    // Let session run for time record_duration_, during this time buffer with
    // valid data would be received in track callback (VideoTrackYUVDataCb).
    sleep(record_duration_);

    ret = recorder_.StopSession(session_id, false);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    ClearSessions();
  }

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());

}

/*
* MultiSessionsWith1080pEncTrack: This test will verify multiple sessions with
* 1080p tracks.
* Api test sequence:
*  - StartCamera
*   - CreateSession
*   - CreateVideoTrack
*   - StartSession
*   loop Start {
*   ------------------
*   - CreateSession
*   - CreateVideoTrack
*   - StartSession
*   - StopSession
*   - DeleteVideoTrack
*   - DeleteSession
*   ------------------
*   } loop End
*   - StopSession
*   - DeleteVideoTrack
*   - DeleteSession
*  - StopCamera
*/
TEST_F(VideoGtest, MultiSessionsWith1080pEncTrack) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  VideoFormat format_type = VideoFormat::kAVC;
  uint32_t width  = 1920;
  uint32_t height = 1080;
  ret = recorder_.StartCamera(camera_id_, 30);
  ASSERT_TRUE(ret == NO_ERROR);

  SessionCb session_status_cb = CreateSessionStatusCb();
  uint32_t session_id;
  ret = recorder_.CreateSession(session_status_cb, &session_id);
  ASSERT_TRUE(session_id > 0);
  ASSERT_TRUE(ret == NO_ERROR);
  VideoTrackCreateParam video_track_param{camera_id_, format_type,
                                          width, height, 30};
  uint32_t video_track_id = 1;
  if (dump_bitstream_.IsEnabled()) {
    StreamDumpInfo dumpinfo = { format_type, session_id, video_track_id,
                                width, height };
    ret = dump_bitstream_.SetUp(dumpinfo);
    ASSERT_TRUE(ret == NO_ERROR);
  }

  TrackCb video_track_cb;
  video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
      std::vector<BufferDescriptor> buffers,
      std::vector<MetaData> meta_buffers) {
        VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
      };

  video_track_cb.event_cb = [&] (uint32_t track_id, EventType event_type,
      void *event_data, size_t event_data_size) { VideoTrackEventCb(track_id,
      event_type, event_data, event_data_size); };

  ret = recorder_.CreateVideoTrack(session_id, video_track_id,
                                    video_track_param, video_track_cb);
  ASSERT_TRUE(ret == NO_ERROR);

  std::vector<uint32_t> track_ids;
  track_ids.push_back(video_track_id);
  sessions_.insert(std::make_pair(session_id, track_ids));

  ret = recorder_.StartSession(session_id);
  ASSERT_TRUE(ret == NO_ERROR);

  for(uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
        test_info_->name(), i);

    uint32_t session_id2;
    ret = recorder_.CreateSession(session_status_cb, &session_id2);
    ASSERT_TRUE(session_id2 > 0);
    ASSERT_TRUE(ret == NO_ERROR);

    uint32_t video_track_id2 = 2;
    TrackCb video_track_cb;
    video_track_cb.data_cb = [&] (uint32_t track_id,
                              std::vector<BufferDescriptor> buffers,
                              std::vector<MetaData> meta_buffers) {
     auto ret = recorder_.ReturnTrackBuffer(session_id2, track_id, buffers);
     ASSERT_TRUE(ret == NO_ERROR); };

    video_track_cb.event_cb = [&] (uint32_t track_id, EventType event_type,
        void *event_data, size_t event_data_size) { VideoTrackEventCb(track_id,
        event_type, event_data, event_data_size); };

    ret = recorder_.CreateVideoTrack(session_id2, video_track_id2,
                                      video_track_param, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.StartSession(session_id2);
    ASSERT_TRUE(ret == NO_ERROR);

    // Let session run for record_duration_, during this time buffer with valid
    // data would be received in track callback (VideoTrackDataCb).
    sleep(record_duration_);

    ret = recorder_.StopSession(session_id2, false);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id2, video_track_id2);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteSession(session_id2);
    ASSERT_TRUE(ret == NO_ERROR);
  }

  sleep(record_duration_);

  ret = recorder_.StopSession(session_id, false);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteVideoTrack(session_id, video_track_id);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteSession(session_id);
  ASSERT_TRUE(ret == NO_ERROR);

  ClearSessions();

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  dump_bitstream_.CloseAll();
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
TEST_F(VideoGtest, SessionWith1080pEncTrack) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  VideoFormat format_type = VideoFormat::kAVC;
  uint32_t width  = 1920;
  uint32_t height = 1080;

  ret = recorder_.StartCamera(camera_id_, 30);
  ASSERT_TRUE(ret == NO_ERROR);

  for(uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
        test_info_->name(), i);

    SessionCb session_status_cb = CreateSessionStatusCb();
    uint32_t session_id;
    ret = recorder_.CreateSession(session_status_cb, &session_id);
    ASSERT_TRUE(session_id > 0);
    ASSERT_TRUE(ret == NO_ERROR);
    VideoTrackCreateParam video_track_param{camera_id_, format_type,
                                            width, height, 30};
    uint32_t video_track_id = 1;

    if (dump_bitstream_.IsEnabled()) {
      StreamDumpInfo dumpinfo = { format_type, session_id, video_track_id,
                                  width, height };
      ret = dump_bitstream_.SetUp(dumpinfo);
      ASSERT_TRUE(ret == NO_ERROR);
    }

    TrackCb video_track_cb;
    video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
      std::vector<BufferDescriptor> buffers,
      std::vector<MetaData> meta_buffers) {
        VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
      };

    video_track_cb.event_cb = [&] (uint32_t track_id, EventType event_type,
        void *event_data, size_t event_data_size) { VideoTrackEventCb(track_id,
        event_type, event_data, event_data_size); };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id,
                                      video_track_param, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    std::vector<uint32_t> track_ids;
    track_ids.push_back(video_track_id);
    sessions_.emplace(session_id, track_ids);

    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    // Let session run for record_duration_, during this time buffer with valid
    // data would be received in track callback (VideoTrackDataCb).
    std::this_thread::sleep_for(std::chrono::seconds(record_duration_));

    ret = recorder_.StopSession(session_id, false);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    dump_bitstream_.CloseAll();
    sessions_.erase(session_id);
  }
  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());

}

/*
* SlavemodeSessionWith480pYuvTrack: This will test session with 480p YUV track.
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
TEST_F(VideoGtest, SlavemodeSessionWith480pYuvTrack) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  VideoFormat format_type = VideoFormat::kYUV;
  uint32_t width  = 640;
  uint32_t height = 480;

  CameraExtraParam extra_param;
  CameraSlaveMode camera_slave_mode;
  camera_slave_mode.mode = SlaveMode::kSlave;
  extra_param.Update(QMMF_CAMERA_SLAVE_MODE, camera_slave_mode);

  ret = recorder_.StartCamera(camera_id_, 30, extra_param);
  if (ret == -ENOENT) {
    // Wait for a master client to open the camera.
    std::unique_lock<std::mutex> lk(camera_state_lock_);
    std::chrono::milliseconds timeout(10000);

    camera_state_updated_.wait_for(lk, timeout, [&]() {
      return (camera_state_[camera_id_] == GtestCameraState::kOpened);
    });
    ret = recorder_.StartCamera(camera_id_, 30, extra_param);
    ASSERT_TRUE(ret == NO_ERROR);
  }

  // Random number generator.
  std::random_device rdev;
  std::mt19937 rgen(rdev());
  std::uniform_int_distribution<int32_t> idist(1,15);

  for(uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
        test_info_->name(), i);

    SessionCb session_status_cb = CreateSessionStatusCb();
    uint32_t session_id;
    ret = recorder_.CreateSession(session_status_cb, &session_id);
    ASSERT_TRUE(session_id > 0);
    ASSERT_TRUE(ret == NO_ERROR);
    VideoTrackCreateParam video_track_param{camera_id_, format_type,
                                            width, height, 30};
    uint32_t video_track_id = 1;

    TrackCb video_track_cb;
    video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
      std::vector<BufferDescriptor> buffers,
      std::vector<MetaData> meta_buffers) {
        VideoTrackYUVDataCb(session_id, track_id, buffers, meta_buffers);
      };

    video_track_cb.event_cb = [&] (uint32_t track_id, EventType event_type,
        void *event_data, size_t event_data_size) { VideoTrackEventCb(track_id,
        event_type, event_data, event_data_size); };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id,
                                      video_track_param, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    std::vector<uint32_t> track_ids;
    track_ids.push_back(video_track_id);
    sessions_.emplace(session_id, track_ids);

    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    // Let session run for random duration or until signaled by the
    // master camera client to close camera.
    {
      std::unique_lock<std::mutex> lk(camera_state_lock_);
      std::chrono::seconds timeout(idist(rdev));

      camera_state_updated_.wait_for(lk, timeout, [&]() {
        return (camera_state_[camera_id_] == GtestCameraState::kClosing);
      });
    }

    ret = recorder_.StopSession(session_id, false);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    sessions_.erase(session_id);
  }
  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());

}

/*
* SessionWith1080Enc30fps1080pMJpeg10fps:
*                            This test will test session with one 1080p H264 30
*                            fps and 1080p Mjpeg 10 fps.
* API test sequence:
*  - StartCamera
*  - CreateSession
*  - CreateVideoTrack for h264,mjpeg
*  - StartVideoTrack
*  - StopSession
*  - DeleteVideoTrack
*  - DeleteSession
*  - StopCamera
*/
TEST_F(VideoGtest, SessionWith1080Enc30fps1080pMJpeg10fps) {
  fprintf(stderr, "\n---------- Run Test %s.%s ------------\n",
          test_info_->test_case_name(), test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  VideoFormat format_type = VideoFormat::kAVC;
  uint32_t width = 1920;
  uint32_t height = 1080;

  ret = recorder_.StartCamera(camera_id_, 30);
  ASSERT_TRUE(ret == NO_ERROR);

  for (uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr, "test iteration = %d/%d\n", i, iteration_count_);

    SessionCb session_status_cb = CreateSessionStatusCb();
    uint32_t session_id;
    ret = recorder_.CreateSession(session_status_cb, &session_id);
    ASSERT_TRUE(session_id > 0);
    ASSERT_TRUE(ret == NO_ERROR);
    VideoTrackCreateParam video_track_param_1{camera_id_, format_type, 1920,
                                              1080, 30};
    uint32_t video_track_id_1 = 1;

    if (dump_bitstream_.IsEnabled()) {
      StreamDumpInfo dumpinfo = { format_type, session_id, video_track_id_1,
                                  width, height };
      ret = dump_bitstream_.SetUp(dumpinfo);
      ASSERT_TRUE(ret == NO_ERROR);
    }

    TrackCb video_track_cb;
    video_track_cb.data_cb = [&, session_id](
        uint32_t track_id, std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
      VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
    };

    video_track_cb.event_cb = [&](uint32_t track_id, EventType event_type,
                                  void *event_data, size_t event_data_size) {
      VideoTrackEventCb(track_id, event_type, event_data, event_data_size);
    };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_1,
                                     video_track_param_1, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    std::vector<uint32_t> track_ids;
    track_ids.push_back(video_track_id_1);
    sessions_.insert(std::make_pair(session_id, track_ids));

    uint32_t video_track_id_mjpeg = 2;
    VideoTrackCreateParam video_track_param_2{camera_id_, VideoFormat::kJPEG,
                                              1920, 1080, 10};
    if (dump_bitstream_.IsEnabled()) {
      StreamDumpInfo dumpinfo = { VideoFormat::kJPEG, session_id,
                                  video_track_id_mjpeg, width, height};
      ret = dump_bitstream_.SetUp(dumpinfo);
      ASSERT_TRUE(ret == NO_ERROR);
    }

    video_track_cb.data_cb = [&, session_id](
        uint32_t track_id, std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
      VideoTrackEncDataCb(session_id, video_track_id_mjpeg, buffers,
                             meta_buffers);
    };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_mjpeg,
                                     video_track_param_2, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);
    track_ids.push_back(video_track_id_mjpeg);
    sessions_.insert(std::make_pair(session_id, track_ids));

    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    sleep(record_duration_/2);

    uint32_t jpeg_quality = 50;
    ret = recorder_.SetVideoTrackParam(session_id, video_track_id_mjpeg,
                                       CodecParamType::kJPEGQuality,
                                       &jpeg_quality, sizeof(jpeg_quality));
    ASSERT_TRUE(ret == NO_ERROR);

    sleep(record_duration_);

    ret = recorder_.StopSession(session_id, false);
    ASSERT_TRUE(ret == NO_ERROR);
    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_1);
    ASSERT_TRUE(ret == NO_ERROR);
    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_mjpeg);
    ASSERT_TRUE(ret == NO_ERROR);
    ret = recorder_.DeleteSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);
    dump_bitstream_.CloseAll();
  }
  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  fprintf(stderr, "---------- Test Completed %s.%s ----------\n",
          test_info_->test_case_name(), test_info_->name());
}

/*
* SessionWith4kMJpeg: This test will test session with one 4k
*                     jpeg encoded track.
* API test sequence:
*  - StartCamera
*  - CreateSession
*  - CreateVideoTrack
*  - StartVideoTrack
*  - StopSession
*  - DeleteVideoTrack
*  - DeleteSession
*  - StopCamera
*/
TEST_F(VideoGtest, SessionWith4kMJpeg) {
  fprintf(stderr, "\n---------- Run Test %s.%s ------------\n",
          test_info_->test_case_name(), test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  VideoFormat format_type = VideoFormat::kJPEG;
  uint32_t width = 3840;
  uint32_t height = 2160;

  ret = recorder_.StartCamera(camera_id_, 30);
  ASSERT_TRUE(ret == NO_ERROR);

  for (uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr, "test iteration = %d/%d\n", i, iteration_count_);
    SessionCb session_status_cb = CreateSessionStatusCb();

    uint32_t session_id;
    ret = recorder_.CreateSession(session_status_cb, &session_id);
    ASSERT_TRUE(session_id > 0);
    ASSERT_TRUE(ret == NO_ERROR);
    VideoTrackCreateParam video_track_param{ camera_id_, format_type,
                                             width, height, 30};
    uint32_t video_track_id = 1;

    if (dump_bitstream_.IsEnabled()) {
      StreamDumpInfo dumpinfo = { format_type, session_id, video_track_id,
                                  width, height };
      ret = dump_bitstream_.SetUp(dumpinfo);
      ASSERT_TRUE(ret == NO_ERROR);
    }

    TrackCb video_track_cb;
    video_track_cb.data_cb = [&, session_id](
        uint32_t track_id, std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
      VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
    };

    video_track_cb.event_cb = [&](uint32_t track_id, EventType event_type,
                                  void *event_data, size_t event_data_size) {
      VideoTrackEventCb(track_id, event_type, event_data, event_data_size);
    };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id,
                                     video_track_param, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    std::vector<uint32_t> track_ids;
    track_ids.push_back(video_track_id);
    sessions_.insert(std::make_pair(session_id, track_ids));

    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    // Let session run for record_duration_, during this time buffer with valid
    // data would be received in track callback (VideoTrackDataCb).
    sleep(record_duration_);

    ret = recorder_.StopSession(session_id, false);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    dump_bitstream_.CloseAll();
  }
  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  fprintf(stderr, "---------- Test Completed %s.%s ----------\n",
          test_info_->test_case_name(), test_info_->name());
}

/*
* SessionWith1080pEncTrackPartialMeta: This test will test session with 1080p
* h264 track. Api test sequence:
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
TEST_F(VideoGtest, SessionWith1080pEncTrackPartialMeta) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  VideoFormat format_type = VideoFormat::kAVC;
  uint32_t width  = 1920;
  uint32_t height = 1080;

  CameraResultCb result_cb = [&] (uint32_t camera_id,
      const CameraMetadata &result) {
        if (result.exists(ANDROID_REQUEST_FRAME_COUNT)) {
          TEST_ERROR("%s: MetaData FrameNumber=%d", __func__,
              result.find(ANDROID_REQUEST_FRAME_COUNT).data.i32[0]);
        }
      };

  CameraExtraParam extra_params;
  PartialMetadata partial_metadata;
  partial_metadata.enable = true;
  extra_params.Update(QMMF_PARTIAL_METADATA, partial_metadata);
  ret = recorder_.StartCamera(camera_id_, 30, extra_params, result_cb);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.GetDefaultCaptureParam(camera_id_, static_info_);
  if (NO_ERROR != ret) {
    TEST_ERROR("%s Unable to query default capture parameters!\n",
          __func__);
  }

  for(uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
        test_info_->name(), i);

    SessionCb session_status_cb = CreateSessionStatusCb();
    uint32_t session_id;
    ret = recorder_.CreateSession(session_status_cb, &session_id);
    ASSERT_TRUE(session_id > 0);
    ASSERT_TRUE(ret == NO_ERROR);
    VideoTrackCreateParam video_track_param{camera_id_, format_type,
                                            width, height, 30};
    uint32_t video_track_id = 1;

    if (dump_bitstream_.IsEnabled()) {
      StreamDumpInfo dumpinfo = { format_type, session_id, video_track_id,
                                  width, height };
      ret = dump_bitstream_.SetUp(dumpinfo);
      ASSERT_TRUE(ret == NO_ERROR);
    }

    TrackCb video_track_cb;
    video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
      std::vector<BufferDescriptor> buffers,
      std::vector<MetaData> meta_buffers) {
        VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
      };

    video_track_cb.event_cb = [&] (uint32_t track_id, EventType event_type,
        void *event_data, size_t event_data_size) { VideoTrackEventCb(track_id,
        event_type, event_data, event_data_size); };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id,
                                      video_track_param, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    std::vector<uint32_t> track_ids;
    track_ids.push_back(video_track_id);
    sessions_.insert(std::make_pair(session_id, track_ids));

    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    // Let session run for record_duration_, during this time buffer with valid
    // data would be received in track callback (VideoTrackDataCb).
    sleep(record_duration_);

    ret = recorder_.StopSession(session_id, false);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    dump_bitstream_.CloseAll();
  }
  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());

}

/*
* SessionWith4kp30fpsEncTrack: This test will test session with one 4k
*                              30fps h264 track.
* Api test sequence:
*  - StartCamera
*  - CreateSession
*  - CreateVideoTrack
*  - StartVideoTrack
*  - StopSession
*  - DeleteVideoTrack
*  - DeleteSession
*  - StopCamera
*/
TEST_F(VideoGtest, SessionWith4kp30fpsEncTrack) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  VideoFormat format_type = VideoFormat::kAVC;
  uint32_t width  = 3840;
  uint32_t height = 2160;
  float fps = 30;

  ret = recorder_.StartCamera(camera_id_, fps);
  ASSERT_TRUE(ret == NO_ERROR);

  SessionCb session_status_cb = CreateSessionStatusCb();
  uint32_t session_id;
  ret = recorder_.CreateSession(session_status_cb, &session_id);
  ASSERT_TRUE(session_id > 0);
  ASSERT_TRUE(ret == NO_ERROR);
  VideoTrackCreateParam video_track_param{camera_id_, format_type,
                                          width, height, fps};

  FrameTrace track_trace(is_frame_debug_enabled_);

  uint32_t video_track_id = 1;
  if (dump_bitstream_.IsEnabled()) {
    StreamDumpInfo dumpinfo = { format_type, session_id, video_track_id,
                                width, height };
    ret = dump_bitstream_.SetUp(dumpinfo);
    ASSERT_TRUE(ret == NO_ERROR);
  }

  track_trace.SetUp(session_id, video_track_id, 30.0);

  TrackCb video_track_cb;
  video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
      std::vector<BufferDescriptor> buffers,
      std::vector<MetaData> meta_buffers) {
        VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
        track_trace.BufferAvailableCb(buffers.at(0));
      };

  video_track_cb.event_cb =
      [this] (uint32_t track_id, EventType event_type,
              void *event_data, size_t event_data_size) -> void
      { VideoTrackEventCb(track_id,
      event_type, event_data, event_data_size); };

  ret = recorder_.CreateVideoTrack(session_id, video_track_id,
                                    video_track_param, video_track_cb);
  ASSERT_TRUE(ret == NO_ERROR);

  std::vector<uint32_t> track_ids;
  track_ids.push_back(video_track_id);
  sessions_.insert(std::make_pair(session_id, track_ids));

  ret = recorder_.StartSession(session_id);
  ASSERT_TRUE(ret == NO_ERROR);

  // Let session run for record_duration_, during this time buffer with valid
  // data would be received in track callback (VideoTrackDataCb).
  sleep(record_duration_);

  ret = recorder_.StopSession(session_id, false);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteVideoTrack(session_id, video_track_id);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteSession(session_id);
  ASSERT_TRUE(ret == NO_ERROR);
  track_trace.Reset();
  ClearSessions();

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  dump_bitstream_.CloseAll();
  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
          test_info_->test_case_name(), test_info_->name());
}

/*
* SessionWith27Kp60fpsEncTrack: This test will test session with one 2.7K
* 60fps h264 track.
* Api test sequence:
*  - StartCamera
*  - CreateSession
*  - CreateVideoTrack
*  - StartVideoTrack
*  - StopSession
*  - DeleteVideoTrack
*  - DeleteSession
*  - StopCamera
*/
TEST_F(VideoGtest, SessionWith27Kp60fpsEncTrack) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  VideoFormat format_type = VideoFormat::kAVC;
  uint32_t width  = 2704;
  uint32_t height = 1520;
  float fps = 60;

  ret = recorder_.StartCamera(camera_id_, fps);
  ASSERT_TRUE(ret == NO_ERROR);

  SessionCb session_status_cb = CreateSessionStatusCb();
  uint32_t session_id;
  ret = recorder_.CreateSession(session_status_cb, &session_id);
  ASSERT_TRUE(session_id > 0);
  ASSERT_TRUE(ret == NO_ERROR);
  VideoTrackCreateParam video_track_param{camera_id_, format_type,
                                          width, height, fps};

  uint32_t video_track_id = 1;
  if (dump_bitstream_.IsEnabled()) {
    StreamDumpInfo dumpinfo = { format_type, session_id, video_track_id,
                                width, height };
    ret = dump_bitstream_.SetUp(dumpinfo);
    ASSERT_TRUE(ret == NO_ERROR);
  }

  TrackCb video_track_cb;
  video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
      std::vector<BufferDescriptor> buffers,
      std::vector<MetaData> meta_buffers) {
        VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
      };

  video_track_cb.event_cb =
      [this] (uint32_t track_id, EventType event_type,
              void *event_data, size_t event_data_size) -> void
      { VideoTrackEventCb(track_id,
      event_type, event_data, event_data_size); };

  ret = recorder_.CreateVideoTrack(session_id, video_track_id,
                                    video_track_param, video_track_cb);
  ASSERT_TRUE(ret == NO_ERROR);

  std::vector<uint32_t> track_ids;
  track_ids.push_back(video_track_id);
  sessions_.insert(std::make_pair(session_id, track_ids));

  ret = recorder_.StartSession(session_id);
  ASSERT_TRUE(ret == NO_ERROR);

  // Let session run for record_duration_, during this time buffer with valid
  // data would be received in track callback (VideoTrackDataCb).
  sleep(record_duration_);

  ret = recorder_.StopSession(session_id, false);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteVideoTrack(session_id, video_track_id);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteSession(session_id);
  ASSERT_TRUE(ret == NO_ERROR);

   ClearSessions();

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  dump_bitstream_.CloseAll();
  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
          test_info_->test_case_name(), test_info_->name());
}

/*
 * SessionWith1080p120fps480p30fpsEncTrack: This test will test session with one 1080p
 * 120fps h264 track and preview 480p30fps.
 * Api test sequence:
 *  - StartCamera
 *  - CreateSession
 *  - CreateVideoTrack
 *  - StartVideoTrack
 *  - StopSession
 *  - DeleteVideoTrack
 *  - DeleteSession
 *  - StopCamera
 */
TEST_F(VideoGtest, SessionWith1080p120fps480p30fpsEncTrack) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  VideoFormat format_type = VideoFormat::kAVC;
  uint32_t width  = 1920;
  uint32_t height = 1080;
  float fps = 120;

  ret = recorder_.StartCamera(camera_id_, fps);
  ASSERT_TRUE(ret == NO_ERROR);

  SessionCb session_status_cb = CreateSessionStatusCb();
  uint32_t session_id;
  ret = recorder_.CreateSession(session_status_cb, &session_id);
  ASSERT_TRUE(session_id > 0);
  ASSERT_TRUE(ret == NO_ERROR);
  VideoTrackCreateParam video_track_param{ camera_id_, format_type,
                                          width, height, fps };

  uint32_t video_track_id = 1;
  if (dump_bitstream_.IsEnabled()) {
    StreamDumpInfo dumpinfo = { format_type, session_id, video_track_id,
                                width, height };
    ret = dump_bitstream_.SetUp(dumpinfo);
    ASSERT_TRUE(ret == NO_ERROR);
  }

  TrackCb video_track_cb;
  video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
      std::vector<BufferDescriptor> buffers,
      std::vector<MetaData> meta_buffers) {
        VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
      };

  video_track_cb.event_cb =
      [this] (uint32_t track_id, EventType event_type,
              void *event_data, size_t event_data_size) -> void
      { VideoTrackEventCb(track_id,
      event_type, event_data, event_data_size); };

  ret = recorder_.CreateVideoTrack(session_id, video_track_id,
                                    video_track_param, video_track_cb);
  ASSERT_TRUE(ret == NO_ERROR);

  std::vector<uint32_t> track_ids;
  track_ids.push_back(video_track_id);

  width  = 720;
  height = 480;
  fps = 30;
  uint32_t video_track480p_id = 2;
  if (dump_bitstream_.IsEnabled()) {
    StreamDumpInfo dumpinfo = { format_type, session_id, video_track480p_id,
                                width, height };
    ret = dump_bitstream_.SetUp(dumpinfo);
  }

  video_track_param.camera_id   = camera_id_;
  video_track_param.width       = width;
  video_track_param.height      = height;
  video_track_param.frame_rate  = fps;
  video_track_param.format_type = format_type;
  video_track_param.low_power_mode = true;

  video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
      std::vector<BufferDescriptor> buffers,
      std::vector<MetaData> meta_buffers) {
        VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
      };

  video_track_cb.event_cb =
      [this] (uint32_t track_id, EventType event_type,
              void *event_data, size_t event_data_size) -> void
      { VideoTrackEventCb(track_id,
      event_type, event_data, event_data_size); };

  ret = recorder_.CreateVideoTrack(session_id, video_track480p_id,
                                   video_track_param, video_track_cb);
  ASSERT_TRUE(ret == NO_ERROR);

  track_ids.push_back(video_track480p_id);
  sessions_.insert(std::make_pair(session_id, track_ids));

  ret = recorder_.StartSession(session_id);
  ASSERT_TRUE(ret == NO_ERROR);

  sleep(30);

  ret = recorder_.StopSession(session_id, false);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteVideoTrack(session_id, video_track_id);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteVideoTrack(session_id, video_track480p_id);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteSession(session_id);
  ASSERT_TRUE(ret == NO_ERROR);

  ClearSessions();

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  dump_bitstream_.CloseAll();
  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
          test_info_->test_case_name(), test_info_->name());
}

/*
* SessionWith1080p120fpsEncTrack: This test will test session with one 1080p
* 120fps h264 track.
* Api test sequence:
*  - StartCamera
*  - CreateSession
*  - CreateVideoTrack
*  - StartVideoTrack
*  - StopSession
*  - DeleteVideoTrack
*  - DeleteSession
*  - StopCamera
*/
TEST_F(VideoGtest, SessionWith1080p120fpsEncTrack) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  VideoFormat format_type = VideoFormat::kAVC;
  uint32_t width  = 1920;
  uint32_t height = 1080;
  float fps = 120;

  ret = recorder_.StartCamera(camera_id_, fps);
  ASSERT_TRUE(ret == NO_ERROR);

  SessionCb session_status_cb = CreateSessionStatusCb();
  uint32_t session_id;
  ret = recorder_.CreateSession(session_status_cb, &session_id);
  ASSERT_TRUE(session_id > 0);
  ASSERT_TRUE(ret == NO_ERROR);
  VideoTrackCreateParam video_track_param{camera_id_, format_type,
                                          width, height, fps};
  uint32_t video_track_id = 1;
  if (dump_bitstream_.IsEnabled()) {
    StreamDumpInfo dumpinfo = { format_type, session_id, video_track_id,
                                width, height };
    ret = dump_bitstream_.SetUp(dumpinfo);
    ASSERT_TRUE(ret == NO_ERROR);
  }

  TrackCb video_track_cb;
  video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
      std::vector<BufferDescriptor> buffers,
      std::vector<MetaData> meta_buffers) {
        VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
      };

  video_track_cb.event_cb =
      [this] (uint32_t track_id, EventType event_type,
              void *event_data, size_t event_data_size) -> void
      { VideoTrackEventCb(track_id,
      event_type, event_data, event_data_size); };

  ret = recorder_.CreateVideoTrack(session_id, video_track_id,
                                    video_track_param, video_track_cb);
  ASSERT_TRUE(ret == NO_ERROR);

  std::vector<uint32_t> track_ids;
  track_ids.push_back(video_track_id);
  sessions_.insert(std::make_pair(session_id, track_ids));

  ret = recorder_.StartSession(session_id);
  ASSERT_TRUE(ret == NO_ERROR);

  sleep(30);

  ret = recorder_.StopSession(session_id, false);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteVideoTrack(session_id, video_track_id);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteSession(session_id);
  ASSERT_TRUE(ret == NO_ERROR);

  ClearSessions();

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  dump_bitstream_.CloseAll();
  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
          test_info_->test_case_name(), test_info_->name());
}

/*
* SessionWith1080p60fpsEncTrack: This test will test session with one 1080p
* 60fps h264 track.
* Api test sequence:
*  - StartCamera
*  - CreateSession
*  - CreateVideoTrack
*  - StartVideoTrack
*  - StopSession
*  - DeleteVideoTrack
*  - DeleteSession
*  - StopCamera
*/
TEST_F(VideoGtest, SessionWith1080p60fpsEncTrack) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  VideoFormat format_type = VideoFormat::kAVC;
  uint32_t width  = 1920;
  uint32_t height = 1080;
  float fps = 60;

  ret = recorder_.StartCamera(camera_id_, fps);
  ASSERT_TRUE(ret == NO_ERROR);

  SessionCb session_status_cb = CreateSessionStatusCb();
  uint32_t session_id;
  ret = recorder_.CreateSession(session_status_cb, &session_id);
  ASSERT_TRUE(session_id > 0);
  ASSERT_TRUE(ret == NO_ERROR);
  VideoTrackCreateParam video_track_param{camera_id_, format_type,
                                          width, height, fps};
  uint32_t video_track_id = 1;
  if (dump_bitstream_.IsEnabled()) {
    StreamDumpInfo dumpinfo = { format_type, session_id, video_track_id,
                                width, height };
    ret = dump_bitstream_.SetUp(dumpinfo);
    ASSERT_TRUE(ret == NO_ERROR);
  }

  TrackCb video_track_cb;
  video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
      std::vector<BufferDescriptor> buffers,
      std::vector<MetaData> meta_buffers) {
        VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
      };

  video_track_cb.event_cb =
      [this] (uint32_t track_id, EventType event_type,
              void *event_data, size_t event_data_size) -> void
      { VideoTrackEventCb(track_id,
      event_type, event_data, event_data_size); };

  ret = recorder_.CreateVideoTrack(session_id, video_track_id,
                                    video_track_param, video_track_cb);
  ASSERT_TRUE(ret == NO_ERROR);

  std::vector<uint32_t> track_ids;
  track_ids.push_back(video_track_id);
  sessions_.insert(std::make_pair(session_id, track_ids));

  ret = recorder_.StartSession(session_id);
  ASSERT_TRUE(ret == NO_ERROR);

  sleep(30);

  ret = recorder_.StopSession(session_id, false);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteVideoTrack(session_id, video_track_id);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteSession(session_id);
  ASSERT_TRUE(ret == NO_ERROR);

  ClearSessions();

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  dump_bitstream_.CloseAll();
  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
          test_info_->test_case_name(), test_info_->name());
}

/*
* SessionWith4kp30fps480p30fpsEncTrack: This test will test session with one 4k
* 30fps h264 track and one 480p 30fps h264 track.
* Api test sequence:
*  - StartCamera
*  - CreateSession
*  - CreateVideoTrack
*  - StartVideoTrack
*  - StopSession
*  - DeleteVideoTrack
*  - DeleteSession
*  - StopCamera
*/
TEST_F(VideoGtest, SessionWith4kp30fps480p30fpsEncTrack) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  VideoFormat format_type = VideoFormat::kAVC;
  uint32_t width  = 3840;
  uint32_t height = 2160;
  float fps = 30;

  ret = recorder_.StartCamera(camera_id_, fps);
  ASSERT_TRUE(ret == NO_ERROR);

  SessionCb session_status_cb = CreateSessionStatusCb();
  uint32_t session_id;
  ret = recorder_.CreateSession(session_status_cb, &session_id);
  ASSERT_TRUE(session_id > 0);
  ASSERT_TRUE(ret == NO_ERROR);
  VideoTrackCreateParam video_track_param{camera_id_, format_type,
                                          width, height, fps};

  uint32_t video_track_id = 1;
  if (dump_bitstream_.IsEnabled()) {
    StreamDumpInfo dumpinfo = { format_type, session_id, video_track_id,
                                width, height };
    ret = dump_bitstream_.SetUp(dumpinfo);
    ASSERT_TRUE(ret == NO_ERROR);
  }

  TrackCb video_track_cb;
  video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
      std::vector<BufferDescriptor> buffers,
      std::vector<MetaData> meta_buffers) {
        VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
      };

  video_track_cb.event_cb =
      [this] (uint32_t track_id, EventType event_type,
              void *event_data, size_t event_data_size) -> void
      { VideoTrackEventCb(track_id,
      event_type, event_data, event_data_size); };

  ret = recorder_.CreateVideoTrack(session_id, video_track_id,
                                    video_track_param, video_track_cb);
  ASSERT_TRUE(ret == NO_ERROR);

  std::vector<uint32_t> track_ids;
  track_ids.push_back(video_track_id);

  width  = 720;
  height = 480;
  uint32_t video_track480p_id = 2;
  if (dump_bitstream_.IsEnabled()) {
    StreamDumpInfo dumpinfo = { format_type, session_id, video_track480p_id,
                                width, height };
    ret = dump_bitstream_.SetUp(dumpinfo);
  }


  video_track_param.width       = width;
  video_track_param.height      = height;
  if (ubwc_stream_enable_) {
    video_track_param.low_power_mode = true;
  }

  video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
      std::vector<BufferDescriptor> buffers,
      std::vector<MetaData> meta_buffers) {
        VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
      };

  video_track_cb.event_cb =
      [this] (uint32_t track_id, EventType event_type,
              void *event_data, size_t event_data_size) -> void
      { VideoTrackEventCb(track_id,
      event_type, event_data, event_data_size); };

  ret = recorder_.CreateVideoTrack(session_id, video_track480p_id,
                                   video_track_param, video_track_cb);
  ASSERT_TRUE(ret == NO_ERROR);

  track_ids.push_back(video_track480p_id);
  sessions_.insert(std::make_pair(session_id, track_ids));

  ret = recorder_.StartSession(session_id);
  ASSERT_TRUE(ret == NO_ERROR);

  sleep(30);

  ret = recorder_.StopSession(session_id, false);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteVideoTrack(session_id, video_track_id);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteVideoTrack(session_id, video_track480p_id);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteSession(session_id);
  ASSERT_TRUE(ret == NO_ERROR);

   ClearSessions();

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  dump_bitstream_.CloseAll();
  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
          test_info_->test_case_name(), test_info_->name());
}

/*
* SessionWith27Kp60fps480p30fpsEncTrack: This test will test session with one 2.7K
* 60fps h264 track and one 480p 30 fps h264 track.
* Api test sequence:
*  - StartCamera
*  - CreateSession
*  - CreateVideoTrack
*  - StartVideoTrack
*  - StopSession
*  - DeleteVideoTrack
*  - DeleteSession
*  - StopCamera
*/
TEST_F(VideoGtest, SessionWith27Kp60fps480p30fpsEncTrack) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  VideoFormat format_type = VideoFormat::kAVC;
  uint32_t width  = 2704;
  uint32_t height = 1520;
  float fps = 60;

  ret = recorder_.StartCamera(camera_id_, fps);
  ASSERT_TRUE(ret == NO_ERROR);

  SessionCb session_status_cb = CreateSessionStatusCb();
  uint32_t session_id;
  ret = recorder_.CreateSession(session_status_cb, &session_id);
  ASSERT_TRUE(session_id > 0);
  ASSERT_TRUE(ret == NO_ERROR);
  VideoTrackCreateParam video_track_param{camera_id_, format_type,
                                          width, height, fps};
  uint32_t video_track_id = 1;
  if (dump_bitstream_.IsEnabled()) {
    StreamDumpInfo dumpinfo = { format_type, session_id, video_track_id,
                                width, height };
    ret = dump_bitstream_.SetUp(dumpinfo);
    ASSERT_TRUE(ret == NO_ERROR);
  }

  TrackCb video_track_cb;
  video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
      std::vector<BufferDescriptor> buffers,
      std::vector<MetaData> meta_buffers) {
        VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
      };

  video_track_cb.event_cb =
      [this] (uint32_t track_id, EventType event_type,
              void *event_data, size_t event_data_size) -> void
      { VideoTrackEventCb(track_id,
      event_type, event_data, event_data_size); };

  ret = recorder_.CreateVideoTrack(session_id, video_track_id,
                                    video_track_param, video_track_cb);
  ASSERT_TRUE(ret == NO_ERROR);

  std::vector<uint32_t> track_ids;
  track_ids.push_back(video_track_id);

  width  = 720;
  height = 480;
  fps = 30;
  uint32_t video_track480p_id = 2;

  if (dump_bitstream_.IsEnabled()) {
    StreamDumpInfo dumpinfo = { format_type, session_id, video_track480p_id,
                                width, height };
    ret = dump_bitstream_.SetUp(dumpinfo);
  }

  video_track_param.width       = width;
  video_track_param.height      = height;
  video_track_param.frame_rate  = fps;
  if (ubwc_stream_enable_) {
    video_track_param.low_power_mode = true;
  }

  video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
      std::vector<BufferDescriptor> buffers,
      std::vector<MetaData> meta_buffers) {
        VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
      };

  video_track_cb.event_cb =
      [this] (uint32_t track_id, EventType event_type,
              void *event_data, size_t event_data_size) -> void
      { VideoTrackEventCb(track_id,
      event_type, event_data, event_data_size); };

  ret = recorder_.CreateVideoTrack(session_id, video_track480p_id,
                                   video_track_param, video_track_cb);
  ASSERT_TRUE(ret == NO_ERROR);

  track_ids.push_back(video_track480p_id);
  sessions_.insert(std::make_pair(session_id, track_ids));

  ret = recorder_.StartSession(session_id);
  ASSERT_TRUE(ret == NO_ERROR);

  sleep(30);

  ret = recorder_.StopSession(session_id, false);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteVideoTrack(session_id, video_track_id);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteVideoTrack(session_id, video_track480p_id);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteSession(session_id);
  ASSERT_TRUE(ret == NO_ERROR);

   ClearSessions();

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  dump_bitstream_.CloseAll();
  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
          test_info_->test_case_name(), test_info_->name());
}

/*
* SessionWith27Kp30fps480p30fpsEncTrack: This test will test session with one 2.7K
* 30fps h264 track and one 480p 30 fps h264 track.
* Api test sequence:
*  - StartCamera
*  - CreateSession
*  - CreateVideoTrack
*  - StartVideoTrack
*  - StopSession
*  - DeleteVideoTrack
*  - DeleteSession
*  - StopCamera
*/
TEST_F(VideoGtest, SessionWith27Kp30fps480p30fpsEncTrack) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  VideoFormat format_type = VideoFormat::kAVC;
  uint32_t width  = 2704;
  uint32_t height = 1520;
  float fps = 30;

  ret = recorder_.StartCamera(camera_id_, fps);
  ASSERT_TRUE(ret == NO_ERROR);

  SessionCb session_status_cb = CreateSessionStatusCb();
  uint32_t session_id;
  ret = recorder_.CreateSession(session_status_cb, &session_id);
  ASSERT_TRUE(session_id > 0);
  ASSERT_TRUE(ret == NO_ERROR);
  VideoTrackCreateParam video_track_param{camera_id_, format_type, width,
                                          height, fps};
  uint32_t video_track_id = 1;
  if (dump_bitstream_.IsEnabled()) {
    StreamDumpInfo dumpinfo = { format_type, session_id, video_track_id,
                                width, height };
    ret = dump_bitstream_.SetUp(dumpinfo);
    ASSERT_TRUE(ret == NO_ERROR);
  }

  TrackCb video_track_cb;
  video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
      std::vector<BufferDescriptor> buffers,
      std::vector<MetaData> meta_buffers) {
        VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
      };

  video_track_cb.event_cb =
      [this] (uint32_t track_id, EventType event_type,
              void *event_data, size_t event_data_size) -> void
      { VideoTrackEventCb(track_id,
      event_type, event_data, event_data_size); };

  ret = recorder_.CreateVideoTrack(session_id, video_track_id,
                                    video_track_param, video_track_cb);
  ASSERT_TRUE(ret == NO_ERROR);

  std::vector<uint32_t> track_ids;
  track_ids.push_back(video_track_id);

  width  = 720;
  height = 480;
  uint32_t video_track480p_id = 2;

  if (dump_bitstream_.IsEnabled()) {
    StreamDumpInfo dumpinfo = { format_type, session_id, video_track480p_id,
                                width, height };
    ret = dump_bitstream_.SetUp(dumpinfo);
  }

  video_track_param.width       = width;
  video_track_param.height      = height;
  if (ubwc_stream_enable_) {
    video_track_param.low_power_mode = true;
  }
  video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
      std::vector<BufferDescriptor> buffers,
      std::vector<MetaData> meta_buffers) {
        VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
      };

  video_track_cb.event_cb =
      [this] (uint32_t track_id, EventType event_type,
              void *event_data, size_t event_data_size) -> void
      { VideoTrackEventCb(track_id,
      event_type, event_data, event_data_size); };

  ret = recorder_.CreateVideoTrack(session_id, video_track480p_id,
                                   video_track_param, video_track_cb);
  ASSERT_TRUE(ret == NO_ERROR);

  track_ids.push_back(video_track480p_id);
  sessions_.insert(std::make_pair(session_id, track_ids));

  ret = recorder_.StartSession(session_id);
  ASSERT_TRUE(ret == NO_ERROR);

  sleep(30);

  ret = recorder_.StopSession(session_id, false);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteVideoTrack(session_id, video_track_id);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteVideoTrack(session_id, video_track480p_id);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteSession(session_id);
  ASSERT_TRUE(ret == NO_ERROR);

  ClearSessions();

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  dump_bitstream_.CloseAll();
  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
          test_info_->test_case_name(), test_info_->name());
}

/*
* SessionWith1080p90fps480p30fpsEncTrack: This test will test session
* with one 1080p 90fps h264 track and one 480p 30fps h264 track.
* Api test sequence:
*  - StartCamera
*  - CreateSession
*  - CreateVideoTrack
*  - StartSession
*  - StopSession
*  - DeleteVideoTrack
*  - DeleteSession
*  - StopCamera
*/
TEST_F(VideoGtest, SessionWith1080p90fps480p30fpsEncTrack) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  VideoFormat format_type = VideoFormat::kAVC;
  uint32_t width  = 1920;
  uint32_t height = 1080;
  float fps = 90;

  ret = recorder_.StartCamera(camera_id_, fps);
  ASSERT_TRUE(ret == NO_ERROR);

  SessionCb session_status_cb = CreateSessionStatusCb();
  uint32_t session_id;
  ret = recorder_.CreateSession(session_status_cb, &session_id);
  ASSERT_TRUE(session_id > 0);
  ASSERT_TRUE(ret == NO_ERROR);

  VideoTrackCreateParam video_track_param{ camera_id_, format_type,
                                           width, height, fps };
  uint32_t video_track_id = 1;
  if (dump_bitstream_.IsEnabled()) {
    StreamDumpInfo dumpinfo = { format_type, session_id, video_track_id,
                                width, height };
    ret = dump_bitstream_.SetUp(dumpinfo);
    ASSERT_TRUE(ret == NO_ERROR);
  }

  TrackCb video_track_cb;
  video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
      std::vector<BufferDescriptor> buffers,
      std::vector<MetaData> meta_buffers) {
        VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
      };

  video_track_cb.event_cb =
      [this] (uint32_t track_id, EventType event_type,
              void *event_data, size_t event_data_size) -> void
      { VideoTrackEventCb(track_id,
      event_type, event_data, event_data_size); };

  ret = recorder_.CreateVideoTrack(session_id, video_track_id,
                                    video_track_param, video_track_cb);
  ASSERT_TRUE(ret == NO_ERROR);

  std::vector<uint32_t> track_ids;
  track_ids.push_back(video_track_id);

  width  = 720;
  height = 480;
  fps = 30;
  uint32_t video_track480p_id = 2;

  if (dump_bitstream_.IsEnabled()) {
    StreamDumpInfo dumpinfo = { format_type, session_id, video_track480p_id,
                                width, height };
    ret = dump_bitstream_.SetUp(dumpinfo);
  }

  video_track_param.width       = width;
  video_track_param.height      = height;
  video_track_param.frame_rate  = fps;

  video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
      std::vector<BufferDescriptor> buffers,
      std::vector<MetaData> meta_buffers) {
        VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
      };

  video_track_cb.event_cb =
      [this] (uint32_t track_id, EventType event_type,
              void *event_data, size_t event_data_size) -> void
      { VideoTrackEventCb(track_id,
      event_type, event_data, event_data_size); };

  ret = recorder_.CreateVideoTrack(session_id, video_track480p_id,
                                   video_track_param, video_track_cb);
  ASSERT_TRUE(ret == NO_ERROR);

  track_ids.push_back(video_track480p_id);
  sessions_.insert(std::make_pair(session_id, track_ids));

  ret = recorder_.StartSession(session_id);
  ASSERT_TRUE(ret == NO_ERROR);

  //Record for some time
  sleep(30);

  ret = recorder_.StopSession(session_id, false);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteVideoTrack(session_id, video_track_id);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteVideoTrack(session_id, video_track480p_id);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteSession(session_id);
  ASSERT_TRUE(ret == NO_ERROR);

  ClearSessions();

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  dump_bitstream_.CloseAll();
  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
          test_info_->test_case_name(), test_info_->name());
}

/*
* SessionWith480pEncTrack: This test will test session with one 480p
* 30fps h264 track.
* Api test sequence:
*  - StartCamera
*  - CreateSession
*  - CreateVideoTrack
*  - StartVideoTrack
*  - StopSession
*  - DeleteVideoTrack
*  - DeleteSession
*  - StopCamera
*/
TEST_F(VideoGtest, SessionWith480pEncTrack) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  VideoFormat format_type = VideoFormat::kAVC;
  uint32_t width  = 720;
  uint32_t height = 480;
  float fps = 30;

  ret = recorder_.StartCamera(camera_id_, fps);
  ASSERT_TRUE(ret == NO_ERROR);

  SessionCb session_status_cb = CreateSessionStatusCb();
  uint32_t session_id;
  ret = recorder_.CreateSession(session_status_cb, &session_id);
  ASSERT_TRUE(session_id > 0);
  ASSERT_TRUE(ret == NO_ERROR);
  VideoTrackCreateParam video_track_param{camera_id_, format_type,
                                          width, height, fps};
  uint32_t video_track_id = 1;
  if (ubwc_stream_enable_) {
    video_track_param.low_power_mode = true;
  }
  if (dump_bitstream_.IsEnabled()) {
    StreamDumpInfo dumpinfo = { format_type, session_id, video_track_id,
                                width, height };
    ret = dump_bitstream_.SetUp(dumpinfo);
    ASSERT_TRUE(ret == NO_ERROR);
  }

  TrackCb video_track_cb;
  video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
      std::vector<BufferDescriptor> buffers,
      std::vector<MetaData> meta_buffers) {
        VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
      };

  video_track_cb.event_cb =
      [this] (uint32_t track_id, EventType event_type,
              void *event_data, size_t event_data_size) -> void
      { VideoTrackEventCb(track_id,
      event_type, event_data, event_data_size); };

  ret = recorder_.CreateVideoTrack(session_id, video_track_id,
                                    video_track_param, video_track_cb);
  ASSERT_TRUE(ret == NO_ERROR);

  std::vector<uint32_t> track_ids;
  track_ids.push_back(video_track_id);
  sessions_.insert(std::make_pair(session_id, track_ids));

  ret = recorder_.StartSession(session_id);
  ASSERT_TRUE(ret == NO_ERROR);

  // Let session run for record_duration_, during this time buffer with valid
  // data would be received in track callback (VideoTrackDataCb).
  sleep(record_duration_);

  ret = recorder_.StopSession(session_id, false);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteVideoTrack(session_id, video_track_id);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteSession(session_id);
  ASSERT_TRUE(ret == NO_ERROR);

   ClearSessions();

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  dump_bitstream_.CloseAll();
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
TEST_F(VideoGtest, SessionWith4KEncTrack) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  VideoFormat format_type = VideoFormat::kAVC;
  uint32_t width  = 3840;
  uint32_t height = 2160;

  ret = recorder_.StartCamera(camera_id_, 30);
  ASSERT_TRUE(ret == NO_ERROR);

  for(uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
        test_info_->name(), i);

    SessionCb session_status_cb = CreateSessionStatusCb();
    uint32_t session_id;
    ret = recorder_.CreateSession(session_status_cb, &session_id);
    ASSERT_TRUE(session_id > 0);
    ASSERT_TRUE(ret == NO_ERROR);

    VideoTrackCreateParam video_track_param{camera_id_, format_type,
                                            width, height, 30};
    uint32_t video_track_id = 1;

    if (dump_bitstream_.IsEnabled()) {
      StreamDumpInfo dumpinfo = { format_type, session_id,
                                  video_track_id, width, height };
      ret = dump_bitstream_.SetUp(dumpinfo);
      ASSERT_TRUE(ret == NO_ERROR);
    }

  TrackCb video_track_cb;
  video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
      std::vector<BufferDescriptor> buffers,
      std::vector<MetaData> meta_buffers) {
        VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
      };

    video_track_cb.event_cb =
        [this] (uint32_t track_id, EventType event_type,
                void *event_data, size_t event_data_size) -> void
        { VideoTrackEventCb(track_id,
        event_type, event_data, event_data_size); };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id,
                                      video_track_param, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    std::vector<uint32_t> track_ids;
    track_ids.push_back(video_track_id);
    sessions_.insert(std::make_pair(session_id, track_ids));

    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    // Let session run for record_duration_, during this time buffer with valid
    // data would be received in track callback (VideoTrackDataCb).
    sleep(record_duration_);

    ret = recorder_.StopSession(session_id, false);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    ClearSessions();
    dump_bitstream_.CloseAll();
  }
  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());
}

/*
* SessionWith4KEncTrackWithAecWaitMode: This test will test session with one
* 4K h264 track in AEC wait mode. In this mode, skip the camera frames until
* the AEC is Converged otherwise the frames are over/under Exposed.
* API test sequence:
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
TEST_F(VideoGtest, SessionWith4KEncTrackWithAecWaitMode) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  VideoFormat format_type = VideoFormat::kAVC;
  uint32_t width  = 3840;
  uint32_t height = 2160;

  ret = recorder_.StartCamera(camera_id_, 30);
  ASSERT_TRUE(ret == NO_ERROR);

  for(uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
        test_info_->name(), i);

    SessionCb session_status_cb = CreateSessionStatusCb();
    uint32_t session_id;
    ret = recorder_.CreateSession(session_status_cb, &session_id);
    ASSERT_TRUE(session_id > 0);
    ASSERT_TRUE(ret == NO_ERROR);

    VideoTrackCreateParam video_track_param{camera_id_, format_type,
                                            width, height, 30};
    uint32_t video_track_id = 1;

    if (dump_bitstream_.IsEnabled()) {
      StreamDumpInfo dumpinfo = { format_type, session_id,
                                  video_track_id, width, height };
      ret = dump_bitstream_.SetUp(dumpinfo);
      ASSERT_TRUE(ret == NO_ERROR);
    }

    VideoExtraParam extra_param;
    VideoWaitAECMode wait_aec;
    wait_aec.enable = true;
    extra_param.Update(QMMF_VIDEO_WAIT_AEC_MODE, wait_aec);

    TrackCb video_track_cb;
    video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
      std::vector<BufferDescriptor> buffers,
      std::vector<MetaData> meta_buffers) {
        VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
      };

    video_track_cb.event_cb =
        [this] (uint32_t track_id, EventType event_type,
                void *event_data, size_t event_data_size) -> void
        { VideoTrackEventCb(track_id,
        event_type, event_data, event_data_size); };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id,
                                      video_track_param, extra_param,
                                      video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    std::vector<uint32_t> track_ids;
    track_ids.push_back(video_track_id);
    sessions_.insert(std::make_pair(session_id, track_ids));

    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    // Let session run for record_duration_, during this time buffer with valid
    // data would be received in track callback (VideoTrackDataCb).
    sleep(record_duration_);

    ret = recorder_.StopSession(session_id, false);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    ClearSessions();
    dump_bitstream_.CloseAll();
  }

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());
}

/*
* SessionWith4kPrivacyMaskEncTrack: This test will test session with
*        4k h264 track and PrivacyMask.
*
* Api test sequence:
*  - StartCamera
*   loop Start {
*   ------------------
*   - CreateSession
*   - ConfigPlugin
*   - CreateVideoTrack
*   - StartVideoTrack
*   - StopSession
*   - DeleteVideoTrack
*   - DeletePlugin
*   - DeleteSession
*   ------------------
*   } loop End
*  - StopCamera
*/
TEST_F(VideoGtest, SessionWith4kPrivacyMaskEncTrack) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  VideoFormat format_type = VideoFormat::kAVC;
  uint32_t width  = 3840;
  uint32_t height = 2160;

  ret = recorder_.StartCamera(camera_id_, 30);
  ASSERT_TRUE(ret == NO_ERROR);

  for(uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
        test_info_->name(), i);

    SessionCb session_status_cb;
    session_status_cb.event_cb = [this] (EventType event_type, void *event_data,
                                         size_t event_data_size) -> void {
        SessionCallbackHandler(event_type, event_data, event_data_size);
    };

    uint32_t session_id;
    ret = recorder_.CreateSession(session_status_cb, &session_id);
    ASSERT_TRUE(session_id > 0);
    ASSERT_TRUE(ret == NO_ERROR);

    VideoTrackCreateParam video_track_param{camera_id_, format_type,
                                            width,
                                            height,
                                            30};
    uint32_t video_track_id = 1;

    if (dump_bitstream_.IsEnabled()) {
      StreamDumpInfo dumpinfo = {
        format_type,
        session_id,
        video_track_id,
        width,
        height };
      ret = dump_bitstream_.SetUp(dumpinfo);
      ASSERT_TRUE(ret == NO_ERROR);
    }

    TrackCb video_track_cb;
    video_track_cb.data_cb =
      [&, session_id] (uint32_t track_id, std::vector<BufferDescriptor> buffers,
                       std::vector<MetaData> meta_buffers) {
          VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
      };

    video_track_cb.event_cb = [&] (uint32_t track_id, EventType event_type,
                                   void *event_data, size_t event_data_size) {
        VideoTrackEventCb(track_id, event_type, event_data, event_data_size);
    };

    VideoExtraParam extra_param;
    PostprocPlugin pmr_plugin;

    SupportedPlugins supported_plugins;
    ret = recorder_.GetSupportedPlugins(&supported_plugins);
    ASSERT_TRUE(ret == NO_ERROR);

    bool found = false;
    for (auto const& plugin_info : supported_plugins) {
      if (plugin_info.name == "PrivacyMask") {
        ret = recorder_.CreatePlugin(&pmr_plugin.uid, plugin_info);
        ASSERT_TRUE(ret == NO_ERROR);

        std::string config =
        "{\"circles\": [         \
          {                      \
            \"radius\": 1320,    \
              \"centre\": {      \
                \"x\": 1344,     \
                \"y\": 760       \
              },                 \
            \"color\": {         \
              \"y\": 0,          \
              \"u\": 128,        \
              \"v\": 128         \
            }                    \
          }                      \
        ]                        \
        }";

       fprintf(stderr,"---------- Test ConfigPlugin %s----------\n",
           config.c_str());

       ret = recorder_.ConfigPlugin(pmr_plugin.uid, config);
       ASSERT_TRUE(ret == NO_ERROR);

        extra_param.Update(QMMF_POSTPROCESS_PLUGIN, pmr_plugin);
        found = true;
      }
    }
    ASSERT_TRUE(found == true);

    ret = recorder_.CreateVideoTrack(session_id, video_track_id,
                                     video_track_param, extra_param,
                                     video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    std::vector<uint32_t> track_ids;
    track_ids.push_back(video_track_id);
    sessions_.insert(std::make_pair(session_id, track_ids));

    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    // Let session run for record_duration_, during this time buffer with valid
    // data would be received in track callback (VideoTrackDataCb).
    sleep(record_duration_);

    ret = recorder_.StopSession(session_id, false);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeletePlugin(pmr_plugin.uid);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    dump_bitstream_.CloseAll();
  }

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());

}

/*
* SessionWith4KSwTnrEncTrack: This test will test session with
*        1080p h264 track and post processing. Post processing pipe is
*        Sw Tnr.
*
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
TEST_F(VideoGtest, SessionWith4KSwTnrEncTrack) {
  fprintf(stderr, "\n---------- Run Test %s.%s ------------\n",
          test_info_->test_case_name(), test_info_->name());

  auto ret = Init();
  assert(ret == NO_ERROR);

  VideoFormat format_type = VideoFormat::kAVC;
  uint32_t width = 3840;
  uint32_t height = 2160;

  ret = recorder_.StartCamera(camera_id_, 30);
  assert(ret == NO_ERROR);

  for (uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr, "test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
              test_info_->name(), i);

    SessionCb session_status_cb;
    session_status_cb.event_cb = [this](EventType event_type, void *event_data,
                                        size_t event_data_size) -> void {
      SessionCallbackHandler(event_type, event_data, event_data_size);
    };

    uint32_t session_id;
    ret = recorder_.CreateSession(session_status_cb, &session_id);
    assert(session_id > 0);
    assert(ret == NO_ERROR);
    VideoTrackCreateParam video_track_param{camera_id_, format_type, width,
                                            height, 30};
    uint32_t video_track_id = 1;

    if (dump_bitstream_.IsEnabled()) {
      StreamDumpInfo dumpinfo = { format_type, session_id, video_track_id,
                                  width, height };
      ret = dump_bitstream_.SetUp(dumpinfo);
      assert(ret == NO_ERROR);
    }

    TrackCb video_track_cb;
    video_track_cb.data_cb = [&, session_id](
        uint32_t track_id, std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
      VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
    };

    video_track_cb.event_cb = [&](uint32_t track_id, EventType event_type,
                                  void *event_data, size_t event_data_size) {
      VideoTrackEventCb(track_id, event_type, event_data, event_data_size);
    };

    VideoExtraParam extra_param;
    PostprocPlugin sw_tnr_plugin;

    SupportedPlugins supported_plugins;
    ret = recorder_.GetSupportedPlugins(&supported_plugins);
    assert(ret == NO_ERROR);

    for (auto const &plugin_info : supported_plugins) {
      if (plugin_info.name == "SwTnr") {
        ret = recorder_.CreatePlugin(&sw_tnr_plugin.uid, plugin_info);
        assert(ret == NO_ERROR);

        extra_param.Update(QMMF_POSTPROCESS_PLUGIN, sw_tnr_plugin);
      }
    }

    ret = recorder_.CreateVideoTrack(session_id, video_track_id,
                                     video_track_param, extra_param,
                                     video_track_cb);
    assert(ret == NO_ERROR);

    std::vector<uint32_t> track_ids;
    track_ids.push_back(video_track_id);
    sessions_.insert(std::make_pair(session_id, track_ids));

    ret = recorder_.StartSession(session_id);
    assert(ret == NO_ERROR);

    // Let session run for record_duration_, during this time buffer with valid
    // data would be received in track callback (VideoTrackDataCb).
    sleep(record_duration_);

    ret = recorder_.StopSession(session_id, false);
    assert(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id);
    assert(ret == NO_ERROR);

    ret = recorder_.DeletePlugin(sw_tnr_plugin.uid);
    assert(ret == NO_ERROR);

    ret = recorder_.DeleteSession(session_id);
    assert(ret == NO_ERROR);

    dump_bitstream_.CloseAll();
  }

  ret = recorder_.StopCamera(camera_id_);
  assert(ret == NO_ERROR);

  ret = DeInit();
  assert(ret == NO_ERROR);

  fprintf(stderr, "---------- Test Completed %s.%s ----------\n",
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
TEST_F(VideoGtest, SessionWithTwo1080pEncTracks) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  VideoFormat format_type = VideoFormat::kAVC;
  uint32_t width  = 1920;
  uint32_t height = 1080;
  uint32_t video_track_id1 = 1;
  uint32_t video_track_id2 = 2;

  ret = recorder_.StartCamera(camera_id_, 30);
  ASSERT_TRUE(ret == NO_ERROR);

  SessionCb session_status_cb = CreateSessionStatusCb();
  uint32_t session_id;
  ret = recorder_.CreateSession(session_status_cb, &session_id);
  ASSERT_TRUE(session_id > 0);
  ASSERT_TRUE(ret == NO_ERROR);
  VideoTrackCreateParam video_track_param{camera_id_, format_type,
                                          width, height, 30};

  TrackCb video_track_cb;
  video_track_cb.event_cb =
      [this] (uint32_t track_id, EventType event_type,
              void *event_data, size_t event_data_size) -> void {
      VideoTrackEventCb(track_id, event_type, event_data, event_data_size); };

  std::vector<uint32_t> track_ids;
  for(uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
        test_info_->name(), i);

    TrackCb video_track_cb;
    video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
        std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
          VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
        };

    // Create 1080p encode track.
    ret = recorder_.CreateVideoTrack(session_id, video_track_id1,
                                     video_track_param, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);
    if (dump_bitstream_.IsEnabled()) {
      StreamDumpInfo dumpinfo = { format_type, session_id, video_track_id1,
                                  width, height };
      ret = dump_bitstream_.SetUp(dumpinfo);
      ASSERT_TRUE(ret == NO_ERROR);
    }
    track_ids.push_back(video_track_id1);

  video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
      std::vector<BufferDescriptor> buffers,
      std::vector<MetaData> meta_buffers) {
        VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
      };

    // Create another 1080p encode track.
    ret = recorder_.CreateVideoTrack(session_id, video_track_id2,
                                     video_track_param, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);
    if (dump_bitstream_.IsEnabled()) {
      StreamDumpInfo dumpinfo = { format_type, session_id, video_track_id2,
                                  width, height };
      ret = dump_bitstream_.SetUp(dumpinfo);
    }

    track_ids.push_back(video_track_id2);

    sessions_.insert(std::make_pair(session_id, track_ids));

    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    // Let session run for record_duration_, during this time buffer with valid
    // data would be received in track callback (VideoTrackDataCb).
    sleep(record_duration_);

    ret = recorder_.StopSession(session_id, false);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id1);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id2);
    ASSERT_TRUE(ret == NO_ERROR);

    dump_bitstream_.CloseAll();
  }
  ret = recorder_.DeleteSession(session_id);
  ASSERT_TRUE(ret == NO_ERROR);

  ClearSessions();

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

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
TEST_F(VideoGtest, SessionWith4KAnd1080pYUVTrack) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.StartCamera(camera_id_, 30);
  ASSERT_TRUE(ret == NO_ERROR);

  SessionCb session_status_cb = CreateSessionStatusCb();
  uint32_t session_id;
  ret = recorder_.CreateSession(session_status_cb, &session_id);
  ASSERT_TRUE(session_id > 0);
  ASSERT_TRUE(ret == NO_ERROR);
  uint32_t track1_id = 1;
  uint32_t track2_id = 2;

  std::vector<uint32_t> track_ids;
  for(uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
        test_info_->name(), i);
    VideoTrackCreateParam video_track_param{camera_id_, VideoFormat::kYUV,
                                            3840, 2160, 30};

    if (ubwc_stream_enable_) {
      video_track_param.low_power_mode = true;
    }

    TrackCb video_track_cb;
    video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
                                std::vector<BufferDescriptor> buffers,
                                std::vector<MetaData> meta_buffers) {
      VideoTrackYUVDataCb(session_id, track_id, buffers, meta_buffers); };

    video_track_cb.event_cb =
        [this] (uint32_t track_id, EventType event_type,
                void *event_data, size_t event_data_size) -> void {
        VideoTrackEventCb(track_id, event_type, event_data, event_data_size); };

    ret = recorder_.CreateVideoTrack(session_id, track1_id,
                                      video_track_param, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);
    track_ids.push_back(track1_id);

    video_track_param.width  = 1920;
    video_track_param.height = 1080;
    ret = recorder_.CreateVideoTrack(session_id, track2_id,
                                      video_track_param, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    track_ids.push_back(track2_id);

    sessions_.insert(std::make_pair(session_id, track_ids));

    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    // Let session run for record_duration_, during this time buffer with valid
    // data would be received in track callback (VideoTrackYUVDataCb).
    sleep(record_duration_);

    ret = recorder_.StopSession(session_id, false);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, track1_id);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, track2_id);
    ASSERT_TRUE(ret == NO_ERROR);
  }

  ret = recorder_.DeleteSession(session_id);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ClearSessions();

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());
}

/*
* 1080pEncWithStaticImageOverlay: This test will apply static image overlay
*                                 ontop of 1080 video.
* Api test sequence:
*  - StartCamera
*   - CreateSession
*   - CreateVideoTrack
*   - StartVideoTrack
*   - CreateOverlayObject
*   - SetOverlay
*   loop Start {
*   ------------------
*    - GetOverlayObjectParams
*    - UpdateOverlayObjectParams
*   ------------------
*   } loop End
*   - RemoveOverlay
*   - DeleteOverlayObject
*   - StopSession
*   - DeleteVideoTrack
*   - DeleteSession
*   - StopCamera
*/
TEST_F(VideoGtest, 1080pEncWithStaticImageOverlay) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  VideoFormat format_type = VideoFormat::kAVC;
  uint32_t width  = 1920;
  uint32_t height = 1080;

  ret = recorder_.StartCamera(camera_id_, 30);
  ASSERT_TRUE(ret == NO_ERROR);

  SessionCb session_status_cb = CreateSessionStatusCb();
  uint32_t session_id;
  ret = recorder_.CreateSession(session_status_cb, &session_id);
  ASSERT_TRUE(session_id > 0);
  ASSERT_TRUE(ret == NO_ERROR);
  VideoTrackCreateParam video_track_param{ camera_id_, format_type,
                                           width, height, 30 };
  uint32_t video_track_id = 1;
  if (dump_bitstream_.IsEnabled()) {
    StreamDumpInfo dumpinfo = { format_type, session_id, video_track_id,
                                width, height };
    ret = dump_bitstream_.SetUp(dumpinfo);
    ASSERT_TRUE(ret == NO_ERROR);
  }

  TrackCb video_track_cb;
  video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
      std::vector<BufferDescriptor> buffers,
      std::vector<MetaData> meta_buffers) {
        VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
      };

  video_track_cb.event_cb =
      [this] (uint32_t track_id, EventType event_type,
              void *event_data, size_t event_data_size) -> void
      { VideoTrackEventCb(track_id,
      event_type, event_data, event_data_size); };

  ret = recorder_.CreateVideoTrack(session_id, video_track_id,
                                    video_track_param, video_track_cb);
  ASSERT_TRUE(ret == NO_ERROR);

  std::vector<uint32_t> track_ids;
  track_ids.push_back(video_track_id);
  sessions_.insert(std::make_pair(session_id, track_ids));

  ret = recorder_.StartSession(session_id);
  ASSERT_TRUE(ret == NO_ERROR);

  // Create Static Image type overlay.
  OverlayParam object_params{};
  uint32_t static_img_id;
  object_params.type = OverlayType::kStaticImage;
  object_params.location = OverlayLocationType::kBottomRight;
  std::string str(OVERLAY_TEST_FILE);
  str.copy(object_params.image_info.image_location, str.length());
  object_params.dst_rect.width  = 451;
  object_params.dst_rect.height = 109;
  ret = recorder_.CreateOverlayObject(video_track_id, object_params,
                                      &static_img_id);
  ASSERT_TRUE(ret == 0);
  // Apply overlay object on video track.
  ret = recorder_.SetOverlay(video_track_id, static_img_id);
  ASSERT_TRUE(ret == 0);
  for(uint32_t i = 1, location = 0; i <= iteration_count_; ++i, ++location) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
      test_info_->name(), i);

    object_params = {};
    ret = recorder_.GetOverlayObjectParams(video_track_id, static_img_id,
                                           object_params);
    ASSERT_TRUE(ret == 0);

    if (location == 0) {
      object_params.location = OverlayLocationType::kTopLeft;
    } else if (location == 1) {
      object_params.location = OverlayLocationType::kTopRight;
    } else if (location == 2) {
      object_params.location = OverlayLocationType::kCenter;
    } else if (location == 3) {
      object_params.location = OverlayLocationType::kBottomLeft;
    } else if (location == 4) {
      object_params.location = OverlayLocationType::kBottomRight;
    } else {
      location = -1;
    }

    ret = recorder_.UpdateOverlayObjectParams(video_track_id, static_img_id,
                                              object_params);
    ASSERT_TRUE(ret == 0);
    // Record video with overlay.
    sleep(5);
  }
  // Remove overlay object from video track.
  ret = recorder_.RemoveOverlay(video_track_id, static_img_id);
  ASSERT_TRUE(ret == 0);

  // Delete overlay object.
  ret = recorder_.DeleteOverlayObject(video_track_id, static_img_id);
  ASSERT_TRUE(ret == 0);

  ret = recorder_.StopSession(session_id, false);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteVideoTrack(session_id, video_track_id);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteSession(session_id);
  ASSERT_TRUE(ret == NO_ERROR);

  ClearSessions();

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  dump_bitstream_.CloseAll();
  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());
}

/*
* 1080pEncWithDateAndTimeOverlay: This test applies date and time overlay type
*                                 ontop of 1080 video.
* Api test sequence:
*  - StartCamera
*   - CreateSession
*   - CreateVideoTrack
*   - StartVideoTrack
*   - CreateOverlayObject
*   - SetOverlay
*   loop Start {
*   ------------------
*    - GetOverlayObjectParams
*    - UpdateOverlayObjectParams
*   ------------------
*   } loop End
*   - RemoveOverlay
*   - DeleteOverlayObject
*   - StopSession
*   - DeleteVideoTrack
*   - DeleteSession
*   - StopCamera
*/
TEST_F(VideoGtest, 1080pEncWithDateAndTimeOverlay) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  VideoFormat format_type = VideoFormat::kAVC;
  uint32_t width  = 1920;
  uint32_t height = 1080;

  ret = recorder_.StartCamera(camera_id_, 30);
  ASSERT_TRUE(ret == NO_ERROR);

  SessionCb session_status_cb = CreateSessionStatusCb();
  uint32_t session_id;
  ret = recorder_.CreateSession(session_status_cb, &session_id);
  ASSERT_TRUE(session_id > 0);
  ASSERT_TRUE(ret == NO_ERROR);
  VideoTrackCreateParam video_track_param{camera_id_, format_type,
                                          width, height, 30};
  uint32_t video_track_id = 1;
  if (dump_bitstream_.IsEnabled()) {
    StreamDumpInfo dumpinfo = { format_type, session_id, video_track_id,
                                width, height };
    ret = dump_bitstream_.SetUp(dumpinfo);
    ASSERT_TRUE(ret == NO_ERROR);
  }

  TrackCb video_track_cb;
  video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
      std::vector<BufferDescriptor> buffers,
      std::vector<MetaData> meta_buffers) {
        VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
      };

  video_track_cb.event_cb =
      [this] (uint32_t track_id, EventType event_type,
              void *event_data, size_t event_data_size) -> void
      { VideoTrackEventCb(track_id,
      event_type, event_data, event_data_size); };

  ret = recorder_.CreateVideoTrack(session_id, video_track_id,
                                    video_track_param, video_track_cb);
  ASSERT_TRUE(ret == NO_ERROR);

  std::vector<uint32_t> track_ids;
  track_ids.push_back(video_track_id);
  sessions_.insert(std::make_pair(session_id, track_ids));

  ret = recorder_.StartSession(session_id);
  ASSERT_TRUE(ret == NO_ERROR);

  // Create Date & Time type overlay.
  OverlayParam object_params{};
  object_params.type = OverlayType::kDateType;
  object_params.location = OverlayLocationType::kBottomLeft;
  object_params.color    = kColorDarkGray;
  object_params.date_time.time_format = OverlayTimeFormatType::kHHMMSS_AMPM;
  object_params.date_time.date_format = OverlayDateFormatType::kMMDDYYYY;

  uint32_t date_time_id;
  ret = recorder_.CreateOverlayObject(video_track_id, object_params,
                                       &date_time_id);
  ASSERT_TRUE(ret == 0);
  // One track can have multiple types of overlay.
  ret = recorder_.SetOverlay(video_track_id, date_time_id);
  ASSERT_TRUE(ret == 0);
  for(uint32_t i = 1, location = 0; i <= iteration_count_; ++i, ++location) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
      test_info_->name(), i);

    object_params = {};
    ret = recorder_.GetOverlayObjectParams(video_track_id, date_time_id,
                                           object_params);
    ASSERT_TRUE(ret == 0);

    // Update different types of Time & Date formats along with text color
    // and location on video.
    if (location == 0) {
      object_params.location = OverlayLocationType::kTopLeft;
      object_params.date_time.time_format = OverlayTimeFormatType::kHHMMSS_AMPM;
      object_params.date_time.date_format = OverlayDateFormatType::kMMDDYYYY;
      object_params.color    = kColorDarkGray;
    } else if (location == 1) {
      object_params.location = OverlayLocationType::kTopRight;
      object_params.date_time.time_format = OverlayTimeFormatType::kHHMMSS_24HR;
      object_params.date_time.date_format = OverlayDateFormatType::kMMDDYYYY;
      object_params.color    = kColorYellow;
    } else if (location == 2) {
      object_params.location = OverlayLocationType::kCenter;
      object_params.date_time.time_format = OverlayTimeFormatType::kHHMM_24HR;
      object_params.date_time.date_format = OverlayDateFormatType::kYYYYMMDD;
      object_params.color    = kColorBlue;
    } else if (location == 3) {
      object_params.location = OverlayLocationType::kBottomLeft;
      object_params.date_time.time_format = OverlayTimeFormatType::kHHMM_AMPM;
      object_params.date_time.date_format = OverlayDateFormatType::kYYYYMMDD;
      object_params.color    = kColorWhilte;
    } else if (location == 4) {
      object_params.location = OverlayLocationType::kBottomRight;
      object_params.date_time.time_format = OverlayTimeFormatType::kHHMMSS_AMPM;
      object_params.date_time.date_format = OverlayDateFormatType::kYYYYMMDD;
      object_params.color    = kColorOrange;
    } else {
      location = -1;
    }

    ret = recorder_.UpdateOverlayObjectParams(video_track_id, date_time_id,
                                              object_params);
    ASSERT_TRUE(ret == 0);
    // Record video with overlay.
    sleep(5);
  }

  ret = recorder_.RemoveOverlay(video_track_id, date_time_id);
  ASSERT_TRUE(ret == 0);

  // Delete overlay object.
  ret = recorder_.DeleteOverlayObject(video_track_id, date_time_id);
  ASSERT_TRUE(ret == 0);

  ret = recorder_.StopSession(session_id, false);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteVideoTrack(session_id, video_track_id);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteSession(session_id);
  ASSERT_TRUE(ret == NO_ERROR);

  ClearSessions();

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  dump_bitstream_.CloseAll();
  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());
}

/*
* 1080pEncWithBoundingBoxOverlay: This test applies bounding box overlay type
*                                 ontop of 1080 video.
* Api test sequence:
*  - StartCamera
*   - CreateSession
*   - CreateVideoTrack
*   - StartVideoTrack
*   - CreateOverlayObject
*   - SetOverlay
*   loop Start {
*   ------------------
*    - GetOverlayObjectParams
*    - UpdateOverlayObjectParams
*   ------------------
*   } loop End
*   - RemoveOverlay
*   - DeleteOverlayObject
*   - StopSession
*   - DeleteVideoTrack
*   - DeleteSession
*   - StopCamera
*/
TEST_F(VideoGtest, 1080pEncWithBoundingBoxOverlay) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  VideoFormat format_type = VideoFormat::kAVC;
  uint32_t width  = 1920;
  uint32_t height = 1080;

  ret = recorder_.StartCamera(camera_id_, 30);
  ASSERT_TRUE(ret == NO_ERROR);

  SessionCb session_status_cb = CreateSessionStatusCb();
  uint32_t session_id;
  ret = recorder_.CreateSession(session_status_cb, &session_id);
  ASSERT_TRUE(session_id > 0);
  ASSERT_TRUE(ret == NO_ERROR);
  VideoTrackCreateParam video_track_param{camera_id_, format_type,
                                          width, height, 30};
  uint32_t video_track_id = 1;
  if (dump_bitstream_.IsEnabled()) {
    StreamDumpInfo dumpinfo = { format_type, session_id, video_track_id,
                                width, height };
    ret = dump_bitstream_.SetUp(dumpinfo);
    ASSERT_TRUE(ret == NO_ERROR);
  }

  TrackCb video_track_cb;
  video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
      std::vector<BufferDescriptor> buffers,
      std::vector<MetaData> meta_buffers) {
        VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
      };

  video_track_cb.event_cb =
      [this] (uint32_t track_id, EventType event_type,
              void *event_data, size_t event_data_size) -> void
      { VideoTrackEventCb(track_id,
      event_type, event_data, event_data_size); };

  ret = recorder_.CreateVideoTrack(session_id, video_track_id,
                                    video_track_param, video_track_cb);
  ASSERT_TRUE(ret == NO_ERROR);

  std::vector<uint32_t> track_ids;
  track_ids.push_back(video_track_id);
  sessions_.insert(std::make_pair(session_id, track_ids));

  ret = recorder_.StartSession(session_id);
  ASSERT_TRUE(ret == NO_ERROR);

  // Create BoundingBox type overlay.
  OverlayParam object_params{};
  object_params.type  = OverlayType::kBoundingBox;
  object_params.color = kColorLightGreen;
  // Dummy coordinates for test purpose.
  object_params.dst_rect.start_x = 20;
  object_params.dst_rect.start_y = 20;
  object_params.dst_rect.width   = 400;
  object_params.dst_rect.height  = 200;
  std::string bb_text("Test BBox..");
  bb_text.copy(object_params.bounding_box.box_name, bb_text.length());

  uint32_t bbox_id;
  ret = recorder_.CreateOverlayObject(video_track_id, object_params,
                                       &bbox_id);
  ASSERT_TRUE(ret == 0);
  ret = recorder_.SetOverlay(video_track_id, bbox_id);
  ASSERT_TRUE(ret == 0);

  //Mimic moving bounding box.
  for (uint32_t j = 0; j < 100; ++j) {
    ret = recorder_.GetOverlayObjectParams(video_track_id, bbox_id,
                                           object_params);
    ASSERT_TRUE(ret == 0);

    object_params.dst_rect.start_x = ((object_params.dst_rect.start_x + 5 +
        object_params.dst_rect.width) < static_cast<int32_t> (width)) ?
                                object_params.dst_rect.start_x + 5 : 20;

    object_params.dst_rect.width = ((object_params.dst_rect.start_x + 5 +
        object_params.dst_rect.width) < static_cast<int32_t> (width)) ?
                                object_params.dst_rect.width + 5 : 200;

    object_params.dst_rect.start_y = ((object_params.dst_rect.start_y + 2 +
        object_params.dst_rect.height) < static_cast<int32_t> (height)) ?
                                  object_params.dst_rect.start_y + 2 : 20;

    object_params.dst_rect.height = ((object_params.dst_rect.start_y + 2 +
        object_params.dst_rect.height) < static_cast<int32_t> (height)) ?
                                  object_params.dst_rect.height + 2 : 100;

    ret = recorder_.UpdateOverlayObjectParams(video_track_id, bbox_id,
                                              object_params);
    ASSERT_TRUE(ret == 0);
    usleep(250000);
  }

  // Remove overlay object from video track.
  ret = recorder_.RemoveOverlay(video_track_id, bbox_id);
  ASSERT_TRUE(ret == 0);

  // Delete overlay object.
  ret = recorder_.DeleteOverlayObject(video_track_id, bbox_id);
  ASSERT_TRUE(ret == 0);

  ret = recorder_.StopSession(session_id, false);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteVideoTrack(session_id, video_track_id);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteSession(session_id);
  ASSERT_TRUE(ret == NO_ERROR);

  ClearSessions();

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  dump_bitstream_.CloseAll();
  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());
}

/*
* 4KEncWithBoundingBoxOverlay: This test applies bounding box overlay type
*                              ontop of 4K video.
* Api test sequence:
*  - StartCamera
*   - CreateSession
*   - CreateVideoTrack
*   - StartVideoTrack
*   - CreateOverlayObject
*   - SetOverlay
*   loop Start {
*   ------------------
*    - GetOverlayObjectParams
*    - UpdateOverlayObjectParams
*   ------------------
*   } loop End
*   - RemoveOverlay
*   - DeleteOverlayObject
*   - StopSession
*   - DeleteVideoTrack
*   - DeleteSession
*   - StopCamera
*/
TEST_F(VideoGtest, 4KEncWithBoundingBoxOverlay) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  VideoFormat format_type = VideoFormat::kAVC;
  uint32_t width  = 3840;
  uint32_t height = 2160;
  ret = recorder_.StartCamera(camera_id_, 30);
  ASSERT_TRUE(ret == NO_ERROR);

  SessionCb session_status_cb = CreateSessionStatusCb();
  uint32_t session_id;
  ret = recorder_.CreateSession(session_status_cb, &session_id);
  ASSERT_TRUE(session_id > 0);
  ASSERT_TRUE(ret == NO_ERROR);
  VideoTrackCreateParam video_track_param{camera_id_, format_type,
                                          width, height, 30};
  uint32_t video_track_id = 1;
  if (dump_bitstream_.IsEnabled()) {
    StreamDumpInfo dumpinfo = { format_type, session_id, video_track_id,
                                width, height };
    ret = dump_bitstream_.SetUp(dumpinfo);
    ASSERT_TRUE(ret == NO_ERROR);
  }

  TrackCb video_track_cb;
  video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
      std::vector<BufferDescriptor> buffers,
      std::vector<MetaData> meta_buffers) {
        VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
      };

  video_track_cb.event_cb =
      [this] (uint32_t track_id, EventType event_type,
              void *event_data, size_t event_data_size) -> void
      { VideoTrackEventCb(track_id,
      event_type, event_data, event_data_size); };

  ret = recorder_.CreateVideoTrack(session_id, video_track_id,
                                    video_track_param, video_track_cb);
  ASSERT_TRUE(ret == NO_ERROR);

  std::vector<uint32_t> track_ids;
  track_ids.push_back(video_track_id);
  sessions_.insert(std::make_pair(session_id, track_ids));

  ret = recorder_.StartSession(session_id);
  ASSERT_TRUE(ret == NO_ERROR);

  // Create BoundingBox type overlay.
  OverlayParam object_params{};
  object_params.type  = OverlayType::kBoundingBox;
  object_params.color = kColorLightGreen;
  // Dummy coordinates for test purpose.
  object_params.dst_rect.start_x = 40;
  object_params.dst_rect.start_y = 40;
  object_params.dst_rect.width   = 800;
  object_params.dst_rect.height  = 400;
  std::string bb_text("Test BBox..");
  bb_text.copy(object_params.bounding_box.box_name, bb_text.length());

  uint32_t bbox_id;
  ret = recorder_.CreateOverlayObject(video_track_id, object_params,
                                       &bbox_id);
  ASSERT_TRUE(ret == 0);
  ret = recorder_.SetOverlay(video_track_id, bbox_id);
  ASSERT_TRUE(ret == 0);

  //Mimic moving bounding box.
  for (uint32_t j = 0; j < 100; ++j) {
    ret = recorder_.GetOverlayObjectParams(video_track_id, bbox_id,
                                           object_params);
    ASSERT_TRUE(ret == 0);

    object_params.dst_rect.start_x = ((object_params.dst_rect.start_x + 5 +
        object_params.dst_rect.width) < static_cast<int32_t> (width)) ?
                                object_params.dst_rect.start_x + 5 : 20;

    object_params.dst_rect.width = ((object_params.dst_rect.start_x + 5 +
        object_params.dst_rect.width) < static_cast<int32_t> (width)) ?
                                object_params.dst_rect.width + 5 : 200;

    object_params.dst_rect.start_y = ((object_params.dst_rect.start_y + 2 +
        object_params.dst_rect.height) < static_cast<int32_t> (height)) ?
                                  object_params.dst_rect.start_y + 2 : 20;

    object_params.dst_rect.height = ((object_params.dst_rect.start_y + 2 +
        object_params.dst_rect.height) < static_cast<int32_t> (height)) ?
                                  object_params.dst_rect.height + 2 : 100;

    ret = recorder_.UpdateOverlayObjectParams(video_track_id, bbox_id,
                                              object_params);
    ASSERT_TRUE(ret == 0);
    usleep(250000);
  }

  // Remove overlay object from video track.
  ret = recorder_.RemoveOverlay(video_track_id, bbox_id);
  ASSERT_TRUE(ret == 0);

  // Delete overlay object.
  ret = recorder_.DeleteOverlayObject(video_track_id, bbox_id);
  ASSERT_TRUE(ret == 0);

  ret = recorder_.StopSession(session_id, false);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteVideoTrack(session_id, video_track_id);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteSession(session_id);
  ASSERT_TRUE(ret == NO_ERROR);

  ClearSessions();

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  dump_bitstream_.CloseAll();
  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());
}

/*
* 1080pEncWithUserTextOverlay: This test applies custom user text ontop of 1080
*                              video.
* Api test sequence:
*  - StartCamera
*   - CreateSession
*   - CreateVideoTrack
*   - StartVideoTrack
*   - CreateOverlayObject
*   - SetOverlay
*   loop Start {
*   ------------------
*    - GetOverlayObjectParams
*    - UpdateOverlayObjectParams
*   ------------------
*   } loop End
*   - RemoveOverlay
*   - DeleteOverlayObject
*   - StopSession
*   - DeleteVideoTrack
*   - DeleteSession
*   - StopCamera
*/
TEST_F(VideoGtest, 1080pEncWithUserTextOverlay) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  VideoFormat format_type = VideoFormat::kAVC;
  uint32_t width  = 1920;
  uint32_t height = 1080;

  ret = recorder_.StartCamera(camera_id_, 30);
  ASSERT_TRUE(ret == NO_ERROR);

  SessionCb session_status_cb = CreateSessionStatusCb();
  uint32_t session_id;
  ret = recorder_.CreateSession(session_status_cb, &session_id);
  ASSERT_TRUE(session_id > 0);
  ASSERT_TRUE(ret == NO_ERROR);
  VideoTrackCreateParam video_track_param{camera_id_, format_type,
                                          width, height, 30};
  uint32_t video_track_id = 1;

  if (dump_bitstream_.IsEnabled()) {
    StreamDumpInfo dumpinfo = { format_type, session_id,
                                video_track_id, width, height };
    ret = dump_bitstream_.SetUp(dumpinfo);
    ASSERT_TRUE(ret == NO_ERROR);
  }

  TrackCb video_track_cb;
  video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
      std::vector<BufferDescriptor> buffers,
      std::vector<MetaData> meta_buffers) {
        VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
      };

  video_track_cb.event_cb =
      [this] (uint32_t track_id, EventType event_type,
              void *event_data, size_t event_data_size) -> void
      { VideoTrackEventCb(track_id,
      event_type, event_data, event_data_size); };

  ret = recorder_.CreateVideoTrack(session_id, video_track_id,
                                    video_track_param, video_track_cb);
  ASSERT_TRUE(ret == NO_ERROR);

  std::vector<uint32_t> track_ids;
  track_ids.push_back(video_track_id);
  sessions_.insert(std::make_pair(session_id, track_ids));

  ret = recorder_.StartSession(session_id);
  ASSERT_TRUE(ret == NO_ERROR);

  // Create UserText type overlay.
  OverlayParam object_params{};
  object_params.type = OverlayType::kUserText;
  object_params.location = OverlayLocationType::kTopRight;
  object_params.color    = kColorLightBlue;
  std::string user_text("Simple User Text For Testing!!");
  user_text.copy(object_params.user_text, user_text.length());

  uint32_t user_text_id;
  ret = recorder_.CreateOverlayObject(video_track_id, object_params,
                                       &user_text_id);
  ASSERT_TRUE(ret == 0);
  ret = recorder_.SetOverlay(video_track_id, user_text_id);
  ASSERT_TRUE(ret == 0);

  for(uint32_t i = 1, location = 0; i <= iteration_count_; ++i, ++location) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
      test_info_->name(), i);

    object_params = {};
    ret = recorder_.GetOverlayObjectParams(video_track_id, user_text_id,
                                           object_params);
    ASSERT_TRUE(ret == 0);

    // Update custom with text color and location on video.
    if (location == 0) {
      object_params.location = OverlayLocationType::kTopLeft;
      object_params.color    = kColorLightBlue;
      std::string user_text("TopLeft:Simple User Text!!");
      user_text.copy(object_params.user_text, user_text.length());
    } else if (location == 1) {
      object_params.location = OverlayLocationType::kTopRight;
      object_params.color    = kColorYellow;
      std::string user_text("TopRight:Simple User Text!!");
      user_text.copy(object_params.user_text, user_text.length());
    } else if (location == 2) {
      object_params.location = OverlayLocationType::kCenter;
      object_params.color    = kColorBlue;
      std::string user_text("Center:Simple User Text!!");
      user_text.copy(object_params.user_text, user_text.length());
    } else if (location == 3) {
      object_params.location = OverlayLocationType::kBottomLeft;
      object_params.color    = kColorWhilte;
      std::string user_text("BottomLeft:Simple User Text!!");
      user_text.copy(object_params.user_text, user_text.length());
    } else if (location == 4) {
      object_params.location = OverlayLocationType::kBottomRight;
      object_params.color    = kColorOrange;
      std::string user_text("BottomRight:Simple User Text!!");
      user_text.copy(object_params.user_text, user_text.length());
    } else {
      location = -1;
    }

    ret = recorder_.UpdateOverlayObjectParams(video_track_id, user_text_id,
                                              object_params);
    ASSERT_TRUE(ret == 0);
    // Record video with overlay.
    sleep(5);
  }

  // Remove overlay object from video track.
  ret = recorder_.RemoveOverlay(video_track_id, user_text_id);
  ASSERT_TRUE(ret == 0);

  // Delete overlay object.
  ret = recorder_.DeleteOverlayObject(video_track_id, user_text_id);
  ASSERT_TRUE(ret == 0);

  ret = recorder_.StopSession(session_id, false);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteVideoTrack(session_id, video_track_id);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteSession(session_id);
  ASSERT_TRUE(ret == NO_ERROR);

  ClearSessions();

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  dump_bitstream_.CloseAll();
  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());
}

/*
* 1080pEncWithPrivacyMaskOverlay: This test applies privacy mask overlay type
*                                 ontop of 1080 video.
* Api test sequence:
*  - StartCamera
*   - CreateSession
*   - CreateVideoTrack
*   - StartVideoTrack
*   - CreateOverlayObject
*   - SetOverlay
*   loop Start {
*   ------------------
*    - GetOverlayObjectParams
*    - UpdateOverlayObjectParams
*   ------------------
*   } loop End
*   - RemoveOverlay
*   - DeleteOverlayObject
*   - StopSession
*   - DeleteVideoTrack
*   - DeleteSession
*   - StopCamera
*/
TEST_F(VideoGtest, 1080pEncWithPrivacyMaskOverlay) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  VideoFormat format_type = VideoFormat::kAVC;
  uint32_t width  = 1920;
  uint32_t height = 1080;

  ret = recorder_.StartCamera(camera_id_, 30);
  ASSERT_TRUE(ret == NO_ERROR);

  SessionCb session_status_cb = CreateSessionStatusCb();
  uint32_t session_id;
  ret = recorder_.CreateSession(session_status_cb, &session_id);
  ASSERT_TRUE(session_id > 0);
  ASSERT_TRUE(ret == NO_ERROR);
  VideoTrackCreateParam video_track_param{camera_id_, format_type,
                                          width, height, 30};
  uint32_t video_track_id = 1;

  if (dump_bitstream_.IsEnabled()) {
    StreamDumpInfo dumpinfo = { format_type, session_id, video_track_id,
                                width, height };
    ret = dump_bitstream_.SetUp(dumpinfo);
    ASSERT_TRUE(ret == NO_ERROR);
  }

  TrackCb video_track_cb;
  video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
      std::vector<BufferDescriptor> buffers,
      std::vector<MetaData> meta_buffers) {
        VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
      };

  video_track_cb.event_cb =
      [this] (uint32_t track_id, EventType event_type,
              void *event_data, size_t event_data_size) -> void
      { VideoTrackEventCb(track_id,
      event_type, event_data, event_data_size); };

  ret = recorder_.CreateVideoTrack(session_id, video_track_id,
                                    video_track_param, video_track_cb);
  ASSERT_TRUE(ret == NO_ERROR);

  std::vector<uint32_t> track_ids;
  track_ids.push_back(video_track_id);
  sessions_.insert(std::make_pair(session_id, track_ids));

  ret = recorder_.StartSession(session_id);
  ASSERT_TRUE(ret == NO_ERROR);

  // Create BoundingBox type overlay.
  OverlayParam object_params{};
  object_params.type  = OverlayType::kPrivacyMask;
  object_params.color = 0xFF9933FF; //Fill mask with color.
  // Dummy coordinates for test purpose.
  object_params.dst_rect.start_x = 20;
  object_params.dst_rect.start_y = 40;
  object_params.dst_rect.width   = 1920/8;
  object_params.dst_rect.height  = 1080/8;

  uint32_t mask_id;
  ret = recorder_.CreateOverlayObject(video_track_id, object_params,
                                       &mask_id);
  ASSERT_TRUE(ret == 0);
  ret = recorder_.SetOverlay(video_track_id, mask_id);
  ASSERT_TRUE(ret == 0);
  uint32_t sleep_duration = record_duration_ * 1000000 / 20;
  if (sleep_duration < 250000)
    sleep_duration = 250000;
  for(uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
      test_info_->name(), i);

    for (uint32_t j = 0; j < 20; ++j) {
      ret = recorder_.GetOverlayObjectParams(video_track_id, mask_id,
                                             object_params);
      ASSERT_TRUE(ret == 0);

      object_params.dst_rect.start_x = (object_params.dst_rect.start_x + 20 +
          object_params.dst_rect.width < 1920) ? object_params.dst_rect.start_x + 20 : 20;

      object_params.dst_rect.width = (object_params.dst_rect.start_x + 50 +
          object_params.dst_rect.width < 1920) ? object_params.dst_rect.width + 50 : 1920/8;

      object_params.dst_rect.start_y = (object_params.dst_rect.start_y + 10 +
          object_params.dst_rect.height < 1080) ? object_params.dst_rect.start_y + 10 : 40;

      object_params.dst_rect.height = (object_params.dst_rect.start_y + 50 +
          object_params.dst_rect.height < 1080) ? object_params.dst_rect.height + 50 : 1080/8;

      ret = recorder_.UpdateOverlayObjectParams(video_track_id, mask_id,
                                                object_params);
      ASSERT_TRUE(ret == 0);
      usleep(sleep_duration);
    }

  }
  // Remove overlay object from video track.
  ret = recorder_.RemoveOverlay(video_track_id, mask_id);
  ASSERT_TRUE(ret == 0);

  // Delete overlay object.
  ret = recorder_.DeleteOverlayObject(video_track_id, mask_id);
  ASSERT_TRUE(ret == 0);

  ret = recorder_.StopSession(session_id, false);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteVideoTrack(session_id, video_track_id);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteSession(session_id);
  ASSERT_TRUE(ret == NO_ERROR);

  ClearSessions();

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  dump_bitstream_.CloseAll();
  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());
}

/*
* 1080pEncWithStaticImageBlobOverlay:This test will apply static image blob
*                                    overlay ontop of 1080 video.
* Api test sequence:
*  - StartCamera
*   - CreateSession
*   - CreateVideoTrack
*   - StartVideoTrack
*   - CreateOverlayObject
*   - SetOverlay
*   loop Start {
*   ------------------
*    - GetOverlayObjectParams
*    - UpdateOverlayObjectParams
*   ------------------
*   } loop End
*   - RemoveOverlay
*   - DeleteOverlayObject
*   - StopSession
*   - DeleteVideoTrack
*   - DeleteSession
*   - StopCamera
*/
TEST_F(VideoGtest, 1080pEncWithStaticImageBlobOverlay) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  VideoFormat format_type = VideoFormat::kAVC;
  uint32_t width  = 1920;
  uint32_t height = 1080;
  ret = recorder_.StartCamera(camera_id_, 30);
  ASSERT_TRUE(ret == NO_ERROR);

  SessionCb session_status_cb = CreateSessionStatusCb();
  uint32_t session_id;
  ret = recorder_.CreateSession(session_status_cb, &session_id);
  ASSERT_TRUE(session_id > 0);
  ASSERT_TRUE(ret == NO_ERROR);
  VideoTrackCreateParam video_track_param{camera_id_, format_type,
                                          width, height, 30};
  uint32_t video_track_id = 1;
  if (dump_bitstream_.IsEnabled()) {
    StreamDumpInfo dumpinfo = { format_type, session_id, video_track_id,
                                width, height };
    ret = dump_bitstream_.SetUp(dumpinfo);
    ASSERT_TRUE(ret == NO_ERROR);
  }

  TrackCb video_track_cb;
  video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
      std::vector<BufferDescriptor> buffers,
      std::vector<MetaData> meta_buffers) {
        VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
      };

  video_track_cb.event_cb =
      [this] (uint32_t track_id, EventType event_type,
              void *event_data, size_t event_data_size) -> void
      { VideoTrackEventCb(track_id,
      event_type, event_data, event_data_size); };

  ret = recorder_.CreateVideoTrack(session_id, video_track_id,
                                    video_track_param, video_track_cb);
  ASSERT_TRUE(ret == NO_ERROR);

  std::vector<uint32_t> track_ids;
  track_ids.push_back(video_track_id);
  sessions_.insert(std::make_pair(session_id, track_ids));

  ret = recorder_.StartSession(session_id);
  ASSERT_TRUE(ret == NO_ERROR);

  // Create Static Image blob type overlay.
  OverlayParam object_params{};
  uint32_t static_img_id;
  char * image_buffer;
  uint32_t image_size;
  int32_t image_width;
  int32_t image_height;
  // Create Image buffer blob type overlay.
  object_params.type = OverlayType::kStaticImage;
  object_params.location = OverlayLocationType::kRandom;
  object_params.image_info.image_type = OverlayImageType::kBlobType;
  object_params.dst_rect.start_x = 1200;
  object_params.dst_rect.start_y = 580;
  object_params.dst_rect.width   = 451;
  object_params.dst_rect.height  = 109;

  object_params.image_info.source_rect.start_x = 0;
  object_params.image_info.source_rect.start_y = 0;
  object_params.image_info.source_rect.width  = 451;
  object_params.image_info.source_rect.height = 109;
  object_params.image_info.buffer_updated = false;

  image_width = object_params.image_info.source_rect.width;
  image_height = object_params.image_info.source_rect.height;

  FILE *image = nullptr;
  image = fopen(OVERLAY_TEST_FILE, "r");
  if (!image) {
   TEST_ERROR("%s: Unable to open file", __func__);
   ASSERT_TRUE(image == nullptr);
  }

  object_params.image_info.image_size = image_width * image_height * 4;
  image_size = object_params.image_info.image_size;

  object_params.image_info.image_buffer =
      reinterpret_cast<char *>(malloc(sizeof(char) * image_size));
  image_buffer = object_params.image_info.image_buffer;

  fread(object_params.image_info.image_buffer, sizeof(char),
      object_params.image_info.image_size, image);

  fclose(image);

  ret = recorder_.CreateOverlayObject(video_track_id, object_params,
                                      &static_img_id);

  ASSERT_TRUE(ret == 0);
  // Apply overlay object on video track.
  ret = recorder_.SetOverlay(video_track_id, static_img_id);
  ASSERT_TRUE(ret == 0);

  for(uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
      test_info_->name(), i);

    // Mimic moving Static Image blob type.
    for (uint32_t j = 0; j < 100; ++j) {
      ret = recorder_.GetOverlayObjectParams(video_track_id, static_img_id,
                                             object_params);
      ASSERT_TRUE(ret == 0);

      object_params.type = OverlayType::kStaticImage;
      object_params.location = OverlayLocationType::kRandom;
      object_params.image_info.image_type = OverlayImageType::kBlobType;

      object_params.dst_rect.start_x =
          ((object_params.dst_rect.start_x  + 5 + object_params.dst_rect.width) <
           static_cast<int32_t>(width))
              ? object_params.dst_rect.start_x + 5
              : 20;

      object_params.dst_rect.width =
          ((object_params.dst_rect.start_x + 5 + object_params.dst_rect.width) <
           static_cast<int32_t>(width))
              ? object_params.dst_rect.width + 5
              : 200;

      object_params.dst_rect.start_y =
          ((object_params.dst_rect.start_y + 2 + object_params.dst_rect.height) <
           static_cast<int32_t>(height))
              ? object_params.dst_rect.start_y + 2
              : 20;

      object_params.dst_rect.height =
          ((object_params.dst_rect.start_y + 2 + object_params.dst_rect.height) <
           static_cast<int32_t>(height))
              ? object_params.dst_rect.height + 2
              : 100;

      object_params.image_info.image_size = image_size;
      object_params.image_info.image_buffer = image_buffer;
      object_params.image_info.source_rect.start_x = 0;
      object_params.image_info.source_rect.start_y = 0;
      object_params.image_info.source_rect.width = 451;
      object_params.image_info.source_rect.height = 109;
      object_params.image_info.buffer_updated = false;

      ret = recorder_.UpdateOverlayObjectParams(video_track_id, static_img_id,
                                                object_params);
      ASSERT_TRUE(ret == 0);
      usleep(250000);
    }
  }
  // Remove overlay object from video track.
  ret = recorder_.RemoveOverlay(video_track_id, static_img_id);
  ASSERT_TRUE(ret == 0);

  // Delete overlay object.
  ret = recorder_.DeleteOverlayObject(video_track_id, static_img_id);
  ASSERT_TRUE(ret == 0);

  ret = recorder_.StopSession(session_id, false);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteVideoTrack(session_id, video_track_id);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteSession(session_id);
  ASSERT_TRUE(ret == NO_ERROR);

  ClearSessions();

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  dump_bitstream_.CloseAll();
  free(image_buffer);
  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());
}

/*
* 1080pEncWithStaticImageBlobUpdateBufferOverlay:This test will apply static
*         image blob overlay ontop of 1080 video and it's content is updating.
* Api test sequence:
*  - StartCamera
*   - CreateSession
*   - CreateVideoTrack
*   - StartVideoTrack
*   - CreateOverlayObject
*   - SetOverlay
*   loop Start {
*   ------------------
*    - GetOverlayObjectParams
*    - UpdateOverlayObjectParams
*   ------------------
*   } loop End
*   - RemoveOverlay
*   - DeleteOverlayObject
*   - StopSession
*   - DeleteVideoTrack
*   - DeleteSession
*   - StopCamera
*/
TEST_F(VideoGtest, 1080pEncWithStaticImageBlobUpdateBufferOverlay) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  VideoFormat format_type = VideoFormat::kAVC;
  uint32_t width  = 1920;
  uint32_t height = 1080;
  ret = recorder_.StartCamera(camera_id_, 30);
  ASSERT_TRUE(ret == NO_ERROR);

  SessionCb session_status_cb = CreateSessionStatusCb();
  uint32_t session_id;
  ret = recorder_.CreateSession(session_status_cb, &session_id);
  ASSERT_TRUE(session_id > 0);
  ASSERT_TRUE(ret == NO_ERROR);
  VideoTrackCreateParam video_track_param{camera_id_, format_type,
                                          width, height, 30};
  uint32_t video_track_id = 1;

  if (dump_bitstream_.IsEnabled()) {
    StreamDumpInfo dumpinfo = { format_type, session_id, video_track_id,
                                width, height };
    ret = dump_bitstream_.SetUp(dumpinfo);
    ASSERT_TRUE(ret == NO_ERROR);
  }

  TrackCb video_track_cb;
  video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
      std::vector<BufferDescriptor> buffers,
      std::vector<MetaData> meta_buffers) {
        VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
      };

  video_track_cb.event_cb =
      [this] (uint32_t track_id, EventType event_type,
              void *event_data, size_t event_data_size) -> void
      { VideoTrackEventCb(track_id,
      event_type, event_data, event_data_size); };

  ret = recorder_.CreateVideoTrack(session_id, video_track_id,
                                    video_track_param, video_track_cb);
  ASSERT_TRUE(ret == NO_ERROR);

  std::vector<uint32_t> track_ids;
  track_ids.push_back(video_track_id);
  sessions_.insert(std::make_pair(session_id, track_ids));

  ret = recorder_.StartSession(session_id);
  ASSERT_TRUE(ret == NO_ERROR);

  // Create Static Image blob type overlay.
  OverlayParam object_params{};
  uint32_t static_img_id;
  char * image_buffer;
  uint32_t image_size;
  int32_t image_width;
  int32_t image_height;
  // Create Image buffer blob type overlay.
  object_params.type = OverlayType::kStaticImage;
  object_params.location = OverlayLocationType::kRandom;
  object_params.image_info.image_type = OverlayImageType::kBlobType;
  object_params.dst_rect.start_x = 1200;
  object_params.dst_rect.start_y = 800;
  object_params.dst_rect.width   = 451;
  object_params.dst_rect.height  = 109;

  object_params.image_info.source_rect.start_x = 0;
  object_params.image_info.source_rect.start_y = 0;
  object_params.image_info.source_rect.width   = 451;
  object_params.image_info.source_rect.height  = 109;
  object_params.image_info.buffer_updated = false;

  image_width = object_params.image_info.source_rect.width;
  image_height = object_params.image_info.source_rect.height;

  object_params.image_info.image_size = image_width * image_height * 4;
  image_size = object_params.image_info.image_size;

  object_params.image_info.image_buffer =
      reinterpret_cast<char *>(malloc(sizeof(char) * image_size));
  image_buffer = object_params.image_info.image_buffer;

  DrawOverlay(object_params.image_info.image_buffer,
      object_params.image_info.source_rect.width,
      object_params.image_info.source_rect.height);

  ret = recorder_.CreateOverlayObject(video_track_id, object_params,
                                      &static_img_id);

  ASSERT_TRUE(ret == 0);
  // Apply overlay object on video track.
  ret = recorder_.SetOverlay(video_track_id, static_img_id);
  ASSERT_TRUE(ret == 0);

  for(uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
        test_info_->name(), i);

    // Mimic movement and buffer update for static image blob type.
    for (uint32_t j = 0; j < 100; ++j) {
      ret = recorder_.GetOverlayObjectParams(video_track_id, static_img_id,
                                             object_params);
      ASSERT_TRUE(ret == 0);

      object_params.type = OverlayType::kStaticImage;
      object_params.location = OverlayLocationType::kRandom;
      object_params.image_info.image_type = OverlayImageType::kBlobType;

      object_params.dst_rect.start_x =
          ((object_params.dst_rect.start_x + 5 + object_params.dst_rect.width) <
           static_cast<int32_t>(width))
              ? object_params.dst_rect.start_x + 5
              : 20;

      object_params.dst_rect.width =
          ((object_params.dst_rect.start_x + 5 + object_params.dst_rect.width) <
           static_cast<int32_t>(width))
              ? object_params.dst_rect.width + 5
              : 200;

      object_params.dst_rect.start_y =
          ((object_params.dst_rect.start_y + 2 + object_params.dst_rect.height) <
           static_cast<int32_t>(height))
              ? object_params.dst_rect.start_y + 2
              : 20;

      object_params.dst_rect.height =
          ((object_params.dst_rect.start_y + 2 + object_params.dst_rect.height) <
           static_cast<int32_t>(height))
              ? object_params.dst_rect.height + 2
              : 100;

      object_params.image_info.image_size = image_size;
      object_params.image_info.image_buffer = image_buffer;
      object_params.image_info.source_rect.start_x = 0;
      object_params.image_info.source_rect.start_y = 0;
      object_params.image_info.source_rect.width = 451;
      object_params.image_info.source_rect.height = 109;
      object_params.image_info.buffer_updated = true;

      DrawOverlay(object_params.image_info.image_buffer,
                  object_params.image_info.source_rect.width,
                  object_params.image_info.source_rect.height);

      ret = recorder_.UpdateOverlayObjectParams(video_track_id, static_img_id,
                                                object_params);
      ASSERT_TRUE(ret == 0);
      usleep(500000);
    }
  }
  // Remove overlay object from video track.
  ret = recorder_.RemoveOverlay(video_track_id, static_img_id);
  ASSERT_TRUE(ret == 0);

  // Delete overlay object.
  ret = recorder_.DeleteOverlayObject(video_track_id, static_img_id);
  ASSERT_TRUE(ret == 0);

  ret = recorder_.StopSession(session_id, false);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteVideoTrack(session_id, video_track_id);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteSession(session_id);
  ASSERT_TRUE(ret == NO_ERROR);

  ClearSessions();

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  dump_bitstream_.CloseAll();
  free(image_buffer);
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
TEST_F(VideoGtest, SessionWith1080pEncTrackStartStop) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  VideoFormat format_type = VideoFormat::kAVC;
  uint32_t width  = 1920;
  uint32_t height = 1080;

  ret = recorder_.StartCamera(camera_id_, 30);
  ASSERT_TRUE(ret == NO_ERROR);

  SessionCb session_status_cb = CreateSessionStatusCb();
  uint32_t session_id;
  ret = recorder_.CreateSession(session_status_cb, &session_id);
  ASSERT_TRUE(session_id > 0);
  ASSERT_TRUE(ret == NO_ERROR);
  VideoTrackCreateParam video_track_param{camera_id_, format_type,
                                          width, height, 30};
  uint32_t video_track_id = 1;
  if (dump_bitstream_.IsEnabled()) {
    StreamDumpInfo dumpinfo = { format_type, session_id, video_track_id,
                                width, height };
    ret = dump_bitstream_.SetUp(dumpinfo);
    ASSERT_TRUE(ret == NO_ERROR);
  }

  TrackCb video_track_cb;
  video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
      std::vector<BufferDescriptor> buffers,
      std::vector<MetaData> meta_buffers) {
        VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
      };

  video_track_cb.event_cb =
      [this] (uint32_t track_id, EventType event_type,
              void *event_data, size_t event_data_size) -> void {
      VideoTrackEventCb(track_id, event_type, event_data, event_data_size); };

  ret = recorder_.CreateVideoTrack(session_id, video_track_id,
                                    video_track_param, video_track_cb);
  ASSERT_TRUE(ret == NO_ERROR);

  std::vector<uint32_t> track_ids;
  track_ids.push_back(video_track_id);
  sessions_.insert(std::make_pair(session_id, track_ids));

  for(uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
        test_info_->name(), i);

    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    // Let session run for record_duration_, during this time buffer with valid
    // data would be received in track callback (VideoTrackDataCb).
    sleep(record_duration_);

    ret = recorder_.StopSession(session_id, false);
    ASSERT_TRUE(ret == NO_ERROR);
  }

  ret = recorder_.DeleteVideoTrack(session_id, video_track_id);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteSession(session_id);
  ASSERT_TRUE(ret == NO_ERROR);

  ClearSessions();

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  dump_bitstream_.CloseAll();
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
TEST_F(VideoGtest, SessionWith4KEncTrackStartStop) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  VideoFormat format_type = VideoFormat::kAVC;
  uint32_t width  = 3840;
  uint32_t height = 2160;

  ret = recorder_.StartCamera(camera_id_, 30);
  ASSERT_TRUE(ret == NO_ERROR);

  SessionCb session_status_cb = CreateSessionStatusCb();
  uint32_t session_id;
  ret = recorder_.CreateSession(session_status_cb, &session_id);
  ASSERT_TRUE(session_id > 0);
  ASSERT_TRUE(ret == NO_ERROR);

  VideoTrackCreateParam video_track_param{camera_id_, format_type,
                                          width, height, 30};
  uint32_t video_track_id = 1;
  if (dump_bitstream_.IsEnabled()) {
    StreamDumpInfo dumpinfo = { format_type, session_id, video_track_id,
                                width, height };
    ret = dump_bitstream_.SetUp(dumpinfo);
    ASSERT_TRUE(ret == NO_ERROR);
  }

  TrackCb video_track_cb;
  video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
      std::vector<BufferDescriptor> buffers,
      std::vector<MetaData> meta_buffers) {
        VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
      };

  video_track_cb.event_cb =
      [this] (uint32_t track_id, EventType event_type,
              void *event_data, size_t event_data_size) -> void {
      VideoTrackEventCb(track_id, event_type, event_data, event_data_size); };

  ret = recorder_.CreateVideoTrack(session_id, video_track_id,
                                    video_track_param, video_track_cb);
  ASSERT_TRUE(ret == NO_ERROR);

  std::vector<uint32_t> track_ids;
  track_ids.push_back(video_track_id);
  sessions_.insert(std::make_pair(session_id, track_ids));

  for(uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
        test_info_->name(), i);

    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    // Let session run for record_duration_, during this time buffer with valid
    // data would be received in track callback (VideoTrackDataCb).
    sleep(record_duration_);

    ret = recorder_.StopSession(session_id, false);
    ASSERT_TRUE(ret == NO_ERROR);
  }
  ret = recorder_.DeleteVideoTrack(session_id, video_track_id);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteSession(session_id);
  ASSERT_TRUE(ret == NO_ERROR);

  ClearSessions();

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  dump_bitstream_.CloseAll();
  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());
}

/*
* SessionWith4KAnd1080pYUVTrackStartStop: This test will test session with 4k
*                                         and 1080p YUV tracks.
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
TEST_F(VideoGtest, SessionWith4KAnd1080pYUVTrackStartStop) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.StartCamera(camera_id_, 30);
  ASSERT_TRUE(ret == NO_ERROR);

  SessionCb session_status_cb = CreateSessionStatusCb();
  uint32_t session_id;
  ret = recorder_.CreateSession(session_status_cb, &session_id);
  ASSERT_TRUE(session_id > 0);
  ASSERT_TRUE(ret == NO_ERROR);
  uint32_t track1_id = 1;
  uint32_t track2_id = 2;

  std::vector<uint32_t> track_ids;
  VideoTrackCreateParam video_track_param{camera_id_, VideoFormat::kYUV,
                                          3840, 2160, 30};

  if (ubwc_stream_enable_) {
    video_track_param.low_power_mode = true;
  }

  TrackCb video_track_cb;
  video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
      std::vector<BufferDescriptor> buffers,
      std::vector<MetaData> meta_buffers) {
        VideoTrackYUVDataCb(session_id, track_id, buffers, meta_buffers); };

  video_track_cb.event_cb =
      [this] (uint32_t track_id, EventType event_type,
              void *event_data, size_t event_data_size) -> void {
      VideoTrackEventCb(track_id, event_type, event_data, event_data_size); };

  ret = recorder_.CreateVideoTrack(session_id, track1_id,
                                    video_track_param, video_track_cb);
  ASSERT_TRUE(ret == NO_ERROR);
  track_ids.push_back(track1_id);

  video_track_param.width  = 1920;
  video_track_param.height = 1080;

  ret = recorder_.CreateVideoTrack(session_id, track2_id,
                                    video_track_param, video_track_cb);
  ASSERT_TRUE(ret == NO_ERROR);

  track_ids.push_back(track2_id);
  sessions_.insert(std::make_pair(session_id, track_ids));

  for(uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
        test_info_->name(), i);

    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    // Let session run for record_duration_, during this time buffer with valid
    // data would be received in track callback (VideoTrackYUVDataCb).
    sleep(record_duration_);

    ret = recorder_.StopSession(session_id, false);
    ASSERT_TRUE(ret == NO_ERROR);
  }
  ret = recorder_.DeleteVideoTrack(session_id, track1_id);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteVideoTrack(session_id, track2_id);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteSession(session_id);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ClearSessions();

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

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
TEST_F(VideoGtest, SessionWithTwo1080pEncTracksStartStop) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  VideoFormat format_type = VideoFormat::kAVC;
  uint32_t width  = 1920;
  uint32_t height = 1080;
  uint32_t video_track_id1 = 1;
  uint32_t video_track_id2 = 2;

  ret = recorder_.StartCamera(camera_id_, 30);
  ASSERT_TRUE(ret == NO_ERROR);

  SessionCb session_status_cb = CreateSessionStatusCb();
  uint32_t session_id;
  ret = recorder_.CreateSession(session_status_cb, &session_id);
  ASSERT_TRUE(session_id > 0);
  ASSERT_TRUE(ret == NO_ERROR);

  std::vector<uint32_t> track_ids;
  VideoTrackCreateParam video_track_param{camera_id_, format_type,
                                          width, height, 30};

  TrackCb video_track_cb;
  video_track_cb.event_cb =
      [this] (uint32_t track_id, EventType event_type,
              void *event_data, size_t event_data_size) -> void {
      VideoTrackEventCb(track_id, event_type, event_data, event_data_size); };

  video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
      std::vector<BufferDescriptor> buffers,
      std::vector<MetaData> meta_buffers) {
        VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
      };

  // Create 1080p encode track.
  ret = recorder_.CreateVideoTrack(session_id, video_track_id1,
                                   video_track_param, video_track_cb);
  ASSERT_TRUE(ret == NO_ERROR);
  if (dump_bitstream_.IsEnabled()) {
    StreamDumpInfo dumpinfo = { format_type, session_id, video_track_id1,
                                width, height };
    ret = dump_bitstream_.SetUp(dumpinfo);
    ASSERT_TRUE(ret == NO_ERROR);
  }
  track_ids.push_back(video_track_id1);

  video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
      std::vector<BufferDescriptor> buffers,
      std::vector<MetaData> meta_buffers) {
        VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
      };

  // Create another 1080p encode track.
  ret = recorder_.CreateVideoTrack(session_id, video_track_id2,
                                   video_track_param, video_track_cb);
  ASSERT_TRUE(ret == NO_ERROR);

  if (dump_bitstream_.IsEnabled()) {
    StreamDumpInfo dumpinfo = { format_type, session_id, video_track_id2,
                                width, height };
    ret = dump_bitstream_.SetUp(dumpinfo);
  }
  track_ids.push_back(video_track_id1);

  sessions_.insert(std::make_pair(session_id, track_ids));

  for(uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
        test_info_->name(), i);

    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    // Let session run for record_duration_, during this time buffer with valid
    // data would be received in track callback (VideoTrackDataCb).
    sleep(record_duration_);

    ret = recorder_.StopSession(session_id, false);
    ASSERT_TRUE(ret == NO_ERROR);
  }

  ret = recorder_.DeleteVideoTrack(session_id, video_track_id1);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteVideoTrack(session_id, video_track_id2);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteSession(session_id);
  ASSERT_TRUE(ret == NO_ERROR);

  ClearSessions();

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  dump_bitstream_.CloseAll();
  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());
}

/*
* DynamicFloatingFrameRate: This test will test session with one 4K h264 track
* with dynaminc change of frame rate .
* Api test sequence:
*  - StartCamera
*   loop Start {
*   ------------------
*   - CreateSession
*   - CreateVideoTrack
*   - StartVideoTrack
*   - SetVideoTrackParam (change frame rate)
*   - StopSession
*   - DeleteVideoTrack
*   - DeleteSession
*   ------------------
*   } loop End
*  - StopCamera
*/
TEST_F(VideoGtest, DynamicFloatingFrameRate) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  VideoFormat format_type = VideoFormat::kAVC;
  uint32_t width  = 3840;
  uint32_t height = 2160;
  CodecParamType param_type = CodecParamType::kFrameRateType;
  float fps = 30.0;

  ret = recorder_.StartCamera(camera_id_, 30);
  ASSERT_TRUE(ret == NO_ERROR);

  for (uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
        test_info_->name(), i);

    SessionCb session_status_cb = CreateSessionStatusCb();
    uint32_t session_id;
    ret = recorder_.CreateSession(session_status_cb, &session_id);
    ASSERT_TRUE(session_id > 0);
    ASSERT_TRUE(ret == NO_ERROR);
    VideoTrackCreateParam video_track_param{camera_id_, format_type,
                                            width, height, 30};

    uint32_t video_track_id = 1;
    if (dump_bitstream_.IsEnabled()) {
      StreamDumpInfo dumpinfo = { format_type, session_id, video_track_id,
                                  width, height };
      ret = dump_bitstream_.SetUp(dumpinfo);
      ASSERT_TRUE(ret == NO_ERROR);
    }

    TrackCb video_track_cb;
    video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
        std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
          VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
        };

    video_track_cb.event_cb =
        [this] (uint32_t track_id, EventType event_type,
                void *event_data, size_t event_data_size) -> void
        { VideoTrackEventCb(track_id,
        event_type, event_data, event_data_size); };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id,
                                      video_track_param, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    std::vector<uint32_t> track_ids;
    track_ids.push_back(video_track_id);
    sessions_.insert(std::make_pair(session_id, track_ids));

    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    // Let session run for 10s, than change frame rate.
    // During this time buffer with valid
    // data would be received in track callback (VideoTrackDataCb).
    sleep(10);

    fps = 12.5;
    ret = recorder_.SetVideoTrackParam(session_id, video_track_id, param_type,
      &fps, sizeof(fps));
    ASSERT_TRUE(ret == NO_ERROR);

    sleep(10);
    fps = 14.99;
    ret = recorder_.SetVideoTrackParam(session_id, video_track_id, param_type,
      &fps, sizeof(fps));
    ASSERT_TRUE(ret == NO_ERROR);

    sleep(10);
    fps = 23.98;
    ret = recorder_.SetVideoTrackParam(session_id, video_track_id, param_type,
      &fps, sizeof(fps));
    ASSERT_TRUE(ret == NO_ERROR);

    sleep(10);
    fps = 29.97;
    ret = recorder_.SetVideoTrackParam(session_id, video_track_id, param_type,
      &fps, sizeof(fps));
    ASSERT_TRUE(ret == NO_ERROR);

    sleep(10);

    ret = recorder_.StopSession(session_id, false);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    ClearSessions();
    dump_bitstream_.CloseAll();
  }
  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);


  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());
}

/*
* 1080pYUVTrackMatchCameraMetaData: This test demonstrates how track buffer can
* be matched exactly with it's corresponding CameraMetaData using meta frame
* number.
* can b session with one 1080p YUV track.
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
TEST_F(VideoGtest, 1080pYUVTrackMatchCameraMetaData) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  CameraResultCb result_cb = [&] (uint32_t camera_id,
      const CameraMetadata &result) {
        ResultCallbackHandlerMatchCameraMeta(camera_id, result);
      };

  CameraExtraParam empty_extra_params;
  ret = recorder_.StartCamera(camera_id_, 30, empty_extra_params, result_cb);
  ASSERT_TRUE(ret == NO_ERROR);

  SessionCb session_status_cb = CreateSessionStatusCb();
  uint32_t session_id;
  ret = recorder_.CreateSession(session_status_cb, &session_id);
  ASSERT_TRUE(session_id > 0);
  ASSERT_TRUE(ret == NO_ERROR);
  VideoTrackCreateParam video_track_param{camera_id_, VideoFormat::kYUV,
                                          1920,
                                          1080,
                                          30};

  uint32_t video_track_id       = 1;
  TrackCb video_track_cb;
  video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
      std::vector<BufferDescriptor> buffers,
      std::vector<MetaData> meta_buffers) {
        VideoTrackDataCbMatchCameraMeta(session_id, track_id, buffers,
                                        meta_buffers);
      };

  video_track_cb.event_cb = [&] (uint32_t track_id, EventType event_type,
      void *event_data, size_t event_data_size) { VideoTrackEventCb(track_id,
      event_type, event_data, event_data_size); };

  ret = recorder_.CreateVideoTrack(session_id, video_track_id,
                                   video_track_param, video_track_cb);
  ASSERT_TRUE(ret == NO_ERROR);

  std::vector<uint32_t> track_ids;
  track_ids.push_back(video_track_id);
  sessions_.insert(std::make_pair(session_id, track_ids));

  ret = recorder_.StartSession(session_id);
  ASSERT_TRUE(ret == NO_ERROR);

  // Let session run for time record_duration_, during this time buffer with
  // valid data would be received in track callback
  // (VideoTrackDataCbMatchCameraMeta).
  sleep(record_duration_*2);

  ret = recorder_.StopSession(session_id, false);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteVideoTrack(session_id, video_track_id);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteSession(session_id);
  ASSERT_TRUE(ret == NO_ERROR);

  ClearSessions();

  buffer_metadata_map_.clear();

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());

}

/*
* 1080pWithFrameRepeat: This test will test session with one 4K h264 track
* with frame repeat.
* Api test sequence:
*  - StartCamera
*   loop Start {
*   ------------------
*   - CreateSession
*   - CreateVideoTrack
*   - StartVideoTrack
*   - SetVideoTrackParam
*   - StopSession
*   - DeleteVideoTrack
*   - DeleteSession
*   ------------------
*   } loop End
*  - StopCamera
*/
TEST_F(VideoGtest, 1080pWithFrameRepeat) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  VideoFormat format_type = VideoFormat::kAVC;
  uint32_t width  = 1920;
  uint32_t height = 1080;
  CodecParamType param_type = CodecParamType::kFrameRateType;
  CodecParamType fr_repeat = CodecParamType::kEnableFrameRepeat;
  float fps;

  ret = recorder_.StartCamera(camera_id_, 30);
  ASSERT_TRUE(ret == NO_ERROR);

  for (uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
              test_info_->name(), i);

    SessionCb session_status_cb = CreateSessionStatusCb();
    uint32_t session_id;
    ret = recorder_.CreateSession(session_status_cb, &session_id);
    ASSERT_TRUE(session_id > 0);
    ASSERT_TRUE(ret == NO_ERROR);
    fps = 30.0;
    VideoTrackCreateParam video_track_param{camera_id_, format_type,
                                            width, height, fps};
    uint32_t video_track_id = 1;

    if (dump_bitstream_.IsEnabled()) {
      StreamDumpInfo dumpinfo = { format_type, session_id, video_track_id,
                                  width, height };
      ret = dump_bitstream_.SetUp(dumpinfo);
      ASSERT_TRUE(ret == NO_ERROR);
    }

    TrackCb video_track_cb;
    video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
        std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
          VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
        };

    video_track_cb.event_cb =
        [this] (uint32_t track_id, EventType event_type,
                void *event_data, size_t event_data_size) -> void
        { VideoTrackEventCb(track_id,
        event_type, event_data, event_data_size); };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id,
                                     video_track_param, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    std::vector<uint32_t> track_ids;
    track_ids.push_back(video_track_id);
    sessions_.insert(std::make_pair(session_id, track_ids));

    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    float threshold = 70.0;
    float step = (threshold - fps) / 2;
    const uint32_t step_count = 4;

    bool enable_frame_repeat = true;
    ret = recorder_.SetVideoTrackParam(session_id, video_track_id, fr_repeat,
                                       &enable_frame_repeat,
                                       sizeof(enable_frame_repeat));
    ASSERT_TRUE(ret == NO_ERROR);

    // Let session run for 5s, then increase frame rate to threshold
    // in two steps. After reaching threshold, decrease back in two steps.
    // Run for 5s between steps.
    // During this time buffer with valid
    // data would be received in track callback (VideoTrackDataCb).
    sleep(5);
    if (fps < threshold) {
      for (uint32_t j = 0; j < step_count; j++) {
        if (j < step_count / 2) {
          fps += step;
        } else {
          fps -= step;
        }
        ret = recorder_.SetVideoTrackParam(session_id, video_track_id,
                                           param_type, &fps, sizeof(fps));
        ASSERT_TRUE(ret == NO_ERROR);

        sleep(5);
      }
    }

    ret = recorder_.StopSession(session_id, false);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    ClearSessions();
    dump_bitstream_.CloseAll();
  }
  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
          test_info_->test_case_name(), test_info_->name());
}

/*
 * SessionWith4kEnc960EncAndCopy768YUVTrackWithCrop: This test will test session
 *     with one 4k Enc, one 960p Enc and one 768p copy YUV track.
 *
 * Api test sequence:
 *  - StartCamera
 *   loop Start {
 *   ------------------
 *   - CreateSession
 *   - CreateVideoTrack - Master
 *   - CreateVideoTrack - Master
 *   - CreateVideoTrack - Copy
 *   - StartSession
 *   - StopSession
 *   - CreateVideoTrack - Copy
 *   - DeleteVideoTrack - Master
 *   - DeleteVideoTrack - Master
 *   - DeleteSession
 *   ------------------
 *   } loop End
 *  - StopCamera
 */
TEST_F(VideoGtest, SessionWith4kEnc960EncAndCopy768YUVTrackWithCrop)
{
  fprintf(stderr, "\n---------- Run Test %s.%s ------------\n",
          test_info_->test_case_name(), test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.StartCamera(camera_id_, 30);
  ASSERT_TRUE(ret == NO_ERROR);

  uint32_t video_track_id_4k_avc = 1;
  uint32_t video_track_id_960p_avc = 2;
  uint32_t video_track_id_768p_yuv_copy = 3;

  SessionCb session_status_cb = CreateSessionStatusCb();

  uint32_t session_id;
  ret = recorder_.CreateSession(session_status_cb, &session_id);
  ASSERT_TRUE(session_id > 0);
  ASSERT_TRUE(ret == NO_ERROR);

  if (dump_bitstream_.IsEnabled())
  {
    StreamDumpInfo dumpinfo1 = {
        VideoFormat::kAVC, session_id, video_track_id_4k_avc, 3840, 2160};
    ret = dump_bitstream_.SetUp(dumpinfo1);
    ASSERT_TRUE(ret == NO_ERROR);

    StreamDumpInfo dumpinfo2 = {
        VideoFormat::kAVC, session_id, video_track_id_960p_avc, 1280, 960};
    ret = dump_bitstream_.SetUp(dumpinfo2);
    ASSERT_TRUE(ret == NO_ERROR);
  }

  for (uint32_t i = 1; i <= iteration_count_; i++)
  {
    fprintf(stderr, "test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
              test_info_->name(), i);

    // Track1: 4K @30 AVC
    VideoTrackCreateParam video_track_param{camera_id_, VideoFormat::kAVC,
                                            3840,
                                            2160,
                                            30};
    TrackCb video_track_cb;
    video_track_cb.data_cb = [&, session_id](uint32_t track_id,
                             std::vector<BufferDescriptor> buffers,
                             std::vector<MetaData> meta_buffers) {
      VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
    };

    video_track_cb.event_cb = [&](uint32_t track_id, EventType event_type,
                                  void *event_data, size_t event_data_size) {
        VideoTrackEventCb(track_id, event_type, event_data, event_data_size); };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_4k_avc,
                                     video_track_param, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    std::vector<uint32_t> track_ids;
    track_ids.push_back(video_track_id_4k_avc);

    // Track2: 1280x960 @30 AVC
    video_track_param.width = 1280;
    video_track_param.height = 960;

    video_track_cb.data_cb = [&, session_id](uint32_t track_id,
                                             std::vector<BufferDescriptor> buffers,
                                             std::vector<MetaData> meta_buffers) {
      VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
    };

    video_track_cb.event_cb = [&](uint32_t track_id, EventType event_type,
                                  void *event_data, size_t event_data_size) {
        VideoTrackEventCb(track_id,event_type, event_data, event_data_size); };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_960p_avc,
                                     video_track_param, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    track_ids.push_back(video_track_id_960p_avc);

    // Track3: 1024x768 @ 30 YUV, linked track.
    VideoExtraParam extra_param;
    SourceVideoTrack surface_video_copy;
    surface_video_copy.source_track_id = video_track_id_960p_avc;
    extra_param.Update(QMMF_SOURCE_VIDEO_TRACK_ID, surface_video_copy);

    TrackCrop crop_param;
    crop_param.x = 100;
    crop_param.x = 100;
    crop_param.y = 100;
    crop_param.width = 924;
    crop_param.height = 668;
    extra_param.Update(QMMF_TRACK_CROP, crop_param);

    video_track_param.width   = 1024;
    video_track_param.height  = 768;
    video_track_param.format_type = VideoFormat::kYUV;

    video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
        std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
          VideoTrackYUVDataCb(session_id, track_id, buffers, meta_buffers);
    };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_768p_yuv_copy,
                                     video_track_param, extra_param,
                                     video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    track_ids.push_back(video_track_id_768p_yuv_copy);
    sessions_.insert(std::make_pair(session_id, track_ids));

    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    // Let session run for time record_duration_, during this time buffer with
    // valid data would be received in track callback (VideoTrackYUVDataCb).
    sleep(record_duration_);

    ret = recorder_.StopSession(session_id, false);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_768p_yuv_copy);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_960p_avc);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_4k_avc);
    ASSERT_TRUE(ret == NO_ERROR);
  }

  ret = recorder_.DeleteSession(session_id);
  ASSERT_TRUE(ret == NO_ERROR);

  ClearSessions();
  dump_bitstream_.CloseAll();

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  fprintf(stderr, "---------- Test Completed %s.%s ----------\n",
          test_info_->test_case_name(), test_info_->name());
}

/*
* SessionWith4kEncCopy1080EncAndCopy720YUV: This test will test session with
*                                          one 4kp Enc track, one Copy 1080p Enc
                                           Track and one Copy 720p.
* Api test sequence:
*  - StartCamera
*   loop Start {
*   ------------------
*   - CreateSession
*   - CreateVideoTrack - Master
*   - CreateVideoTrack - Copy
*   - CreateVideoTrack - Copy
*   - StartSession
*   - StopSession
*   - DeleteVideoTrack - Master
*   - DeleteVideoTrack - Copy
*   - DeleteVideoTrack - Copy
*   - DeleteSession
*   ------------------
*   } loop End
*  - StopCamera
*/
TEST_F(VideoGtest, SessionWith4kEncCopy1080EncAndCopy720YUV) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.StartCamera(camera_id_, 30);
  ASSERT_TRUE(ret == NO_ERROR);

  uint32_t video_track_id_4k_avc     = 1;
  uint32_t video_track_id_1080p_avc  = 2;
  uint32_t video_track_id_720p_yuv   = 3;

  for(uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
        test_info_->name(), i);

    SessionCb session_status_cb = CreateSessionStatusCb();
    uint32_t session_id;
    ret = recorder_.CreateSession(session_status_cb, &session_id);
    ASSERT_TRUE(session_id > 0);
    ASSERT_TRUE(ret == NO_ERROR);

    if (dump_bitstream_.IsEnabled()) {
      StreamDumpInfo dumpinfo1 = { VideoFormat::kAVC, session_id,
                                   video_track_id_4k_avc, 3840, 2160 };
      ret = dump_bitstream_.SetUp(dumpinfo1);
      ASSERT_TRUE(ret == NO_ERROR);

      StreamDumpInfo dumpinfo2 = { VideoFormat::kAVC, session_id,
                                   video_track_id_1080p_avc, 1920, 1080 };
      ret = dump_bitstream_.SetUp(dumpinfo2);
      ASSERT_TRUE(ret == NO_ERROR);
    }

    VideoTrackCreateParam video_track_param{camera_id_, VideoFormat::kAVC,
                                            3840, 2160, 30};
    TrackCb video_track_cb;
    video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
        std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
          VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
        };

    video_track_cb.event_cb = [&] (uint32_t track_id, EventType event_type,
        void *event_data, size_t event_data_size) { VideoTrackEventCb(track_id,
        event_type, event_data, event_data_size); };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_4k_avc,
                                     video_track_param, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    std::vector<uint32_t> track_ids;
    track_ids.push_back(video_track_id_4k_avc);

    VideoExtraParam extra_param;
    SourceVideoTrack surface_video_copy;
    surface_video_copy.source_track_id = video_track_id_4k_avc;
    extra_param.Update(QMMF_SOURCE_VIDEO_TRACK_ID, surface_video_copy);

    video_track_param.width  = 1920;
    video_track_param.height = 1080;

    video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
        std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
          VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
    };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_1080p_avc,
                                     video_track_param, extra_param,
                                     video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    track_ids.push_back(video_track_id_1080p_avc);

    VideoExtraParam extra_param2;
    SourceVideoTrack surface_video_copy2;
    surface_video_copy2.source_track_id = video_track_id_4k_avc;
    extra_param2.Update(QMMF_SOURCE_VIDEO_TRACK_ID, surface_video_copy2);

    video_track_param.width   = 1280;
    video_track_param.height  = 720;
    video_track_param.format_type = VideoFormat::kYUV;

    video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
        std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
          VideoTrackYUVDataCb(session_id, track_id, buffers, meta_buffers); };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_720p_yuv,
                                     video_track_param, extra_param2,
                                     video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    track_ids.push_back(video_track_id_720p_yuv);
    sessions_.insert(std::make_pair(session_id, track_ids));

    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    // Let session run for time record_duration_, during this time buffer with
    // valid data would be received in track callback (VideoTrackYUVDataCb).
    sleep(record_duration_);

    ret = recorder_.StopSession(session_id, false);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_720p_yuv);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_1080p_avc);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_4k_avc);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    ClearSessions();
  }

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  dump_bitstream_.CloseAll();
  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());
}

/*
* SessionWith4kEncCopy1080EncAndLinked1080YUV: This test will test session with
*                                          one 4kp Enc track, one Copy 1080 Enc
                                           Track and one linked.
* Api test sequence:
*  - StartCamera
*   loop Start {
*   ------------------
*   - CreateSession
*   - CreateVideoTrack - Master
*   - CreateVideoTrack - Copy
*   - CreateVideoTrack - Linked
*   - StartSession
*   - StopSession
*   - DeleteVideoTrack - Linked
*   - DeleteVideoTrack - Copy
*   - DeleteVideoTrack - Master
*   - DeleteSession
*   ------------------
*   } loop End
*  - StopCamera
*/
TEST_F(VideoGtest, SessionWith4kEncCopy1080EncAndLinked1080YUV) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.StartCamera(camera_id_, 30);
  ASSERT_TRUE(ret == NO_ERROR);

  uint32_t video_track_id_4k_avc     = 1;
  uint32_t video_track_id_1080p_avc  = 2;
  uint32_t video_track_id_1080p_yuv  = 3;

  for(uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
        test_info_->name(), i);

    SessionCb session_status_cb = CreateSessionStatusCb();
    uint32_t session_id;
    ret = recorder_.CreateSession(session_status_cb, &session_id);
    ASSERT_TRUE(session_id > 0);
    ASSERT_TRUE(ret == NO_ERROR);

    if (dump_bitstream_.IsEnabled()) {
      StreamDumpInfo dumpinfo1 = { VideoFormat::kAVC, session_id,
                                   video_track_id_4k_avc, 3840, 2160 };
      ret = dump_bitstream_.SetUp(dumpinfo1);
      ASSERT_TRUE(ret == NO_ERROR);

      StreamDumpInfo dumpinfo2 = { VideoFormat::kAVC, session_id,
                                   video_track_id_1080p_avc, 1920, 1080 };
      ret = dump_bitstream_.SetUp(dumpinfo2);
      ASSERT_TRUE(ret == NO_ERROR);
    }

    VideoTrackCreateParam video_track_param{camera_id_, VideoFormat::kAVC,
                                            3840, 2160, 30};
    TrackCb video_track_cb;
    video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
        std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
          VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
        };

    video_track_cb.event_cb = [&] (uint32_t track_id, EventType event_type,
        void *event_data, size_t event_data_size) { VideoTrackEventCb(track_id,
        event_type, event_data, event_data_size); };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_4k_avc,
                                     video_track_param, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    std::vector<uint32_t> track_ids;
    track_ids.push_back(video_track_id_4k_avc);

    VideoExtraParam extra_param;
    SourceVideoTrack surface_video_copy;
    surface_video_copy.source_track_id = video_track_id_4k_avc;
    extra_param.Update(QMMF_SOURCE_VIDEO_TRACK_ID, surface_video_copy);

    video_track_param.width  = 1920;
    video_track_param.height = 1080;

    video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
        std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
          VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
    };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_1080p_avc,
                                     video_track_param, extra_param,
                                     video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    track_ids.push_back(video_track_id_1080p_avc);

    VideoExtraParam extra_param2;
    SourceVideoTrack surface_video_linked;
    surface_video_linked.source_track_id = video_track_id_1080p_avc;
    extra_param2.Update(QMMF_SOURCE_VIDEO_TRACK_ID, surface_video_linked);

    video_track_param.width   = 1920;
    video_track_param.height  = 1080;
    video_track_param.format_type = VideoFormat::kYUV;

    video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
        std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
          VideoTrackYUVDataCb(session_id, track_id, buffers, meta_buffers); };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_1080p_yuv,
                                     video_track_param, extra_param2,
                                     video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    track_ids.push_back(video_track_id_1080p_yuv);
    sessions_.insert(std::make_pair(session_id, track_ids));

    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    // Let session run for time record_duration_, during this time buffer with
    // valid data would be received in track callback (VideoTrackYUVDataCb).
    sleep(record_duration_);

    ret = recorder_.StopSession(session_id, false);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_1080p_yuv);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_1080p_avc);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_4k_avc);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    ClearSessions();
    dump_bitstream_.CloseAll();
  }

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());
}

/*
 * SessionWith1440pEncCopy480EncAndLinked480YUV: This test will test session with
 *                                          one 1440 Enc track, one Copy 480 Enc
 *                                          Track and one 480 linked.
 * Api test sequence:
 *  - StartCamera
 *   loop Start {
 *   ------------------
 *   - CreateSession
 *   - CreateVideoTrack - Master
 *   - CreateVideoTrack - Copy
 *   - CreateVideoTrack - Linked
 *   - StartSession
 *   - StopSession
 *   - DeleteVideoTrack - Linked
 *   - DeleteVideoTrack - Copy
 *   - DeleteVideoTrack - Master
 *   - DeleteSession
 *   ------------------
 *   } loop End
 *  - StopCamera
 */
TEST_F(VideoGtest, SessionWith1440pEncCopy480EncAndLinked480YUV) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.StartCamera(camera_id_, 30);
  ASSERT_TRUE(ret == NO_ERROR);

  uint32_t video_track_id_1440p_avc = 1;
  uint32_t video_track_id_480p_avc  = 2;
  uint32_t video_track_id_480p_yuv  = 3;

  SessionCb session_status_cb = CreateSessionStatusCb();

  uint32_t session_id;
  ret = recorder_.CreateSession(session_status_cb, &session_id);
  ASSERT_TRUE(session_id > 0);
  ASSERT_TRUE(ret == NO_ERROR);

  if (dump_bitstream_.IsEnabled()) {
    StreamDumpInfo dumpinfo1 = {
      VideoFormat::kAVC,
      session_id,
      video_track_id_1440p_avc, 1920, 1440
    };
    ret = dump_bitstream_.SetUp(dumpinfo1);
    ASSERT_TRUE(ret == NO_ERROR);

    StreamDumpInfo dumpinfo2 = {
      VideoFormat::kAVC,
      session_id,
      video_track_id_480p_avc, 848, 480
    };
    ret = dump_bitstream_.SetUp(dumpinfo2);
    ASSERT_TRUE(ret == NO_ERROR);
  }

  for(uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
        test_info_->name(), i);

    VideoTrackCreateParam video_track_param{camera_id_, VideoFormat::kAVC,
                                            1920,
                                            1440,
                                            30};
    TrackCb video_track_cb;
    video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
        std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
          VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
        };

    video_track_cb.event_cb = [&] (uint32_t track_id, EventType event_type,
        void *event_data, size_t event_data_size) { VideoTrackEventCb(track_id,
        event_type, event_data, event_data_size); };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_1440p_avc,
                                     video_track_param, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    std::vector<uint32_t> track_ids;
    track_ids.push_back(video_track_id_1440p_avc);

    VideoExtraParam extra_param;
    SourceVideoTrack surface_video_copy;
    surface_video_copy.source_track_id = video_track_id_1440p_avc;
    extra_param.Update(QMMF_SOURCE_VIDEO_TRACK_ID, surface_video_copy);

    video_track_param.width  = 848;
    video_track_param.height = 480;

    video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
        std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
          VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
    };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_480p_avc,
                                     video_track_param, extra_param,
                                     video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    track_ids.push_back(video_track_id_480p_avc);

    VideoExtraParam extra_param2;
    SourceVideoTrack surface_video_linked;
    surface_video_linked.source_track_id = video_track_id_480p_avc;
    extra_param2.Update(QMMF_SOURCE_VIDEO_TRACK_ID, surface_video_linked);

    video_track_param.width   = 848;
    video_track_param.height  = 480;
    video_track_param.format_type = VideoFormat::kYUV;

    video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
        std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
          VideoTrackYUVDataCb(session_id, track_id, buffers, meta_buffers);
    };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_480p_yuv,
                                     video_track_param, extra_param2,
                                     video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    track_ids.push_back(video_track_id_480p_yuv);
    sessions_.insert(std::make_pair(session_id, track_ids));

    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    // Let session run for time record_duration_, during this time buffer with
    // valid data would be received in track callback (VideoTrackYUVDataCb).
    sleep(record_duration_);

    ret = recorder_.StopSession(session_id, false);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_480p_yuv);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_480p_avc);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_1440p_avc);
    ASSERT_TRUE(ret == NO_ERROR);
  }

  ret = recorder_.DeleteSession(session_id);
  ASSERT_TRUE(ret == NO_ERROR);

  ClearSessions();
  dump_bitstream_.CloseAll();

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());
}

/*
* SessionWith4kEnc960EncAndLinked960YUVTrack: This test will test session with
*                                             one 4k Enc, one 960p Enc and one
*                                             960p linked YUV track.
* Api test sequence:
*  - StartCamera
*   loop Start {
*   ------------------
*   - CreateSession
*   - CreateVideoTrack - Master
*   - CreateVideoTrack - Master
*   - CreateVideoTrack - Copy
*   - StartSession
*   - StopSession
*   - CreateVideoTrack - Copy
*   - DeleteVideoTrack - Master
*   - DeleteVideoTrack - Master
*   - DeleteSession
*   ------------------
*   } loop End
*  - StopCamera
*/
TEST_F(VideoGtest, SessionWith4kEnc960EncAndLinked960YUVTrack) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.StartCamera(camera_id_, 30);
  ASSERT_TRUE(ret == NO_ERROR);

  uint32_t video_track_id_4k_avc   = 1;
  uint32_t video_track_id_960p_avc = 2;
  uint32_t video_track_id_960p_yuv_linked = 3;

  for(uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
        test_info_->name(), i);

    SessionCb session_status_cb = CreateSessionStatusCb();
    uint32_t session_id;
    ret = recorder_.CreateSession(session_status_cb, &session_id);
    ASSERT_TRUE(session_id > 0);
    ASSERT_TRUE(ret == NO_ERROR);

    if (dump_bitstream_.IsEnabled()) {
      StreamDumpInfo dumpinfo1 = { VideoFormat::kAVC, session_id,
                                   video_track_id_4k_avc, 3840, 2160 };
      ret = dump_bitstream_.SetUp(dumpinfo1);
      ASSERT_TRUE(ret == NO_ERROR);

      StreamDumpInfo dumpinfo2 = { VideoFormat::kAVC, session_id,
                                   video_track_id_960p_avc, 1280, 960 };
      ret = dump_bitstream_.SetUp(dumpinfo2);
      ASSERT_TRUE(ret == NO_ERROR);
    }

    // Track1: 4K @30 AVC
    VideoTrackCreateParam video_track_param{camera_id_, VideoFormat::kAVC,
                                            3840, 2160, 30};
    TrackCb video_track_cb;
    video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
        std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
          VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
        };

    video_track_cb.event_cb = [&] (uint32_t track_id, EventType event_type,
        void *event_data, size_t event_data_size) { VideoTrackEventCb(track_id,
        event_type, event_data, event_data_size); };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_4k_avc,
                                     video_track_param, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    std::vector<uint32_t> track_ids;
    track_ids.push_back(video_track_id_4k_avc);

    // Track2: 1280x960 @30 AVC
    video_track_param.width   = 1280;
    video_track_param.height  = 960;

    video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
        std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
          VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
    };

    video_track_cb.event_cb = [&] (uint32_t track_id, EventType event_type,
        void *event_data, size_t event_data_size) { VideoTrackEventCb(track_id,
        event_type, event_data, event_data_size); };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_960p_avc,
                                     video_track_param, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    track_ids.push_back(video_track_id_960p_avc);

    // Track3: 1280x960 @ 30 YUV, linked track.
    VideoExtraParam extra_param;
    SourceVideoTrack surface_video_copy;
    surface_video_copy.source_track_id = video_track_id_960p_avc;
    extra_param.Update(QMMF_SOURCE_VIDEO_TRACK_ID, surface_video_copy);

    video_track_param.width   = 1280;
    video_track_param.height  = 960;
    video_track_param.format_type = VideoFormat::kYUV;

    video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
        std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
          VideoTrackYUVDataCb(session_id, track_id, buffers, meta_buffers); };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_960p_yuv_linked,
                                     video_track_param, extra_param,
                                     video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    track_ids.push_back(video_track_id_960p_yuv_linked);
    sessions_.insert(std::make_pair(session_id, track_ids));

    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    // Let session run for time record_duration_, during this time buffer with
    // valid data would be received in track callback (VideoTrackYUVDataCb).
    sleep(record_duration_);

    ret = recorder_.StopSession(session_id, false);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_960p_yuv_linked);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_960p_avc);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_4k_avc);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    ClearSessions();
    dump_bitstream_.CloseAll();
  }

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());
}

/*
* SessionWith4kEncCopy720EncAndLinked720Enc: This test will test session with
*  one 720p Enc track, and one linked Enc track.
* Api test sequence:
*  - StartCamera
*   loop Start {
*   ------------------
*   - CreateSession
*   - CreateVideoTrack - Master
*   - CreateVideoTrack - Linked
*   - StartSession
*   - StopSession
*   - DeleteVideoTrack - Linked
*   - DeleteVideoTrack - Master
*   - DeleteSession
*   ------------------
*   } loop End
*  - StopCamera
*/
TEST_F(VideoGtest, SessionWith720EncAndLinked720Enc) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);
  float fps = 120;

  ret = recorder_.StartCamera(camera_id_, fps);
  ASSERT_TRUE(ret == NO_ERROR);

  uint32_t video_track_id_720p_HFR_avc = 1;
  uint32_t video_track_id_720p_avc     = 2;

  for(uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
        test_info_->name(), i);

    SessionCb session_status_cb = CreateSessionStatusCb();
    uint32_t session_id;
    ret = recorder_.CreateSession(session_status_cb, &session_id);
    ASSERT_TRUE(session_id > 0);
    ASSERT_TRUE(ret == NO_ERROR);

    if (dump_bitstream_.IsEnabled()) {
      StreamDumpInfo dumpinfo1 = { VideoFormat::kAVC, session_id,
                                    video_track_id_720p_HFR_avc, 1280, 720 };
      ret = dump_bitstream_.SetUp(dumpinfo1);
      ASSERT_TRUE(ret == NO_ERROR);

      StreamDumpInfo dumpinfo2 = { VideoFormat::kAVC, session_id,
                                   video_track_id_720p_avc, 1280, 720 };
      ret = dump_bitstream_.SetUp(dumpinfo2);
      ASSERT_TRUE(ret == NO_ERROR);
    }

    VideoTrackCreateParam video_track_param{camera_id_, VideoFormat::kAVC,
                                            1280, 720, fps };
    if (ubwc_stream_enable_) {
      video_track_param.low_power_mode = true;
    }
    TrackCb video_track_cb;
    video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
        std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
          VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
        };

    video_track_cb.event_cb = [&] (uint32_t track_id, EventType event_type,
        void *event_data, size_t event_data_size) { VideoTrackEventCb(track_id,
        event_type, event_data, event_data_size); };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_720p_HFR_avc,
                                     video_track_param, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    std::vector<uint32_t> track_ids;
    track_ids.push_back(video_track_id_720p_HFR_avc);

    VideoExtraParam extra_param;
    SourceVideoTrack surface_video_copy;
    surface_video_copy.source_track_id = video_track_id_720p_HFR_avc;
    extra_param.Update(QMMF_SOURCE_VIDEO_TRACK_ID, surface_video_copy);

    video_track_param.format_type = VideoFormat::kAVC;
    video_track_param.width       = 1280;
    video_track_param.height      = 720;
    video_track_param.frame_rate  = 30.0;

    video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
        std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
            VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
    };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_720p_avc,
                                     video_track_param, extra_param,
                                     video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    track_ids.push_back(video_track_id_720p_avc);
    sessions_.insert(std::make_pair(session_id, track_ids));

    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    // Let session run for time record_duration_, during this time buffer with
    // valid data would be received in track callback (VideoTrackYUVDataCb).
    sleep(record_duration_);

    ret = recorder_.StopSession(session_id, false);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_720p_avc);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_720p_HFR_avc);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    ClearSessions();
    dump_bitstream_.CloseAll();
  }

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());
}

/*
 * SessionWith4kEncAndCopy1080RGB: This test will test session with
 *                             one 4kp Enc track, one Copy 1080 RGB
 *                             Track.
 * Api test sequence:
 *  - StartCamera
 *   loop Start {
 *   ------------------
 *   - CreateSession
 *   - CreateVideoTrack - Master
 *   - CreateVideoTrack - Copy
 *   - StartSession
 *   - StopSession
 *   - DeleteVideoTrack - Copy
 *   - DeleteVideoTrack - Master
 *   - DeleteSession
 *   ------------------
 *   } loop End
 *  - StopCamera
 */
TEST_F(VideoGtest, SessionWith4kEncAndCopy1080RGB) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.StartCamera(camera_id_, 30);
  ASSERT_TRUE(ret == NO_ERROR);

  uint32_t video_track_id_4k_avc     = 1;
  uint32_t video_track_id_1080p_rgb  = 2;
  property_set("persist.qmmf.rescaler.type", "C2D");

  SessionCb session_status_cb = CreateSessionStatusCb();

  uint32_t session_id;
  ret = recorder_.CreateSession(session_status_cb, &session_id);
  ASSERT_TRUE(session_id > 0);
  ASSERT_TRUE(ret == NO_ERROR);

  if (dump_bitstream_.IsEnabled()) {
    StreamDumpInfo dumpinfo1 = {
      VideoFormat::kAVC,
      session_id,
      video_track_id_4k_avc, 3840, 2160
    };
    ret = dump_bitstream_.SetUp(dumpinfo1);
    ASSERT_TRUE(ret == NO_ERROR);
  }

  for(uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
        test_info_->name(), i);

    VideoTrackCreateParam video_track_param{camera_id_, VideoFormat::kAVC,
                                            3840,
                                            2160,
                                            30};
    TrackCb video_track_cb;
    video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
        std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
          VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
        };

    video_track_cb.event_cb = [&] (uint32_t track_id, EventType event_type,
        void *event_data, size_t event_data_size) { VideoTrackEventCb(track_id,
        event_type, event_data, event_data_size); };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_4k_avc,
                                     video_track_param, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    std::vector<uint32_t> track_ids;
    track_ids.push_back(video_track_id_4k_avc);

    VideoExtraParam extra_param2;
    SourceVideoTrack surface_video_linked;
    surface_video_linked.source_track_id = video_track_id_4k_avc;
    extra_param2.Update(QMMF_SOURCE_VIDEO_TRACK_ID, surface_video_linked);

    video_track_param.width   = 1920;
    video_track_param.height  = 1080;
    video_track_param.format_type = VideoFormat::kRGB;

    video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
        std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
          VideoTrackRGBDataCb(session_id, track_id, buffers, meta_buffers);
    };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_1080p_rgb,
                                     video_track_param, extra_param2,
                                     video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    track_ids.push_back(video_track_id_1080p_rgb);
    sessions_.insert(std::make_pair(session_id, track_ids));

    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    // Let session run for time record_duration_, during this time buffer with
    // valid data would be received in track callback (VideoTrackYUVDataCb).
    sleep(record_duration_);

    ret = recorder_.StopSession(session_id, false);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_1080p_rgb);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_4k_avc);
    ASSERT_TRUE(ret == NO_ERROR);
  }
  property_set("persist.qmmf.rescaler.type", "Neon");

  ret = recorder_.DeleteSession(session_id);
  ASSERT_TRUE(ret == NO_ERROR);

  ClearSessions();
  dump_bitstream_.CloseAll();

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());
}


/*
* TimeLapse1080pEncTrack: This test will test session with 1080p h264 track in
*                         timelapse mode.
* API test sequence:
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
TEST_F(VideoGtest, TimeLapse1080pEncTrack) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  VideoFormat format_type = VideoFormat::kAVC;
  uint32_t width  = 1920;
  uint32_t height = 1080;

  ret = recorder_.StartCamera(camera_id_, 30);
  ASSERT_TRUE(ret == NO_ERROR);

  for(uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
        test_info_->name(), i);

    SessionCb session_status_cb = CreateSessionStatusCb();
    uint32_t session_id;
    ret = recorder_.CreateSession(session_status_cb, &session_id);
    ASSERT_TRUE(session_id > 0);
    ASSERT_TRUE(ret == NO_ERROR);
    VideoTrackCreateParam video_track_param{camera_id_, format_type,
                                            width, height, 30};
    uint32_t video_track_id = 1;

    if (dump_bitstream_.IsEnabled()) {
      StreamDumpInfo dumpinfo = { format_type, session_id, video_track_id,
                                  width, height };
      ret = dump_bitstream_.SetUp(dumpinfo);
      ASSERT_TRUE(ret == NO_ERROR);
    }

    TrackCb video_track_cb;
    video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
      std::vector<BufferDescriptor> buffers,
      std::vector<MetaData> meta_buffers) {
        VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
      };

    video_track_cb.event_cb = [&] (uint32_t track_id, EventType event_type,
        void *event_data, size_t event_data_size) { VideoTrackEventCb(track_id,
        event_type, event_data, event_data_size); };

    VideoExtraParam extra_param;
    VideoTimeLapse timelapse;
    timelapse.time_interval = 33; //ms
    extra_param.Update(QMMF_VIDEO_TIMELAPSE_INTERVAL, timelapse);

    ret = recorder_.CreateVideoTrack(session_id, video_track_id,
                                     video_track_param, extra_param,
                                     video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    std::vector<uint32_t> track_ids;
    track_ids.push_back(video_track_id);
    sessions_.insert(std::make_pair(session_id, track_ids));

    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    float fps = 4.0;
    CodecParamType param_type;
    param_type = CodecParamType::kFrameRateType;
    ret = recorder_.SetVideoTrackParam(session_id, 1, param_type, &fps , sizeof(fps));

    // Let session run for record_duration_, during this time buffer with valid
    // data would be received in track callback (VideoTrackDataCb).
    sleep(record_duration_);

    ret = recorder_.StopSession(session_id, false);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    dump_bitstream_.CloseAll();
  }
  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());

}

/*
* SessionWith1440EncAndLinked1440pEncAndLinked1440pYUVTrack: This test will test
*                                   one session with one 1440p Enc track,
*                                   linked 1440p Enc and linked 1440p YUV track.
* Api test sequence:
*  - StartCamera
*   loop Start {
*   ------------------
*   - CreateSession
*   - CreateVideoTrack - Master
*   - CreateVideoTrack - Linked
*   - CreateVideoTrack - Linked
*   - StartSession
*   - StopSession
*   - DeleteVideoTrack - Linked
*   - DeleteVideoTrack - Linked
*   - DeleteVideoTrack - Master
*   - DeleteSession
*   ------------------
*   } loop End
*  - StopCamera
*/
TEST_F(VideoGtest, SessionWith1440EncAndLinked1440pEncAndLinked1440pYUVTrack) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.StartCamera(camera_id_, 30);
  ASSERT_TRUE(ret == NO_ERROR);

  uint32_t video_track_id_1440p_avc1  = 1;
  uint32_t video_track_id_1440p_avc2  = 2;
  uint32_t video_track_id_1440p_yuv   = 3;

  for(uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
        test_info_->name(), i);

    SessionCb session_status_cb = CreateSessionStatusCb();
    uint32_t session_id;
    ret = recorder_.CreateSession(session_status_cb, &session_id);
    ASSERT_TRUE(session_id > 0);
    ASSERT_TRUE(ret == NO_ERROR);
    if (dump_bitstream_.IsEnabled()) {
      StreamDumpInfo dumpinfo1 = { VideoFormat::kAVC, session_id,
                                   video_track_id_1440p_avc1, 3840, 2160 };
      ret = dump_bitstream_.SetUp(dumpinfo1);
      ASSERT_TRUE(ret == NO_ERROR);

      StreamDumpInfo dumpinfo2 = { VideoFormat::kAVC, session_id,
                                   video_track_id_1440p_avc2, 1920, 1080 };
      ret = dump_bitstream_.SetUp(dumpinfo2);
      ASSERT_TRUE(ret == NO_ERROR);
    }
    VideoTrackCreateParam video_track_param{camera_id_, VideoFormat::kAVC,
                                            1920, 1440, 30};
    TrackCb video_track_cb;
    video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
        std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
          VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
        };

    video_track_cb.event_cb = [&] (uint32_t track_id, EventType event_type,
        void *event_data, size_t event_data_size) { VideoTrackEventCb(track_id,
        event_type, event_data, event_data_size); };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_1440p_avc1,
                                     video_track_param, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    std::vector<uint32_t> track_ids;
    track_ids.push_back(video_track_id_1440p_avc1);

    VideoExtraParam extra_param;
    SourceVideoTrack surface_video_copy;
    surface_video_copy.source_track_id = video_track_id_1440p_avc1;
    extra_param.Update(QMMF_SOURCE_VIDEO_TRACK_ID, surface_video_copy);

    video_track_param.width  = 1920;
    video_track_param.height = 1440;

    video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
        std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
          VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
    };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_1440p_avc2,
                                     video_track_param, extra_param,
                                     video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    track_ids.push_back(video_track_id_1440p_avc2);

    VideoExtraParam extra_param2;
    SourceVideoTrack surface_video_linked;
    surface_video_linked.source_track_id = video_track_id_1440p_avc2;
    extra_param2.Update(QMMF_SOURCE_VIDEO_TRACK_ID, surface_video_linked);

    video_track_param.width   = 1920;
    video_track_param.height  = 1440;
    video_track_param.format_type = VideoFormat::kYUV;

    video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
        std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
          VideoTrackYUVDataCb(session_id, track_id, buffers, meta_buffers); };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_1440p_yuv,
                                     video_track_param, extra_param2,
                                     video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    track_ids.push_back(video_track_id_1440p_yuv);
    sessions_.insert(std::make_pair(session_id, track_ids));

    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    // Let session run for time record_duration_, during this time buffer with
    // valid data would be received in track callback (VideoTrackYUVDataCb).
    sleep(record_duration_);

    ret = recorder_.StopSession(session_id, false);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_1440p_yuv);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_1440p_avc2);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_1440p_avc1);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    ClearSessions();
    dump_bitstream_.CloseAll();
  }

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());
}

#ifdef USE_SURFACEFLINGER
/*
* Session4kYUVTrackWithDisplay: This test will be used to test display
* functionality. This test will create session with 4k YUV track and
* push received YUV cb frames to display.
* API test sequence:
*  - StartCamera
*   loop Start {
*   ------------------
*   - CreateSession
*   - CreateVideoTrack
*   - StartStream
*   - StartVideoTrack
*   - StopSession
*   - StopStream
*   - StartVideoTrack
*   - StopSession
*   - DeleteVideoTrack
*   - DeleteSession
*   ------------------
*   } loop End
*  - StopCamera
*/
TEST_F(VideoGtest, Session4kYUVTrackWithDisplay) {
  fprintf(stderr, "\n---------- Run Test %s.%s ------------\n",
          test_info_->test_case_name(), test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  use_sf_ = true;
  uint32_t stream_width  = FHD_1080p_STREAM_WIDTH*2;
  uint32_t stream_height = FHD_1080p_STREAM_HEIGHT*2;

  ret = recorder_.StartCamera(camera_id_, 30);
  ASSERT_TRUE(ret == NO_ERROR);

  for (uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr, "test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
              test_info_->name(), i);

    SessionCb session_status_cb = CreateSessionStatusCb();
    uint32_t session_id;

    ret = recorder_.CreateSession(session_status_cb, &session_id);
    ASSERT_TRUE(session_id > 0);
    ASSERT_TRUE(ret == NO_ERROR);

    VideoTrackCreateParam video_track_param{camera_id_, VideoFormat::kYUV,
                                            stream_width, stream_height, 30};

    uint32_t video_track_id_1 = 1;

    TrackCb video_track_cb;
    video_track_cb.data_cb = [&, session_id](
        uint32_t track_id, std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
      VideoTrackYUVDataCb(session_id, track_id, buffers, meta_buffers); };

    video_track_cb.event_cb = [&](uint32_t track_id, EventType event_type,
                                  void *event_data, size_t event_data_size) {
      VideoTrackEventCb(track_id, event_type, event_data, event_data_size);
    };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_1,
                                     video_track_param, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    sfdisplay_ = new SFDisplaySink(stream_width, stream_height);
    if (nullptr == sfdisplay_) {
      TEST_ERROR("%s: Failed to create SFDisplaySink", __func__);
      use_sf_ = false;
    }

    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    sleep(record_duration_);

    ret = recorder_.StopSession(session_id, false);
    ASSERT_TRUE(ret == NO_ERROR);

    if(use_sf_) {
      delete sfdisplay_;
      sfdisplay_ = nullptr;
    }

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_1);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    ClearSessions();
  }

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  fprintf(stderr, "---------- Test Completed %s.%s ----------\n",
          test_info_->test_case_name(), test_info_->name());
}
#endif

/*
* Session1080pYUVTrackWithDisplay: This test will be used to test display
* functionality. This test will create session with 1080p YUV track and
* push received YUV cb frames to display.
* API test sequence:
*  - StartCamera
*   loop Start {
*   ------------------
*   - CreateSession
*   - CreateVideoTrack
*   - StartDisplay
*   - StartVideoTrack
*   - StopSession
*   - StopDisplay
*   - StartVideoTrack
*   - StopSession
*   - DeleteVideoTrack
*   - DeleteSession
*   ------------------
*   } loop End
*  - StopCamera
*/
TEST_F(VideoGtest, Session1080pYUVTrackWithDisplay) {
  fprintf(stderr, "\n---------- Run Test %s.%s ------------\n",
          test_info_->test_case_name(), test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  use_display_ = true;
  uint32_t stream_width = FHD_1080p_STREAM_WIDTH;
  uint32_t stream_height = FHD_1080p_STREAM_HEIGHT;

  ret = recorder_.StartCamera(camera_id_, 30);
  ASSERT_TRUE(ret == NO_ERROR);

  for (uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr, "test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
              test_info_->name(), i);
    SessionCb session_status_cb = CreateSessionStatusCb();
    uint32_t session_id;

    ret = recorder_.CreateSession(session_status_cb, &session_id);
    ASSERT_TRUE(session_id > 0);
    ASSERT_TRUE(ret == NO_ERROR);

    VideoTrackCreateParam video_track_param{camera_id_, VideoFormat::kYUV,
                                            stream_width, stream_height, 30};

    uint32_t video_track_id_1 = 1;

    TrackCb video_track_cb;
    video_track_cb.data_cb = [&, session_id](
        uint32_t track_id, std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
      VideoTrackYUVDataCb(session_id, track_id, buffers, meta_buffers); };

    video_track_cb.event_cb = [&](uint32_t track_id, EventType event_type,
                                  void *event_data, size_t event_data_size) {
      VideoTrackEventCb(track_id, event_type, event_data, event_data_size);
    };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_1,
                                     video_track_param, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = StartDisplay(DisplayType::kPrimary, stream_width, stream_height,
                       stream_width, stream_height);
    if (ret != 0) {
      TEST_ERROR("%s StartDisplay Failed!!", __func__);
    }

    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    sleep(record_duration_);

    ret = recorder_.StopSession(session_id, false);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = StopDisplay(DisplayType::kPrimary);
    if (ret != 0) {
      TEST_ERROR("%s StopDisplay Failed!!", __func__);
    }

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_1);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    ClearSessions();
  }

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);
  fprintf(stderr, "---------- Test Completed %s.%s ----------\n",
          test_info_->test_case_name(), test_info_->name());
}

/*
* SessionWith960p90FPSEncCopy480pEncAndLinked480pYUVTrack:
*                                          This test will test session with
*                                          one 960p90 Enc track, one Copy 480
*                                          Enc Track and one linked.
* Api test sequence:
*  - StartCamera
*   loop Start {
*   ------------------
*   - CreateSession
*   - CreateVideoTrack - Master
*   - CreateVideoTrack - Copy
*   - CreateVideoTrack - Linked
*   - StartSession
*   - StopSession
*   - DeleteVideoTrack - Linked
*   - DeleteVideoTrack - Copy
*   - DeleteVideoTrack - Master
*   - DeleteSession
*   ------------------
*   } loop End
*  - StopCamera
*/
TEST_F(VideoGtest, SessionWith960p90FPSEncCopy480pEncAndLinked480pYUVTrack) {
  fprintf(stderr, "\n---------- Run Test %s.%s ------------\n",
          test_info_->test_case_name(), test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.StartCamera(camera_id_, 90);
  ASSERT_TRUE(ret == NO_ERROR);

  uint32_t video_track_id_960p_avc = 1;
  uint32_t video_track_id_480p_avc = 2;
  uint32_t video_track_id_480p_yuv = 3;

  CameraMetadata meta;

  for (uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr, "test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
              test_info_->name(), i);
    uint32_t width = 1280;
    uint32_t height = 960;

    SessionCb session_status_cb = CreateSessionStatusCb();
    uint32_t session_id;
    ret = recorder_.CreateSession(session_status_cb, &session_id);
    ASSERT_TRUE(session_id > 0);
    ASSERT_TRUE(ret == NO_ERROR);

    if (dump_bitstream_.IsEnabled()) {
      StreamDumpInfo dumpinfo1 = {VideoFormat::kAVC, session_id, video_track_id_960p_avc,
                                  width, height};
      ret = dump_bitstream_.SetUp(dumpinfo1);
      ASSERT_TRUE(ret == NO_ERROR);

      StreamDumpInfo dumpinfo2 = {VideoFormat::kAVC, session_id, video_track_id_480p_avc,
                                  640, 480};
      ret = dump_bitstream_.SetUp(dumpinfo2);
      ASSERT_TRUE(ret == NO_ERROR);
    }

    VideoTrackCreateParam video_track_param{camera_id_, VideoFormat::kAVC,
                                            width, height, 90};

    video_track_param.codec_param.avc.bitrate = kBitRate960p90;
    video_track_param.codec_param.avc.ratecontrol_type =
        VideoRateControlType::kVariable;

    TrackCb video_track_cb;
    video_track_cb.data_cb = [&, session_id](
        uint32_t track_id, std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
      VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
    };

    video_track_cb.event_cb = [&](uint32_t track_id, EventType event_type,
                                  void *event_data, size_t event_data_size) {
      VideoTrackEventCb(track_id, event_type, event_data, event_data_size);
    };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_960p_avc,
                                     video_track_param, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    std::vector<uint32_t> track_ids;
    track_ids.push_back(video_track_id_960p_avc);

    VideoExtraParam extra_param;
    SourceVideoTrack surface_video_copy;
    surface_video_copy.source_track_id = video_track_id_960p_avc;
    extra_param.Update(QMMF_SOURCE_VIDEO_TRACK_ID, surface_video_copy);

    width = 640;
    height = 480;
    video_track_param.width = width;
    video_track_param.height = height;
    video_track_param.frame_rate = 90;
    video_track_param.codec_param.avc.bitrate = kBitRate480p;

    video_track_cb.data_cb = [&, session_id](
        uint32_t track_id, std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
      VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
    };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_480p_avc,
                                     video_track_param, extra_param,
                                     video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    track_ids.push_back(video_track_id_480p_avc);

    VideoExtraParam extra_param2;
    SourceVideoTrack surface_video_linked;
    surface_video_linked.source_track_id = video_track_id_480p_avc;
    extra_param2.Update(QMMF_SOURCE_VIDEO_TRACK_ID, surface_video_linked);

    video_track_param.width = width;
    video_track_param.height = height;
    video_track_param.format_type = VideoFormat::kYUV;
    video_track_param.frame_rate = 30;

    video_track_cb.data_cb = [&, session_id](
        uint32_t track_id, std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
      VideoTrackYUVDataCb(session_id, track_id, buffers, meta_buffers);
    };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_480p_yuv,
                                     video_track_param, extra_param2,
                                     video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    track_ids.push_back(video_track_id_480p_yuv);
    sessions_.insert(std::make_pair(session_id, track_ids));

    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    // Let session run for time record_duration_, during this time buffer with
    // valid data would be received in track callback (VideoTrackYUVDataCb).
    sleep(record_duration_);

    ret = recorder_.StopSession(session_id, false);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_480p_yuv);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_480p_avc);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_960p_avc);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    ClearSessions();
    dump_bitstream_.CloseAll();
  }

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  fprintf(stderr, "---------- Test Completed %s.%s ----------\n",
          test_info_->test_case_name(), test_info_->name());
}

/*
* SessionWith960p90FPSEncCopy480pEncAndLinked480pYUVTrackEISEnabled:
*                                          This test will test session with
*                                          one 960p90 Enc track, one Copy 480
*                                          Enc Track and one linked.
* Api test sequence:
*  - StartCamera
*   loop Start {
*   ------------------
*   - CreateSession
*   - CreateVideoTrack - Master
*   - CreateVideoTrack - Copy
*   - CreateVideoTrack - Linked
*   - StartSession
*   - StopSession
*   - DeleteVideoTrack - Linked
*   - DeleteVideoTrack - Copy
*   - DeleteVideoTrack - Master
*   - DeleteSession
*   ------------------
*   } loop End
*  - StopCamera
*/
TEST_F(VideoGtest,
       SessionWith960p90FPSEncCopy480pEncAndLinked480pYUVTrackEISEnabled) {
  fprintf(stderr, "\n---------- Run Test %s.%s ------------\n",
          test_info_->test_case_name(), test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.StartCamera(camera_id_, 90);
  ASSERT_TRUE(ret == NO_ERROR);

  uint32_t video_track_id_960p_avc = 1;
  uint32_t video_track_id_480p_avc = 2;
  uint32_t video_track_id_480p_yuv = 3;

  CameraMetadata meta;
  uint8_t vstab_mode;

  for (uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr, "test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
              test_info_->name(), i);
    uint32_t width = 1280;
    uint32_t height = 960;

    SessionCb session_status_cb = CreateSessionStatusCb();
    uint32_t session_id;
    ret = recorder_.CreateSession(session_status_cb, &session_id);
    ASSERT_TRUE(session_id > 0);
    ASSERT_TRUE(ret == NO_ERROR);

    if (dump_bitstream_.IsEnabled()) {
      StreamDumpInfo dumpinfo1 = {VideoFormat::kAVC, session_id, video_track_id_960p_avc,
                                  width, height};
      ret = dump_bitstream_.SetUp(dumpinfo1);
      ASSERT_TRUE(ret == NO_ERROR);

      StreamDumpInfo dumpinfo2 = {VideoFormat::kAVC, session_id, video_track_id_480p_avc,
                                  640, 480};
      ret = dump_bitstream_.SetUp(dumpinfo2);
      ASSERT_TRUE(ret == NO_ERROR);
    }

    VideoTrackCreateParam video_track_param{camera_id_, VideoFormat::kAVC,
                                            width, height, 90};

    video_track_param.codec_param.avc.bitrate = kBitRate960p90;
    video_track_param.codec_param.avc.ratecontrol_type =
        VideoRateControlType::kVariable;

    TrackCb video_track_cb;
    video_track_cb.data_cb = [&, session_id](
        uint32_t track_id, std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
      VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
    };

    video_track_cb.event_cb = [&](uint32_t track_id, EventType event_type,
                                  void *event_data, size_t event_data_size) {
      VideoTrackEventCb(track_id, event_type, event_data, event_data_size);
    };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_960p_avc,
                                     video_track_param, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    std::vector<uint32_t> track_ids;
    track_ids.push_back(video_track_id_960p_avc);

    VideoExtraParam extra_param;
    SourceVideoTrack surface_video_copy;
    surface_video_copy.source_track_id = video_track_id_960p_avc;
    extra_param.Update(QMMF_SOURCE_VIDEO_TRACK_ID, surface_video_copy);

    width = 640;
    height = 480;
    video_track_param.width = width;
    video_track_param.height = height;
    video_track_param.frame_rate = 90;
    video_track_param.codec_param.avc.bitrate = kBitRate480p;

    video_track_cb.data_cb = [&, session_id](
        uint32_t track_id, std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
      VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
    };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_480p_avc,
                                     video_track_param, extra_param,
                                     video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    track_ids.push_back(video_track_id_480p_avc);

    VideoExtraParam extra_param2;
    SourceVideoTrack surface_video_linked;
    surface_video_linked.source_track_id = video_track_id_480p_avc;
    extra_param2.Update(QMMF_SOURCE_VIDEO_TRACK_ID, surface_video_linked);

    video_track_param.width = width;
    video_track_param.height = height;
    video_track_param.format_type = VideoFormat::kYUV;
    video_track_param.frame_rate = 30;

    video_track_cb.data_cb = [&, session_id](
        uint32_t track_id, std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
      VideoTrackYUVDataCb(session_id, track_id, buffers, meta_buffers);
    };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_480p_yuv,
                                     video_track_param, extra_param2,
                                     video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    track_ids.push_back(video_track_id_480p_yuv);
    sessions_.insert(std::make_pair(session_id, track_ids));

    auto status = recorder_.GetCameraParam(camera_id_, meta);
    if (NO_ERROR == status) {
      // EIS horizontal margin.
      float hmargin = (eis_h_margin_ >= 0) ? eis_h_margin_ : 0.100;
      meta.update(QCAMERA3_IS_H_MARGIN_CFG, &hmargin, 1);

      // EIS vertical margin.
      float vmargin = (eis_v_margin_ >= 0) ? eis_v_margin_ : 0.100;
      meta.update(QCAMERA3_IS_V_MARGIN_CFG, &vmargin, 1);

      // Enable EIS
      vstab_mode = ANDROID_CONTROL_VIDEO_STABILIZATION_MODE_ON;
      meta.update(ANDROID_CONTROL_VIDEO_STABILIZATION_MODE, &vstab_mode, 1);

      ret = recorder_.SetCameraParam(camera_id_, meta);
      ASSERT_TRUE(ret == NO_ERROR);
    }

    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    // Let session run for time record_duration_, during this time buffer with
    // valid data would be received in track callback (VideoTrackYUVDataCb).
    sleep(record_duration_);

    ret = recorder_.StopSession(session_id, false);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_480p_yuv);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_480p_avc);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_960p_avc);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    ClearSessions();
    dump_bitstream_.CloseAll();
  }

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  fprintf(stderr, "---------- Test Completed %s.%s ----------\n",
          test_info_->test_case_name(), test_info_->name());
}

/*
* SessionWith960p90FPSEncCopy480pEncAndLinked480pYUVTrackTNREISEnabled:
*                                          This test will test session with
*                                          one 960p90 Enc track, one Copy 480
*                                          Enc Track and one linked.
* Api test sequence:
*  - StartCamera
*   loop Start {
*   ------------------
*   - CreateSession
*   - CreateVideoTrack - Master
*   - CreateVideoTrack - Copy
*   - CreateVideoTrack - Linked
*   - StartSession
*   - StopSession
*   - DeleteVideoTrack - Linked
*   - DeleteVideoTrack - Copy
*   - DeleteVideoTrack - Master
*   - DeleteSession
*   ------------------
*   } loop End
*  - StopCamera
*/
TEST_F(VideoGtest,
       SessionWith960p90FPSEncCopy480pEncAndLinked480pYUVTrackTNREISEnabled) {
  fprintf(stderr, "\n---------- Run Test %s.%s ------------\n",
          test_info_->test_case_name(), test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.StartCamera(camera_id_, 90);
  ASSERT_TRUE(ret == NO_ERROR);

  uint32_t video_track_id_960p_avc = 1;
  uint32_t video_track_id_480p_avc = 2;
  uint32_t video_track_id_480p_yuv = 3;

  CameraMetadata meta;
  uint8_t vstab_mode;
  uint8_t tnr_mode;

  for (uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr, "test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
              test_info_->name(), i);
    uint32_t width = 1280;
    uint32_t height = 960;

    SessionCb session_status_cb = CreateSessionStatusCb();
    uint32_t session_id;
    ret = recorder_.CreateSession(session_status_cb, &session_id);
    ASSERT_TRUE(session_id > 0);
    ASSERT_TRUE(ret == NO_ERROR);

    if (dump_bitstream_.IsEnabled()) {
      StreamDumpInfo dumpinfo1 = {VideoFormat::kAVC, session_id, video_track_id_960p_avc,
                                  width, height};
      ret = dump_bitstream_.SetUp(dumpinfo1);
      ASSERT_TRUE(ret == NO_ERROR);

      StreamDumpInfo dumpinfo2 = {VideoFormat::kAVC, session_id, video_track_id_480p_avc,
                                  640, 480};
      ret = dump_bitstream_.SetUp(dumpinfo2);
      ASSERT_TRUE(ret == NO_ERROR);
    }

    VideoTrackCreateParam video_track_param{camera_id_, VideoFormat::kAVC,
                                            width, height, 90};

    video_track_param.codec_param.avc.bitrate = kBitRate960p90;
    video_track_param.codec_param.avc.ratecontrol_type =
        VideoRateControlType::kVariable;

    TrackCb video_track_cb;
    video_track_cb.data_cb = [&, session_id](
        uint32_t track_id, std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
      VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
    };

    video_track_cb.event_cb = [&](uint32_t track_id, EventType event_type,
                                  void *event_data, size_t event_data_size) {
      VideoTrackEventCb(track_id, event_type, event_data, event_data_size);
    };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_960p_avc,
                                     video_track_param, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    std::vector<uint32_t> track_ids;
    track_ids.push_back(video_track_id_960p_avc);

    VideoExtraParam extra_param;
    SourceVideoTrack surface_video_copy;
    surface_video_copy.source_track_id = video_track_id_960p_avc;
    extra_param.Update(QMMF_SOURCE_VIDEO_TRACK_ID, surface_video_copy);

    width = 640;
    height = 480;
    video_track_param.width = width;
    video_track_param.height = height;
    video_track_param.frame_rate = 90;
    video_track_param.codec_param.avc.bitrate = kBitRate480p;

    video_track_cb.data_cb = [&, session_id](
        uint32_t track_id, std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
      VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
    };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_480p_avc,
                                     video_track_param, extra_param,
                                     video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    track_ids.push_back(video_track_id_480p_avc);

    VideoExtraParam extra_param2;
    SourceVideoTrack surface_video_linked;
    surface_video_linked.source_track_id = video_track_id_480p_avc;
    extra_param2.Update(QMMF_SOURCE_VIDEO_TRACK_ID, surface_video_linked);

    video_track_param.width = width;
    video_track_param.height = height;
    video_track_param.format_type = VideoFormat::kYUV;
    video_track_param.frame_rate = 30;

    video_track_cb.data_cb = [&, session_id](
        uint32_t track_id, std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
      VideoTrackYUVDataCb(session_id, track_id, buffers, meta_buffers); };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_480p_yuv,
                                     video_track_param, extra_param2,
                                     video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    track_ids.push_back(video_track_id_480p_yuv);
    sessions_.insert(std::make_pair(session_id, track_ids));

    auto status = recorder_.GetCameraParam(camera_id_, meta);
    if (NO_ERROR == status) {
      // EIS horizontal margin.
      float hmargin = (eis_h_margin_ >= 0) ? eis_h_margin_ : 0.100;
      meta.update(QCAMERA3_IS_H_MARGIN_CFG, &hmargin, 1);

      // EIS vertical margin.
      float vmargin = (eis_v_margin_ >= 0) ? eis_v_margin_ : 0.100;
      meta.update(QCAMERA3_IS_V_MARGIN_CFG, &vmargin, 1);

      // Enable EIS
      vstab_mode = ANDROID_CONTROL_VIDEO_STABILIZATION_MODE_ON;
      meta.update(ANDROID_CONTROL_VIDEO_STABILIZATION_MODE, &vstab_mode, 1);

      // Enable TNR
      tnr_mode = ANDROID_NOISE_REDUCTION_MODE_HIGH_QUALITY;
      meta.update(ANDROID_NOISE_REDUCTION_MODE, &tnr_mode, 1);

      ret = recorder_.SetCameraParam(camera_id_, meta);
      ASSERT_TRUE(ret == NO_ERROR);
    }

    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    // Let session run for time record_duration_, during this time buffer with
    // valid data would be received in track callback (VideoTrackYUVDataCb).
    sleep(record_duration_);

    ret = recorder_.StopSession(session_id, false);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_480p_yuv);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_480p_avc);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_960p_avc);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    ClearSessions();
    dump_bitstream_.CloseAll();
  }

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  fprintf(stderr, "---------- Test Completed %s.%s ----------\n",
          test_info_->test_case_name(), test_info_->name());
}

/** LandscapeToPortraitRotation: This test will test session with 1440p h264
*                                track with rotation applied.
* API test sequence:
*  - StartCamera
*   loop Start {
*   ------------------
*   - CreateSession 1
*   - CreateVideoTrack 1
*   - CreateSession 2
*   - CreateVideoTrack 2 for rotation
*   - StartSession
*   - StopSession
*   - DeleteVideoTrack
*   - DeleteSession
*   ------------------
*   } loop End
*  - StopCamera
*/
TEST_F(VideoGtest, LandscapeToPortraitRotation) {
  fprintf(stderr, "\n---------- Run Test %s.%s ------------\n",
          test_info_->test_case_name(), test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  uint32_t stream_width = 1920;
  uint32_t stream_height = 1440;
  float stream_fps = 30;
  VideoFormat format_type = VideoFormat::kAVC;
  uint32_t video_track_id_1 = 1;
  uint32_t video_track_id_2 = 2;


  ret = recorder_.StartCamera(camera_id_, stream_fps);
  ASSERT_TRUE(ret == NO_ERROR);

  SessionCb session_status_cb = CreateSessionStatusCb();
  uint32_t session_id_1;
  ret = recorder_.CreateSession(session_status_cb, &session_id_1);
  ASSERT_TRUE(session_id_1 > 0);
  ASSERT_TRUE(ret == NO_ERROR);
  VideoTrackCreateParam video_track_param{camera_id_, format_type, stream_width,
                                          stream_height, stream_fps};

  TrackCb video_track_cb;
  video_track_cb.data_cb = [&, session_id_1](
      uint32_t track_id, std::vector<BufferDescriptor> buffers,
      std::vector<MetaData> meta_buffers) {
    VideoTrackEncDataCb(session_id_1, track_id, buffers, meta_buffers);
  };

  video_track_cb.event_cb = [&](uint32_t track_id, EventType event_type,
                                void *event_data, size_t event_data_size) {
    VideoTrackEventCb(track_id, event_type, event_data, event_data_size);
  };

  ret = recorder_.CreateVideoTrack(session_id_1, video_track_id_1,
                                   video_track_param, video_track_cb);
  ASSERT_TRUE(ret == NO_ERROR);

  uint32_t session_id_2;
  ret = recorder_.CreateSession(session_status_cb, &session_id_2);
  ASSERT_TRUE(session_id_2 > 0);
  ASSERT_TRUE(ret == NO_ERROR);

  stream_width = 1440;
  stream_height = 1920;

  video_track_param.width = stream_width;
  video_track_param.height = stream_height;
  video_track_param.format_type = VideoFormat::kAVC;

  video_track_cb.data_cb = [&, session_id_2](
      uint32_t track_id, std::vector<BufferDescriptor> buffers,
      std::vector<MetaData> meta_buffers) {
    VideoTrackEncDataCb(session_id_2, track_id, buffers, meta_buffers);
  };

  video_track_cb.event_cb = [&](uint32_t track_id, EventType event_type,
                                void *event_data, size_t event_data_size) {
    VideoTrackEventCb(track_id, event_type, event_data, event_data_size);
  };
  VideoExtraParam extra_param;
  VideoRotate rotate_param;
  rotate_param.flags = RotationFlags::kRotate90;  // 90 Degree
  extra_param.Update(QMMF_VIDEO_ROTATE, rotate_param);

  ret = recorder_.CreateVideoTrack(session_id_2, video_track_id_2,
                                   video_track_param, extra_param,
                                   video_track_cb);
  ASSERT_TRUE(ret == NO_ERROR);

  for (uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr, "test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
              test_info_->name(), i);

    if (dump_bitstream_.IsEnabled()) {
      StreamDumpInfo dumpinfo = {video_track_param.format_type,
        session_id_2, video_track_id_1, stream_width, stream_height};
      ret = dump_bitstream_.SetUp(dumpinfo);
      ASSERT_TRUE(ret == NO_ERROR);
    }
    ret = recorder_.StartSession(session_id_1);
    ASSERT_TRUE(ret == NO_ERROR);

    sleep(record_duration_ / 2);

    ret = recorder_.StopSession(session_id_1, false);
    ASSERT_TRUE(ret == NO_ERROR);

    // Now Switch to Portrait Mode
    ret = recorder_.StartSession(session_id_2);
    ASSERT_TRUE(ret == NO_ERROR);

    sleep(record_duration_ / 2);

    ret = recorder_.StopSession(session_id_2, false);
    ASSERT_TRUE(ret == NO_ERROR);

    dump_bitstream_.CloseAll();
  }

  ret = recorder_.DeleteVideoTrack(session_id_2, video_track_id_2);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteSession(session_id_2);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteVideoTrack(session_id_1, video_track_id_1);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteSession(session_id_1);
  ASSERT_TRUE(ret == NO_ERROR);


  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());
}

/** SessionWith4kEncWithSliceModeAUDAndSPSPPSEnabled: This test will test
*                                                     session with 4k h264 track
*                                                     with slice based encoding.
* API test sequence:
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
TEST_F(VideoGtest, SessionWith4kEncWithSliceModeAUDAndSPSPPSEnabled) {
  fprintf(stderr, "\n---------- Run Test %s.%s ------------\n",
          test_info_->test_case_name(), test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  VideoFormat format_type = VideoFormat::kAVC;
  uint32_t width = 3840;
  uint32_t height = 2160;

  ret = recorder_.StartCamera(camera_id_, 30);
  ASSERT_TRUE(ret == NO_ERROR);

  for (uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr, "test iteration = %d/%d\n", i, iteration_count_);

    SessionCb session_status_cb = CreateSessionStatusCb();
    uint32_t session_id;
    ret = recorder_.CreateSession(session_status_cb, &session_id);
    ASSERT_TRUE(session_id > 0);
    ASSERT_TRUE(ret == NO_ERROR);
    VideoTrackCreateParam video_track_param{camera_id_, format_type, width,
                                            height, 30};

    video_track_param.codec_param.avc.insert_aud_delimiter = true;
    video_track_param.codec_param.avc.prepend_sps_pps_to_idr = true;
    video_track_param.codec_param.avc.slice_enabled = true;
    video_track_param.codec_param.avc.slice_header_spacing = 8192;

    uint32_t video_track_id = 1;
    if (dump_bitstream_.IsEnabled()) {
      StreamDumpInfo dumpinfo = { format_type, session_id, video_track_id,
                                  width, height };
      ret = dump_bitstream_.SetUp(dumpinfo);
      ASSERT_TRUE(ret == NO_ERROR);
    }

    TrackCb video_track_cb;
    video_track_cb.data_cb = [&, session_id](
        uint32_t track_id, std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
      VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
    };

    video_track_cb.event_cb = [&](uint32_t track_id, EventType event_type,
                                  void *event_data, size_t event_data_size) {
      VideoTrackEventCb(track_id, event_type, event_data, event_data_size);
    };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id,
                                     video_track_param, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    std::vector<uint32_t> track_ids;
    track_ids.push_back(video_track_id);
    sessions_.insert(std::make_pair(session_id, track_ids));

    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    sleep(record_duration_);

    ret = recorder_.StopSession(session_id, false);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    dump_bitstream_.CloseAll();
  }
  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  fprintf(stderr, "---------- Test Completed %s.%s ----------\n",
          test_info_->test_case_name(), test_info_->name());
}

/*
* SmoothZoomWith1080pEncTrack: This test will test SmoothZoom with 1080p h264 track.
* Api test sequence:
*  - StartCamera
*   loop Start {
*   ------------------
*   - CreateSession
*   - CreateVideoTrack
*   - StartVideoTrack
*   - SmoothZoom apply
*   - StopSession
*   - DeleteVideoTrack
*   - DeleteSession
*   ------------------
*   } loop End
*  - StopCamera
*/
TEST_F(VideoGtest, SmoothZoomWith1080pEncTrack) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  float zoom = 2.5f;

  VideoFormat format_type = VideoFormat::kAVC;
  uint32_t width  = 1920;
  uint32_t height = 1080;

  ret = recorder_.StartCamera(camera_id_, 30);
  ASSERT_TRUE(ret == NO_ERROR);

  for(uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
        test_info_->name(), i);

    SessionCb session_status_cb = CreateSessionStatusCb();
    uint32_t session_id;
    ret = recorder_.CreateSession(session_status_cb, &session_id);
    ASSERT_TRUE(session_id > 0);
    ASSERT_TRUE(ret == NO_ERROR);
    VideoTrackCreateParam video_track_param{camera_id_, format_type,
                                            width, height, 30};
    uint32_t video_track_id = 1;

    if (dump_bitstream_.IsEnabled()) {
      StreamDumpInfo dumpinfo = { format_type, session_id, video_track_id,
                                  width, height };
      ret = dump_bitstream_.SetUp(dumpinfo);
      ASSERT_TRUE(ret == NO_ERROR);
    }

    TrackCb video_track_cb;
    video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
      std::vector<BufferDescriptor> buffers,
      std::vector<MetaData> meta_buffers) {
        VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
      };

    video_track_cb.event_cb = [&] (uint32_t track_id, EventType event_type,
        void *event_data, size_t event_data_size) { VideoTrackEventCb(track_id,
        event_type, event_data, event_data_size); };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id,
                                      video_track_param, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    std::vector<uint32_t> track_ids;
    track_ids.push_back(video_track_id);
    sessions_.insert(std::make_pair(session_id, track_ids));

    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    // Record a few seconds
    sleep(5);

    // Set zoom.
    CameraMetadata video_meta;
    ret = recorder_.GetCameraParam(camera_id_, video_meta);
    ASSERT_TRUE(ret == NO_ERROR);

    CameraMetadata static_meta;
    ret = recorder_.GetCameraCharacteristics(camera_id_, static_meta);
    ASSERT_TRUE(ret == NO_ERROR);

    int32_t crop[4];
    int32_t width = 0;
    int32_t height = 0;

    if (static_meta.exists(ANDROID_SCALER_AVAILABLE_RAW_SIZES)) {
      width =
        static_meta.find(ANDROID_SCALER_AVAILABLE_RAW_SIZES).data.i32[0];
      height =
        static_meta.find(ANDROID_SCALER_AVAILABLE_RAW_SIZES).data.i32[1];
    }
    ASSERT_TRUE (width && height > 0);

    crop[2] = static_cast<int32_t>(width / zoom);
    crop[3] = (crop[2] * height / width);
    crop[0] = (width - crop[2]) / 2;
    crop[1] = (height - crop[3]) / 2;
    ret = video_meta.update(ANDROID_SCALER_CROP_REGION, crop, 4);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.SetCameraParam(camera_id_, video_meta);
    ASSERT_TRUE(ret == NO_ERROR);

    sleep(5);

    crop[2] = width;
    crop[3] = (crop[2] * height / width);
    crop[0] = (width - crop[2]) / 2;
    crop[1] = (height - crop[3]) / 2;
    ret = video_meta.update(ANDROID_SCALER_CROP_REGION, crop, 4);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.SetCameraParam(camera_id_, video_meta);
    ASSERT_TRUE(ret == NO_ERROR);

    sleep(5);

    ret = recorder_.StopSession(session_id, false);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    dump_bitstream_.CloseAll();
  }
  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());

}

/*
* SessionWithVGA480pEncAndLinked480pWithDisplay:
*     This test will test session with one 640x480 @ 30 fps encoded track,
*     one linked 640x480 @ 30 fps track for display.
*
* API test sequence:
*  - StartCamera
*   loop Start {
*   ------------------
*   - CreateSession
*   - CreateVideoTrack - Master
*   - CreateVideoTrack - Linked
*   - StartSession
*   - StopSession
*   - DeleteVideoTrack - Linked
*   - DeleteVideoTrack - Master
*   - DeleteSession
*   ------------------
*   } loop End
*  - StopCamera
*/
TEST_F(VideoGtest,
       SessionWithVGA480pEncAndLinked480pWithDisplay) {
  fprintf(stderr, "\n---------- Run Test %s.%s ------------\n",
          test_info_->test_case_name(), test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.StartCamera(camera_id_, 30);
  ASSERT_TRUE(ret == NO_ERROR);

  uint32_t video_track_id_480p_avc = 1;
  uint32_t video_track_id_480p_yuv = 2;

  uint32_t width = 640;
  uint32_t height = 480;
  float frame_rate = 30;

  CameraMetadata meta;

  for (uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr, "test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
              test_info_->name(), i);

    SessionCb session_status_cb = CreateSessionStatusCb();
    uint32_t session_id;
    ret = recorder_.CreateSession(session_status_cb, &session_id);
    ASSERT_TRUE(session_id > 0);
    ASSERT_TRUE(ret == NO_ERROR);

    if (dump_bitstream_.IsEnabled()) {
      StreamDumpInfo dumpinfo = {VideoFormat::kAVC, session_id, video_track_id_480p_avc,
                                 width, height};
      ret = dump_bitstream_.SetUp(dumpinfo);
      ASSERT_TRUE(ret == NO_ERROR);
    }

    VideoTrackCreateParam video_track_param{camera_id_, VideoFormat::kAVC,
                                            width, height, frame_rate};
    TrackCb video_track_cb;
    video_track_cb.data_cb = [&, session_id](
        uint32_t track_id, std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
      VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
    };

    video_track_cb.event_cb = [&](uint32_t track_id, EventType event_type,
                                  void *event_data, size_t event_data_size) {
      VideoTrackEventCb(track_id, event_type, event_data, event_data_size);
    };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_480p_avc,
                                     video_track_param, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    std::vector<uint32_t> track_ids;
    track_ids.push_back(video_track_id_480p_avc);

    VideoExtraParam extra_param;
    SourceVideoTrack surface_video_linked;
    surface_video_linked.source_track_id = video_track_id_480p_avc;
    extra_param.Update(QMMF_SOURCE_VIDEO_TRACK_ID, surface_video_linked);

    video_track_param.width = width;
    video_track_param.height = height;
    video_track_param.format_type = VideoFormat::kYUV;
    video_track_param.frame_rate = frame_rate;

    video_track_cb.data_cb = [&, session_id](
        uint32_t track_id, std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
      VideoTrackYUVDataCb(session_id, track_id, buffers, meta_buffers); };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_480p_yuv,
                                     video_track_param, extra_param,
                                     video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    track_ids.push_back(video_track_id_480p_yuv);
    sessions_.insert(std::make_pair(session_id, track_ids));

    ret = SetCameraFocalLength(7.0);
    ASSERT_TRUE(ret == NO_ERROR);

    if (use_display_) {
      ret = StartDisplay(DisplayType::kPrimary, width, height, 480, 360);
      ASSERT_TRUE(ret == NO_ERROR);
    }

    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    // Let session run for time record_duration_, during this time buffer with
    // valid data would be received in track callback (VideoTrackYUVDataCb).
    sleep(record_duration_);

    ret = recorder_.StopSession(session_id, false);
    ASSERT_TRUE(ret == NO_ERROR);

    if (use_display_) {
      ret = StopDisplay(DisplayType::kPrimary);
      ASSERT_TRUE(ret == NO_ERROR);
    }

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_480p_yuv);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_480p_avc);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    ClearSessions();
    dump_bitstream_.CloseAll();
  }

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  fprintf(stderr, "---------- Test Completed %s.%s ----------\n",
          test_info_->test_case_name(), test_info_->name());
}

#ifndef DISABLE_DISPLAY
/*
* SessionWithFWVGA480pEncAndLinked480pWithDisplay:
*     This test will test session with one 848x480 @ 30 fps encoded track,
*     one linked 848x480 @ 30 fps track for display.
*
* API test sequence:
*  - StartCamera
*   loop Start {
*   ------------------
*   - CreateSession
*   - CreateVideoTrack - Master
*   - CreateVideoTrack - Linked
*   - StartSession
*   - StopSession
*   - DeleteVideoTrack - Linked
*   - DeleteVideoTrack - Master
*   - DeleteSession
*   ------------------
*   } loop End
*  - StopCamera
*/
TEST_F(VideoGtest,
       SessionWithFWVGA480pEncAndLinked480pWithDisplay) {
  fprintf(stderr, "\n---------- Run Test %s.%s ------------\n",
          test_info_->test_case_name(), test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.StartCamera(camera_id_, 30);
  ASSERT_TRUE(ret == NO_ERROR);

  uint32_t video_track_id_480p_avc = 1;
  uint32_t video_track_id_480p_yuv = 2;

  uint32_t width = 864;
  uint32_t height = 480;
  float frame_rate = 30;

  CameraMetadata meta;

  for (uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr, "test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
              test_info_->name(), i);

    SessionCb session_status_cb = CreateSessionStatusCb();
    uint32_t session_id;
    ret = recorder_.CreateSession(session_status_cb, &session_id);
    ASSERT_TRUE(session_id > 0);
    ASSERT_TRUE(ret == NO_ERROR);

    if (dump_bitstream_.IsEnabled()) {
      StreamDumpInfo dumpinfo = {VideoFormat::kAVC, session_id, video_track_id_480p_avc,
                                 width, height};
      ret = dump_bitstream_.SetUp(dumpinfo);
      ASSERT_TRUE(ret == NO_ERROR);
    }

    VideoTrackCreateParam video_track_param{camera_id_, VideoFormat::kAVC,
                                            width, height, frame_rate};
    TrackCb video_track_cb;
    video_track_cb.data_cb = [&, session_id](
        uint32_t track_id, std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
      VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
    };

    video_track_cb.event_cb = [&](uint32_t track_id, EventType event_type,
                                  void *event_data, size_t event_data_size) {
      VideoTrackEventCb(track_id, event_type, event_data, event_data_size);
    };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_480p_avc,
                                     video_track_param, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    std::vector<uint32_t> track_ids;
    track_ids.push_back(video_track_id_480p_avc);

    VideoExtraParam extra_param;
    SourceVideoTrack surface_video_linked;
    surface_video_linked.source_track_id = video_track_id_480p_avc;
    extra_param.Update(QMMF_SOURCE_VIDEO_TRACK_ID, surface_video_linked);

    video_track_param.width = width;
    video_track_param.height = height;
    video_track_param.format_type = VideoFormat::kYUV;
    video_track_param.frame_rate = frame_rate;

    video_track_cb.data_cb = [&, session_id](
        uint32_t track_id, std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
      VideoTrackYUVDataCb(session_id, track_id, buffers, meta_buffers); };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_480p_yuv,
                                     video_track_param, extra_param,
                                     video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    track_ids.push_back(video_track_id_480p_yuv);
    sessions_.insert(std::make_pair(session_id, track_ids));

    ret = SetCameraFocalLength(6.0);
    ASSERT_TRUE(ret == NO_ERROR);

    if (use_display_) {
      ret = StartDisplay(DisplayType::kPrimary, width, height, 480, 320);
      ASSERT_TRUE(ret == NO_ERROR);
    }

    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    // Let session run for time record_duration_, during this time buffer with
    // valid data would be received in track callback (VideoTrackYUVDataCb).
    sleep(record_duration_);

    ret = recorder_.StopSession(session_id, false);
    ASSERT_TRUE(ret == NO_ERROR);

    if (use_display_) {
      ret = StopDisplay(DisplayType::kPrimary);
      ASSERT_TRUE(ret == NO_ERROR);
    }

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_480p_yuv);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_480p_avc);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    ClearSessions();
    dump_bitstream_.CloseAll();
  }

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  fprintf(stderr, "---------- Test Completed %s.%s ----------\n",
          test_info_->test_case_name(), test_info_->name());
}

/*
* SessionWithVGA480pEncAndLinked480pWithDisplayEISLCAC:
*     This test will test session with one 640x480 @ 30 fps encoded track,
*     one linked 640x480 @ 30 fps track for display. Both having EIS and LCAC.
*
* API test sequence:
*  - StartCamera
*   loop Start {
*   ------------------
*   - CreateSession
*   - CreateVideoTrack - Master
*   - CreateVideoTrack - Linked
*   - StartSession
*   - StopSession
*   - DeleteVideoTrack - Linked
*   - DeleteVideoTrack - Master
*   - DeleteSession
*   ------------------
*   } loop End
*  - StopCamera
*/
TEST_F(VideoGtest,
       SessionWithVGA480pEncAndLinked480pWithDisplayEISLCAC) {
  fprintf(stderr, "\n---------- Run Test %s.%s ------------\n",
          test_info_->test_case_name(), test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.StartCamera(camera_id_, 30);
  ASSERT_TRUE(ret == NO_ERROR);

  uint32_t video_track_id_480p_avc = 1;
  uint32_t video_track_id_480p_yuv = 2;

  CameraMetadata meta;
  uint8_t enable_lcac;
  uint8_t vstab_mode;

  uint32_t width = 640;
  uint32_t height = 480;
  float frame_rate = 30;

  for (uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr, "test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
              test_info_->name(), i);

    SessionCb session_status_cb = CreateSessionStatusCb();
    uint32_t session_id;
    ret = recorder_.CreateSession(session_status_cb, &session_id);
    ASSERT_TRUE(session_id > 0);
    ASSERT_TRUE(ret == NO_ERROR);

    if (dump_bitstream_.IsEnabled()) {
      StreamDumpInfo dumpinfo = {VideoFormat::kAVC, session_id, video_track_id_480p_avc,
                                 width, height};
      ret = dump_bitstream_.SetUp(dumpinfo);
      ASSERT_TRUE(ret == NO_ERROR);
    }

    VideoTrackCreateParam video_track_param{camera_id_, VideoFormat::kAVC,
                                            width, height, frame_rate};
    TrackCb video_track_cb;
    video_track_cb.data_cb = [&, session_id](
        uint32_t track_id, std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
      VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
    };

    video_track_cb.event_cb = [&](uint32_t track_id, EventType event_type,
                                  void *event_data, size_t event_data_size) {
      VideoTrackEventCb(track_id, event_type, event_data, event_data_size);
    };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_480p_avc,
                                     video_track_param, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    std::vector<uint32_t> track_ids;
    track_ids.push_back(video_track_id_480p_avc);

    VideoExtraParam extra_param;
    SourceVideoTrack surface_video_linked;
    surface_video_linked.source_track_id = video_track_id_480p_avc;
    extra_param.Update(QMMF_SOURCE_VIDEO_TRACK_ID, surface_video_linked);

    video_track_param.width = width;
    video_track_param.height = height;
    video_track_param.format_type = VideoFormat::kYUV;
    video_track_param.frame_rate = frame_rate;

    video_track_cb.data_cb = [&, session_id](
        uint32_t track_id, std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
      VideoTrackYUVDataCb(session_id, track_id, buffers, meta_buffers); };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_480p_yuv,
                                     video_track_param, extra_param,
                                     video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    track_ids.push_back(video_track_id_480p_yuv);
    sessions_.insert(std::make_pair(session_id, track_ids));

    // Enable YUV LCAC
    auto status = recorder_.GetCameraParam(camera_id_, meta);
    if (NO_ERROR == status) {
      enable_lcac = 1;
      meta.update( QCAMERA3_LCAC_PROCESSING_ENABLE, &enable_lcac, 1);

      // Set sensor mode via focal lenth
      float focal_length = 6.0;
      meta.update(ANDROID_LENS_FOCAL_LENGTH, &focal_length, 1);

      // EIS horizontal margin.
      float hmargin = (eis_h_margin_ >= 0) ? eis_h_margin_ : 0.100;
      meta.update(QCAMERA3_IS_H_MARGIN_CFG, &hmargin, 1);

      // EIS vertical margin.
      float vmargin = (eis_v_margin_ >= 0) ? eis_v_margin_ : 0.100;
      meta.update(QCAMERA3_IS_V_MARGIN_CFG, &vmargin, 1);

      // Enable EIS
      vstab_mode = ANDROID_CONTROL_VIDEO_STABILIZATION_MODE_ON;
      meta.update(ANDROID_CONTROL_VIDEO_STABILIZATION_MODE, &vstab_mode, 1);

      ret = recorder_.SetCameraParam(camera_id_, meta);
      ASSERT_TRUE(ret == NO_ERROR);
    }

    if (use_display_) {
      ret = StartDisplay(DisplayType::kPrimary, width, height, 480, 360);
      ASSERT_TRUE(ret == NO_ERROR);
    }

    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    // Let session run for time record_duration_, during this time buffer with
    // valid data would be received in track callback (VideoTrackYUVDataCb).
    sleep(record_duration_);

    ret = recorder_.StopSession(session_id, false);
    ASSERT_TRUE(ret == NO_ERROR);

    if (use_display_) {
      ret = StopDisplay(DisplayType::kPrimary);
      ASSERT_TRUE(ret == NO_ERROR);
    }

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_480p_yuv);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_480p_avc);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    ClearSessions();
    dump_bitstream_.CloseAll();
  }

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  fprintf(stderr, "---------- Test Completed %s.%s ----------\n",
          test_info_->test_case_name(), test_info_->name());
}

/*
* SessionWithFWVGA480pEncAndLinked480pWithDisplayEISLCAC:
*     This test will test session with one 848x480 @ 30 fps encoded track,
*     one linked 848x480 @ 30 fps track for display. Both having EIS and LCAC.
*
* API test sequence:
*  - StartCamera
*   loop Start {
*   ------------------
*   - CreateSession
*   - CreateVideoTrack - Master
*   - CreateVideoTrack - Linked
*   - StartSession
*   - StopSession
*   - DeleteVideoTrack - Linked
*   - DeleteVideoTrack - Master
*   - DeleteSession
*   ------------------
*   } loop End
*  - StopCamera
*/
TEST_F(VideoGtest,
       SessionWithFWVGA480pEncAndLinked480pWithDisplayEISLCAC) {
  fprintf(stderr, "\n---------- Run Test %s.%s ------------\n",
          test_info_->test_case_name(), test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.StartCamera(camera_id_, 30);
  ASSERT_TRUE(ret == NO_ERROR);

  uint32_t video_track_id_480p_avc = 1;
  uint32_t video_track_id_480p_yuv = 2;

  CameraMetadata meta;
  uint8_t enable_lcac;
  uint8_t vstab_mode;

  uint32_t width = 864;
  uint32_t height = 480;
  float frame_rate = 30;

  for (uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr, "test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
              test_info_->name(), i);

    SessionCb session_status_cb = CreateSessionStatusCb();
    uint32_t session_id;
    ret = recorder_.CreateSession(session_status_cb, &session_id);
    ASSERT_TRUE(session_id > 0);
    ASSERT_TRUE(ret == NO_ERROR);

    if (dump_bitstream_.IsEnabled()) {
      StreamDumpInfo dumpinfo = {VideoFormat::kAVC, session_id, video_track_id_480p_avc,
                                 width, height};
      ret = dump_bitstream_.SetUp(dumpinfo);
      ASSERT_TRUE(ret == NO_ERROR);
    }

    VideoTrackCreateParam video_track_param{camera_id_, VideoFormat::kAVC,
                                            width, height, frame_rate};
    TrackCb video_track_cb;
    video_track_cb.data_cb = [&, session_id](
        uint32_t track_id, std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
      VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
    };

    video_track_cb.event_cb = [&](uint32_t track_id, EventType event_type,
                                  void *event_data, size_t event_data_size) {
      VideoTrackEventCb(track_id, event_type, event_data, event_data_size);
    };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_480p_avc,
                                     video_track_param, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    std::vector<uint32_t> track_ids;
    track_ids.push_back(video_track_id_480p_avc);

    VideoExtraParam extra_param;
    SourceVideoTrack surface_video_linked;
    surface_video_linked.source_track_id = video_track_id_480p_avc;
    extra_param.Update(QMMF_SOURCE_VIDEO_TRACK_ID, surface_video_linked);

    video_track_param.width = width;
    video_track_param.height = height;
    video_track_param.format_type = VideoFormat::kYUV;
    video_track_param.frame_rate = frame_rate;

    video_track_cb.data_cb = [&, session_id](
        uint32_t track_id, std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
      VideoTrackYUVDataCb(session_id, track_id, buffers, meta_buffers);
    };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_480p_yuv,
                                     video_track_param, extra_param,
                                     video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    track_ids.push_back(video_track_id_480p_yuv);
    sessions_.insert(std::make_pair(session_id, track_ids));

    // Enable YUV LCAC
    auto status = recorder_.GetCameraParam(camera_id_, meta);
    if (NO_ERROR == status) {
      enable_lcac = 1;
      meta.update( QCAMERA3_LCAC_PROCESSING_ENABLE, &enable_lcac, 1);

      // Set sensor mode via focal lenth
      float focal_length = 7.0;
      meta.update(ANDROID_LENS_FOCAL_LENGTH, &focal_length, 1);

      // EIS horizontal margin.
      float hmargin = (eis_h_margin_ >= 0) ? eis_h_margin_ : 0.100;
      meta.update(QCAMERA3_IS_H_MARGIN_CFG, &hmargin, 1);

      // EIS vertical margin.
      float vmargin = (eis_v_margin_ >= 0) ? eis_v_margin_ : 0.100;
      meta.update(QCAMERA3_IS_V_MARGIN_CFG, &vmargin, 1);

      // Enable EIS
      vstab_mode = ANDROID_CONTROL_VIDEO_STABILIZATION_MODE_ON;
      meta.update(ANDROID_CONTROL_VIDEO_STABILIZATION_MODE, &vstab_mode, 1);

      ret = recorder_.SetCameraParam(camera_id_, meta);
      ASSERT_TRUE(ret == NO_ERROR);
    }

    if (use_display_) {
      ret = StartDisplay(DisplayType::kPrimary, width, height, 480, 360);
      ASSERT_TRUE(ret == NO_ERROR);
    }

    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    // Let session run for time record_duration_, during this time buffer with
    // valid data would be received in track callback (VideoTrackYUVDataCb).
    sleep(record_duration_);

    ret = recorder_.StopSession(session_id, false);
    ASSERT_TRUE(ret == NO_ERROR);

    if (use_display_) {
      ret = StopDisplay(DisplayType::kPrimary);
      ASSERT_TRUE(ret == NO_ERROR);
    }

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_480p_yuv);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_480p_avc);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    ClearSessions();
    dump_bitstream_.CloseAll();
  }

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  fprintf(stderr, "---------- Test Completed %s.%s ----------\n",
          test_info_->test_case_name(), test_info_->name());
}


/*
* SessionWith480p30FpsYUVDisplayAnd1440p60FpsEncTrackEISLCAC:
*     This test will test session with one 640x480 30fps YUV track. After some
*     time a 1440p h264 encoded track at 60 fps will be added to the session
*     and both tracks will be ran together. Both have LCAC and EIS enabled.
*
* Api test sequence:
*  - StartCamera
*  - CreateSession
*  - CreateVideoTrack 480p@30fps
*  - StartSession
*  - StopSession
*  - DeleteVideoTrack 480p@30fps
*  - CreateVideoTrack 1440p@60fps
*  - CreateVideoTrack 480p@30fps Linked
*  - StartSession
*  - StopSession
*  - DeleteVideoTrack 480p@30fps Linked
*  - DeleteVideoTrack 1440p@60fps
*  - CreateVideoTrack 480p@30fps
*  - StartSession
*  - StopSession
*  - DeleteVideoTrack 480p@30fps
*  - DeleteSession
*  - StopCamera
*/
TEST_F(VideoGtest,
       SessionWith480p30FpsYUVDisplayAnd1440p60FpsEncTrackEISLCAC) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.StartCamera(camera_id_, 60);
  ASSERT_TRUE(ret == NO_ERROR);

  SessionCb session_status_cb = CreateSessionStatusCb();
  uint32_t session_id;
  ret = recorder_.CreateSession(session_status_cb, &session_id);
  ASSERT_TRUE(session_id > 0);
  ASSERT_TRUE(ret == NO_ERROR);
  uint32_t track_480p_id = 1;
  uint32_t track_1440p_id = 2;

  std::vector<uint32_t> track_ids;
  for(uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
        test_info_->name(), i);
    VideoTrackCreateParam video_track_param{camera_id_, VideoFormat::kYUV,
                                            640, 480, 30};

    TrackCb video_track_cb;
    video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
                                std::vector<BufferDescriptor> buffers,
                                std::vector<MetaData> meta_buffers) {
      VideoTrackYUVDataCb(session_id, track_id, buffers, meta_buffers); };

    video_track_cb.event_cb =
        [this] (uint32_t track_id, EventType event_type,
                void *event_data, size_t event_data_size) -> void {
        VideoTrackEventCb(track_id, event_type, event_data, event_data_size); };

    ret = recorder_.CreateVideoTrack(session_id, track_480p_id,
                                     video_track_param, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    track_ids.push_back(track_480p_id);
    sessions_.insert(std::make_pair(session_id, track_ids));

    CameraMetadata meta;
    ret = recorder_.GetCameraParam(camera_id_, meta);
    ASSERT_TRUE(ret == NO_ERROR);

    // Enable YUV LCAC and EIS
    uint8_t enable_lcac = 1;
    ret = meta.update( QCAMERA3_LCAC_PROCESSING_ENABLE, &enable_lcac, 1);
    ASSERT_TRUE(ret == NO_ERROR);

    // EIS horizontal margin.
    float hmargin = (eis_h_margin_ >= 0) ? eis_h_margin_ : 0.100;
    meta.update(QCAMERA3_IS_H_MARGIN_CFG, &hmargin, 1);

    // EIS vertical margin.
    float vmargin = (eis_v_margin_ >= 0) ? eis_v_margin_ : 0.100;
    meta.update(QCAMERA3_IS_V_MARGIN_CFG, &vmargin, 1);

    uint8_t vstab_mode = ANDROID_CONTROL_VIDEO_STABILIZATION_MODE_ON;
    meta.update(ANDROID_CONTROL_VIDEO_STABILIZATION_MODE, &vstab_mode, 1);

    // Set sensor mode via focal lenth
    float focal_length = 6.0;
    meta.update(ANDROID_LENS_FOCAL_LENGTH, &focal_length, 1);

    ret = recorder_.SetCameraParam(camera_id_, meta);
    ASSERT_TRUE(ret == NO_ERROR);

    if (use_display_) {
      ret = StartDisplay(DisplayType::kPrimary, 640, 480, 480, 360);
      ASSERT_TRUE(ret == NO_ERROR);
    }

    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    sleep(record_duration_);

    ret = recorder_.StopSession(session_id, false);
    ASSERT_TRUE(ret == NO_ERROR);

    if (use_display_) {
      ret = StopDisplay(DisplayType::kPrimary);
      ASSERT_TRUE(ret == NO_ERROR);
    }

    ret = recorder_.DeleteVideoTrack(session_id, track_480p_id);
    ASSERT_TRUE(ret == NO_ERROR);

    track_ids.clear();
    sessions_.clear();

    if (dump_bitstream_.IsEnabled()) {
      StreamDumpInfo dumpinfo = {
        VideoFormat::kAVC,
        session_id,
        track_1440p_id,
        1920,
        1440 };
      ret = dump_bitstream_.SetUp(dumpinfo);
      ASSERT_TRUE(ret == NO_ERROR);
    }

    video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
        std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
          VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
        };

    video_track_cb.event_cb =
        [this] (uint32_t track_id, EventType event_type,
                void *event_data, size_t event_data_size) -> void {
        VideoTrackEventCb(track_id, event_type, event_data, event_data_size); };

    video_track_param.width  = 1920;
    video_track_param.height = 1440;
    video_track_param.frame_rate = 60;
    video_track_param.format_type = VideoFormat::kAVC;
    video_track_param.setAVCDefaultVideoParam();
    video_track_param.codec_param.avc.bitrate = 12000000;
    ret = recorder_.CreateVideoTrack(session_id, track_1440p_id,
                                     video_track_param, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
                                std::vector<BufferDescriptor> buffers,
                                std::vector<MetaData> meta_buffers) {
      VideoTrackYUVDataCb(session_id, track_id, buffers, meta_buffers); };

    video_track_cb.event_cb =
        [this] (uint32_t track_id, EventType event_type,
                void *event_data, size_t event_data_size) -> void {
        VideoTrackEventCb(track_id, event_type, event_data, event_data_size); };

    video_track_param.width  = 640;
    video_track_param.height = 480;
    video_track_param.frame_rate = 30;
    video_track_param.format_type = VideoFormat::kYUV;

    VideoExtraParam extra_param;
    SourceVideoTrack source_track;
    source_track.source_track_id = track_1440p_id;
    extra_param.Update(QMMF_SOURCE_VIDEO_TRACK_ID, source_track);

    ret = recorder_.CreateVideoTrack(session_id, track_480p_id,
                                     video_track_param, extra_param,
                                     video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    track_ids.push_back(track_480p_id);
    track_ids.push_back(track_1440p_id);
    sessions_.insert(std::make_pair(session_id, track_ids));

    ret = recorder_.GetCameraParam(camera_id_, meta);
    ASSERT_TRUE(ret == NO_ERROR);

    // Set sensor mode via focal lenth
    focal_length = 4.0;
    meta.update(ANDROID_LENS_FOCAL_LENGTH, &focal_length, 1);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.SetCameraParam(camera_id_, meta);
    ASSERT_TRUE(ret == NO_ERROR);

    if (use_display_) {
      ret = StartDisplay(DisplayType::kPrimary, 640, 480, 480, 360);
      ASSERT_TRUE(ret == NO_ERROR);
    }

    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    sleep(record_duration_);

    ret = recorder_.StopSession(session_id, false);
    ASSERT_TRUE(ret == NO_ERROR);

    if (use_display_) {
      ret = StopDisplay(DisplayType::kPrimary);
      ASSERT_TRUE(ret == NO_ERROR);
    }

    ret = recorder_.DeleteVideoTrack(session_id, track_480p_id);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, track_1440p_id);
    ASSERT_TRUE(ret == NO_ERROR);

    video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
                                std::vector<BufferDescriptor> buffers,
                                std::vector<MetaData> meta_buffers) {
      VideoTrackYUVDataCb(session_id, track_id, buffers, meta_buffers); };

    video_track_cb.event_cb =
        [this] (uint32_t track_id, EventType event_type,
                void *event_data, size_t event_data_size) -> void {
        VideoTrackEventCb(track_id, event_type, event_data, event_data_size); };

    video_track_param.width  = 640;
    video_track_param.height = 480;
    video_track_param.frame_rate = 30;
    video_track_param.format_type = VideoFormat::kYUV;

    ret = recorder_.CreateVideoTrack(session_id, track_480p_id,
                                     video_track_param, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.GetCameraParam(camera_id_, meta);
    ASSERT_TRUE(ret == NO_ERROR);

    // Set sensor mode via focal lenth
    focal_length = 6.0;
    meta.update(ANDROID_LENS_FOCAL_LENGTH, &focal_length, 1);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.SetCameraParam(camera_id_, meta);
    ASSERT_TRUE(ret == NO_ERROR);

    if (use_display_) {
      ret = StartDisplay(DisplayType::kPrimary, 640, 480, 480, 360);
      ASSERT_TRUE(ret == NO_ERROR);
    }

    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    sleep(record_duration_);

    ret = recorder_.StopSession(session_id, false);
    ASSERT_TRUE(ret == NO_ERROR);

    if (use_display_) {
      ret = StopDisplay(DisplayType::kPrimary);
      ASSERT_TRUE(ret == NO_ERROR);
    }

    ret = recorder_.DeleteVideoTrack(session_id, track_480p_id);
    ASSERT_TRUE(ret == NO_ERROR);
  }

  ret = recorder_.DeleteSession(session_id);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ClearSessions();

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());
}

/*
* Session1080pYUVTrackWithDisplayAlongWithGfxPlane: This test will be used to test display
* functionality. This test will create session with 1080p YUV track and
* push received YUV cb frames to display.
* Api test sequence:
*  - StartCamera
*   loop Start {
*   ------------------
*   - CreateSession
*   - CreateVideoTrack
*   - StartDisplay
*   - StartVideoTrack
*   - StopSession
*   - StopDisplay
*   - DeleteVideoTrack
*   - DeleteSession
*   ------------------
*   } loop End
*  - StopCamera
*/

TEST_F(VideoGtest, Session1080pYUVTrackWithDisplayAlongWithGfxPlane) {
  fprintf(stderr, "\n---------- Run Test %s.%s ------------\n",
          test_info_->test_case_name(), test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  use_display_ = true;
  enable_gfx_ = true;
  uint32_t stream_width = FHD_1080p_STREAM_WIDTH;
  uint32_t stream_height = FHD_1080p_STREAM_HEIGHT;

  ret = recorder_.StartCamera(camera_id_, 30);
  ASSERT_TRUE(ret == NO_ERROR);

  for (uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr, "test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
        test_info_->name(), i);

    SessionCb session_status_cb = CreateSessionStatusCb();
    uint32_t session_id;

    ret = recorder_.CreateSession(session_status_cb, &session_id);
    ASSERT_TRUE(session_id > 0);
    ASSERT_TRUE(ret == NO_ERROR);

    VideoTrackCreateParam video_track_param{camera_id_, VideoFormat::kYUV,
                                            stream_width, stream_height, 30};

    uint32_t video_track_id_1 = 1;

    TrackCb video_track_cb;
    video_track_cb.data_cb = [&, session_id](
        uint32_t track_id, std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
      VideoTrackYUVDataCb(session_id, track_id, buffers, meta_buffers); };

    video_track_cb.event_cb = [&](uint32_t track_id, EventType event_type,
                                  void *event_data, size_t event_data_size) {
      VideoTrackEventCb(track_id, event_type, event_data, event_data_size);
    };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_1,
                                     video_track_param, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = StartDisplay(DisplayType::kPrimary, stream_width, stream_height,
                       stream_width, stream_height);
    if (ret != 0) {
      TEST_ERROR("%s: StartDisplay Failed!!", __func__);
    }

    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    sleep(record_duration_);

    ret = recorder_.StopSession(session_id, false);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = StopDisplay(DisplayType::kPrimary);
    if (ret != 0) {
      TEST_ERROR("%s: StopDisplay Failed!!", __func__);
    }

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_1);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    ClearSessions();
  }

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);
  fprintf(stderr, "---------- Test Completed %s.%s ----------\n",
          test_info_->test_case_name(), test_info_->name());
}
#endif

/* SessionWith1080pYUVTrackFocalLength: This test will test session with one
* 1080p YUV track. Api test sequence:
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
TEST_F(VideoGtest, SessionWith1080pYUVTrackFocalLength) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.StartCamera(camera_id_, 30);
  ASSERT_TRUE(ret == NO_ERROR);

  for(uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s Running Test(%s) iteration = %d ", __func__,
        test_info_->name(), i);

    SessionCb session_status_cb = CreateSessionStatusCb();
    uint32_t session_id;
    ret = recorder_.CreateSession(session_status_cb, &session_id);
    ASSERT_TRUE(session_id > 0);
    ASSERT_TRUE(ret == NO_ERROR);
    VideoTrackCreateParam video_track_param{camera_id_, VideoFormat::kYUV,
                                            1920, 1080, 30};
    uint32_t video_track_id = 1;
    TrackCb video_track_cb;
    video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
        std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
          VideoTrackYUVDataCb(session_id, track_id, buffers, meta_buffers); };

    video_track_cb.event_cb = [&] (uint32_t track_id, EventType event_type,
        void *event_data, size_t event_data_size) { VideoTrackEventCb(track_id,
        event_type, event_data, event_data_size); };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id,
                                     video_track_param, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = SetCameraFocalLength(5.0);
    ASSERT_TRUE(ret == NO_ERROR);

    std::vector<uint32_t> track_ids;
    track_ids.push_back(video_track_id);
    sessions_.insert(std::make_pair(session_id, track_ids));

    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    // Let session run for time record_duration_, during this time buffer with
    // valid data would be received in track callback (VideoTrackYUVDataCb).
    sleep(record_duration_);

    ret = recorder_.StopSession(session_id, false);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    ClearSessions();
  }

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());
}

/* SessionWith1080pEncTrackChangeFocalLength: This test will test session with
* one 1080p Enc track. Api test sequence:
*  - StartCamera
*   loop Start {
*   ------------------
*   - CreateSession
*   - CreateVideoTrack
*   - SetCameraFocalLength - 5.0
*   - StartSession
*   - StopSession
*   - SetCameraFocalLength - 5.13
*   - StartSession
*   - StopSession
*   - DeleteVideoTrack
*   - DeleteSession
*   ------------------
*   } loop End
*  - StopCamera
*/
TEST_F(VideoGtest, SessionWith1080pEncTrackChangeFocalLength) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.StartCamera(camera_id_, 15);
  ASSERT_TRUE(ret == NO_ERROR);

  for(uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s Running Test(%s) iteration = %d ", __func__,
        test_info_->name(), i);

    SessionCb session_status_cb = CreateSessionStatusCb();
    uint32_t session_id;
    ret = recorder_.CreateSession(session_status_cb, &session_id);
    ASSERT_TRUE(session_id > 0);
    ASSERT_TRUE(ret == NO_ERROR);
    VideoTrackCreateParam video_track_param{ camera_id_, VideoFormat::kAVC,
                                             1920, 1080, 15 };
    uint32_t video_track_id = 1;

    if (dump_bitstream_.IsEnabled()) {
      StreamDumpInfo dumpinfo = { VideoFormat::kAVC, session_id,
                                  video_track_id, 1920, 1080 };
      ret = dump_bitstream_.SetUp(dumpinfo);
      ASSERT_TRUE(ret == NO_ERROR);
    }

    TrackCb video_track_cb;
    video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
        std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
          VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
        };

    video_track_cb.event_cb = [&] (uint32_t track_id, EventType event_type,
        void *event_data, size_t event_data_size) { VideoTrackEventCb(track_id,
        event_type, event_data, event_data_size); };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id,
                                     video_track_param, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = SetCameraFocalLength(5.0);
    ASSERT_TRUE(ret == NO_ERROR);

    std::vector<uint32_t> track_ids;
    track_ids.push_back(video_track_id);
    sessions_.insert(std::make_pair(session_id, track_ids));

    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    // Let session run for time record_duration_, during this time buffer with
    // valid data would be received in track callback (VideoTrackYUVDataCb).
    sleep(record_duration_);

    ret = recorder_.StopSession(session_id, false);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.CreateVideoTrack(session_id, video_track_id,
                                     video_track_param, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = SetCameraFocalLength(4.71);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    sleep(record_duration_);

    ret = recorder_.StopSession(session_id, false);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    ClearSessions();
    dump_bitstream_.CloseAll();
  }

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());
}

/*
 * SessionWith720pEnc1080EncTracksChangeFocalLength: This test will test session
 * with 1080p h264 track. Api test sequence:
 *  - StartCamera
 *   loop Start {
 *   ------------------
 *   - CreateSession (1)
 *   - CreateVideoTrack
 *   - SetCameraFocalLength (4.83)
 *   - StartSession (1)
 *   - CreateSession (2)
 *   - CreateVideoTrack
 *   - SetCameraFocalLength (4.71)
 *   - StartSession (2)
 *   - StopSession (2)
 *   - DeleteVideoTrack (2)
 *   - StopSession (1)
 *   - DeleteVideoTrack (1)
 *   - SetCameraFocalLength (4.83)
 *   - CreateVideoTrack
 *   - StartSession (1)
 *   - StopSession (1)
 *   - DeleteVideoTrack (1)
 *   - DeleteSession (1)
 *   - DeleteSession (2)
 *   ------------------
 *   } loop End
 *  - StopCamera
 */
TEST_F(VideoGtest, SessionWith720pEnc1080EncTracksChangeFocalLength) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  VideoFormat format_type;
  uint32_t width;
  uint32_t height;

  ret = recorder_.StartCamera(camera_id_, 30);
  ASSERT_TRUE(ret == NO_ERROR);

  for(uint32_t i = 1; i <= iteration_count_; i++) {
    format_type = VideoFormat::kAVC;
    width  = 1280;
    height = 720;

    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
        test_info_->name(), i);

    SessionCb session_status_cb = CreateSessionStatusCb();
    uint32_t session_id;
    ret = recorder_.CreateSession(session_status_cb, &session_id);
    ASSERT_TRUE(session_id > 0);
    ASSERT_TRUE(ret == NO_ERROR);
    VideoTrackCreateParam video_track_param{camera_id_, format_type,
                                            width, height, 4};
    uint32_t video_track_id = 1;

    if (dump_bitstream_.IsEnabled()) {
      StreamDumpInfo dumpinfo = { format_type, session_id, video_track_id,
                                  width, height };
      ret = dump_bitstream_.SetUp(dumpinfo);
      ASSERT_TRUE(ret == NO_ERROR);
    }

    TrackCb video_track_cb;
    video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
      std::vector<BufferDescriptor> buffers,
      std::vector<MetaData> meta_buffers) {
        VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
      };

    video_track_cb.event_cb = [&] (uint32_t track_id, EventType event_type,
        void *event_data, size_t event_data_size) { VideoTrackEventCb(track_id,
        event_type, event_data, event_data_size); };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id,
                                      video_track_param, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    std::vector<uint32_t> track_ids;
    track_ids.push_back(video_track_id);
    sessions_.insert(std::make_pair(session_id, track_ids));

    ret = SetCameraFocalLength(4.83);
    ASSERT_TRUE(ret == NO_ERROR);

    // low resolution preview
    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    // low
    sleep(record_duration_);

    uint32_t session_id2;
    ret = recorder_.CreateSession(session_status_cb, &session_id2);
    ASSERT_TRUE(session_id2 > 0);
    ASSERT_TRUE(ret == NO_ERROR);
    width = 1920;
    height = 1080;
    VideoTrackCreateParam video_track_param2{camera_id_, format_type,
                                            width, height, 30};
    uint32_t video_track_id2 = 2;

    if (dump_bitstream_.IsEnabled()) {
      StreamDumpInfo dumpinfo = { format_type, session_id, video_track_id2,
                                  width, height };
      ret = dump_bitstream_.SetUp(dumpinfo);
      ASSERT_TRUE(ret == NO_ERROR);
    }

    TrackCb video_track_cb2;
    video_track_cb2.data_cb = [&, session_id2] (uint32_t track_id,
      std::vector<BufferDescriptor> buffers,
      std::vector<MetaData> meta_buffers) {
        VideoTrackEncDataCb(session_id2, track_id, buffers, meta_buffers);
      };

    video_track_cb2.event_cb = [&] (uint32_t track_id, EventType event_type,
        void *event_data, size_t event_data_size) {
        VideoTrackEventCb(track_id, event_type, event_data, event_data_size); };

    ret = SetCameraFocalLength(4.71);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.CreateVideoTrack(session_id2, video_track_id2,
                                      video_track_param2, video_track_cb2);
    ASSERT_TRUE(ret == NO_ERROR);

    std::vector<uint32_t> track2_ids;
    track2_ids.push_back(video_track_id2);
    sessions_.insert(std::make_pair(session_id2, track2_ids));

    // high resolution preview
    ret = recorder_.StartSession(session_id2);
    ASSERT_TRUE(ret == NO_ERROR);

    // low + high
    sleep(record_duration_);

    // high resolution preview
    ret = recorder_.StopSession(session_id2, false);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id2, video_track_id2);
    ASSERT_TRUE(ret == NO_ERROR);

    // low resolution preview
    ret = recorder_.StopSession(session_id, false);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = SetCameraFocalLength(4.83);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.CreateVideoTrack(session_id, video_track_id,
                                      video_track_param, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    // low resolution preview
    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    // low
    sleep(record_duration_);

    // low resolution preview
    ret = recorder_.StopSession(session_id, false);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteSession(session_id2);
    ASSERT_TRUE(ret == NO_ERROR);

    ClearSessions();
    dump_bitstream_.CloseAll();
  }
  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());
}

/*
* SessionWith1440p30FPSSmoothZoom: This test will test session with
*                                  one 1440p60 and demostrate zoom.
*
* Api test sequence:
*  - StartCamera
*   loop Start {
*   ------------------
*   - CreateSession
*   - CreateVideoTrack
*   - StartSession
*   - StopSession
*   - DeleteVideoTrack
*   - DeleteSession
*   ------------------
*   } loop End
*  - StopCamera
*/
TEST_F(VideoGtest, SessionWith1440p30FPSSmoothZoom) {
  fprintf(stderr, "\n---------- Run Test %s.%s ------------\n",
          test_info_->test_case_name(), test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.StartCamera(camera_id_, 30);
  ASSERT_TRUE(ret == NO_ERROR);

  uint32_t video_track_id_1440p_avc = 1;

  CameraMetadata meta;

  for (uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr, "test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
              test_info_->name(), i);
    SessionCb session_status_cb = CreateSessionStatusCb();
    uint32_t session_id;
    ret = recorder_.CreateSession(session_status_cb, &session_id);
    ASSERT_TRUE(session_id > 0);
    ASSERT_TRUE(ret == NO_ERROR);

    uint32_t track_width = 1920;
    uint32_t track_height = 1440;
    if (dump_bitstream_.IsEnabled()) {
      StreamDumpInfo dumpinfo1 = {VideoFormat::kAVC, session_id,
                                  video_track_id_1440p_avc, track_width, track_height};
      ret = dump_bitstream_.SetUp(dumpinfo1);
      ASSERT_TRUE(ret == NO_ERROR);
    }
    VideoTrackCreateParam video_track_param{camera_id_, VideoFormat::kAVC,
                                            track_width, track_height, 30};
    TrackCb video_track_cb;
    video_track_cb.data_cb = [&, session_id](
        uint32_t track_id, std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
      VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
    };

    video_track_cb.event_cb = [&](uint32_t track_id, EventType event_type,
                                  void *event_data, size_t event_data_size) {
      VideoTrackEventCb(track_id, event_type, event_data, event_data_size);
    };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_1440p_avc,
                                     video_track_param, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    std::vector<uint32_t> track_ids;
    track_ids.push_back(video_track_id_1440p_avc);
    sessions_.insert(std::make_pair(session_id, track_ids));

    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    // Record for sometime.
    sleep(5);

    // Set zoom.
    auto status = recorder_.GetCameraParam(camera_id_, meta);
    ASSERT_TRUE(status == NO_ERROR);

    CameraMetadata static_meta;
    ret = recorder_.GetCameraCharacteristics(camera_id_, static_meta);
    ASSERT_TRUE(ret == NO_ERROR);

    float zoom = 2.0f;
    camera_metadata_entry active_array_size;
    if (static_meta.exists(ANDROID_SENSOR_INFO_ACTIVE_ARRAY_SIZE)) {
      active_array_size = static_meta.find(ANDROID_SENSOR_INFO_ACTIVE_ARRAY_SIZE);
    }
    ASSERT_TRUE(active_array_size.count > 0);

    int32_t x = active_array_size.data.i32[0];
    int32_t y = active_array_size.data.i32[1];
    int32_t width = active_array_size.data.i32[2];
    int32_t height = active_array_size.data.i32[3];
    TEST_INFO("%s: x=%d, y=%d, width=%d, height=%d", __func__, x, y, width,
        height);

    int32_t crop[4];
    crop[2] = static_cast<int32_t>(width / zoom);
    crop[3] = (crop[2] * height / width);
    crop[0] = (width - crop[2]) / 2;
    crop[1] = (height - crop[3]) / 2;

    TEST_INFO("%s: crop[0]=%d, crop[1]=%d, crop[2]=%d, crop[3]=%d", __func__,
        crop[0], crop[1], crop[2], crop[3]);

    ret = meta.update(ANDROID_SCALER_CROP_REGION, crop, 4);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.SetCameraParam(camera_id_, meta);
    ASSERT_TRUE(ret == NO_ERROR);

    sleep(5);

    TEST_INFO("%s: Set it back!!", __func__);

    crop[2] = width;
    crop[3] = (crop[2] * height / width);
    crop[0] = (width - crop[2]) / 2;
    crop[1] = (height - crop[3]) / 2;

    TEST_INFO("%s: crop[0]=%d, crop[1]=%d, crop[2]=%d, crop[3]=%d", __func__,
        crop[0], crop[1], crop[2], crop[3]);

    ret = meta.update(ANDROID_SCALER_CROP_REGION, crop, 4);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.SetCameraParam(camera_id_, meta);
    ASSERT_TRUE(ret == NO_ERROR);

    sleep(5);

    ret = recorder_.StopSession(session_id, false);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_1440p_avc);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    ClearSessions();
    dump_bitstream_.CloseAll();
  }

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  fprintf(stderr, "---------- Test Completed %s.%s ----------\n",
          test_info_->test_case_name(), test_info_->name());
}

/*
* SessionWith960p90FPSSmoothZoom: This test will test session with
*                                  one 1440p60 and demostrate zoom.
*
* Api test sequence:
*  - StartCamera
*   loop Start {
*   ------------------
*   - CreateSession
*   - CreateVideoTrack
*   - StartSession
*   - StopSession
*   - DeleteVideoTrack
*   - DeleteSession
*   ------------------
*   } loop End
*  - StopCamera
*/
TEST_F(VideoGtest, SessionWith960p90FPSSmoothZoom) {
  fprintf(stderr, "\n---------- Run Test %s.%s ------------\n",
          test_info_->test_case_name(), test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.StartCamera(camera_id_, 90);
  ASSERT_TRUE(ret == NO_ERROR);

  uint32_t video_track_id_1440p_avc = 1;

  CameraMetadata meta;

  for (uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr, "test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
              test_info_->name(), i);
    SessionCb session_status_cb = CreateSessionStatusCb();
    uint32_t session_id;
    ret = recorder_.CreateSession(session_status_cb, &session_id);
    ASSERT_TRUE(session_id > 0);
    ASSERT_TRUE(ret == NO_ERROR);

    uint32_t track_width = 1280;
    uint32_t track_height = 960;
    if (dump_bitstream_.IsEnabled()) {
      StreamDumpInfo dumpinfo1 = {VideoFormat::kAVC, session_id,
                                  video_track_id_1440p_avc, track_width, track_height};
      ret = dump_bitstream_.SetUp(dumpinfo1);
      ASSERT_TRUE(ret == NO_ERROR);
    }
    VideoTrackCreateParam video_track_param{camera_id_, VideoFormat::kAVC,
                                            track_width, track_height, 90};
    TrackCb video_track_cb;
    video_track_cb.data_cb = [&, session_id](
        uint32_t track_id, std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
      VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
    };

    video_track_cb.event_cb = [&](uint32_t track_id, EventType event_type,
                                  void *event_data, size_t event_data_size) {
      VideoTrackEventCb(track_id, event_type, event_data, event_data_size);
    };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_1440p_avc,
                                     video_track_param, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    std::vector<uint32_t> track_ids;
    track_ids.push_back(video_track_id_1440p_avc);
    sessions_.insert(std::make_pair(session_id, track_ids));

    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    // Record for sometime.
    sleep(5);

    // Set zoom.
    auto status = recorder_.GetCameraParam(camera_id_, meta);
    ASSERT_TRUE(status == NO_ERROR);

    CameraMetadata static_meta;
    ret = recorder_.GetCameraCharacteristics(camera_id_, static_meta);
    ASSERT_TRUE(ret == NO_ERROR);

    float zoom = 2.0f;
    camera_metadata_entry active_array_size;
    if (static_meta.exists(ANDROID_SENSOR_INFO_ACTIVE_ARRAY_SIZE)) {
      active_array_size = static_meta.find(ANDROID_SENSOR_INFO_ACTIVE_ARRAY_SIZE);
    }
    ASSERT_TRUE(active_array_size.count > 0);

    int32_t x = active_array_size.data.i32[0];
    int32_t y = active_array_size.data.i32[1];
    int32_t width = active_array_size.data.i32[2];
    int32_t height = active_array_size.data.i32[3];
    TEST_INFO("%s: x=%d, y=%d, width=%d, height=%d", __func__, x, y, width,
        height);

    int32_t crop[4];
    crop[2] = static_cast<int32_t>(width / zoom);
    crop[3] = (crop[2] * height / width);
    crop[0] = (width - crop[2]) / 2;
    crop[1] = (height - crop[3]) / 2;

    TEST_INFO("%s: crop[0]=%d, crop[1]=%d, crop[2]=%d, crop[3]=%d", __func__,
        crop[0], crop[1], crop[2], crop[3]);

    ret = meta.update(ANDROID_SCALER_CROP_REGION, crop, 4);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.SetCameraParam(camera_id_, meta);
    ASSERT_TRUE(ret == NO_ERROR);

    sleep(5);

    TEST_INFO("%s: Set it back!!", __func__);

    crop[2] = width;
    crop[3] = (crop[2] * height / width);
    crop[0] = (width - crop[2]) / 2;
    crop[1] = (height - crop[3]) / 2;

    TEST_INFO("%s: crop[0]=%d, crop[1]=%d, crop[2]=%d, crop[3]=%d", __func__,
        crop[0], crop[1], crop[2], crop[3]);

    ret = meta.update(ANDROID_SCALER_CROP_REGION, crop, 4);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.SetCameraParam(camera_id_, meta);
    ASSERT_TRUE(ret == NO_ERROR);

    sleep(5);

    ret = recorder_.StopSession(session_id, false);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_1440p_avc);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    ClearSessions();
    dump_bitstream_.CloseAll();
  }

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  fprintf(stderr, "---------- Test Completed %s.%s ----------\n",
          test_info_->test_case_name(), test_info_->name());
}

/*
* SessionWithSingleCam4kEncLinked4kEncAndVGA: This test will test
*                           single cam session with one 4k Enc track,
*                           1 linked 4k Enc Track and one more VGA stream
*                           from camera
*
* API test sequence:
*  - StartCamera
*   loop Start {
*   ------------------
*   - CreateSession
*   - CreateVideoTrack - 4K avc, Master From Camera
*   - CreateVideoTrack - 4K avc, Linked
*   - CreateVideoTrack - 480p yuv, From Camera
*   - StartSession
*   - StopSession
*   - DeleteVideoTrack - 4K avc, Linked
*   - DeleteVideoTrack - 4K avc, Master From Camera
*   - DeleteVideoTrack - 480p yuv, From Camera
*   - DeleteSession
*   ------------------
*   } loop End
*  - StopCamera
*/
TEST_F(VideoGtest, SessionWithSingleCam4kEncLinked4kEncAndVGA) {
  fprintf(stderr, "\n---------- Run Test %s.%s ------------\n",
          test_info_->test_case_name(), test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);
  ret = recorder_.StartCamera(camera_id_, 30);
  ASSERT_TRUE(ret == NO_ERROR);

  uint32_t video_track_id_4k_avc_main = 1;
  uint32_t video_track_id_vga = 2;
  uint32_t video_track_id_4k_avc_slave = 3;

  for (uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr, "test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
              test_info_->name(), i);

    SessionCb session_status_cb = CreateSessionStatusCb();
    uint32_t session_id;
    ret = recorder_.CreateSession(session_status_cb, &session_id);
    ASSERT_TRUE(session_id > 0);
    ASSERT_TRUE(ret == NO_ERROR);

    if (dump_bitstream_.IsEnabled()) {
      StreamDumpInfo dumpinfo1 = {VideoFormat::kAVC, session_id,
                                  video_track_id_4k_avc_main, 3840, 2160};
      ret = dump_bitstream_.SetUp(dumpinfo1);
      ASSERT_TRUE(ret == NO_ERROR);

      StreamDumpInfo dumpinfo2 = {VideoFormat::kYUV, session_id,
                                  video_track_id_vga, 640, 480};
      ret = dump_bitstream_.SetUp(dumpinfo2);
      ASSERT_TRUE(ret == NO_ERROR);

      StreamDumpInfo dumpinfo3 = {VideoFormat::kAVC, session_id,
                                  video_track_id_4k_avc_slave, 3840, 2160};
      ret = dump_bitstream_.SetUp(dumpinfo3);
      ASSERT_TRUE(ret == NO_ERROR);
    }

    VideoTrackCreateParam video_track_param{camera_id_, VideoFormat::kAVC, 3840,
                                            2160, 30};
    TrackCb video_track_cb;
    video_track_cb.data_cb = [&, session_id](
        uint32_t track_id, std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
      VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
    };

    video_track_cb.event_cb = [&](uint32_t track_id, EventType event_type,
                                  void *event_data, size_t event_data_size) {
      VideoTrackEventCb(track_id, event_type, event_data, event_data_size);
    };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_4k_avc_main,
                                     video_track_param, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    std::vector<uint32_t> track_ids;
    track_ids.push_back(video_track_id_4k_avc_main);

    VideoExtraParam extra_param;
    SourceVideoTrack surface_video_linked_1;
    surface_video_linked_1.source_track_id = video_track_id_4k_avc_main;
    extra_param.Update(QMMF_SOURCE_VIDEO_TRACK_ID, surface_video_linked_1);

    video_track_param.width = 3840;
    video_track_param.height = 2160;

    video_track_cb.data_cb = [&, session_id](
        uint32_t track_id, std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
      VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
    };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_4k_avc_slave,
                                     video_track_param, extra_param,
                                     video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    track_ids.push_back(video_track_id_4k_avc_slave);

    video_track_param.width = 640;
    video_track_param.height = 480;
    video_track_param.format_type = VideoFormat::kYUV;

    video_track_cb.data_cb = [&, session_id](
        uint32_t track_id, std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
      VideoTrackYUVDataCb(session_id, track_id, buffers, meta_buffers);
    };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_vga,
                                     video_track_param, video_track_cb);

    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    // Let session run for time record_duration_, during this time buffer with
    // valid data would be received in track callback (VideoTrackYUVDataCb).
    sleep(record_duration_);

    ret = recorder_.StopSession(session_id, false);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_4k_avc_slave);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_4k_avc_main);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_vga);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    ClearSessions();
    dump_bitstream_.CloseAll();
  }

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  fprintf(stderr, "---------- Test Completed %s.%s ----------\n",
          test_info_->test_case_name(), test_info_->name());
}

/*
* SessionWith4kEnc1080pEncAnd720pYUVTrack: This test will test
*                                   one session with one 4k Enc track,
*                                   1080p Enc and 720p YUV track.
* API test sequence:
*  - StartCamera
*   loop Start {
*   ------------------
*   - CreateSession
*   - CreateVideoTrack - 4k Enc
*   - CreateVideoTrack - 1080p Enc
*   - CreateVideoTrack - 720p YUV
*   - StartSession
*   - StopSession
*   - DeleteVideoTrack - 720p YUV
*   - DeleteVideoTrack - 1080p Enc
*   - DeleteVideoTrack - 4k Enc
*   - DeleteSession
*   ------------------
*   } loop End
*  - StopCamera
*/
TEST_F(VideoGtest, SessionWith4kEnc1080pEncAnd720pYUVTrack) {
  fprintf(stderr, "\n---------- Run Test %s.%s ------------\n",
          test_info_->test_case_name(), test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.StartCamera(camera_id_, 30);
  ASSERT_TRUE(ret == NO_ERROR);

  uint32_t video_track_id_4k_avc1 = 1;
  uint32_t video_track_id_1080p_avc1 = 2;
  uint32_t video_track_id_720p_yuv = 3;

  for (uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr, "test iteration = %d/%d\n", i, iteration_count_);

    SessionCb session_status_cb = CreateSessionStatusCb();
    uint32_t session_id;
    ret = recorder_.CreateSession(session_status_cb, &session_id);
    ASSERT_TRUE(session_id > 0);
    ASSERT_TRUE(ret == NO_ERROR);

    if (dump_bitstream_.IsEnabled()) {
      StreamDumpInfo dumpinfo1 = {VideoFormat::kAVC, session_id,
                                  video_track_id_4k_avc1, 3840, 2160};
      ret = dump_bitstream_.SetUp(dumpinfo1);
      ASSERT_TRUE(ret == NO_ERROR);

      StreamDumpInfo dumpinfo2 = {VideoFormat::kAVC, session_id,
                                  video_track_id_1080p_avc1, 1920, 1080};
      ret = dump_bitstream_.SetUp(dumpinfo2);
      ASSERT_TRUE(ret == NO_ERROR);

      StreamDumpInfo dumpinfo3 = {VideoFormat::kYUV, session_id,
                                  video_track_id_720p_yuv, 1280, 720};
      ret = dump_bitstream_.SetUp(dumpinfo3);
      ASSERT_TRUE(ret == NO_ERROR);
    }
    VideoTrackCreateParam video_track_param{camera_id_, VideoFormat::kAVC,
                                            3840, 2160, 30};
    TrackCb video_track_cb;
    video_track_cb.data_cb = [&, session_id](
        uint32_t track_id, std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
      VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
    };

    video_track_cb.event_cb = [&](uint32_t track_id, EventType event_type,
                                  void *event_data, size_t event_data_size) {
      VideoTrackEventCb(track_id, event_type, event_data, event_data_size);
    };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_4k_avc1,
                                     video_track_param, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    std::vector<uint32_t> track_ids;
    track_ids.push_back(video_track_id_4k_avc1);

    video_track_param.width = 1920;
    video_track_param.height = 1080;

    video_track_cb.data_cb = [&, session_id](
        uint32_t track_id, std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
      VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
    };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_1080p_avc1,
                                     video_track_param, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    track_ids.push_back(video_track_id_1080p_avc1);

    video_track_param.width = 1280;
    video_track_param.height = 720;
    video_track_param.format_type = VideoFormat::kYUV;
    if (ubwc_stream_enable_) {
      video_track_param.low_power_mode = true;
    }
    video_track_cb.data_cb = [&, session_id](
        uint32_t track_id, std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
      VideoTrackYUVDataCb(session_id, track_id, buffers, meta_buffers);
    };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_720p_yuv,
                                     video_track_param, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    track_ids.push_back(video_track_id_720p_yuv);
    sessions_.insert(std::make_pair(session_id, track_ids));

    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    sleep(record_duration_);

    ret = recorder_.StopSession(session_id, false);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_720p_yuv);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_1080p_avc1);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_4k_avc1);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    ClearSessions();
    dump_bitstream_.CloseAll();
  }

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  fprintf(stderr, "---------- Test Completed %s.%s ----------\n",
          test_info_->test_case_name(), test_info_->name());
}

#ifdef CAM_ARCH_V2
/*
* SessionWithSingleCam4KEncDeFogTables: This case will test a single cam
*                 session with 3840x2160 h264 encoded track, during which
*                 in regular intervals DeFog Table changes.
* API test sequence:
*  - StartCamera
*   loop Start {
*   --------------------------
*   - CreateSession
*   - CreateVideoTrack
*   - StartSession
*   - Change defog table after every record_duration_ seconds
*   - StopSession
*   - DeleteVideoTrack
*   - DeleteSession
*   --------------------------
*   } loop End
*  - StopCamera
*/

TEST_F(VideoGtest, SessionWithSingleCam4KEncDeFogTables) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  std::vector<DeFogTable> defog_tables;

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.StartCamera(camera_id_, 30);
  ASSERT_TRUE(ret == NO_ERROR);

  VideoFormat format_type = VideoFormat::kAVC;
  uint32_t stream_width   = 3840;
  uint32_t stream_height  = 2160;

  for (uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
        test_info_->name(), i);

    SessionCb session_status_cb = CreateSessionStatusCb();
    uint32_t session_id;
    ret = recorder_.CreateSession(session_status_cb, &session_id);
    ASSERT_TRUE(session_id > 0);
    ASSERT_TRUE(ret == NO_ERROR);
    VideoTrackCreateParam video_track_param{camera_id_, format_type,
                                            stream_width,
                                            stream_height,
                                            30};

    uint32_t video_track_id = 1;

    if (dump_bitstream_.IsEnabled()) {
      StreamDumpInfo dumpinfo = { video_track_param.format_type, session_id,
                                  video_track_id, stream_width,
                                  stream_height };
      ret = dump_bitstream_.SetUp(dumpinfo);
      ASSERT_TRUE(ret == NO_ERROR);
    }

    TrackCb video_track_cb;
    video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
                              std::vector<BufferDescriptor> buffers,
                              std::vector<MetaData> meta_buffers) {
    VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers); };

    video_track_cb.event_cb = [&] (uint32_t track_id, EventType event_type,
                               void *event_data, size_t event_data_size) {
    VideoTrackEventCb(track_id, event_type, event_data, event_data_size); };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id,
                                     video_track_param, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    if (defog_tables.size() > 0) {
      defog_tables.clear();
    }

    ret = PopulateDeFogTables(defog_tables);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    camera_metadata_entry_t entry;
    uint32_t defog_tables_vtag;
    uint32_t defog_tables_range_vtag;
    CameraMetadata meta;
    int32_t min_irange = 0, max_irange = 0;
    float min_frange = 0, max_frange = 0;

    ret = recorder_.GetCameraParam(camera_id_, meta);
    ASSERT_TRUE(ret == NO_ERROR);

    CameraMetadata static_meta;
    ret = recorder_.GetCameraCharacteristics(camera_id_, static_meta);
    ASSERT_TRUE(ret == NO_ERROR);

    for (auto defog_table : defog_tables) {
      if (VendorTagSupported(String8("enable"),
                             String8("org.quic.camera.defog"),
                             &defog_tables_vtag)) {
        ret = meta.update(defog_tables_vtag, &defog_table.enable, 1);
        ASSERT_TRUE(ret == NO_ERROR);
      }

      if (VendorTagSupported(String8("algo_type"),
                             String8("org.quic.camera.defog"),
                             &defog_tables_vtag)) {
        ret = meta.update(defog_tables_vtag, &defog_table.algo_type, 1);
        ASSERT_TRUE(ret == NO_ERROR);
      }

      if (VendorTagSupported(String8("algo_decision_mode"),
                             String8("org.quic.camera.defog"),
                             &defog_tables_vtag)) {
        ret = meta.update(defog_tables_vtag, &defog_table.algo_decision_mode,
                          1);
        ASSERT_TRUE(ret == NO_ERROR);
      }

      if (VendorTagExistsInMeta(static_meta, String8("strength_range"),
                                String8("org.quic.camera.defog"),
                                &defog_tables_range_vtag)) {
        entry = static_meta.find(defog_tables_range_vtag);
        min_frange = entry.data.f[0];
        max_frange = entry.data.f[1];
        if (defog_table.strength < min_frange ||
            defog_table.strength > max_frange) {
          defog_table.strength = (min_frange + max_frange) / 2;

          TEST_INFO("%s: min_defog_strength = %f, max_defog_strength = %f.. "
                    "Resetting strength to %f", __func__, min_frange,
                    max_frange, defog_table.strength);
        }

        if (VendorTagSupported(String8("strength"),
                               String8("org.quic.camera.defog"),
                               &defog_tables_vtag)) {
          ret = meta.update(defog_tables_vtag, &defog_table.strength, 1);
          ASSERT_TRUE(ret == NO_ERROR);
        }
      }

      if (VendorTagExistsInMeta(static_meta, String8("convergence_speed_range"),
                                String8("org.quic.camera.defog"),
                                &defog_tables_range_vtag)) {
        entry = static_meta.find(defog_tables_range_vtag);
        min_irange = entry.data.i32[0];
        max_irange = entry.data.i32[1];
        if (defog_table.convergence_speed < min_irange ||
            defog_table.convergence_speed > max_irange) {
          defog_table.convergence_speed = (min_irange + max_irange) / 2;

          TEST_INFO("%s: min_defog_speed = %d, max_defog_speed = %d.. "
                    "Resetting speed to %d", __func__, min_irange, max_irange,
                    defog_table.convergence_speed);
        }

        if (VendorTagSupported(String8("convergence_speed"),
                               String8("org.quic.camera.defog"),
                               &defog_tables_vtag)) {
          ret = meta.update(defog_tables_vtag, &defog_table.convergence_speed,
                            1);
          ASSERT_TRUE(ret == NO_ERROR);
        }
      }

      if (VendorTagExistsInMeta(static_meta, String8("lp_color_comp_gain_range"),
                                String8("org.quic.camera.defog"),
                                &defog_tables_range_vtag)) {
        entry = static_meta.find(defog_tables_range_vtag);
        min_frange = entry.data.f[0];
        max_frange = entry.data.f[1];
        if (defog_table.lp_color_comp_gain < min_frange ||
            defog_table.lp_color_comp_gain > max_frange) {
          defog_table.lp_color_comp_gain = (min_frange + max_frange) / 2;

          TEST_INFO("%s: min_defog_lp_color_comp_gain = %f, "
                    "max_defog_lp_color_comp_gain = %f.. Resetting "
                    "lp_color_comp_gain to %f", __func__, min_frange,
                    max_frange, defog_table.lp_color_comp_gain);
        }

        if (VendorTagSupported(String8("lp_color_comp_gain"),
                               String8("org.quic.camera.defog"),
                               &defog_tables_vtag)) {
          ret = meta.update(defog_tables_vtag, &defog_table.lp_color_comp_gain,
                            1);
          ASSERT_TRUE(ret == NO_ERROR);
        }
      }

      if (VendorTagSupported(String8("abc_en"),
                             String8("org.quic.camera.defog"),
                             &defog_tables_vtag)) {
        ret = meta.update(defog_tables_vtag, &defog_table.abc_en, 1);
        ASSERT_TRUE(ret == NO_ERROR);
      }

      if (VendorTagSupported(String8("acc_en"),
                             String8("org.quic.camera.defog"),
                             &defog_tables_vtag)) {
        ret = meta.update(defog_tables_vtag, &defog_table.acc_en, 1);
        ASSERT_TRUE(ret == NO_ERROR);
      }

      if (VendorTagSupported(String8("afsd_en"),
                             String8("org.quic.camera.defog"),
                             &defog_tables_vtag)) {
        ret = meta.update(defog_tables_vtag, &defog_table.afsd_en, 1);
        ASSERT_TRUE(ret == NO_ERROR);
      }

      if (VendorTagSupported(String8("afsd_2a_en"),
                             String8("org.quic.camera.defog"),
                             &defog_tables_vtag)) {
        ret = meta.update(defog_tables_vtag, &defog_table.afsd_2a_en, 1);
        ASSERT_TRUE(ret == NO_ERROR);
      }

      if (VendorTagExistsInMeta(static_meta, String8("defog_dark_thres_range"),
                                String8("org.quic.camera.defog"),
                                &defog_tables_range_vtag)) {
        entry = static_meta.find(defog_tables_range_vtag);
        min_irange = entry.data.i32[0];
        max_irange = entry.data.i32[1];
        if (defog_table.defog_dark_thres < min_irange ||
            defog_table.defog_dark_thres > max_irange) {
          defog_table.defog_dark_thres = (min_irange + max_irange) / 2;

          TEST_INFO("%s: min_defog_dark_thres = %d, max_defog_dark_thres = %d"
                    ".. Resetting defog_dark_thres to %d", __func__,
                    min_irange, max_irange, defog_table.defog_dark_thres);
        }
        if (VendorTagSupported(String8("defog_dark_thres"),
                               String8("org.quic.camera.defog"),
                               &defog_tables_vtag)) {
          ret = meta.update(defog_tables_vtag, &defog_table.defog_dark_thres,
                            1);
          ASSERT_TRUE(ret == NO_ERROR);
        }
      }

      if (VendorTagExistsInMeta(static_meta, String8("defog_bright_thres_range"),
                                String8("org.quic.camera.defog"),
                                &defog_tables_range_vtag)) {
        entry = static_meta.find(defog_tables_range_vtag);
        min_irange = entry.data.i32[0];
        max_irange = entry.data.i32[1];
        if (defog_table.defog_bright_thres < min_irange ||
            defog_table.defog_bright_thres > max_irange) {
          defog_table.defog_bright_thres = (min_irange + max_irange) / 2;

          TEST_INFO("%s: min_defog_bright_thres = %d, "
                    "max_defog_bright_thres = %d.. Resetting "
                    "defog_bright_thres to %d", __func__, min_irange,
                    max_irange, defog_table.defog_bright_thres);
        }

        if (VendorTagSupported(String8("defog_bright_thres"),
                               String8("org.quic.camera.defog"),
                               &defog_tables_vtag)) {
          ret = meta.update(defog_tables_vtag, &defog_table.defog_bright_thres,
                            1);
          ASSERT_TRUE(ret == NO_ERROR);
        }
      }

      if (VendorTagExistsInMeta(static_meta, String8("abc_gain_range"),
                                String8("org.quic.camera.defog"),
                                &defog_tables_range_vtag)) {
        entry = static_meta.find(defog_tables_range_vtag);
        min_frange = entry.data.f[0];
        max_frange = entry.data.f[1];
        if (defog_table.abc_gain < min_frange ||
            defog_table.abc_gain > max_frange) {
          defog_table.abc_gain = (min_frange + max_frange) / 2;

          TEST_INFO("%s: min_defog_abc_gain = %f, max_defog_abc_gain = %f.. "
                    "Resetting abc_gain to %f", __func__, min_frange,
                    max_frange, defog_table.abc_gain);
        }

        if (VendorTagSupported(String8("abc_gain"),
                               String8("org.quic.camera.defog"),
                               &defog_tables_vtag)) {
          ret = meta.update(defog_tables_vtag, &defog_table.abc_gain, 1);
          ASSERT_TRUE(ret == NO_ERROR);
        }
      }

      if (VendorTagExistsInMeta(static_meta, String8("acc_max_dark_str_range"),
                                String8("org.quic.camera.defog"),
                                &defog_tables_range_vtag)) {
        entry = static_meta.find(defog_tables_range_vtag);
        min_frange = entry.data.f[0];
        max_frange = entry.data.f[1];
        if (defog_table.acc_max_dark_str < min_frange ||
            defog_table.acc_max_dark_str > max_frange) {
          defog_table.acc_max_dark_str = (min_frange + max_frange) / 2;

          TEST_INFO("%s: min_defog_acc_max_dark_str = %f, "
                    "max_defog_acc_max_dark_str = %f.. Resetting "
                    "acc_max_dark_str to %f", __func__, min_frange, max_frange,
                    defog_table.acc_max_dark_str);
        }

        if (VendorTagSupported(String8("acc_max_dark_str"),
                               String8("org.quic.camera.defog"),
                               &defog_tables_vtag)) {
          ret = meta.update(defog_tables_vtag, &defog_table.acc_max_dark_str,
                            1);
          ASSERT_TRUE(ret == NO_ERROR);
        }
      }

      if (VendorTagExistsInMeta(static_meta, String8("acc_max_bright_str_range"),
                                String8("org.quic.camera.defog"),
                                &defog_tables_range_vtag)) {
        entry = static_meta.find(defog_tables_range_vtag);
        min_frange = entry.data.f[0];
        max_frange = entry.data.f[1];
        if (defog_table.acc_max_bright_str < min_frange ||
            defog_table.acc_max_bright_str > max_frange) {
          defog_table.acc_max_bright_str = (min_frange + max_frange) / 2;

          TEST_INFO("%s: min_defog_acc_max_bright_str = %f, "
                    "max_defog_acc_max_bright_str = %f.. Resetting "
                    "acc_max_bright_str to %f", __func__, min_frange,
                    max_frange, defog_table.acc_max_bright_str);
        }

        if (VendorTagSupported(String8("acc_max_bright_str"),
                               String8("org.quic.camera.defog"),
                               &defog_tables_vtag)) {
          ret = meta.update(defog_tables_vtag, &defog_table.acc_max_bright_str,
                            1);
          ASSERT_TRUE(ret == NO_ERROR);
        }
      }

      if (VendorTagExistsInMeta(static_meta, String8("dark_limit_range"),
                                String8("org.quic.camera.defog"),
                                &defog_tables_range_vtag)) {
        entry = static_meta.find(defog_tables_range_vtag);
        min_irange = entry.data.i32[0];
        max_irange = entry.data.i32[1];
        if (defog_table.dark_limit < min_irange ||
            defog_table.dark_limit > max_irange) {
          defog_table.dark_limit = (min_irange + max_irange) / 2;

          TEST_INFO("%s: min_defog_dark_limit = %d, "
                    "max_defog_dark_limit = %d.. Resetting "
                    "dark_limit to %d", __func__, min_irange, max_irange,
                    defog_table.dark_limit);
        }

        if (VendorTagSupported(String8("dark_limit"),
                               String8("org.quic.camera.defog"),
                               &defog_tables_vtag)) {
          ret = meta.update(defog_tables_vtag, &defog_table.dark_limit, 1);
          ASSERT_TRUE(ret == NO_ERROR);
        }
      }

      if (VendorTagExistsInMeta(static_meta, String8("bright_limit_range"),
                                String8("org.quic.camera.defog"),
                                &defog_tables_range_vtag)) {
        entry = static_meta.find(defog_tables_range_vtag);
        min_irange = entry.data.i32[0];
        max_irange = entry.data.i32[1];
        if (defog_table.bright_limit < min_irange ||
            defog_table.bright_limit > max_irange) {
          defog_table.bright_limit = (min_irange + max_irange) / 2;

          TEST_INFO("%s: min_defog_bright_limit = %d, "
                    "max_defog_bright_limit = %d.. Resetting "
                    "bright_limit to %d", __func__, min_irange, max_irange,
                    defog_table.bright_limit);
        }

        if (VendorTagSupported(String8("bright_limit"),
                               String8("org.quic.camera.defog"),
                               &defog_tables_vtag)) {
          ret = meta.update(defog_tables_vtag, &defog_table.bright_limit, 1);
          ASSERT_TRUE(ret == NO_ERROR);
        }
      }

      if (VendorTagExistsInMeta(static_meta, String8("dark_preserve_range"),
                                String8("org.quic.camera.defog"),
                                &defog_tables_range_vtag)) {
        entry = static_meta.find(defog_tables_range_vtag);
        min_irange = entry.data.i32[0];
        max_irange = entry.data.i32[1];
        if (defog_table.dark_preserve < min_irange ||
            defog_table.dark_preserve > max_irange) {
          defog_table.dark_preserve = (min_irange + max_irange) / 2;

          TEST_INFO("%s: min_defog_dark_preserve = %d, "
                    "max_defog_dark_preserve = %d.. Resetting "
                    "dark_preserve to %d", __func__, min_irange, max_irange,
                    defog_table.dark_preserve);
        }

        if (VendorTagSupported(String8("dark_preserve"),
                               String8("org.quic.camera.defog"),
                               &defog_tables_vtag)) {
          ret = meta.update(defog_tables_vtag, &defog_table.dark_preserve, 1);
          ASSERT_TRUE(ret == NO_ERROR);
        }
      }

      if (VendorTagExistsInMeta(static_meta, String8("bright_preserve_range"),
                                String8("org.quic.camera.defog"),
                                &defog_tables_range_vtag)) {
        entry = static_meta.find(defog_tables_range_vtag);
        min_irange = entry.data.i32[0];
        max_irange = entry.data.i32[1];
        if (defog_table.bright_preserve < min_irange ||
            defog_table.bright_preserve > max_irange) {
          defog_table.bright_preserve = (min_irange + max_irange) / 2;

          TEST_INFO("%s: min_defog_bright_preserve = %d, "
                    "max_defog_bright_preserve = %d.. Resetting "
                    "bright_preserve to %d", __func__, min_irange, max_irange,
                    defog_table.bright_preserve);
        }

        if (VendorTagSupported(String8("bright_preserve"),
                               String8("org.quic.camera.defog"),
                               &defog_tables_vtag)) {
          ret = meta.update(defog_tables_vtag, &defog_table.bright_preserve,
                            1);
          ASSERT_TRUE(ret == NO_ERROR);
        }
      }

      if (VendorTagExistsInMeta(static_meta, String8("dnr_trigparam_start_range"),
                                String8("org.quic.camera.defog"),
                                &defog_tables_range_vtag)) {
        entry = static_meta.find(defog_tables_range_vtag);
        min_frange = entry.data.f[0];
        max_frange = entry.data.f[1];
        for (int dnr_index = 0; dnr_index < 3; dnr_index++) {
          if (defog_table.trig_params.dnr_trigger[dnr_index].start <
              min_frange ||
              defog_table.trig_params.dnr_trigger[dnr_index].start >
              max_frange) {
            defog_table.trig_params.dnr_trigger[dnr_index].start =
              (min_frange + max_frange) / 2;

            TEST_INFO("%s: min_defog_dnr_trigparam[%d]_start = %f, "
                      "max_defog_dnr_trigparam[%d]_start = %f.. Resetting "
                      "dnr_trigparam[%d]_start to %f", __func__, dnr_index,
                      min_frange, dnr_index, max_frange, dnr_index,
                      defog_table.trig_params.dnr_trigger[dnr_index].start);
          }
        }
      }

      if (VendorTagExistsInMeta(static_meta, String8("dnr_trigparam_end_range"),
                                String8("org.quic.camera.defog"),
                                &defog_tables_range_vtag)) {
        entry = static_meta.find(defog_tables_range_vtag);
        min_frange = entry.data.f[0];
        max_frange = entry.data.f[1];
        for (int dnr_index = 0; dnr_index < 3; dnr_index++) {
          if (defog_table.trig_params.dnr_trigger[dnr_index].end < min_frange ||
              defog_table.trig_params.dnr_trigger[dnr_index].end > max_frange) {
            defog_table.trig_params.dnr_trigger[dnr_index].end =
              (min_frange + max_frange) / 2;

            TEST_INFO("%s: min_defog_dnr_trigparam[%d]_end = %f, "
                      "max_defog_dnr_trigparam[%d]_end = %f.. Resetting "
                      "dnr_trigparam[%d]_end to %f", __func__, dnr_index,
                      min_frange, dnr_index, max_frange, dnr_index,
                      defog_table.trig_params.dnr_trigger[dnr_index].end);
          }
        }
      }

      if (VendorTagExistsInMeta(static_meta, String8("dnr_trigparam_fog_range"),
                                String8("org.quic.camera.defog"),
                                &defog_tables_range_vtag)) {
        entry = static_meta.find(defog_tables_range_vtag);
        min_irange = entry.data.i32[0];
        max_irange = entry.data.i32[1];
        for (int dnr_index = 0; dnr_index < 3; dnr_index++) {
          if (defog_table.trig_params.dnr_trigger[dnr_index].fog_p <
              min_irange ||
              defog_table.trig_params.dnr_trigger[dnr_index].fog_p >
              max_irange) {
            defog_table.trig_params.dnr_trigger[dnr_index].fog_p =
              (min_irange + max_irange) / 2;

            TEST_INFO("%s: min_defog_dnr_trigparam[%d]_fog_p = %d, "
                      "max_defog_dnr_trigparam[%d]_fog_p = %d.. Resetting "
                      "dnr_trigparam[%d]_fog_p to %d", __func__, dnr_index,
                      min_irange, dnr_index, max_irange, dnr_index,
                      defog_table.trig_params.dnr_trigger[dnr_index].fog_p);
          }
        }
      }

      if (VendorTagExistsInMeta(static_meta, String8("lux_trigparam_start_range"),
                                String8("org.quic.camera.defog"),
                                &defog_tables_range_vtag)) {
        entry = static_meta.find(defog_tables_range_vtag);
        min_frange = entry.data.f[0];
        max_frange = entry.data.f[1];
        for (int lux_index = 0; lux_index < 3; lux_index++) {
          if (defog_table.trig_params.lux_trigger[lux_index].start <
              min_frange ||
              defog_table.trig_params.lux_trigger[lux_index].start >
              max_frange) {
            defog_table.trig_params.lux_trigger[lux_index].start =
              (min_frange + max_frange) / 2;

            TEST_INFO("%s: min_defog_lux_trigparam[%d]_start = %f, "
                      "max_defog_lux_trigparam[%d]_start = %f.. Resetting "
                      "lux_trigparam[%d]_start to %f", __func__, lux_index,
                      min_frange, lux_index, max_frange, lux_index,
                      defog_table.trig_params.lux_trigger[lux_index].start);
          }
        }
      }

      if (VendorTagExistsInMeta(static_meta, String8("lux_trigparam_end_range"),
                                String8("org.quic.camera.defog"),
                                &defog_tables_range_vtag)) {
        entry = static_meta.find(defog_tables_range_vtag);
        min_frange = entry.data.f[0];
        max_frange = entry.data.f[1];
        for (int lux_index = 0; lux_index < 3; lux_index++) {
          if (defog_table.trig_params.lux_trigger[lux_index].end < min_frange ||
              defog_table.trig_params.lux_trigger[lux_index].end > max_frange) {
            defog_table.trig_params.lux_trigger[lux_index].end =
              (min_frange + max_frange) / 2;

            TEST_INFO("%s: min_defog_lux_trigparam[%d]_end = %f, "
                      "max_defog_lux_trigparam[%d]_end = %f.. Resetting "
                      "lux_trigparam[%d]_end to %f", __func__, lux_index,
                      min_frange, lux_index, max_frange, lux_index,
                      defog_table.trig_params.lux_trigger[lux_index].end);
          }
        }
      }

      if (VendorTagExistsInMeta(static_meta, String8("lux_trigparam_fog_range"),
                                String8("org.quic.camera.defog"),
                                &defog_tables_range_vtag)) {
        entry = static_meta.find(defog_tables_range_vtag);
        min_irange = entry.data.i32[0];
        max_irange = entry.data.i32[1];
        for (int lux_index = 0; lux_index < 3; lux_index++) {
          if (defog_table.trig_params.lux_trigger[lux_index].fog_p <
              min_irange ||
              defog_table.trig_params.lux_trigger[lux_index].fog_p >
              max_irange) {
            defog_table.trig_params.lux_trigger[lux_index].fog_p =
              (min_irange + max_irange) / 2;

            TEST_INFO("%s: min_defog_lux_trigparam[%d]_fog_p = %d, "
                      "max_defog_lux_trigparam[%d]_fog_p = %d.. Resetting "
                      "lux_trigparam[%d]_fog_p to %d", __func__, lux_index,
                      min_irange, lux_index, max_irange, lux_index,
                      defog_table.trig_params.lux_trigger[lux_index].fog_p);
          }
        }
      }

      if (VendorTagExistsInMeta(static_meta, String8("cct_trigparam_start_range"),
                                String8("org.quic.camera.defog"),
                                &defog_tables_range_vtag)) {
        entry = static_meta.find(defog_tables_range_vtag);
        min_frange = entry.data.f[0];
        max_frange = entry.data.f[1];
        for (int cct_index = 0; cct_index < 4; cct_index++) {
          if (defog_table.trig_params.cct_trigger[cct_index].start <
              min_frange ||
              defog_table.trig_params.cct_trigger[cct_index].start >
              max_frange) {
            defog_table.trig_params.cct_trigger[cct_index].start =
              (min_frange + max_frange) / 2;

            TEST_INFO("%s: min_defog_cct_trigparam[%d]_start = %f, "
                      "max_defog_cct_trigparam[%d]_start = %f.. Resetting "
                      "cct_trigparam[%d]_start to %f", __func__, cct_index,
                      min_frange, cct_index, max_frange, cct_index,
                      defog_table.trig_params.cct_trigger[cct_index].start);
          }
        }
      }

      if (VendorTagExistsInMeta(static_meta, String8("cct_trigparam_end_range"),
                                String8("org.quic.camera.defog"),
                                &defog_tables_range_vtag)) {
        entry = static_meta.find(defog_tables_range_vtag);
        min_frange = entry.data.f[0];
        max_frange = entry.data.f[1];
        for (int cct_index = 0; cct_index < 4; cct_index++) {
          if (defog_table.trig_params.cct_trigger[cct_index].end < min_frange ||
              defog_table.trig_params.cct_trigger[cct_index].end > max_frange) {
            defog_table.trig_params.cct_trigger[cct_index].end =
              (min_frange + max_frange) / 2;

            TEST_INFO("%s: min_defog_cct_trigparam[%d]_end = %f, "
                      "max_defog_cct_trigparam[%d]_end = %f.. Resetting "
                      "cct_trigparam[%d]_end to %f", __func__, cct_index,
                      min_frange, cct_index, max_frange, cct_index,
                      defog_table.trig_params.cct_trigger[cct_index].end);
          }
        }
      }

      if (VendorTagExistsInMeta(static_meta, String8("cct_trigparam_fog_range"),
                                String8("org.quic.camera.defog"),
                                &defog_tables_range_vtag)) {
        entry = static_meta.find(defog_tables_range_vtag);
        min_irange = entry.data.i32[0];
        max_irange = entry.data.i32[1];
        for (int cct_index = 0; cct_index < 4; cct_index++) {
          if (defog_table.trig_params.cct_trigger[cct_index].fog_p <
              min_irange ||
              defog_table.trig_params.cct_trigger[cct_index].fog_p >
              max_irange) {
            defog_table.trig_params.cct_trigger[cct_index].fog_p =
              (min_irange + max_irange) / 2;

            TEST_INFO("%s: min_defog_cct_trigparam[%d]_fog_p = %d, "
                      "max_defog_cct_trigparam[%d]_fog_p = %d.. Resetting "
                      "cct_trigparam[%d]_fog_p to %d", __func__, cct_index,
                      min_irange, cct_index, max_irange, cct_index,
                      defog_table.trig_params.cct_trigger[cct_index].fog_p);
          }
        }
      }

      if (VendorTagSupported(String8("trig_params"),
                             String8("org.quic.camera.defog"),
                             &defog_tables_vtag)) {
        uint8_t *trig_params_bytes = reinterpret_cast<uint8_t *>(
                                     &defog_table.trig_params);
        ret = meta.update(defog_tables_vtag, trig_params_bytes,
                          sizeof(defog_table.trig_params));
        ASSERT_TRUE(ret == NO_ERROR);
      }

      ret = recorder_.SetCameraParam(camera_id_, meta);
      ASSERT_TRUE(ret == NO_ERROR);

      sleep(record_duration_);
    }

    ret = recorder_.StopSession(session_id, false);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    dump_bitstream_.CloseAll();
  }

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());
}

/*
* SessionWithSingleCam4KEncDynamicAecConvSpeed: This will test session with one 4K h264
*                                               track with dynamic AEC convergence speed.
* Api test sequence:
*  - StartCamera
*   loop Start {
*   ------------------
*   - CreateSession
*   - CreateVideoTrack
*   - StartVideoTrack
*   - StartSession
*   - Set invalid conv speed
*   - Set valid conv speed 1
*   - Set valid conv speed 2
*   - StopSession
*   - DeleteVideoTrack
*   - DeleteSession
*   ------------------
*   } loop End
*  - StopCamera
*/

TEST_F(VideoGtest, SessionWithSingleCam4KEncDynamicAecConvSpeed) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.StartCamera(camera_id_, 30);
  ASSERT_TRUE(ret == NO_ERROR);

  VideoFormat format_type = VideoFormat::kAVC;
  uint32_t stream_width  = 3840;
  uint32_t stream_height = 2160;

  for (uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
        test_info_->name(), i);

    SessionCb session_status_cb = CreateSessionStatusCb();

    uint32_t session_id;
    ret = recorder_.CreateSession(session_status_cb, &session_id);
    ASSERT_TRUE(session_id > 0);
    ASSERT_TRUE(ret == NO_ERROR);
    VideoTrackCreateParam video_track_param{camera_id_, format_type,
                                            stream_width, stream_height, 30};

    uint32_t video_track_id = 1;

    if (dump_bitstream_.IsEnabled()) {
      StreamDumpInfo dumpinfo = {
        video_track_param.format_type,
        session_id,
        video_track_id,
        stream_width,
        stream_height };
      ret = dump_bitstream_.SetUp(dumpinfo);
      ASSERT_TRUE(ret == NO_ERROR);
    }

    TrackCb video_track_cb;
    video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
                              std::vector<BufferDescriptor> buffers,
                              std::vector<MetaData> meta_buffers) {
      VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers); };

    video_track_cb.event_cb = [&] (uint32_t track_id, EventType event_type,
                               void *event_data, size_t event_data_size) {
      VideoTrackEventCb(track_id, event_type, event_data, event_data_size); };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id,
                                     video_track_param, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    uint32_t aec_speed_vtag;
    float aec_speed;

    CameraMetadata meta;
    ret = recorder_.GetCameraParam(camera_id_, meta);
    ASSERT_TRUE(ret == NO_ERROR);

    if (VendorTagSupported(String8("aec_speed"),
        String8("org.codeaurora.qcamera3.aec_convergence_speed"),
        &aec_speed_vtag)) {

      // Setting an invalid value to AEC convergence speed
      // so that default value will be picked from chromatix.
      aec_speed = 0.0f;
      fprintf(stderr,"aec_speed = %f\n", aec_speed);
      ret = meta.update(aec_speed_vtag, &aec_speed, 1);
      ASSERT_TRUE(ret == NO_ERROR);

      ret = recorder_.SetCameraParam(camera_id_, meta);
      ASSERT_TRUE(ret == NO_ERROR);

      // Setting an valid value to AEC convergence speed
      aec_speed = 0.5f;
      fprintf(stderr, "aec_speed = %f\n", aec_speed);
      ret = meta.update(aec_speed_vtag, &aec_speed, 1);
      ASSERT_TRUE(ret == NO_ERROR);

      ret = recorder_.SetCameraParam(camera_id_, meta);
      ASSERT_TRUE(ret == NO_ERROR);

      sleep(record_duration_ / 2);

      // Setting an valid value to AEC convergence speed
      aec_speed = 0.9f;
      fprintf(stderr, "aec_speed = %f\n", aec_speed);
      ret = meta.update(aec_speed_vtag, &aec_speed, 1);
      ASSERT_TRUE(ret == NO_ERROR);

      ret = recorder_.SetCameraParam(camera_id_, meta);
      ASSERT_TRUE(ret == NO_ERROR);

      sleep(record_duration_ / 2);

    }

    ret = recorder_.StopSession(session_id, false);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    dump_bitstream_.CloseAll();
  }

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());
}

/*
* SessionWithSingleCam4KEncDynamicAwbConvSpeed: This will test session with one 4K h264
*                                               track with dynamic AWB convergence speed.
* Api test sequence:
*  - StartCamera
*   loop Start {
*   ------------------
*   - CreateSession
*   - CreateVideoTrack
*   - StartVideoTrack
*   - StartSession
*   - Set invalid conv speed
*   - Set valid conv speed 1
*   - Set valid conv speed 2
*   - StopSession
*   - DeleteVideoTrack
*   - DeleteSession
*   ------------------
*   } loop End
*  - StopCamera
*/

TEST_F(VideoGtest, SessionWithSingleCam4KEncDynamicAwbConvSpeed) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.StartCamera(camera_id_, 30);
  ASSERT_TRUE(ret == NO_ERROR);

  VideoFormat format_type = VideoFormat::kAVC;
  uint32_t stream_width  = 3840;
  uint32_t stream_height = 2160;

  for (uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
        test_info_->name(), i);

    SessionCb session_status_cb = CreateSessionStatusCb();

    uint32_t session_id;
    ret = recorder_.CreateSession(session_status_cb, &session_id);
    ASSERT_TRUE(session_id > 0);
    ASSERT_TRUE(ret == NO_ERROR);
    VideoTrackCreateParam video_track_param{camera_id_, format_type,
                                            stream_width, stream_height, 30};

    uint32_t video_track_id = 1;

    if (dump_bitstream_.IsEnabled()) {
      StreamDumpInfo dumpinfo = {
        video_track_param.format_type,
        session_id,
        video_track_id,
        stream_width,
        stream_height };
      ret = dump_bitstream_.SetUp(dumpinfo);
      ASSERT_TRUE(ret == NO_ERROR);
    }

    TrackCb video_track_cb;
    video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
                              std::vector<BufferDescriptor> buffers,
                              std::vector<MetaData> meta_buffers) {
      VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers); };

    video_track_cb.event_cb = [&] (uint32_t track_id, EventType event_type,
                               void *event_data, size_t event_data_size) {
      VideoTrackEventCb(track_id, event_type, event_data, event_data_size); };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id,
                                     video_track_param, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    uint32_t awb_speed_vtag;
    float awb_speed;

    CameraMetadata meta;
    ret = recorder_.GetCameraParam(camera_id_, meta);
    ASSERT_TRUE(ret == NO_ERROR);

    if (VendorTagSupported(String8("awb_speed"),
        String8("org.codeaurora.qcamera3.awb_convergence_speed"),
        &awb_speed_vtag)) {

      // Setting an invalid value to AWB convergence speed,
      // so that default value will be picked from chromatix.
      awb_speed = 0.0f;
      fprintf(stderr,"awb_speed = %f\n", awb_speed);
      ret = meta.update(awb_speed_vtag, &awb_speed, 1);
      ASSERT_TRUE(ret == NO_ERROR);

      ret = recorder_.SetCameraParam(camera_id_, meta);
      ASSERT_TRUE(ret == NO_ERROR);

      sleep(record_duration_ / 2);

      // Setting an valid value to AWB convergence speed
      awb_speed = 0.5f;
      fprintf(stderr, "awb_speed = %f\n", awb_speed);
      ret = meta.update(awb_speed_vtag, &awb_speed, 1);
      ASSERT_TRUE(ret == NO_ERROR);

      ret = recorder_.SetCameraParam(camera_id_, meta);
      ASSERT_TRUE(ret == NO_ERROR);

      sleep(record_duration_ / 2);

      // Setting an valid value to AWB convergence speed
      awb_speed = 0.9f;
      fprintf(stderr, "awb_speed = %f\n", awb_speed);
      ret = meta.update(awb_speed_vtag, &awb_speed, 1);
      ASSERT_TRUE(ret == NO_ERROR);

      ret = recorder_.SetCameraParam(camera_id_, meta);
      ASSERT_TRUE(ret == NO_ERROR);

      sleep(record_duration_ / 2);
    }

    ret = recorder_.StopSession(session_id, false);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    dump_bitstream_.CloseAll();
  }

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());
}

/*
* SessionWithDualCam4k30EncRescale1080p30EncAnd1080p30YUVWithTNRAndZZHDR: This
*                           test will test Dual cam session with one 4k Enc
*                           track, one 1080p Enc Track Rescale and one 1080p LPM.
*                           Note: camera_id_ for dual cam to be set using
*                           adb property.
* Api test sequence:
*  - StartCamera
*   loop Start {
*   ------------------
*   - CreateSession
*   - CreateVideoTrack with zzHDR - Master
*   - CreateVideoTrack - Rescale
*   - CreateVideoTrack - LPM
*   - StartSession
*   - Enable TNR
*   - Enable Overlay
*   - Disable Overlay
*   - Disable TNR
*   - StopSession
*   - DeleteVideoTrack - LPM
*   - DeleteVideoTrack - Rescale
*   - DeleteVideoTrack - Master
*   - DeleteSession
*   ------------------
*   } loop End
*  - StopCamera
*/
TEST_F(VideoGtest, SessionWithDualCam4k30EncRescale1080p30EncAnd1080p30YUVWithTNRAndZZHDR) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  // Setting Enable HDR Extra Param
  CameraExtraParam extra_param_hdr;
  VideoHDRMode vid_hdr_mode;
  vid_hdr_mode.enable = true;
  extra_param_hdr.Update(QMMF_VIDEO_HDR_MODE, vid_hdr_mode);
  ret = recorder_.StartCamera(camera_id_, 30, extra_param_hdr);
  ASSERT_TRUE(ret == NO_ERROR);

  uint32_t video_track_id_4k_avc     = 1;
  uint32_t video_track_id_1080p_avc  = 2;
  uint32_t video_track_id_1080p_yuv  = 3;

  for(uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
        test_info_->name(), i);

    SessionCb session_status_cb = CreateSessionStatusCb();
    uint32_t session_id;
    ret = recorder_.CreateSession(session_status_cb, &session_id);
    ASSERT_TRUE(session_id > 0);
    ASSERT_TRUE(ret == NO_ERROR);

    if (dump_bitstream_.IsEnabled()) {
      StreamDumpInfo dumpinfo1 = {
        VideoFormat::kAVC,
        session_id,
        video_track_id_4k_avc, 4096, 2048
      };
      ret = dump_bitstream_.SetUp(dumpinfo1);
      ASSERT_TRUE(ret == NO_ERROR);

      StreamDumpInfo dumpinfo2 = {
        VideoFormat::kAVC,
        session_id,
        video_track_id_1080p_avc, 2160, 1080
      };
      ret = dump_bitstream_.SetUp(dumpinfo2);
      ASSERT_TRUE(ret == NO_ERROR);
    }

    VideoTrackCreateParam video_track_param{camera_id_, VideoFormat::kAVC,
                                            4096,
                                            2048,
                                            30};
    video_track_param.codec_param.avc.bitrate = 100000000;

    TrackCb video_track_cb;
    video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
        std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
          VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
        };

    video_track_cb.event_cb = [&] (uint32_t track_id, EventType event_type,
        void *event_data, size_t event_data_size) { VideoTrackEventCb(track_id,
        event_type, event_data, event_data_size); };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_4k_avc,
                                     video_track_param,
                                     video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    std::vector<uint32_t> track_ids;
    track_ids.push_back(video_track_id_4k_avc);

    video_track_param.width  = 2160;
    video_track_param.height = 1080;
    video_track_param.codec_param.avc.bitrate = 4000000;

    video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
        std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
          VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
    };

    VideoExtraParam extra_param;
    SourceVideoTrack surface_video_linked;
    surface_video_linked.source_track_id = video_track_id_4k_avc;
    extra_param.Update(QMMF_SOURCE_VIDEO_TRACK_ID, surface_video_linked);

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_1080p_avc,
                                     video_track_param, extra_param,
                                     video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    track_ids.push_back(video_track_id_1080p_avc);

    video_track_param.format_type = VideoFormat::kYUV;
    video_track_param.low_power_mode = true;

    video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
        std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
          VideoTrackYUVDataCb(session_id, track_id, buffers, meta_buffers); };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_1080p_yuv,
                                     video_track_param,
                                     video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    track_ids.push_back(video_track_id_1080p_yuv);
    sessions_.insert(std::make_pair(session_id, track_ids));

    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    //Enable TNR - High quality mode.
    CameraMetadata meta;
    ret= recorder_.GetCameraParam(camera_id_, meta);
    CameraMetadata static_meta;
    ret = recorder_.GetCameraCharacteristics(camera_id_, static_meta);
    ASSERT_TRUE(ret == NO_ERROR);
    if (NO_ERROR == ret) {
      if (static_meta.exists(ANDROID_NOISE_REDUCTION_MODE)) {
        const uint8_t tnr_mode = ANDROID_NOISE_REDUCTION_MODE_HIGH_QUALITY;
        TEST_INFO("%s Enable TNR mode(%d)", __func__, tnr_mode);
        meta.update(ANDROID_NOISE_REDUCTION_MODE, &tnr_mode, 1);
        ret= recorder_.SetCameraParam(camera_id_, meta);
        ASSERT_TRUE(ret == NO_ERROR);
      }
    }

    if (is_apply_overlay_) {
      // Enable overlay
      uint32_t mask_id_4k;
      CreatePrivacyMaskOverlay(video_track_id_4k_avc, 4096, 2048, &mask_id_4k);
      sleep(record_duration_);
      DestroyPrivacyMaskOverlay(video_track_id_4k_avc, mask_id_4k);
    } else {
      sleep(record_duration_);
    }

    ret = recorder_.StopSession(session_id, false);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_1080p_yuv);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_1080p_avc);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_4k_avc);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    ClearSessions();
  }

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  dump_bitstream_.CloseAll();
  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());
}

/*
* SessionWithDualCam4k60EncRescale1080p30EncAnd1080p30YUVWithTNRAndZZHDR: This
*                           test will test Dual cam session with one 4k60 Enc
*                           track, one 1080p Enc Track Rescale and one 1080p LPM.
*                           Note: camera_id_ for dual cam to be set using
*                           adb property.
* Api test sequence:
*  - StartCamera
*   loop Start {
*   ------------------
*   - CreateSession
*   - CreateVideoTrack with zzHDR - Master
*   - CreateVideoTrack - Rescale
*   - CreateVideoTrack - LPM
*   - StartSession
*   - Enable TNR
*   - Enable Overlay
*   - Disable Overlay
*   - Disable TNR
*   - StopSession
*   - DeleteVideoTrack - LPM
*   - DeleteVideoTrack - Rescale
*   - DeleteVideoTrack - Master
*   - DeleteSession
*   ------------------
*   } loop End
*  - StopCamera
*/
TEST_F(VideoGtest, SessionWithDualCam4k60EncRescale1080p30EncAnd1080p30YUVWithTNRAndZZHDR) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  // Setting Enable HDR Extra Param
  CameraExtraParam extra_param_hdr;
  VideoHDRMode vid_hdr_mode;
  vid_hdr_mode.enable = true;
  extra_param_hdr.Update(QMMF_VIDEO_HDR_MODE, vid_hdr_mode);
  ret = recorder_.StartCamera(camera_id_, 60, extra_param_hdr);
  ASSERT_TRUE(ret == NO_ERROR);

  uint32_t video_track_id_4k_avc     = 1;
  uint32_t video_track_id_1080p_avc  = 2;
  uint32_t video_track_id_1080p_yuv  = 3;

  for(uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
        test_info_->name(), i);

    SessionCb session_status_cb = CreateSessionStatusCb();
    uint32_t session_id;
    ret = recorder_.CreateSession(session_status_cb, &session_id);
    ASSERT_TRUE(session_id > 0);
    ASSERT_TRUE(ret == NO_ERROR);

    if (dump_bitstream_.IsEnabled()) {
      StreamDumpInfo dumpinfo1 = {
       VideoFormat::kAVC,
       session_id,
       video_track_id_4k_avc, 4096, 2048
      };
     ret = dump_bitstream_.SetUp(dumpinfo1);
      ASSERT_TRUE(ret == NO_ERROR);

      StreamDumpInfo dumpinfo2 = {
        VideoFormat::kAVC,
        session_id,
        video_track_id_1080p_avc, 2160, 1080
      };
      ret = dump_bitstream_.SetUp(dumpinfo2);
      ASSERT_TRUE(ret == NO_ERROR);
   }

    VideoTrackCreateParam video_track_param{camera_id_, VideoFormat::kAVC,
                                            4096,
                                            2048,
                                            60};
    video_track_param.codec_param.avc.bitrate = 135000000;

    TrackCb video_track_cb;
    video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
        std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
          VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
        };

    video_track_cb.event_cb = [&] (uint32_t track_id, EventType event_type,
        void *event_data, size_t event_data_size) { VideoTrackEventCb(track_id,
        event_type, event_data, event_data_size); };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_4k_avc,
                                     video_track_param,
                                     video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    std::vector<uint32_t> track_ids;
    track_ids.push_back(video_track_id_4k_avc);

    video_track_param.width  = 2160;
    video_track_param.height = 1080;
    video_track_param.frame_rate = 30;
    video_track_param.codec_param.avc.bitrate = 4000000;

    video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
        std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
          VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
    };

    VideoExtraParam extra_param;
    SourceVideoTrack surface_video_linked;
    surface_video_linked.source_track_id = video_track_id_4k_avc;
    extra_param.Update(QMMF_SOURCE_VIDEO_TRACK_ID, surface_video_linked);

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_1080p_avc,
                                     video_track_param, extra_param,
                                     video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    track_ids.push_back(video_track_id_1080p_avc);

    video_track_param.format_type = VideoFormat::kYUV;
    video_track_param.low_power_mode = true;

    video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
        std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
          VideoTrackYUVDataCb(session_id, track_id, buffers, meta_buffers); };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_1080p_yuv,
                                     video_track_param,
                                     video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    track_ids.push_back(video_track_id_1080p_yuv);
    sessions_.insert(std::make_pair(session_id, track_ids));

    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    //Enable TNR - High quality mode.
    CameraMetadata meta;
    ret= recorder_.GetCameraParam(camera_id_, meta);
    CameraMetadata static_meta;
    ret = recorder_.GetCameraCharacteristics(camera_id_, static_meta);
    ASSERT_TRUE(ret == NO_ERROR);
    if (NO_ERROR == ret) {
      if (static_meta.exists(ANDROID_NOISE_REDUCTION_MODE)) {
        const uint8_t tnr_mode = ANDROID_NOISE_REDUCTION_MODE_HIGH_QUALITY;
        TEST_INFO("%s Enable TNR mode(%d)", __func__, tnr_mode);
        meta.update(ANDROID_NOISE_REDUCTION_MODE, &tnr_mode, 1);
        ret= recorder_.SetCameraParam(camera_id_, meta);
        ASSERT_TRUE(ret == NO_ERROR);
      }
    }

    if (is_apply_overlay_) {
      // Enable overlay
      uint32_t mask_id_4k;
      CreatePrivacyMaskOverlay(video_track_id_4k_avc, 4096, 2048, &mask_id_4k);
      sleep(record_duration_);
      DestroyPrivacyMaskOverlay(video_track_id_4k_avc, mask_id_4k);
    } else {
      sleep(record_duration_);
    }

    ret = recorder_.StopSession(session_id, false);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_1080p_yuv);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_1080p_avc);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_4k_avc);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    ClearSessions();
  }

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  dump_bitstream_.CloseAll();
  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());
}

/*
* SessionWithDualCam5_7k30EncRescale1080p30EncAnd1080p30YUVWithTNRAndZZHDR: This
*                           test will test Dual cam session with one 5.7k Enc
*                           track, one 1080p Enc Track Rescale and one 1080p LPM.
*                           Note: camera_id_ for dual cam to be set using
*                           adb property.
* Api test sequence:
*  - StartCamera
*   loop Start {
*   ------------------
*   - CreateSession
*   - CreateVideoTrack with zzHDR - Master
*   - CreateVideoTrack - Rescale
*   - CreateVideoTrack - LPM
*   - StartSession
*   - Enable TNR
*   - Enable Overlay
*   - Disable Overlay
*   - Disable TNR
*   - StopSession
*   - DeleteVideoTrack - LPM
*   - DeleteVideoTrack - Rescale
*   - DeleteVideoTrack - Master
*   - DeleteSession
*   ------------------
*   } loop End
*  - StopCamera
*/
TEST_F(VideoGtest, SessionWithDualCam5_7k30EncRescale1080p30EncAnd1080p30YUVWithTNRAndZZHDR) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  // Setting Enable HDR Extra Param
  CameraExtraParam extra_param_hdr;
  VideoHDRMode vid_hdr_mode;
  vid_hdr_mode.enable = true;
  extra_param_hdr.Update(QMMF_VIDEO_HDR_MODE, vid_hdr_mode);
  ret = recorder_.StartCamera(camera_id_, 30, extra_param_hdr);
  ASSERT_TRUE(ret == NO_ERROR);

  uint32_t video_track_id_5_7k_avc   = 1;
  uint32_t video_track_id_1080p_avc  = 2;
  uint32_t video_track_id_1080p_yuv  = 3;

  for(uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
        test_info_->name(), i);

    SessionCb session_status_cb = CreateSessionStatusCb();

    uint32_t session_id;
    ret = recorder_.CreateSession(session_status_cb, &session_id);
    ASSERT_TRUE(session_id > 0);
    ASSERT_TRUE(ret == NO_ERROR);

    if (dump_bitstream_.IsEnabled()) {
      StreamDumpInfo dumpinfo1 = {
        VideoFormat::kAVC,
        session_id,
        video_track_id_5_7k_avc, 5760, 2880
      };
      ret = dump_bitstream_.SetUp(dumpinfo1);
      ASSERT_TRUE(ret == NO_ERROR);

      StreamDumpInfo dumpinfo2 = {
        VideoFormat::kAVC,
        session_id,
        video_track_id_1080p_avc, 2160, 1080
      };
      ret = dump_bitstream_.SetUp(dumpinfo2);
      ASSERT_TRUE(ret == NO_ERROR);
    }

    VideoTrackCreateParam video_track_param{camera_id_, VideoFormat::kAVC,
                                            5760,
                                            2880,
                                            30};
    video_track_param.codec_param.avc.bitrate = 160000000;

    TrackCb video_track_cb;
    video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
        std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
          VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
        };

    video_track_cb.event_cb = [&] (uint32_t track_id, EventType event_type,
        void *event_data, size_t event_data_size) { VideoTrackEventCb(track_id,
        event_type, event_data, event_data_size); };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_5_7k_avc,
                                     video_track_param,
                                     video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    std::vector<uint32_t> track_ids;
    track_ids.push_back(video_track_id_5_7k_avc);

    video_track_param.width  = 2160;
    video_track_param.height = 1080;
    video_track_param.codec_param.avc.bitrate = 4000000;

    video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
        std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
          VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
    };

    VideoExtraParam extra_param;
    SourceVideoTrack surface_video_linked;
    surface_video_linked.source_track_id = video_track_id_5_7k_avc;
    extra_param.Update(QMMF_SOURCE_VIDEO_TRACK_ID, surface_video_linked);

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_1080p_avc,
                                     video_track_param, extra_param,
                                     video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    track_ids.push_back(video_track_id_1080p_avc);

    video_track_param.format_type = VideoFormat::kYUV;
    video_track_param.low_power_mode = true;

    video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
        std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
          VideoTrackYUVDataCb(session_id, track_id, buffers, meta_buffers); };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_1080p_yuv,
                                     video_track_param,
                                     video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    track_ids.push_back(video_track_id_1080p_yuv);
    sessions_.insert(std::make_pair(session_id, track_ids));

    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    //Enable TNR - High quality mode.
    CameraMetadata meta;
    ret= recorder_.GetCameraParam(camera_id_, meta);
    CameraMetadata static_meta;
    ret = recorder_.GetCameraCharacteristics(camera_id_, static_meta);
    ASSERT_TRUE(ret == NO_ERROR);
    if (NO_ERROR == ret) {
      if (static_meta.exists(ANDROID_NOISE_REDUCTION_MODE)) {
        const uint8_t tnr_mode = ANDROID_NOISE_REDUCTION_MODE_HIGH_QUALITY;
        TEST_INFO("%s Enable TNR mode(%d)", __func__, tnr_mode);
        meta.update(ANDROID_NOISE_REDUCTION_MODE, &tnr_mode, 1);
        ret= recorder_.SetCameraParam(camera_id_, meta);
        ASSERT_TRUE(ret == NO_ERROR);
      }
    }

    if (is_apply_overlay_) {
      // Enable overlay
      uint32_t mask_id_5_7k;
      CreatePrivacyMaskOverlay(video_track_id_5_7k_avc, 5760, 2880, &mask_id_5_7k);
      sleep(record_duration_);
      DestroyPrivacyMaskOverlay(video_track_id_5_7k_avc, mask_id_5_7k);
    } else {
      sleep(record_duration_);
    }

    ret = recorder_.StopSession(session_id, false);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_1080p_yuv);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_1080p_avc);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_5_7k_avc);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    ClearSessions();
  }

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  dump_bitstream_.CloseAll();
  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());
}

/*
* SessionWith4kEncAnd720pEncWithIRFilterModes: This case will test a session
              with 4k and 720p encoded track(LPM) with IR Mode Toggled.
              This test will take snapshot as well.
* API test sequence:
*   StartCamera
*   loop Start {
*   --------------------------
*   - CreateSession
*   - CreateVideoTrack (4k and 720p)
*   - StartVideoTrack
*   - StartSession
*   - Toggle Cut Filter
*   - Take Snapshot
*   - Toggle IR Cut Filter
*   - Take Snapshot
*   - StopSession
*   - DeleteVideoTrack
*   - DeleteSession
*   --------------------------
*   } loop End
*  StopCamera
*/
TEST_F(VideoGtest, SessionWith4kEncAnd720pEncWithIRFilterModes) {
  fprintf(stderr, "\n---------- Run Test %s.%s ------------\n",
          test_info_->test_case_name(), test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.StartCamera(camera_id_, 30);
  ASSERT_TRUE(ret == NO_ERROR);

  uint32_t video_track_id_4k = 1;
  uint32_t video_track_id_720p_avc = 2;

  for (uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr, "test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
              test_info_->name(), i);

    SessionCb session_status_cb = CreateSessionStatusCb();
    uint32_t session_id;

    ret = recorder_.CreateSession(session_status_cb, &session_id);
    ASSERT_TRUE(session_id > 0);
    ASSERT_TRUE(ret == NO_ERROR);

    if (dump_bitstream_.IsEnabled()) {
      StreamDumpInfo dumpinfo1 = {VideoFormat::kAVC, session_id,
                                  video_track_id_4k, 3840, 2160};
      ret = dump_bitstream_.SetUp(dumpinfo1);
      ASSERT_TRUE(ret == NO_ERROR);

      StreamDumpInfo dumpinfo2 = {VideoFormat::kAVC, session_id,
                                  video_track_id_720p_avc, 1280, 720};
      ret = dump_bitstream_.SetUp(dumpinfo2);
      ASSERT_TRUE(ret == NO_ERROR);
    }

    VideoTrackCreateParam video_track_param{camera_id_, VideoFormat::kAVC, 3840,
                                            2160, 30};
    TrackCb video_track_cb;
    video_track_cb.data_cb = [&, session_id](
        uint32_t track_id, std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
      VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
    };

    video_track_cb.event_cb = [&](uint32_t track_id, EventType event_type,
                                  void *event_data, size_t event_data_size) {
      VideoTrackEventCb(track_id, event_type, event_data, event_data_size);
    };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_4k,
                                     video_track_param, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    std::vector<uint32_t> track_ids;
    track_ids.push_back(video_track_id_4k);
    video_track_param.width = 1280;
    video_track_param.height = 720;
    video_track_param.low_power_mode = true;
    video_track_cb.data_cb = [&, session_id](
        uint32_t track_id, std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
      VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
    };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_720p_avc,
                                     video_track_param, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    track_ids.push_back(video_track_id_720p_avc);
    sessions_.insert(std::make_pair(session_id, track_ids));

    ImageParam image_param{};
    image_param.width = 3840;
    image_param.height = 2160;
    image_param.image_format = ImageFormat::kJPEG;
    image_param.image_quality = default_jpeg_quality_;
    std::vector<CameraMetadata> meta_array;
    CameraMetadata meta, meta_img;

    ret = recorder_.GetDefaultCaptureParam(camera_id_, meta_img);
    ASSERT_TRUE(ret == NO_ERROR);

    CameraMetadata static_meta;
    ret = recorder_.GetCameraCharacteristics(camera_id_, static_meta);
    ASSERT_TRUE(ret == NO_ERROR);

    bool res_supported = GtestCommon::ValidateResFromJpegSizes(
        static_meta, image_param.width, image_param.height);
    ASSERT_TRUE(res_supported != false);

    ImageCaptureCb cb = [this](uint32_t camera_id, uint32_t image_count,
                               BufferDescriptor buffer,
                               MetaData meta_data) -> void {
      SnapshotCb(camera_id, image_count, buffer, meta_data);
    };

    ret = recorder_.StartSession(session_id);

    sleep(record_duration_ / 5);

    int32_t modes[2];
    std::map<int32_t, std::string> ir_modes_map;
    ret = recorder_.GetCameraParam(camera_id_, meta);
    ASSERT_TRUE(ret == NO_ERROR);
    uint32_t ir_modes_supported_vtag, mode_vtag;

    if (VendorTagSupported(String8("ir_modes_supported"),
                           String8("org.codeaurora.qcamera3.ir_led"),
                           &ir_modes_supported_vtag)) {
      auto entry = static_meta.find(ir_modes_supported_vtag);
      modes[0] = entry.data.i32[0];
      modes[1] = entry.data.i32[1];
      ir_modes_map.insert(std::make_pair(modes[0], "Off"));
      ir_modes_map.insert(std::make_pair(modes[1], "On"));
      fprintf(stderr, "ir mode supported : %u , %u\n", modes[0], modes[1]);
    } else {
      TEST_ERROR("%s: ir_modes_supported is not supported", __func__);
      ASSERT_TRUE(0);
    }

    if (!VendorTagSupported(String8("mode"),
                           String8("org.codeaurora.qcamera3.ir_led"),
                           &mode_vtag)) {
      TEST_ERROR("%s: mode_vtag is not supported", __func__);
      ASSERT_TRUE(0);
    }

    auto it = ir_modes_map.begin();
    it++;
    // Turn on IR Cut Filter
    ret = meta.update(mode_vtag, &it->first, 1);
    ASSERT_TRUE(ret == NO_ERROR);
    ret = meta_img.update(mode_vtag, &it->first, 1);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.SetCameraParam(camera_id_, meta);
    ASSERT_TRUE(ret == NO_ERROR);

    meta_array.push_back(meta_img);
    ret = recorder_.CaptureImage(camera_id_, image_param, 1, meta_array, cb);
    ASSERT_TRUE(ret == NO_ERROR);
    sleep(record_duration_ / 5);
    ret = recorder_.CancelCaptureImage(camera_id_);
    ASSERT_TRUE(ret == NO_ERROR);

    sleep(record_duration_ / 5);
    it--;

    // Turn off cut filter
    ret = meta.update(mode_vtag, &it->first, 1);
    ASSERT_TRUE(ret == NO_ERROR);
    ret = meta_img.update(mode_vtag, &it->first, 1);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.SetCameraParam(camera_id_, meta);
    ASSERT_TRUE(ret == NO_ERROR);

    meta_array.clear();
    meta_array.push_back(meta_img);
    ret = recorder_.CaptureImage(camera_id_, image_param, 1, meta_array, cb);
    ASSERT_TRUE(ret == NO_ERROR);

    sleep(record_duration_ / 5);

    ret = recorder_.CancelCaptureImage(camera_id_);
    ASSERT_TRUE(ret == NO_ERROR);

    meta_array.clear();

    sleep(record_duration_ / 5);

    ret = recorder_.StopSession(session_id, false);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_720p_avc);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_4k);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    ClearSessions();
    dump_bitstream_.CloseAll();
  }

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  fprintf(stderr, "---------- Test Completed %s.%s ----------\n",
          test_info_->test_case_name(), test_info_->name());
}

/*
* SessionWith4kEncWithIRFilterModes: This case will test a session
              with 4k with IR Mode Toggled.
* API test sequence:
*   StartCamera
*   loop Start {
*   --------------------------
*   - CreateSession
*   - CreateVideoTrack (4k)
*   - StartVideoTrack
*   - StartSession
*   - Toggle Cut Filter
*   - Toggle IR Cut Filter
*   - Take Snapshot
*   - StopSession
*   - DeleteVideoTrack
*   - DeleteSession
*   --------------------------
*   } loop End
*  StopCamera
*/
TEST_F(VideoGtest, SessionWith4kEncWithIRFilterModes) {
  fprintf(stderr, "\n---------- Run Test %s.%s ------------\n",
          test_info_->test_case_name(), test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.StartCamera(camera_id_, 30);
  ASSERT_TRUE(ret == NO_ERROR);

  uint32_t video_track_id_4k = 1;
  CameraMetadata meta;

  for (uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr, "test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
              test_info_->name(), i);

    SessionCb session_status_cb = CreateSessionStatusCb();
    uint32_t session_id;

    ret = recorder_.CreateSession(session_status_cb, &session_id);
    ASSERT_TRUE(session_id > 0);
    ASSERT_TRUE(ret == NO_ERROR);

    if (dump_bitstream_.IsEnabled()) {
      StreamDumpInfo dumpinfo1 = {VideoFormat::kAVC, session_id,
                                  video_track_id_4k, 3840, 2160};
      ret = dump_bitstream_.SetUp(dumpinfo1);
      ASSERT_TRUE(ret == NO_ERROR);
    }

    VideoTrackCreateParam video_track_param{camera_id_, VideoFormat::kAVC,
                                            3840, 2160, 30};

    TrackCb video_track_cb;
    video_track_cb.data_cb = [&, session_id](
        uint32_t track_id, std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
      VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
    };

    video_track_cb.event_cb = [&](uint32_t track_id, EventType event_type,
                                  void *event_data, size_t event_data_size) {
      VideoTrackEventCb(track_id, event_type, event_data, event_data_size);
    };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_4k,
                                     video_track_param, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    std::vector<uint32_t> track_ids;
    track_ids.push_back(video_track_id_4k);
    sessions_.insert(std::make_pair(session_id, track_ids));

    ret = recorder_.StartSession(session_id);

    int32_t modes[2];
    std::map<int32_t, std::string> ir_modes_map;
    ret = recorder_.GetCameraParam(camera_id_, meta);
    ASSERT_TRUE(ret == NO_ERROR);
    CameraMetadata static_meta;
    ret = recorder_.GetCameraCharacteristics(camera_id_, static_meta);
    ASSERT_TRUE(ret == NO_ERROR);
    uint32_t ir_modes_supported_vtag, mode_vtag;

    if (VendorTagSupported(String8("ir_modes_supported"),
                           String8("org.codeaurora.qcamera3.ir_led"),
                           &ir_modes_supported_vtag)) {
      auto entry = static_meta.find(ir_modes_supported_vtag);
      modes[0] = entry.data.i32[0];
      modes[1] = entry.data.i32[1];
      ir_modes_map.insert(std::make_pair(modes[0], "Off"));
      ir_modes_map.insert(std::make_pair(modes[1], "On"));
      fprintf(stderr, "ir mode supported : %u , %u\n", modes[0], modes[1]);
    } else {
      TEST_ERROR("%s: ir_modes_supported is not supported", __func__);
      ASSERT_TRUE(0);
    }

    if (!VendorTagSupported(String8("mode"),
                            String8("org.codeaurora.qcamera3.ir_led"),
                            &mode_vtag)) {
      TEST_ERROR("%s: mode_vtag is not supported", __func__);
      ASSERT_TRUE(0);
    }

    auto it = ir_modes_map.begin();
    it++;
    // Turn on IR Cut Filter
    ret = meta.update(mode_vtag, &it->first, 1);
    ASSERT_TRUE(ret == NO_ERROR);
    ret = recorder_.SetCameraParam(camera_id_, meta);
    ASSERT_TRUE(ret == NO_ERROR);

    sleep(record_duration_ / 2);

    // Turn off cut filter
    it--;
    ret = meta.update(mode_vtag, &it->first, 1);
    ASSERT_TRUE(ret == NO_ERROR);
    ret = recorder_.SetCameraParam(camera_id_, meta);
    ASSERT_TRUE(ret == NO_ERROR);

    sleep(record_duration_ / 2);

    ret = recorder_.StopSession(session_id, false);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_4k);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    ClearSessions();
    dump_bitstream_.CloseAll();
  }

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  fprintf(stderr, "---------- Test Completed %s.%s ----------\n",
          test_info_->test_case_name(), test_info_->name());
}

/*
* SessionWith4k2k60fpsEncAnd720p30fpsEncAndLinked720p30fpsYUV:
*           This test will test: session with one 4k2k 60fps hevc track,
*           session with one 720p 30fps h264 track, one linked tracked to
*           720p30 stream in yuv.
* API test sequence:
*  - StartCamera
*  - CreateSession
*  - CreateVideoTrack1 - one 4k2k@60 hevc - force sensor mode 10
*  - CreateVideoTrack2 - one 720p@30 h264
*  - CreateVideoTrack3 - one linked 720p@30 YUV
*  - StartSession
*  - StopSession
*  - DeleteVideoTrack3 - one linked 720p@30 YUV
*  - DeleteVideoTrack2 - one 720p@30 h264
*  - DeleteVideoTrack1 - one 4k2k@60 hevc
*  - DeleteSession
*  - StopCamera
*/
TEST_F(VideoGtest,
       SessionWith4k2k60fpsEncAnd720p30fpsEncAndLinked720p30fpsYUV) {
  fprintf(stderr, "\n---------- Run Test %s.%s ------------\n",
          test_info_->test_case_name(), test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  // Enable Force Sensor Mode
  CameraExtraParam extra_param_force_mode;
  ForceSensorMode force_sensor_mode;
  // 4056x2288 60 FPS RAW10
  if (!sensor_mode_file_name_.empty()) {
    std::string mode = "4056x2288@60FPS_RAW10";
    force_sensor_mode.mode = FindSensorModeIndex(sensor_mode_file_name_,
                                                 mode);
  }

  extra_param_force_mode.Update(QMMF_FORCE_SENSOR_MODE, force_sensor_mode);
  ret = recorder_.StartCamera(camera_id_, 60, extra_param_force_mode);
  ASSERT_TRUE(ret == NO_ERROR);

  uint32_t video_track_id_4k_hevc = 1;
  uint32_t video_track_id_720p_avc = 2;
  uint32_t video_track_id_720p_yuv_linked = 3;

  FrameTrace track_trace_1(is_frame_debug_enabled_);
  FrameTrace track_trace_2(is_frame_debug_enabled_);

  for (uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr, "test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
              test_info_->name(), i);

    SessionCb session_status_cb = CreateSessionStatusCb();
    uint32_t session_id;
    ret = recorder_.CreateSession(session_status_cb, &session_id);
    ASSERT_TRUE(session_id > 0);
    ASSERT_TRUE(ret == NO_ERROR);

    if (dump_bitstream_.IsEnabled()) {
      StreamDumpInfo dumpinfo1 = {VideoFormat::kHEVC, session_id,
                                  video_track_id_4k_hevc, 3840, 2160};
      ret = dump_bitstream_.SetUp(dumpinfo1);
      ASSERT_TRUE(ret == NO_ERROR);

      StreamDumpInfo dumpinfo2 = {VideoFormat::kAVC, session_id,
                                  video_track_id_720p_avc, 1280, 720};
      ret = dump_bitstream_.SetUp(dumpinfo2);
      ASSERT_TRUE(ret == NO_ERROR);
    }

    VideoTrackCreateParam video_track_param{camera_id_, VideoFormat::kHEVC,
                                            3840, 2160, 60};

    video_track_param.codec_param.hevc.ratecontrol_type =
        VideoRateControlType::kVariable;
    video_track_param.codec_param.hevc.bitrate = kBitRate100Mbps;

    track_trace_1.SetUp(session_id, video_track_id_4k_hevc, 60.0);

    TrackCb video_track_cb;
    video_track_cb.data_cb = [&, session_id](
        uint32_t track_id, std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
      VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
      track_trace_1.BufferAvailableCb(buffers.at(0));
    };

    video_track_cb.event_cb = [&](uint32_t track_id, EventType event_type,
                                  void *event_data, size_t event_data_size) {
      VideoTrackEventCb(track_id, event_type, event_data, event_data_size);
    };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_4k_hevc,
                                     video_track_param,
                                     video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    std::vector<uint32_t> track_ids;
    track_ids.push_back(video_track_id_4k_hevc);

    // Track2: 1280x720 @30 H264
    VideoTrackCreateParam video_track_param_1{camera_id_, VideoFormat::kAVC,
                                              1280, 720, 30};

    video_track_param_1.codec_param.avc.ratecontrol_type =
        VideoRateControlType::kConstant;
    video_track_param_1.codec_param.avc.bitrate = kBitRate10Mbps;

    if (ubwc_stream_enable_) {
      video_track_param_1.low_power_mode = true;
    }
    track_trace_2.SetUp(session_id, video_track_id_720p_avc, 30.0);

    video_track_cb.data_cb = [&, session_id](
        uint32_t track_id, std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
      VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
      track_trace_2.BufferAvailableCb(buffers.at(0));
    };

    video_track_cb.event_cb = [&](uint32_t track_id, EventType event_type,
                                  void *event_data, size_t event_data_size) {
      VideoTrackEventCb(track_id, event_type, event_data, event_data_size);
    };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_720p_avc,
                                     video_track_param_1, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    track_ids.push_back(video_track_id_720p_avc);

    // Track3: 1280x720 @ 30 YUV, linked track.
    VideoExtraParam extra_param;
    SourceVideoTrack surface_video_copy;
    surface_video_copy.source_track_id = video_track_id_720p_avc;
    extra_param.Update(QMMF_SOURCE_VIDEO_TRACK_ID, surface_video_copy);

    video_track_param_1.width = 1280;
    video_track_param_1.height = 720;
    video_track_param_1.low_power_mode = true;
    video_track_param_1.format_type = VideoFormat::kYUV;

    video_track_cb.data_cb = [&, session_id](
        uint32_t track_id, std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
      VideoTrackYUVDataCb(session_id, track_id, buffers, meta_buffers);
    };

    ret = recorder_.CreateVideoTrack(session_id,
                                     video_track_id_720p_yuv_linked,
                                     video_track_param_1, extra_param,
                                     video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    track_ids.push_back(video_track_id_720p_yuv_linked);
    sessions_.insert(std::make_pair(session_id, track_ids));

    // Start Session
    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    // Let session run for time record_duration_
    sleep(record_duration_);

    ret = recorder_.StopSession(session_id, false);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id,
                                     video_track_id_720p_yuv_linked);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_720p_avc);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_4k_hevc);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    track_trace_1.Reset();
    track_trace_2.Reset();

    ClearSessions();
    dump_bitstream_.CloseAll();
  }

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  fprintf(stderr, "---------- Test Completed %s.%s ----------\n",
          test_info_->test_case_name(), test_info_->name());
}

/*
* SessionWithZZHDR4k2k60fpsEncAnd720p30fpsEncAndLinked720p30fpsYUV:
*                     This test will test: session with one 4k2k 60fps hevc
*                     track, session with one 720p 30fps h264 track,
*                     one linked tracked to 720p 30 fps stream in yuv
* API test sequence:
*  - StartCamera
*  - CreateSession
*  - CreateVideoTrack1 - one 4k2k@60 hevc - zzHDR and force sensor mode 11
*  - CreateVideoTrack2 - one 720p@30 h264
*  - CreateVideoTrack3 - one linked 720p@30 YUV
*  - StartSession
*  - StopSession
*  - DeleteVideoTrack3 - one linked 720p@30 YUV
*  - DeleteVideoTrack2 - one 720p@30 h264
*  - DeleteVideoTrack1 - one 4k2k@60 hevc
*  - DeleteSession
*  - StopCamera
*/
TEST_F(VideoGtest,
       SessionWithZZHDR4k2k60fpsEncAnd720p30fpsEncAndLinked720p30fpsYUV) {
  fprintf(stderr, "\n---------- Run Test %s.%s ------------\n",
          test_info_->test_case_name(), test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  // Setting Enable HDR Extra Param
  CameraExtraParam extra_params;
  VideoHDRMode vid_hdr_mode;
  vid_hdr_mode.enable = true;
  extra_params.Update(QMMF_VIDEO_HDR_MODE, vid_hdr_mode);

  // Setting Force Sensor Mode
  ForceSensorMode force_sensor_mode;
  // 4056x2288 60 FPS RAW10 zzHDR Sensor Mode
  if (!sensor_mode_file_name_.empty()) {
    std::string mode = "4056x2288@60FPS_RAW10_ZZHDR";
    force_sensor_mode.mode = FindSensorModeIndex(sensor_mode_file_name_,
                                                 mode);
  }

  extra_params.Update(QMMF_FORCE_SENSOR_MODE, force_sensor_mode);

  ret = recorder_.StartCamera(camera_id_, 60, extra_params);
  ASSERT_TRUE(ret == NO_ERROR);

  uint32_t video_track_id_4k_hevc = 1;
  uint32_t video_track_id_720p_avc = 2;
  uint32_t video_track_id_720p_yuv_linked = 3;

  FrameTrace track_trace_1(is_frame_debug_enabled_);
  FrameTrace track_trace_2(is_frame_debug_enabled_);

  for (uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr, "test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
              test_info_->name(), i);

    SessionCb session_status_cb = CreateSessionStatusCb();
    uint32_t session_id;
    ret = recorder_.CreateSession(session_status_cb, &session_id);
    ASSERT_TRUE(session_id > 0);
    ASSERT_TRUE(ret == NO_ERROR);

    if (dump_bitstream_.IsEnabled()) {
      StreamDumpInfo dumpinfo1 = {VideoFormat::kHEVC, session_id,
                                  video_track_id_4k_hevc, 3840, 2160};
      ret = dump_bitstream_.SetUp(dumpinfo1);
      ASSERT_TRUE(ret == NO_ERROR);

      StreamDumpInfo dumpinfo2 = {VideoFormat::kAVC, session_id,
                                  video_track_id_720p_avc, 1280, 720};
      ret = dump_bitstream_.SetUp(dumpinfo2);
      ASSERT_TRUE(ret == NO_ERROR);
    }

    // Track1: 4K @60 HEVC
    VideoTrackCreateParam video_track_param{camera_id_, VideoFormat::kHEVC,
                                            3840, 2160, 60};

    video_track_param.codec_param.hevc.ratecontrol_type =
        VideoRateControlType::kVariable;
    video_track_param.codec_param.hevc.bitrate = kBitRate100Mbps;

    track_trace_1.SetUp(session_id, video_track_id_4k_hevc, 60.0);

    TrackCb video_track_cb;
    video_track_cb.data_cb = [&, session_id](
        uint32_t track_id, std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
      VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
      track_trace_1.BufferAvailableCb(buffers.at(0));
    };

    video_track_cb.event_cb = [&](uint32_t track_id, EventType event_type,
                                  void *event_data, size_t event_data_size) {
      VideoTrackEventCb(track_id, event_type, event_data, event_data_size);
    };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_4k_hevc,
                                     video_track_param,
                                     video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    std::vector<uint32_t> track_ids;
    track_ids.push_back(video_track_id_4k_hevc);

    // Track2: 1280x720 @30 H264
    VideoTrackCreateParam video_track_param_1{camera_id_, VideoFormat::kAVC,
                                              1280, 720, 30};

    video_track_param_1.codec_param.avc.ratecontrol_type =
        VideoRateControlType::kConstant;
    video_track_param_1.codec_param.avc.bitrate = kBitRate10Mbps;

    if (ubwc_stream_enable_) {
      video_track_param_1.low_power_mode = true;
    }
    track_trace_2.SetUp(session_id, video_track_id_720p_avc, 30.0);

    video_track_cb.data_cb = [&, session_id](
        uint32_t track_id, std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
      VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
      track_trace_2.BufferAvailableCb(buffers.at(0));
    };

    video_track_cb.event_cb = [&](uint32_t track_id, EventType event_type,
                                  void *event_data, size_t event_data_size) {
      VideoTrackEventCb(track_id, event_type, event_data, event_data_size);
    };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_720p_avc,
                                     video_track_param_1,
                                     video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    track_ids.push_back(video_track_id_720p_avc);

    // Track3: 1280x720 @ 30 YUV, linked track.
    VideoExtraParam extra_param;
    SourceVideoTrack surface_video_copy;
    surface_video_copy.source_track_id = video_track_id_720p_avc;
    extra_param.Update(QMMF_SOURCE_VIDEO_TRACK_ID, surface_video_copy);

    video_track_param_1.width = 1280;
    video_track_param_1.height = 720;
    video_track_param_1.low_power_mode = true;
    video_track_param_1.format_type = VideoFormat::kYUV;

    video_track_cb.data_cb = [&, session_id](
        uint32_t track_id, std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
      VideoTrackYUVDataCb(session_id, track_id, buffers, meta_buffers);
    };

    ret = recorder_.CreateVideoTrack(session_id,
                                     video_track_id_720p_yuv_linked,
                                     video_track_param_1,
                                     video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    track_ids.push_back(video_track_id_720p_yuv_linked);
    sessions_.insert(std::make_pair(session_id, track_ids));

    // Start Session
    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    // Let session run for time record_duration_
    sleep(record_duration_);

    ret = recorder_.StopSession(session_id, false);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id,
                                     video_track_id_720p_yuv_linked);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_720p_avc);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_4k_hevc);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    track_trace_1.Reset();
    track_trace_2.Reset();

    ClearSessions();
    dump_bitstream_.CloseAll();
  }

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  fprintf(stderr, "---------- Test Completed %s.%s ----------\n",
          test_info_->test_case_name(), test_info_->name());
}

/*
* SessionWith4k2kFull60fpsEncAnd720p30fpsEncAndLinked720p30fpsYUV:
*                     This test will test: session with one 3840*2160 full at
*                     60fps hevc track, session with one 720p 30fps h264 track,
*                     one linked tracked to 720p 30fps stream in yuv
* API test sequence:
*  - StartCamera
*  - CreateSession
*  - CreateVideoTrack1 - one 3840*2160@60 hevc  - sensor force mode 10
*  - CreateVideoTrack2 - one 720p@30 h264
*  - CreateVideoTrack3 - one linked 720p@30 YUV
*  - StartSession
*  - StopSession
*  - DeleteVideoTrack3 - one linked 720p@30 YUV
*  - DeleteVideoTrack2 - one 720p@30 h264
*  - DeleteVideoTrack1 - one 3840*2160@60 hevc
*  - DeleteSession
*  - StopCamera
*/
TEST_F(VideoGtest,
       SessionWith4k2kFull60fpsEncAnd720p30fpsEncAndLinked720p30fpsYUV) {
  fprintf(stderr, "\n---------- Run Test %s.%s ------------\n",
          test_info_->test_case_name(), test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  // Enable Force Sensor Mode
  CameraExtraParam extra_param_force_mode;
  ForceSensorMode force_sensor_mode;
  // 4056x2288 60 FPS RAW10 Sensor Mode
  if (!sensor_mode_file_name_.empty()) {
    std::string mode = "4056x2288@60FPS_RAW10";
    force_sensor_mode.mode = FindSensorModeIndex(sensor_mode_file_name_,
                                                 mode);
  }

  extra_param_force_mode.Update(QMMF_FORCE_SENSOR_MODE, force_sensor_mode);
  ret = recorder_.StartCamera(camera_id_, 60, extra_param_force_mode);
  ASSERT_TRUE(ret == NO_ERROR);

  uint32_t video_track_id_4kfull_avc = 1;
  uint32_t video_track_id_720p_avc = 2;
  uint32_t video_track_id_720p_yuv_linked = 3;

  FrameTrace track_trace_1(is_frame_debug_enabled_);
  FrameTrace track_trace_2(is_frame_debug_enabled_);

  for (uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr, "test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
              test_info_->name(), i);

    SessionCb session_status_cb = CreateSessionStatusCb();
    uint32_t session_id;
    ret = recorder_.CreateSession(session_status_cb, &session_id);
    ASSERT_TRUE(session_id > 0);
    ASSERT_TRUE(ret == NO_ERROR);

    if (dump_bitstream_.IsEnabled()) {
      StreamDumpInfo dumpinfo1 = {VideoFormat::kHEVC, session_id,
                                  video_track_id_4kfull_avc, 3840, 2160};
      ret = dump_bitstream_.SetUp(dumpinfo1);
      ASSERT_TRUE(ret == NO_ERROR);

      StreamDumpInfo dumpinfo2 = {VideoFormat::kAVC, session_id,
                                  video_track_id_720p_avc, 1280, 720};
      ret = dump_bitstream_.SetUp(dumpinfo2);
      ASSERT_TRUE(ret == NO_ERROR);
    }

    // Track1: 4K @30 HEVC
    VideoTrackCreateParam video_track_param{camera_id_, VideoFormat::kHEVC,
                                            3840, 2160, 60};

    video_track_param.codec_param.hevc.ratecontrol_type =
        VideoRateControlType::kVariable;
    video_track_param.codec_param.hevc.bitrate = kBitRate100Mbps;

    track_trace_1.SetUp(session_id, video_track_id_4kfull_avc, 60.0);

    TrackCb video_track_cb;
    video_track_cb.data_cb = [&, session_id](
        uint32_t track_id, std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
      VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
      track_trace_1.BufferAvailableCb(buffers.at(0));
    };

    video_track_cb.event_cb = [&](uint32_t track_id, EventType event_type,
                                  void *event_data, size_t event_data_size) {
      VideoTrackEventCb(track_id, event_type, event_data, event_data_size);
    };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_4kfull_avc,
                                     video_track_param,
                                     video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    std::vector<uint32_t> track_ids;
    track_ids.push_back(video_track_id_4kfull_avc);

    // Track2: 1280x720 @30 H264
    VideoTrackCreateParam video_track_param_1{camera_id_, VideoFormat::kAVC,
                                              1280, 720, 30};

    video_track_param_1.codec_param.avc.ratecontrol_type =
        VideoRateControlType::kConstant;
    video_track_param_1.codec_param.avc.bitrate = kBitRate10Mbps;

    if (ubwc_stream_enable_) {
      video_track_param_1.low_power_mode = true;
    }
    track_trace_2.SetUp(session_id, video_track_id_720p_avc, 30.0);

    video_track_cb.data_cb = [&, session_id](
        uint32_t track_id, std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
      VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
      track_trace_2.BufferAvailableCb(buffers.at(0));
    };

    video_track_cb.event_cb = [&](uint32_t track_id, EventType event_type,
                                  void *event_data, size_t event_data_size) {
      VideoTrackEventCb(track_id, event_type, event_data, event_data_size);
    };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_720p_avc,
                                     video_track_param_1, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    track_ids.push_back(video_track_id_720p_avc);

    // Track3: 1280x720 @ 30 YUV, linked track.
    VideoExtraParam extra_param;
    SourceVideoTrack surface_video_copy;
    surface_video_copy.source_track_id = video_track_id_720p_avc;
    extra_param.Update(QMMF_SOURCE_VIDEO_TRACK_ID, surface_video_copy);

    video_track_param_1.width = 1280;
    video_track_param_1.height = 720;
    video_track_param_1.low_power_mode = true;
    video_track_param_1.format_type = VideoFormat::kYUV;

    video_track_cb.data_cb = [&, session_id](
        uint32_t track_id, std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
      VideoTrackYUVDataCb(session_id, track_id, buffers, meta_buffers);
    };

    ret = recorder_.CreateVideoTrack(session_id,
                                     video_track_id_720p_yuv_linked,
                                     video_track_param_1, extra_param,
                                     video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    track_ids.push_back(video_track_id_720p_yuv_linked);
    sessions_.insert(std::make_pair(session_id, track_ids));

    // Start Session
    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    // Let session run for time record_duration_
    sleep(record_duration_);

    ret = recorder_.StopSession(session_id, false);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id,
                                     video_track_id_720p_yuv_linked);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_720p_avc);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_4kfull_avc);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    track_trace_1.Reset();
    track_trace_2.Reset();

    ClearSessions();
    dump_bitstream_.CloseAll();
  }

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  fprintf(stderr, "---------- Test Completed %s.%s ----------\n",
          test_info_->test_case_name(), test_info_->name());
}

/*
* SessionWith2k1k120fpsEncAndCopy720p30fpsEncAndLinked720p30fpsYUV:
*                     This test will test: session with one 2k1k 120 fps hevc
*                     track,copied one 720p 30fps h264 track,
*                     one linked tracked to 720p stream in yuv
* API test sequence:
*  - StartCamera
*  - CreateSession
*  - CreateVideoTrack1 - one 2k1k@120 hevc HFR - sensor force mode 12
*  - CreateVideoTrack2 - one copied 720p@30 h264
*  - CreateVideoTrack3 - one linked 720p@30 YUV
*  - StartSession
*  - StopSession
*  - DeleteVideoTrack3 - one linked 720p@30 YUV
*  - DeleteVideoTrack2 - one copied 720p@30 h264
*  - DeleteVideoTrack1 - one 2k1k@120 HFR hevc
*  - DeleteSession
*  - StopCamera
*/
TEST_F(VideoGtest,
       SessionWith2k1k120fpsEncAndCopy720p30fpsEncAndLinked720p30fpsYUV) {
  fprintf(stderr, "\n---------- Run Test %s.%s ------------\n",
          test_info_->test_case_name(), test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  // Enable Force Sensor Mode
  CameraExtraParam extra_param_force_mode;
  ForceSensorMode force_sensor_mode;
  // 2028x1112 120 FPS RAW10 Sensor Mode
  if (!sensor_mode_file_name_.empty()) {
    std::string mode = "2028x1112@120FPS_RAW10";
    force_sensor_mode.mode = FindSensorModeIndex(sensor_mode_file_name_,
                                                 mode);
  }

  extra_param_force_mode.Update(QMMF_FORCE_SENSOR_MODE, force_sensor_mode);
  ret = recorder_.StartCamera(camera_id_, 120, extra_param_force_mode);
  ASSERT_TRUE(ret == NO_ERROR);

  uint32_t video_track_id_2k1k_hevc = 1;
  uint32_t video_track_id_720p_avc_copied = 2;
  uint32_t video_track_id_720p_yuv_linked = 3;

  FrameTrace track_trace_1(is_frame_debug_enabled_);
  FrameTrace track_trace_2(is_frame_debug_enabled_);

  for (uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr, "test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
              test_info_->name(), i);

    SessionCb session_status_cb = CreateSessionStatusCb();
    uint32_t session_id;
    ret = recorder_.CreateSession(session_status_cb, &session_id);
    ASSERT_TRUE(session_id > 0);
    ASSERT_TRUE(ret == NO_ERROR);

    if (dump_bitstream_.IsEnabled()) {
      StreamDumpInfo dumpinfo1 = {VideoFormat::kHEVC, session_id,
                                  video_track_id_2k1k_hevc, 2028, 1112};
      ret = dump_bitstream_.SetUp(dumpinfo1);
      ASSERT_TRUE(ret == NO_ERROR);

      StreamDumpInfo dumpinfo2 = {VideoFormat::kAVC, session_id,
                                  video_track_id_720p_avc_copied, 1280, 720};
      ret = dump_bitstream_.SetUp(dumpinfo2);
      ASSERT_TRUE(ret == NO_ERROR);
    }

    // Track1: 2k1k @120 HEVC
    VideoTrackCreateParam video_track_param{camera_id_, VideoFormat::kHEVC,
                                            2028, 1112, 120};

    video_track_param.codec_param.hevc.ratecontrol_type =
        VideoRateControlType::kVariable;
    video_track_param.codec_param.hevc.bitrate = kBitRate100Mbps;

    track_trace_1.SetUp(session_id, video_track_id_2k1k_hevc, 120.0);

    TrackCb video_track_cb;
    video_track_cb.data_cb = [&, session_id](
        uint32_t track_id, std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
      VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
      track_trace_1.BufferAvailableCb(buffers.at(0));
    };

    video_track_cb.event_cb = [&](uint32_t track_id, EventType event_type,
                                  void *event_data, size_t event_data_size) {
      VideoTrackEventCb(track_id, event_type, event_data, event_data_size);
    };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_2k1k_hevc,
                                     video_track_param,
                                     video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    std::vector<uint32_t> track_ids;
    track_ids.push_back(video_track_id_2k1k_hevc);

    VideoExtraParam extra_param_sec_track;
    SourceVideoTrack surface_video_copied;
    surface_video_copied.source_track_id = video_track_id_2k1k_hevc;
    extra_param_sec_track.Update(QMMF_SOURCE_VIDEO_TRACK_ID, surface_video_copied);

    // Track2: 1280x720 @30 H264
    VideoTrackCreateParam video_track_param_1{camera_id_, VideoFormat::kAVC,
                                              1280, 720, 30};

    video_track_param_1.codec_param.avc.ratecontrol_type =
        VideoRateControlType::kConstant;
    video_track_param_1.codec_param.avc.bitrate = kBitRate10Mbps;

    if (ubwc_stream_enable_) {
      video_track_param_1.low_power_mode = true;
    }
    track_trace_2.SetUp(session_id, video_track_id_720p_avc_copied, 30.0);

    video_track_cb.data_cb = [&, session_id](
        uint32_t track_id, std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
      VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
      track_trace_2.BufferAvailableCb(buffers.at(0));
    };

    video_track_cb.event_cb = [&](uint32_t track_id, EventType event_type,
                                  void *event_data, size_t event_data_size) {
      VideoTrackEventCb(track_id, event_type, event_data, event_data_size);
    };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_720p_avc_copied,
                                     video_track_param_1, extra_param_sec_track,
                   video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    track_ids.push_back(video_track_id_720p_avc_copied);

    // Track3: 1280x720 @ 30 YUV, linked track.
    VideoExtraParam extra_param;
    SourceVideoTrack surface_video_linked;
    surface_video_linked.source_track_id = video_track_id_720p_avc_copied;
    extra_param.Update(QMMF_SOURCE_VIDEO_TRACK_ID, surface_video_linked);

    video_track_param_1.width = 1280;
    video_track_param_1.height = 720;
    video_track_param_1.low_power_mode = true;
    video_track_param_1.format_type = VideoFormat::kYUV;

    video_track_cb.data_cb = [&, session_id](
        uint32_t track_id, std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
      VideoTrackYUVDataCb(session_id, track_id, buffers, meta_buffers);
    };

    ret = recorder_.CreateVideoTrack(session_id,
                                     video_track_id_720p_yuv_linked,
                                     video_track_param_1, extra_param,
                                     video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    track_ids.push_back(video_track_id_720p_yuv_linked);
    sessions_.insert(std::make_pair(session_id, track_ids));

    // Start Session
    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    // Let session run for time record_duration_
    sleep(record_duration_);

    ret = recorder_.StopSession(session_id, false);
    ASSERT_TRUE(ret == NO_ERROR);

    ret =
        recorder_.DeleteVideoTrack(session_id, video_track_id_720p_yuv_linked);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_720p_avc_copied);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_2k1k_hevc);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    track_trace_1.Reset();
    track_trace_2.Reset();

    ClearSessions();
    dump_bitstream_.CloseAll();
  }

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  fprintf(stderr, "---------- Test Completed %s.%s ----------\n",
          test_info_->test_case_name(), test_info_->name());
}

/*
* SessionWith2k1k120fpsEnc:
*                     This test will test: session with one 2k1k 120 fps hevc
*                     track.
* API test sequence:
*  - StartCamera
*  - CreateSession
*  - CreateVideoTrack1 - one 2k1k@120 hevc HFR - sensor force mode 12
*  - StartSession
*  - StopSession
*  - DeleteVideoTrack1 - one 2k1k@120 HFR hevc
*  - DeleteSession
*  - StopCamera
*/
TEST_F(VideoGtest, SessionWith2k1k120fpsEnc) {
  fprintf(stderr, "\n---------- Run Test %s.%s ------------\n",
          test_info_->test_case_name(), test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  // Enable Force Sensor Mode
  CameraExtraParam extra_param_force_mode;
  ForceSensorMode force_sensor_mode;
  // 2028x1112 120 FPS RAW10 Sensor Mode
  if (!sensor_mode_file_name_.empty()) {
    std::string mode = "2028x1112@120FPS_RAW10";
    force_sensor_mode.mode = FindSensorModeIndex(sensor_mode_file_name_,
                                                 mode);
  }

  extra_param_force_mode.Update(QMMF_FORCE_SENSOR_MODE, force_sensor_mode);
  ret = recorder_.StartCamera(camera_id_, 120, extra_param_force_mode);
  ASSERT_TRUE(ret == NO_ERROR);

  uint32_t video_track_id_2k1k_hevc = 1;

  FrameTrace track_trace_1(is_frame_debug_enabled_);

  for (uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr, "test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
              test_info_->name(), i);

    SessionCb session_status_cb = CreateSessionStatusCb();
    uint32_t session_id;
    ret = recorder_.CreateSession(session_status_cb, &session_id);
    ASSERT_TRUE(session_id > 0);
    ASSERT_TRUE(ret == NO_ERROR);

    if (dump_bitstream_.IsEnabled()) {
      StreamDumpInfo dumpinfo1 = {VideoFormat::kHEVC, session_id,
                                  video_track_id_2k1k_hevc, 2028, 1112};
      ret = dump_bitstream_.SetUp(dumpinfo1);
      ASSERT_TRUE(ret == NO_ERROR);
    }

    // Track1: 2k1k @120 HEVC
    VideoTrackCreateParam video_track_param{camera_id_, VideoFormat::kHEVC,
                                            2028, 1112, 120};

    video_track_param.codec_param.hevc.ratecontrol_type =
        VideoRateControlType::kVariable;
    video_track_param.codec_param.hevc.bitrate = kBitRate100Mbps;

    track_trace_1.SetUp(session_id, video_track_id_2k1k_hevc, 120.0);

    TrackCb video_track_cb;
    video_track_cb.data_cb = [&, session_id](
        uint32_t track_id, std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
      VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
      track_trace_1.BufferAvailableCb(buffers.at(0));
    };

    video_track_cb.event_cb = [&](uint32_t track_id, EventType event_type,
                                  void *event_data, size_t event_data_size) {
      VideoTrackEventCb(track_id, event_type, event_data, event_data_size);
    };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_2k1k_hevc,
                                     video_track_param,
                                     video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    std::vector<uint32_t> track_ids;
    track_ids.push_back(video_track_id_2k1k_hevc);

    // Start Session
    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    // Let session run for time record_duration_
    sleep(record_duration_);

    ret = recorder_.StopSession(session_id, false);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_2k1k_hevc);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    track_trace_1.Reset();

    ClearSessions();
    dump_bitstream_.CloseAll();
  }

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  fprintf(stderr, "---------- Test Completed %s.%s ----------\n",
          test_info_->test_case_name(), test_info_->name());
}

/*
* SessionWith2k1k120fpsEncAndCopy720p30fpsEnc:
*                     This test will test: session with one 2k1k 120 fps hevc
*                     track,copied one 720p 30fps h264 track
* API test sequence:
*  - StartCamera
*  - CreateSession
*  - CreateVideoTrack1 - one 2k1k@120 hevc HFR - sensor force mode 12
*  - CreateVideoTrack2 - one copied 720p@30 h264
*  - StartSession
*  - StopSession
*  - DeleteVideoTrack2 - one copied 720p@30 h264
*  - DeleteVideoTrack1 - one 2k1k@120 HFR hevc
*  - DeleteSession
*  - StopCamera
*/
TEST_F(VideoGtest,
       SessionWith2k1k120fpsEncAndCopy720p30fpsEnc) {
  fprintf(stderr, "\n---------- Run Test %s.%s ------------\n",
          test_info_->test_case_name(), test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  // Enable Force Sensor Mode
  CameraExtraParam extra_param_force_mode;
  ForceSensorMode force_sensor_mode;
  // 2028x1112 120 FPS RAW10 Sensor Mode
  if (!sensor_mode_file_name_.empty()) {
    std::string mode = "2028x1112@120FPS_RAW10";
    force_sensor_mode.mode = FindSensorModeIndex(sensor_mode_file_name_,
                                                 mode);
  }

  extra_param_force_mode.Update(QMMF_FORCE_SENSOR_MODE, force_sensor_mode);
  ret = recorder_.StartCamera(camera_id_, 120, extra_param_force_mode);
  ASSERT_TRUE(ret == NO_ERROR);

  uint32_t video_track_id_2k1k_hevc = 1;
  uint32_t video_track_id_720p_avc_copied = 2;

  FrameTrace track_trace_1(is_frame_debug_enabled_);
  FrameTrace track_trace_2(is_frame_debug_enabled_);

  for (uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr, "test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
              test_info_->name(), i);

    SessionCb session_status_cb = CreateSessionStatusCb();
    uint32_t session_id;
    ret = recorder_.CreateSession(session_status_cb, &session_id);
    ASSERT_TRUE(session_id > 0);
    ASSERT_TRUE(ret == NO_ERROR);

    if (dump_bitstream_.IsEnabled()) {
      StreamDumpInfo dumpinfo1 = {VideoFormat::kHEVC, session_id,
                                  video_track_id_2k1k_hevc, 2028, 1112};
      ret = dump_bitstream_.SetUp(dumpinfo1);
      ASSERT_TRUE(ret == NO_ERROR);

      StreamDumpInfo dumpinfo2 = {VideoFormat::kAVC, session_id,
                                  video_track_id_720p_avc_copied, 1280, 720};
      ret = dump_bitstream_.SetUp(dumpinfo2);
      ASSERT_TRUE(ret == NO_ERROR);
    }

    // Track1: 2k1k @120 HEVC
    VideoTrackCreateParam video_track_param{camera_id_, VideoFormat::kHEVC,
                                            2028, 1112, 120};

    video_track_param.codec_param.hevc.ratecontrol_type =
        VideoRateControlType::kVariable;
    video_track_param.codec_param.hevc.bitrate = kBitRate100Mbps;

    track_trace_1.SetUp(session_id, video_track_id_2k1k_hevc, 120.0);

    TrackCb video_track_cb;
    video_track_cb.data_cb = [&, session_id](
        uint32_t track_id, std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
      VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
      track_trace_1.BufferAvailableCb(buffers.at(0));
    };

    video_track_cb.event_cb = [&](uint32_t track_id, EventType event_type,
                                  void *event_data, size_t event_data_size) {
      VideoTrackEventCb(track_id, event_type, event_data, event_data_size);
    };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_2k1k_hevc,
                                     video_track_param,
                                     video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    std::vector<uint32_t> track_ids;
    track_ids.push_back(video_track_id_2k1k_hevc);

    VideoExtraParam extra_param_sec_track;
    SourceVideoTrack surface_video_copied;
    surface_video_copied.source_track_id = video_track_id_2k1k_hevc;
    extra_param_sec_track.Update(QMMF_SOURCE_VIDEO_TRACK_ID, surface_video_copied);

    // Track2: 1280x720 @30 H264
    VideoTrackCreateParam video_track_param_1{camera_id_, VideoFormat::kAVC,
                                              1280, 720, 30};

    video_track_param_1.codec_param.avc.ratecontrol_type =
        VideoRateControlType::kConstant;
    video_track_param_1.codec_param.avc.bitrate = kBitRate10Mbps;

    if (ubwc_stream_enable_) {
      video_track_param_1.low_power_mode = true;
    }
    track_trace_2.SetUp(session_id, video_track_id_720p_avc_copied, 30.0);

    video_track_cb.data_cb = [&, session_id](
        uint32_t track_id, std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
      VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
      track_trace_2.BufferAvailableCb(buffers.at(0));
    };

    video_track_cb.event_cb = [&](uint32_t track_id, EventType event_type,
                                  void *event_data, size_t event_data_size) {
      VideoTrackEventCb(track_id, event_type, event_data, event_data_size);
    };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_720p_avc_copied,
                                     video_track_param_1, extra_param_sec_track,
                   video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    track_ids.push_back(video_track_id_720p_avc_copied);

    // Start Session
    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    // Let session run for time record_duration_
    sleep(record_duration_);

    ret = recorder_.StopSession(session_id, false);
    ASSERT_TRUE(ret == NO_ERROR);


    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_720p_avc_copied);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_2k1k_hevc);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    track_trace_1.Reset();
    track_trace_2.Reset();

    ClearSessions();
    dump_bitstream_.CloseAll();
  }

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  fprintf(stderr, "---------- Test Completed %s.%s ----------\n",
          test_info_->test_case_name(), test_info_->name());
}

/*
* SessionWithTwoConcurrentCam1080pEnc: This test will test two single
*                                     Cameras, each giving one 1080p
*                                     (Enc) stream.
*
* Api test sequence summary:
*  - StartCamera-Cam0
*  - StartCamera-Cam1
*  - CreateSession-Cam0
*  - CreateSession-Cam1
*  - Create1080pEncTrack-Cam0
*  - Create1080pEncTrack-Cam1
*  - StartSession-Cam0
*  - StartSession-Cam1
*  - StopSession-Cam0
*  - StopSession-Cam1
*  - DeleteVideoTracks-Cam0
*  - DeleteVideoTracks-Cam1
*  - DeleteSession-Cam0
*  - DeleteSession-Cam1
*  - StopCamera-Cam0
*  - StopCamera-Cam1
*/
TEST_F(VideoGtest, SessionWithTwoConcurrentCam1080pEnc) {
  fprintf(stderr, "\n---------- Run Test %s.%s ------------\n",
          test_info_->test_case_name(), test_info_->name());
  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  uint32_t cam0_id = 0;
  uint32_t cam1_id = 1;

  uint32_t track_enc_width  = 1920;
  uint32_t track_enc_height = 1080;

  uint32_t cam0_video_track_id_1080p_hevc = 1;
  uint32_t cam1_video_track_id_1080p_hevc = 2;

  ret = recorder_.StartCamera(cam0_id, 30);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.StartCamera(cam1_id, 30);
  ASSERT_TRUE(ret == NO_ERROR);

  SessionCb cam0_session_status_cb;
  cam0_session_status_cb.event_cb = [this](EventType event_type, void *event_data,
                                      size_t event_data_size) -> void {
    SessionCallbackHandler(event_type, event_data, event_data_size);
  };

  uint32_t cam0_session_id;
  ret = recorder_.CreateSession(cam0_session_status_cb, &cam0_session_id);
  ASSERT_TRUE(cam0_session_id > 0);
  ASSERT_TRUE(ret == NO_ERROR);

  SessionCb cam1_session_status_cb;
  cam1_session_status_cb.event_cb = [this](EventType event_type, void *event_data,
                                      size_t event_data_size) -> void {
    SessionCallbackHandler(event_type, event_data, event_data_size);
  };

  uint32_t cam1_session_id;
  ret = recorder_.CreateSession(cam1_session_status_cb, &cam1_session_id);
  ASSERT_TRUE(cam1_session_id > 0);
  ASSERT_TRUE(ret == NO_ERROR);

  TrackCb video_track_cb;
  video_track_cb.data_cb = [&, cam0_session_id](
      uint32_t track_id, std::vector<BufferDescriptor> buffers,
      std::vector<MetaData> meta_buffers) {
    VideoTrackEncDataCb(cam0_session_id, track_id, buffers, meta_buffers);
  };

  video_track_cb.event_cb = [&](uint32_t track_id, EventType event_type,
                                void *event_data, size_t event_data_size) {
    VideoTrackEventCb(track_id, event_type, event_data, event_data_size);
  };

  if (dump_bitstream_.IsEnabled()) {
    StreamDumpInfo dumpinfo1 = {VideoFormat::kHEVC, cam0_session_id,
                                cam0_video_track_id_1080p_hevc, track_enc_width,
                                track_enc_height};
    ret = dump_bitstream_.SetUp(dumpinfo1);
    ASSERT_TRUE(ret == NO_ERROR);

    StreamDumpInfo dumpinfo2 = {VideoFormat::kHEVC, cam1_session_id,
                                cam1_video_track_id_1080p_hevc, track_enc_width,
                                track_enc_height};
    ret = dump_bitstream_.SetUp(dumpinfo2);
    ASSERT_TRUE(ret == NO_ERROR);
  }

  VideoTrackCreateParam video_track_param{cam0_id, VideoFormat::kHEVC,
                                          track_enc_width, track_enc_height, 30};
  ret = recorder_.CreateVideoTrack(cam0_session_id, cam0_video_track_id_1080p_hevc,
                                   video_track_param, video_track_cb);
  ASSERT_TRUE(ret == NO_ERROR);

  std::vector<uint32_t> cam0_track_ids;
  cam0_track_ids.push_back(cam0_video_track_id_1080p_hevc);
  sessions_.insert(std::make_pair(cam0_session_id, cam0_track_ids));

  video_track_cb.data_cb = [&, cam1_session_id](
      uint32_t track_id, std::vector<BufferDescriptor> buffers,
      std::vector<MetaData> meta_buffers) {
    VideoTrackEncDataCb(cam1_session_id, track_id, buffers, meta_buffers);
  };

  video_track_param.camera_id = cam1_id;
  ret = recorder_.CreateVideoTrack(cam1_session_id, cam1_video_track_id_1080p_hevc,
                                   video_track_param, video_track_cb);
  ASSERT_TRUE(ret == NO_ERROR);

  std::vector<uint32_t> cam1_track_ids;
  cam1_track_ids.push_back(cam1_video_track_id_1080p_hevc);
  sessions_.insert(std::make_pair(cam1_session_id, cam1_track_ids));

  ret = recorder_.StartSession(cam0_session_id);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.StartSession(cam1_session_id);
  ASSERT_TRUE(ret == NO_ERROR);

  // Let session run for time record_duration_, during this time buffer with
  // valid data would be received in track callback (VideoTrackYUVDataCb).
  sleep(record_duration_);

  ret = recorder_.StopSession(cam0_session_id, false);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.StopSession(cam1_session_id, false);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteVideoTrack(cam0_session_id, cam0_video_track_id_1080p_hevc);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteVideoTrack(cam1_session_id, cam1_video_track_id_1080p_hevc);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteSession(cam0_session_id);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteSession(cam1_session_id);
  ASSERT_TRUE(ret == NO_ERROR);

  ClearSessions();

  ret = recorder_.StopCamera(cam0_id);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.StopCamera(cam1_id);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  dump_bitstream_.CloseAll();
  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());
}

/*
* SessionWithTwoConcurrentCam1080pEncAnd720pYUV: This test will test two single
*                                              Cameras, each giving one 1080p
*                                              (Enc) and one 720p (YUV) streams.
*
* Api test sequence summary:
*  - StartCamera-Cam0
*  - StartCamera-Cam1
*  - CreateSession-Cam0
*  - CreateSession-Cam1
*  - Create1080pEncTrack-Cam0
*  - Create1080pEncTrack-Cam1
*  - Create720pVAMTrack-Cam0
*  - Create720pVAMTrack-Cam1
*  - StartSession-Cam0
*  - StartSession-Cam1
*  - StopSession-Cam0
*  - StopSession-Cam1
*  - DeleteVideoTracks-Cam0
*  - DeleteVideoTracks-Cam1
*  - DeleteSession-Cam0
*  - DeleteSession-Cam1
*  - StopCamera-Cam0
*  - StopCamera-Cam1
*/
TEST_F(VideoGtest, SessionWithTwoConcurrentCam1080pEncAnd720pYUV) {
  fprintf(stderr, "\n---------- Run Test %s.%s ------------\n",
          test_info_->test_case_name(), test_info_->name());
  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  uint32_t cam0_id = 0;
  uint32_t cam1_id = 1;

  uint32_t track_enc_width  = 1920;
  uint32_t track_enc_height = 1080;
  uint32_t track_vam_width  = 1280;
  uint32_t track_vam_height = 720;

  uint32_t cam0_video_track_id_1080p_hevc = 1;
  uint32_t cam0_video_track_id_720p_yuv = 2;
  uint32_t cam1_video_track_id_1080p_hevc = 3;
  uint32_t cam1_video_track_id_720p_yuv = 4;

  ret = recorder_.StartCamera(cam0_id, 30);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.StartCamera(cam1_id, 30);
  ASSERT_TRUE(ret == NO_ERROR);

  SessionCb cam0_session_status_cb;
  cam0_session_status_cb.event_cb = [this](EventType event_type, void *event_data,
                                      size_t event_data_size) -> void {
    SessionCallbackHandler(event_type, event_data, event_data_size);
  };

  uint32_t cam0_session_id;
  ret = recorder_.CreateSession(cam0_session_status_cb, &cam0_session_id);
  ASSERT_TRUE(cam0_session_id > 0);
  ASSERT_TRUE(ret == NO_ERROR);

  SessionCb cam1_session_status_cb;
  cam1_session_status_cb.event_cb = [this](EventType event_type, void *event_data,
                                      size_t event_data_size) -> void {
    SessionCallbackHandler(event_type, event_data, event_data_size);
  };

  uint32_t cam1_session_id;
  ret = recorder_.CreateSession(cam1_session_status_cb, &cam1_session_id);
  ASSERT_TRUE(cam1_session_id > 0);
  ASSERT_TRUE(ret == NO_ERROR);

  TrackCb video_track_cb;
  video_track_cb.data_cb = [&, cam0_session_id](
      uint32_t track_id, std::vector<BufferDescriptor> buffers,
      std::vector<MetaData> meta_buffers) {
    VideoTrackEncDataCb(cam0_session_id, track_id, buffers, meta_buffers);
  };

  video_track_cb.event_cb = [&](uint32_t track_id, EventType event_type,
                                void *event_data, size_t event_data_size) {
    VideoTrackEventCb(track_id, event_type, event_data, event_data_size);
  };

  if (dump_bitstream_.IsEnabled()) {
    StreamDumpInfo dumpinfo1 = {VideoFormat::kHEVC, cam0_session_id,
                                cam0_video_track_id_1080p_hevc,
                                track_enc_width, track_enc_height};
    ret = dump_bitstream_.SetUp(dumpinfo1);
    ASSERT_TRUE(ret == NO_ERROR);

    StreamDumpInfo dumpinfo2 = {VideoFormat::kHEVC, cam1_session_id,
                                cam1_video_track_id_1080p_hevc,
                                track_enc_width, track_enc_height};
    ret = dump_bitstream_.SetUp(dumpinfo2);
    ASSERT_TRUE(ret == NO_ERROR);
  }

  VideoTrackCreateParam video_track_param{cam0_id, VideoFormat::kHEVC,
                                          track_enc_width, track_enc_height, 30};
  ret = recorder_.CreateVideoTrack(cam0_session_id, cam0_video_track_id_1080p_hevc,
                                   video_track_param, video_track_cb);
  ASSERT_TRUE(ret == NO_ERROR);

  std::vector<uint32_t> cam0_track_ids;
  cam0_track_ids.push_back(cam0_video_track_id_1080p_hevc);

  video_track_cb.data_cb = [&, cam1_session_id](
      uint32_t track_id, std::vector<BufferDescriptor> buffers,
      std::vector<MetaData> meta_buffers) {
    VideoTrackEncDataCb(cam1_session_id, track_id, buffers, meta_buffers);
  };

  video_track_param.camera_id = cam1_id;
  ret = recorder_.CreateVideoTrack(cam1_session_id, cam1_video_track_id_1080p_hevc,
                                   video_track_param, video_track_cb);
  ASSERT_TRUE(ret == NO_ERROR);

  std::vector<uint32_t> cam1_track_ids;
  cam1_track_ids.push_back(cam1_video_track_id_1080p_hevc);

  video_track_cb.data_cb = [&, cam0_session_id](
      uint32_t track_id, std::vector<BufferDescriptor> buffers,
      std::vector<MetaData> meta_buffers) {
    VideoTrackYUVDataCb(cam0_session_id, track_id, buffers, meta_buffers);
  };

  video_track_param.camera_id = cam0_id;
  video_track_param.width = track_vam_width;
  video_track_param.height = track_vam_height;
  video_track_param.format_type = VideoFormat::kYUV;
  ret = recorder_.CreateVideoTrack(cam0_session_id, cam0_video_track_id_720p_yuv,
                                   video_track_param, video_track_cb);
  ASSERT_TRUE(ret == NO_ERROR);

  cam0_track_ids.push_back(cam0_video_track_id_720p_yuv);
  sessions_.insert(std::make_pair(cam0_session_id, cam0_track_ids));

  video_track_cb.data_cb = [&, cam1_session_id](
      uint32_t track_id, std::vector<BufferDescriptor> buffers,
      std::vector<MetaData> meta_buffers) {
    VideoTrackYUVDataCb(cam1_session_id, track_id, buffers, meta_buffers);
  };

  video_track_param.camera_id = cam1_id;
  ret = recorder_.CreateVideoTrack(cam1_session_id, cam1_video_track_id_720p_yuv,
                                   video_track_param, video_track_cb);
  ASSERT_TRUE(ret == NO_ERROR);

  cam1_track_ids.push_back(cam1_video_track_id_720p_yuv);
  sessions_.insert(std::make_pair(cam1_session_id, cam1_track_ids));

  ret = recorder_.StartSession(cam0_session_id);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.StartSession(cam1_session_id);
  ASSERT_TRUE(ret == NO_ERROR);

  // Let session run for time record_duration_, during this time buffer with
  // valid data would be received in track callback (VideoTrackYUVDataCb).
  sleep(record_duration_);

  ret = recorder_.StopSession(cam0_session_id, false);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.StopSession(cam1_session_id, false);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteVideoTrack(cam0_session_id, cam0_video_track_id_720p_yuv);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteVideoTrack(cam0_session_id, cam0_video_track_id_1080p_hevc);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteVideoTrack(cam1_session_id, cam1_video_track_id_720p_yuv);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteVideoTrack(cam1_session_id, cam1_video_track_id_1080p_hevc);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteSession(cam0_session_id);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteSession(cam1_session_id);
  ASSERT_TRUE(ret == NO_ERROR);

  ClearSessions();

  ret = recorder_.StopCamera(cam0_id);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.StopCamera(cam1_id);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  dump_bitstream_.CloseAll();
  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());
}

/*
* SessionWith4kp30fpsEncAnd720p30fpsEncSwitchForceToAutoSensorMode:
*     This test will start session with one 4k 30fps hevc track and another one
*     with 720p 30fps h264 track, setting force sensor mode, then delete both
*     tracks, re-create 4k 30fps hevc track, add it to session, which will
*     re-start session in auto mode.
*
* API test sequence:
*  - StartCamera
*  loop Start {
*  ------------------
*  - CreateSession
*  - CreateVideoTrack1 - 4k@30 hevc - force sensor mode 7
*  - CreateVideoTrack2 - 720p@30 h264
*  - StartSession
*  - StopSession
*  - DeleteVideoTrack2 - 720p@30 h264
*  - DeleteVideoTrack1 - 4k@30 hevc
*  - CreateVideoTrack1 - 4k@30 hevc - auto mode
*  - StartSession
*  - StopSession
*  - DeleteVideoTrack1 - 4k@30 hevc
*  - DeleteSession
*  ------------------
*  } loop End
*  - StopCamera
*/
TEST_F(
    VideoGtest,
    SessionWith4kp30fpsEncAnd720p30fpsEncSwitchForceToAutoSensorMode) {
  fprintf(stderr, "\n---------- Run Test %s.%s ------------\n",
          test_info_->test_case_name(), test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  // Enable Force Sensor Mode
  CameraExtraParam extra_param_force_mode;
  ForceSensorMode force_sensor_mode;
  if (!sensor_mode_file_name_.empty()) {
    std::string mode = "4056x3040@30FPS_RAW12";
    force_sensor_mode.mode = FindSensorModeIndex(sensor_mode_file_name_,
                                                 mode);
  }

  fprintf(stderr, "Setting force sensor mode %d \n", force_sensor_mode.mode);
  extra_param_force_mode.Update(QMMF_FORCE_SENSOR_MODE, force_sensor_mode);
  ret = recorder_.StartCamera(camera_id_, 30, extra_param_force_mode);
  ASSERT_TRUE(ret == NO_ERROR);

  uint32_t video_track_id_4k_hevc = 1;
  uint32_t video_track_id_720p_avc = 2;

  for (uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr, "test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
              test_info_->name(), i);

    SessionCb session_status_cb = CreateSessionStatusCb();
    uint32_t session_id;
    ret = recorder_.CreateSession(session_status_cb, &session_id);
    ASSERT_TRUE(session_id > 0);
    ASSERT_TRUE(ret == NO_ERROR);

    if (dump_bitstream_.IsEnabled()) {
      StreamDumpInfo dumpinfo1 = {VideoFormat::kHEVC, session_id,
                                  video_track_id_4k_hevc, 3840, 2160};
      ret = dump_bitstream_.SetUp(dumpinfo1);
      ASSERT_TRUE(ret == NO_ERROR);

      StreamDumpInfo dumpinfo2 = {VideoFormat::kAVC, session_id,
                                  video_track_id_720p_avc, 1280, 720};
      ret = dump_bitstream_.SetUp(dumpinfo2);
      ASSERT_TRUE(ret == NO_ERROR);
    }

    // Track1: 4K @30 HEVC
    VideoTrackCreateParam video_track_param{camera_id_, VideoFormat::kHEVC,
                                            3840, 2160, 30};

    video_track_param.codec_param.hevc.ratecontrol_type =
        VideoRateControlType::kConstant;
    video_track_param.codec_param.hevc.bitrate = kBitRate100Mbps;

    TrackCb video_track_cb;
    video_track_cb.data_cb = [&, session_id](
        uint32_t track_id, std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
      VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
    };

    video_track_cb.event_cb = [&](uint32_t track_id, EventType event_type,
                                  void *event_data, size_t event_data_size) {
      VideoTrackEventCb(track_id, event_type, event_data, event_data_size);
    };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_4k_hevc,
                                     video_track_param,
                                     video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    std::vector<uint32_t> track_ids;
    track_ids.push_back(video_track_id_4k_hevc);

    // Track2: 1280x720 @30 H264
    VideoTrackCreateParam video_track_param_1{camera_id_, VideoFormat::kAVC,
                                              1280, 720, 30};

    video_track_param_1.codec_param.avc.ratecontrol_type =
        VideoRateControlType::kConstant;
    video_track_param_1.codec_param.avc.bitrate = kBitRate10Mbps;
    video_track_param_1.low_power_mode = true;

    video_track_cb.data_cb = [&, session_id](
        uint32_t track_id, std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
      VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
    };

    video_track_cb.event_cb = [&](uint32_t track_id, EventType event_type,
                                  void *event_data, size_t event_data_size) {
      VideoTrackEventCb(track_id, event_type, event_data, event_data_size);
    };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_720p_avc,
                                     video_track_param_1, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    track_ids.push_back(video_track_id_720p_avc);
    sessions_.insert(std::make_pair(session_id, track_ids));

    // Start Session
    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    sleep(record_duration_/2);

    ret = recorder_.StopSession(session_id, false);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_720p_avc);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_4k_hevc);
    ASSERT_TRUE(ret == NO_ERROR);

    ClearSessions();
    track_ids.clear();
    dump_bitstream_.CloseAll();

    fprintf(stderr, "Switched to Auto mode \n");

    if (dump_bitstream_.IsEnabled()) {
      StreamDumpInfo dumpinfo3 = {VideoFormat::kHEVC, session_id,
                                  video_track_id_4k_hevc, 3840, 2160};
      ret = dump_bitstream_.SetUp(dumpinfo3);
      ASSERT_TRUE(ret == NO_ERROR);
    }

    // Re-create 4k hevc track without force mode extra param
    ret = recorder_.CreateVideoTrack(session_id, video_track_id_4k_hevc,
                                     video_track_param, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    track_ids.push_back(video_track_id_4k_hevc);
    sessions_.insert(std::make_pair(session_id, track_ids));

    // Restart previous session with new track
    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    sleep(record_duration_/2);

    ret = recorder_.StopSession(session_id, false);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_4k_hevc);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    ClearSessions();
    track_ids.clear();
    dump_bitstream_.CloseAll();
  }

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  fprintf(stderr, "---------- Test Completed %s.%s ----------\n",
          test_info_->test_case_name(), test_info_->name());
}


/*
* SessionWith4k2k60fpsEncAnd720p30fpsEncAndLinked720p30fpsYUVWithEIS:
*           This test will test: session with one 4k2k 60fps hevc track,
*           session with one 720p 30fps h264 track, one linked tracked to
*           720p30 stream in yuv.
* API test sequence:
*  - StartCamera
*  - CreateSession
*  - CreateVideoTrack1 - one 4k2k@60 hevc - force sensor mode 10 [Enable EIS]
*  - CreateVideoTrack2 - one 720p@30 h264
*  - CreateVideoTrack3 - one linked 720p@30 YUV
*  - StartSession
*  - StopSession
*  - DeleteVideoTrack3 - one linked 720p@30 YUV
*  - DeleteVideoTrack2 - one 720p@30 h264
*  - DeleteVideoTrack1 - one 4k2k@60 hevc
*  - DeleteSession
*  - StopCamera
*/
TEST_F(VideoGtest,
       SessionWith4k2k60fpsEncAnd720p30fpsEncAndLinked720p30fpsYUVWithEIS) {
  fprintf(stderr, "\n---------- Run Test %s.%s ------------\n",
          test_info_->test_case_name(), test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  // Enable Force Sensor Mode
  CameraExtraParam extra_param_force_mode;
  ForceSensorMode force_sensor_mode;
  // 4056x2288 60 FPS RAW10
  if (!sensor_mode_file_name_.empty()) {
    std::string mode = "4056x2288@60FPS_RAW10";
    force_sensor_mode.mode =
        FindSensorModeIndex(sensor_mode_file_name_, mode);
  }

  extra_param_force_mode.Update(QMMF_FORCE_SENSOR_MODE, force_sensor_mode);

  // Enable EIS
  EISSetup eis_mode;
  eis_mode.enable = true;
  extra_param_force_mode.Update(QMMF_EIS, eis_mode);
  ret = recorder_.StartCamera(camera_id_, 60, extra_param_force_mode);
  ASSERT_TRUE(ret == NO_ERROR);

  uint32_t video_track_id_4k_hevc = 1;
  uint32_t video_track_id_720p_avc = 2;
  uint32_t video_track_id_720p_yuv_linked = 3;

  FrameTrace track_trace_1(is_frame_debug_enabled_);
  FrameTrace track_trace_2(is_frame_debug_enabled_);

  for (uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr, "test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
              test_info_->name(), i);

    SessionCb session_status_cb = CreateSessionStatusCb();
    uint32_t session_id;
    ret = recorder_.CreateSession(session_status_cb, &session_id);
    ASSERT_TRUE(session_id > 0);
    ASSERT_TRUE(ret == NO_ERROR);

    if (dump_bitstream_.IsEnabled()) {
      StreamDumpInfo dumpinfo1 = {VideoFormat::kHEVC, session_id,
                                  video_track_id_4k_hevc, 3840, 2160};
      ret = dump_bitstream_.SetUp(dumpinfo1);
      ASSERT_TRUE(ret == NO_ERROR);

      StreamDumpInfo dumpinfo2 = {VideoFormat::kAVC, session_id,
                                  video_track_id_720p_avc, 1280, 720};
      ret = dump_bitstream_.SetUp(dumpinfo2);
      ASSERT_TRUE(ret == NO_ERROR);
    }

    VideoTrackCreateParam video_track_param{camera_id_, VideoFormat::kHEVC,
                                            3840, 2160, 60};

    video_track_param.codec_param.hevc.ratecontrol_type =
        VideoRateControlType::kVariable;
    video_track_param.codec_param.hevc.bitrate = kBitRate100Mbps;

    track_trace_1.SetUp(session_id, video_track_id_4k_hevc, 60.0);

    TrackCb video_track_cb;
    video_track_cb.data_cb = [&, session_id](
        uint32_t track_id, std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
      VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
      track_trace_1.BufferAvailableCb(buffers.at(0));
    };

    video_track_cb.event_cb = [&](uint32_t track_id, EventType event_type,
                                  void *event_data, size_t event_data_size) {
      VideoTrackEventCb(track_id, event_type, event_data, event_data_size);
    };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_4k_hevc,
                                     video_track_param,
                                     video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    std::vector<uint32_t> track_ids;
    track_ids.push_back(video_track_id_4k_hevc);

    // Track2: 1280x720 @30 H264
    VideoTrackCreateParam video_track_param_1{camera_id_, VideoFormat::kAVC,
                                              1280, 720, 30};

    video_track_param_1.codec_param.avc.ratecontrol_type =
        VideoRateControlType::kConstant;
    video_track_param_1.codec_param.avc.bitrate = kBitRate10Mbps;

    if (ubwc_stream_enable_) {
      video_track_param_1.low_power_mode = true;
    }
    track_trace_2.SetUp(session_id, video_track_id_720p_avc, 30.0);

    video_track_cb.data_cb = [&, session_id](
        uint32_t track_id, std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
      VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
      track_trace_2.BufferAvailableCb(buffers.at(0));
    };

    video_track_cb.event_cb = [&](uint32_t track_id, EventType event_type,
                                  void *event_data, size_t event_data_size) {
      VideoTrackEventCb(track_id, event_type, event_data, event_data_size);
    };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_720p_avc,
                                     video_track_param_1, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    track_ids.push_back(video_track_id_720p_avc);

    // Track3: 1280x720 @ 30 YUV, linked track.
    VideoExtraParam extra_param;
    SourceVideoTrack surface_video_copy;
    surface_video_copy.source_track_id = video_track_id_720p_avc;
    extra_param.Update(QMMF_SOURCE_VIDEO_TRACK_ID, surface_video_copy);

    video_track_param_1.width = 1280;
    video_track_param_1.height = 720;
    video_track_param_1.low_power_mode = true;
    video_track_param_1.format_type = VideoFormat::kYUV;

    video_track_cb.data_cb = [&, session_id](
        uint32_t track_id, std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
      VideoTrackYUVDataCb(session_id, track_id, buffers, meta_buffers);
    };

    ret = recorder_.CreateVideoTrack(session_id,
                                     video_track_id_720p_yuv_linked,
                                     video_track_param_1, extra_param,
                                     video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    track_ids.push_back(video_track_id_720p_yuv_linked);
    sessions_.insert(std::make_pair(session_id, track_ids));

    // Start Session
    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    // Let session run for time record_duration_
    sleep(record_duration_);

    ret = recorder_.StopSession(session_id, false);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id,
                                     video_track_id_720p_yuv_linked);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_720p_avc);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_4k_hevc);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    track_trace_1.Reset();
    track_trace_2.Reset();

    ClearSessions();
    dump_bitstream_.CloseAll();
  }

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  fprintf(stderr, "---------- Test Completed %s.%s ----------\n",
          test_info_->test_case_name(), test_info_->name());
}

/*
* SessionWithZZHDR4k2k60fpsEncAnd720p30fpsEncAndLinked720p30fpsYUVWithEIS:
*                     This test will test: session with one 4k2k 60fps hevc
*                     track, session with one 720p 30fps h264 track,
*                     one linked tracked to 720p 30 fps stream in yuv
* API test sequence:
*  - StartCamera
*  - CreateSession
*  - CreateVideoTrack1 - one 4k2k@60 hevc with EIS, zzHDR, force sensor mode 11
*  - CreateVideoTrack2 - one 720p@30 h264
*  - CreateVideoTrack3 - one linked 720p@30 YUV
*  - StartSession
*  - StopSession
*  - DeleteVideoTrack3 - one linked 720p@30 YUV
*  - DeleteVideoTrack2 - one 720p@30 h264
*  - DeleteVideoTrack1 - one 4k2k@60 hevc
*  - DeleteSession
*  - StopCamera
*/
TEST_F(
    VideoGtest,
    SessionWithZZHDR4k2k60fpsEncAnd720p30fpsEncAndLinked720p30fpsYUVWithEIS) {
  fprintf(stderr, "\n---------- Run Test %s.%s ------------\n",
          test_info_->test_case_name(), test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  // Setting Enable HDR Extra Param
  CameraExtraParam extra_params;
  VideoHDRMode vid_hdr_mode;
  vid_hdr_mode.enable = true;
  extra_params.Update(QMMF_VIDEO_HDR_MODE, vid_hdr_mode);

  // Setting Force Sensor Mode
  ForceSensorMode force_sensor_mode;
  // 4056x2288 60 FPS RAW10 zzHDR Sensor Mode
  if (!sensor_mode_file_name_.empty()) {
    std::string mode = "4056x2288@60FPS_RAW10_ZZHDR";
    force_sensor_mode.mode =
        FindSensorModeIndex(sensor_mode_file_name_, mode);
  }

  extra_params.Update(QMMF_FORCE_SENSOR_MODE, force_sensor_mode);

  // Enable EIS
  EISSetup eis_mode;
  eis_mode.enable = true;
  extra_params.Update(QMMF_EIS, eis_mode);
  ret = recorder_.StartCamera(camera_id_, 60, extra_params);
  ASSERT_TRUE(ret == NO_ERROR);

  uint32_t video_track_id_4k_hevc = 1;
  uint32_t video_track_id_720p_avc = 2;
  uint32_t video_track_id_720p_yuv_linked = 3;

  FrameTrace track_trace_1(is_frame_debug_enabled_);
  FrameTrace track_trace_2(is_frame_debug_enabled_);

  for (uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr, "test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
              test_info_->name(), i);

    SessionCb session_status_cb = CreateSessionStatusCb();
    uint32_t session_id;
    ret = recorder_.CreateSession(session_status_cb, &session_id);
    ASSERT_TRUE(session_id > 0);
    ASSERT_TRUE(ret == NO_ERROR);

    if (dump_bitstream_.IsEnabled()) {
      StreamDumpInfo dumpinfo1 = {VideoFormat::kHEVC, session_id,
                                  video_track_id_4k_hevc, 3840, 2160};
      ret = dump_bitstream_.SetUp(dumpinfo1);
      ASSERT_TRUE(ret == NO_ERROR);

      StreamDumpInfo dumpinfo2 = {VideoFormat::kAVC, session_id,
                                  video_track_id_720p_avc, 1280, 720};
      ret = dump_bitstream_.SetUp(dumpinfo2);
      ASSERT_TRUE(ret == NO_ERROR);
    }

    // Track1: 4K @60 HEVC
    VideoTrackCreateParam video_track_param{camera_id_, VideoFormat::kHEVC,
                                            3840, 2160, 60};

    video_track_param.codec_param.hevc.ratecontrol_type =
        VideoRateControlType::kVariable;
    video_track_param.codec_param.hevc.bitrate = kBitRate100Mbps;

    track_trace_1.SetUp(session_id, video_track_id_4k_hevc, 60.0);

    TrackCb video_track_cb;
    video_track_cb.data_cb = [&, session_id](
        uint32_t track_id, std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
      VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
      track_trace_1.BufferAvailableCb(buffers.at(0));
    };

    video_track_cb.event_cb = [&](uint32_t track_id, EventType event_type,
                                  void *event_data, size_t event_data_size) {
      VideoTrackEventCb(track_id, event_type, event_data, event_data_size);
    };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_4k_hevc,
                                     video_track_param,
                                     video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    std::vector<uint32_t> track_ids;
    track_ids.push_back(video_track_id_4k_hevc);

    // Track2: 1280x720 @30 H264
    VideoTrackCreateParam video_track_param_1{camera_id_, VideoFormat::kAVC,
                                              1280, 720, 30};

    video_track_param_1.codec_param.avc.ratecontrol_type =
        VideoRateControlType::kConstant;
    video_track_param_1.codec_param.avc.bitrate = kBitRate10Mbps;

    if (ubwc_stream_enable_) {
      video_track_param_1.low_power_mode = true;
    }
    track_trace_2.SetUp(session_id, video_track_id_720p_avc, 30.0);

    video_track_cb.data_cb = [&, session_id](
        uint32_t track_id, std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
      VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
      track_trace_2.BufferAvailableCb(buffers.at(0));
    };

    video_track_cb.event_cb = [&](uint32_t track_id, EventType event_type,
                                  void *event_data, size_t event_data_size) {
      VideoTrackEventCb(track_id, event_type, event_data, event_data_size);
    };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_720p_avc,
                                     video_track_param_1,
                                     video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    track_ids.push_back(video_track_id_720p_avc);

    // Track3: 1280x720 @ 30 YUV, linked track.
    VideoExtraParam extra_param;
    SourceVideoTrack surface_video_copy;
    surface_video_copy.source_track_id = video_track_id_720p_avc;
    extra_param.Update(QMMF_SOURCE_VIDEO_TRACK_ID, surface_video_copy);

    video_track_param_1.width = 1280;
    video_track_param_1.height = 720;
    video_track_param_1.low_power_mode = true;
    video_track_param_1.format_type = VideoFormat::kYUV;

    video_track_cb.data_cb = [&, session_id](
        uint32_t track_id, std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
      VideoTrackYUVDataCb(session_id, track_id, buffers, meta_buffers);
    };

    ret = recorder_.CreateVideoTrack(session_id,
                                     video_track_id_720p_yuv_linked,
                                     video_track_param_1,
                                     video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    track_ids.push_back(video_track_id_720p_yuv_linked);
    sessions_.insert(std::make_pair(session_id, track_ids));

    // Start Session
    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    // Let session run for time record_duration_
    sleep(record_duration_);

    ret = recorder_.StopSession(session_id, false);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id,
                                     video_track_id_720p_yuv_linked);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_720p_avc);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_4k_hevc);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    track_trace_1.Reset();
    track_trace_2.Reset();

    ClearSessions();
    dump_bitstream_.CloseAll();
  }

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  fprintf(stderr, "---------- Test Completed %s.%s ----------\n",
          test_info_->test_case_name(), test_info_->name());
}

/*
* SessionWith4k2kFull60fpsEncAnd720p30fpsEncAndLinked720p30fpsYUVWithEIS:
*                     This test will test: session with one 3840*2160 full at
*                     60fps hevc track, session with one 720p 30fps h264 track,
*                     one linked tracked to 720p 30fps stream in yuv
* API test sequence:
*  - StartCamera
*  - CreateSession
*  - CreateVideoTrack1 - one 3840*2160@60 hevc with EIS - sensor force mode 10
*  - CreateVideoTrack2 - one 720p@30 h264
*  - CreateVideoTrack3 - one linked 720p@30 YUV
*  - StartSession
*  - StopSession
*  - DeleteVideoTrack3 - one linked 720p@30 YUV
*  - DeleteVideoTrack2 - one 720p@30 h264
*  - DeleteVideoTrack1 - one 3840*2160@60 hevc
*  - DeleteSession
*  - StopCamera
*/
TEST_F(
    VideoGtest,
    SessionWith4k2kFull60fpsEncAnd720p30fpsEncAndLinked720p30fpsYUVWithEIS) {
  fprintf(stderr, "\n---------- Run Test %s.%s ------------\n",
          test_info_->test_case_name(), test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  // Enable Force Sensor Mode
  CameraExtraParam extra_param_force_mode;
  ForceSensorMode force_sensor_mode;
  // 4056x2288 60 FPS RAW10 Sensor Mode
  if (!sensor_mode_file_name_.empty()) {
    std::string mode = "4056x2288@60FPS_RAW10";
    force_sensor_mode.mode =
        FindSensorModeIndex(sensor_mode_file_name_, mode);
  }

  extra_param_force_mode.Update(QMMF_FORCE_SENSOR_MODE, force_sensor_mode);

  // Enable EIS
  EISSetup eis_mode;
  eis_mode.enable = true;
  extra_param_force_mode.Update(QMMF_EIS, eis_mode);
  ret = recorder_.StartCamera(camera_id_, 60, extra_param_force_mode);
  ASSERT_TRUE(ret == NO_ERROR);

  uint32_t video_track_id_4kfull_avc = 1;
  uint32_t video_track_id_720p_avc = 2;
  uint32_t video_track_id_720p_yuv_linked = 3;

  FrameTrace track_trace_1(is_frame_debug_enabled_);
  FrameTrace track_trace_2(is_frame_debug_enabled_);

  for (uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr, "test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
              test_info_->name(), i);

    SessionCb session_status_cb = CreateSessionStatusCb();
    uint32_t session_id;
    ret = recorder_.CreateSession(session_status_cb, &session_id);
    ASSERT_TRUE(session_id > 0);
    ASSERT_TRUE(ret == NO_ERROR);

    if (dump_bitstream_.IsEnabled()) {
      StreamDumpInfo dumpinfo1 = {VideoFormat::kHEVC, session_id,
                                  video_track_id_4kfull_avc, 3840, 2160};
      ret = dump_bitstream_.SetUp(dumpinfo1);
      ASSERT_TRUE(ret == NO_ERROR);

      StreamDumpInfo dumpinfo2 = {VideoFormat::kAVC, session_id,
                                  video_track_id_720p_avc, 1280, 720};
      ret = dump_bitstream_.SetUp(dumpinfo2);
      ASSERT_TRUE(ret == NO_ERROR);
    }

    // Track1: 4K @30 HEVC
    VideoTrackCreateParam video_track_param{camera_id_, VideoFormat::kHEVC,
                                            3840, 2160, 60};

    video_track_param.codec_param.hevc.ratecontrol_type =
        VideoRateControlType::kVariable;
    video_track_param.codec_param.hevc.bitrate = kBitRate100Mbps;

    track_trace_1.SetUp(session_id, video_track_id_4kfull_avc, 60.0);

    TrackCb video_track_cb;
    video_track_cb.data_cb = [&, session_id](
        uint32_t track_id, std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
      VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
      track_trace_1.BufferAvailableCb(buffers.at(0));
    };

    video_track_cb.event_cb = [&](uint32_t track_id, EventType event_type,
                                  void *event_data, size_t event_data_size) {
      VideoTrackEventCb(track_id, event_type, event_data, event_data_size);
    };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_4kfull_avc,
                                     video_track_param,
                                     video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    std::vector<uint32_t> track_ids;
    track_ids.push_back(video_track_id_4kfull_avc);

    // Track2: 1280x720 @30 H264
    VideoTrackCreateParam video_track_param_1{camera_id_, VideoFormat::kAVC,
                                              1280, 720, 30};

    video_track_param_1.codec_param.avc.ratecontrol_type =
        VideoRateControlType::kConstant;
    video_track_param_1.codec_param.avc.bitrate = kBitRate10Mbps;

    if (ubwc_stream_enable_) {
      video_track_param_1.low_power_mode = true;
    }
    track_trace_2.SetUp(session_id, video_track_id_720p_avc, 30.0);

    video_track_cb.data_cb = [&, session_id](
        uint32_t track_id, std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
      VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
      track_trace_2.BufferAvailableCb(buffers.at(0));
    };

    video_track_cb.event_cb = [&](uint32_t track_id, EventType event_type,
                                  void *event_data, size_t event_data_size) {
      VideoTrackEventCb(track_id, event_type, event_data, event_data_size);
    };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_720p_avc,
                                     video_track_param_1, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    track_ids.push_back(video_track_id_720p_avc);

    // Track3: 1280x720 @ 30 YUV, linked track.
    VideoExtraParam extra_param;
    SourceVideoTrack surface_video_copy;
    surface_video_copy.source_track_id = video_track_id_720p_avc;
    extra_param.Update(QMMF_SOURCE_VIDEO_TRACK_ID, surface_video_copy);

    video_track_param_1.width = 1280;
    video_track_param_1.height = 720;
    video_track_param_1.low_power_mode = true;
    video_track_param_1.format_type = VideoFormat::kYUV;

    video_track_cb.data_cb = [&, session_id](
        uint32_t track_id, std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
      VideoTrackYUVDataCb(session_id, track_id, buffers, meta_buffers);
    };

    ret = recorder_.CreateVideoTrack(session_id,
                                     video_track_id_720p_yuv_linked,
                                     video_track_param_1, extra_param,
                                     video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    track_ids.push_back(video_track_id_720p_yuv_linked);
    sessions_.insert(std::make_pair(session_id, track_ids));

    // Start Session
    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    // Let session run for time record_duration_
    sleep(record_duration_);

    ret = recorder_.StopSession(session_id, false);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id,
                                     video_track_id_720p_yuv_linked);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_720p_avc);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_4kfull_avc);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    track_trace_1.Reset();
    track_trace_2.Reset();

    ClearSessions();
    dump_bitstream_.CloseAll();
  }

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  fprintf(stderr, "---------- Test Completed %s.%s ----------\n",
          test_info_->test_case_name(), test_info_->name());
}

#endif
