/*
* Copyright (c) 2018, The Linux Foundation. All rights reserved.
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
#define LOG_TAG "RecorderBaseGTest"

#include "recorder/test/gtest/qmmf_recorder_base_gtest.h"

static const std::string gtest_type = "base_gtest";

/*
* ConnectToService: This test case will test Connect/Disconnect Api.
* Api test sequence:
*  - Connect
*  - Disconnect
*/
TEST_F(RecorderBaseGTest, ConnectToService) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  for(uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
        test_info_->name(), i);

    auto ret = recorder_.Connect(recorder_status_cb_);
    ASSERT_TRUE(ret == NO_ERROR);
    sleep(3);

    ret = recorder_.Disconnect();
    ASSERT_TRUE(ret == NO_ERROR);
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
TEST_F(RecorderBaseGTest, StartStopCamera) {

  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);
  for(uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
        test_info_->name(), i);

    ret = recorder_.StartCamera(camera_id_, camera_start_params_);
    ASSERT_TRUE(ret == NO_ERROR);
    sleep(3);

    ret = recorder_.StopCamera(camera_id_);
    ASSERT_TRUE(ret == NO_ERROR);
  }
  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);
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
TEST_F(RecorderBaseGTest, CreateDeleteSession) {

  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);
  for(uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
        test_info_->name(), i);

    ret = recorder_.StartCamera(camera_id_, camera_start_params_);
    ASSERT_TRUE(ret == NO_ERROR);

    SessionCb session_status_cb = CreateSessionStatusCb();

    uint32_t session_id;
    ret = recorder_.CreateSession(session_status_cb, &session_id);
    ASSERT_TRUE(session_id > 0);
    ASSERT_TRUE(ret == NO_ERROR);
    sleep(2);
    ret = recorder_.DeleteSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);
    sleep(1);
    ret = recorder_.StopCamera(camera_id_);
    ASSERT_TRUE(ret == NO_ERROR);
  }
  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());
}

/*
* SessionWith480pYUVTrack: This test case will test 480p YUV video track
*
* Api test sequence:
*   loop Start {
*   ------------------
*   - StartCamera
*   - CreateSession
*   - CreateVideoTrack
*   - StartSession
*   - StopSession
*   - DeleteVideoTrack
*   - DeleteSession
*   - StopCamera
*   ------------------
*   } loop End
*/
TEST_F(RecorderBaseGTest, SessionWith480pYUVTrack) {

  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  uint32_t video_track_id_480p_yuv = 1;
  uint32_t width = 864;
  uint32_t height = 480;
  float frame_rate = 30;

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  for (uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
        test_info_->name(), i);

    ret = recorder_.StartCamera(camera_id_, camera_start_params_);
    ASSERT_TRUE(ret == NO_ERROR);


    SessionCb session_status_cb = CreateSessionStatusCb();
    uint32_t session_id;
    ret = recorder_.CreateSession(session_status_cb, &session_id);
    ASSERT_TRUE(session_id > 0);
    ASSERT_TRUE(ret == NO_ERROR);

    /************************ Create Preview Track ****************************/

    VideoTrackCreateParam video_track_param{camera_id_, VideoFormat::kYUV,
                                            width, height, frame_rate};

    TrackCb video_track_cb;
    video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
        std::vector<BufferDescriptor> buffers, std::vector<MetaData> meta_buffers)
        { VideoTrackYUVDataCb(session_id, track_id, buffers, meta_buffers); };

    video_track_cb.event_cb = [&] (uint32_t track_id, EventType event_type,
        void *event_data, size_t event_data_size) { VideoTrackEventCb(track_id,
        event_type, event_data, event_data_size); };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_480p_yuv,
                                     video_track_param, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    std::vector<uint32_t> track_ids;
    track_ids.push_back(video_track_id_480p_yuv);
    sessions_.insert(std::make_pair(session_id, track_ids));

    /************************ Start Preview  **********************************/

    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    sleep(record_duration_);

    /************************ Stop Preview ************************************/

    ret = recorder_.StopSession(session_id, false);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_480p_yuv);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    ClearSessions();

    ret = recorder_.StopCamera(camera_id_);
    ASSERT_TRUE(ret == NO_ERROR);
  }

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());
}

/*
* StartStopCameraZSLMode: This test case will test Start & Stop ZSL APIs
*                         while preview is running.
* Api test sequence:
*   loop Start {
*   ------------------
*   - StartCamera
*   - CreateSession
*   - CreateVideoTrack
*   - StartSession
*   - ConfigImageCapture - enable ZSL
*   - CancelCaptureImage - disable ZSL
*   - ConfigImageCapture - enable ZSL
*   - ConfigImageCapture - disable ZSL
*   - StopSession
*   - DeleteVideoTrack
*   - DeleteSession
*   - StopCamera
*   ------------------
*   } loop End
*/
TEST_F(RecorderBaseGTest, StartStopCameraZSLMode) {

  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  uint32_t video_track_id_480p_yuv = 1;
  uint32_t width = 864;
  uint32_t height = 480;
  float frame_rate = 30;

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  for (uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
        test_info_->name(), i);

    ret = recorder_.StartCamera(camera_id_, camera_start_params_);
    ASSERT_TRUE(ret == NO_ERROR);

    SessionCb session_status_cb = CreateSessionStatusCb();
    uint32_t session_id;
    ret = recorder_.CreateSession(session_status_cb, &session_id);
    ASSERT_TRUE(session_id > 0);
    ASSERT_TRUE(ret == NO_ERROR);

    CameraMetadata meta;
    ret = recorder_.GetDefaultCaptureParam(camera_id_, meta);
    ASSERT_TRUE(ret == NO_ERROR);

    /************************ Create Preview Track ****************************/

    VideoTrackCreateParam video_track_param{camera_id_, VideoFormat::kYUV,
                                            width, height, frame_rate};

    TrackCb video_track_cb;
    video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
        std::vector<BufferDescriptor> buffers, std::vector<MetaData> meta_buffers)
        { VideoTrackYUVDataCb(session_id, track_id, buffers, meta_buffers); };

    video_track_cb.event_cb = [&] (uint32_t track_id, EventType event_type,
        void *event_data, size_t event_data_size) { VideoTrackEventCb(track_id,
        event_type, event_data, event_data_size); };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_480p_yuv,
                                     video_track_param, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    std::vector<uint32_t> track_ids;
    track_ids.push_back(video_track_id_480p_yuv);
    sessions_.insert(std::make_pair(session_id, track_ids));

    /************************ Start Preview  **********************************/

    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    sleep(record_duration_ / 5 + 3);

    /************************ Enable ZSL **************************************/

    ImageConfigParam image_config;
    SnapshotType snapshot_type;
    snapshot_type.type = SnapshotMode::kZsl;

    // ZSL queue params
    snapshot_type.zsl_queue_params.width = kZslWidth;
    snapshot_type.zsl_queue_params.height = kZslHeight;
    snapshot_type.zsl_queue_params.queue_depth = kZslQDepth;
    snapshot_type.zsl_queue_params.image_format = ImageFormat::kNV21;

    // output image params
    snapshot_type.zsl_image_param.width = kZslWidth;
    snapshot_type.zsl_image_param.height = kZslHeight;
    snapshot_type.zsl_image_param.image_quality = default_jpeg_quality_;
    snapshot_type.zsl_image_param.image_format = ImageFormat::kJPEG;

    bool res_supported = GtestCommon::ValidateResFromProcessedSizes(meta,
        snapshot_type.zsl_queue_params.width,
        snapshot_type.zsl_queue_params.height);
    ASSERT_TRUE (res_supported == true);

    res_supported = GtestCommon::ValidateResFromJpegSizes(meta,
        snapshot_type.zsl_image_param.width,
        snapshot_type.zsl_image_param.height);
    ASSERT_TRUE (res_supported == true);

    image_config.Update(QMMF_SNAPSHOT_TYPE, snapshot_type, 0);

    ret = recorder_.ConfigImageCapture(camera_id_, image_config);
    ASSERT_TRUE(ret == NO_ERROR);

    sleep(record_duration_ / 5 + 3);

    /************************ Disable ZSL *************************************/

    ret = recorder_.CancelCaptureImage(camera_id_);
    ASSERT_TRUE(ret == NO_ERROR);

    sleep(record_duration_ / 5 + 3);


    /************************ Enable ZSL **************************************/

    ret = recorder_.ConfigImageCapture(camera_id_, image_config);
    ASSERT_TRUE(ret == NO_ERROR);

    sleep(record_duration_ / 5 + 3);

    /************************ Disable ZSL *************************************/

    snapshot_type.type = SnapshotMode::kVideo;
    image_config.Update(QMMF_SNAPSHOT_TYPE, snapshot_type, 0);

    ret = recorder_.ConfigImageCapture(camera_id_, image_config);
    ASSERT_TRUE(ret == NO_ERROR);

    sleep(record_duration_ / 5 + 3);

    /************************ Stop Preview ************************************/

    ret = recorder_.StopSession(session_id, false);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_480p_yuv);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    ClearSessions();

    ret = recorder_.StopCamera(camera_id_);
    ASSERT_TRUE(ret == NO_ERROR);
  }

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());
}

/*
* CancelCapture: This test will exercise CancelCapture Api, it will trigger
* CancelCaptureRequest api after submitting Burst Capture request.
* Api test sequence:
*  - StartCamera
*   loop Start {
*   --------------------
*  - CaptureImage - Burst of 30 images.
*  - CancelCaptureImage
*   ---------------------
*   } loop End
*  - StopCamera
*/
TEST_F(RecorderBaseGTest, CancelCaptureImage) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  camera_start_params_.frame_rate = 30;

  ret = recorder_.StartCamera(camera_id_, camera_start_params_);
  ASSERT_TRUE(ret == NO_ERROR);

  ImageParam image_param{};
  image_param.width         = 3840;
  image_param.height        = 2160;
  image_param.image_format  = ImageFormat::kJPEG;
  image_param.image_quality = default_jpeg_quality_;

  std::vector<CameraMetadata> meta_array;
  camera_metadata_entry_t entry;
  CameraMetadata meta;

  ret = recorder_.GetDefaultCaptureParam(camera_id_, meta);
  ASSERT_TRUE(ret == NO_ERROR);

  CameraMetadata static_meta;
  ret = recorder_.GetCameraCharacteristics(camera_id_, static_meta);
  ASSERT_TRUE(ret == NO_ERROR);

  bool res_supported = false;
  // Check Supported Raw YUV snapshot resolutions.
  if (static_meta.exists(ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS)) {
    entry = static_meta.find(ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS);
    for (uint32_t i = 0 ; i < entry.count; i += 4) {
      if (HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED == entry.data.i32[i]) {
        if (ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT ==
            entry.data.i32[i+3]) {
          if (image_param.width == static_cast<uint32_t>(entry.data.i32[i+1])
              && image_param.height ==
                  static_cast<uint32_t>(entry.data.i32[i+2])) {
            res_supported = true;
          }
        }
      }
    }
  }
  ASSERT_TRUE(res_supported != false);

  TEST_INFO("%s: Running Test(%s)", __func__,
    test_info_->name());

  ImageCaptureCb cb = [this] (uint32_t camera_id, uint32_t image_count,
                                BufferDescriptor buffer,
                                MetaData meta_data) -> void
      { SnapshotCb(camera_id, image_count, buffer, meta_data); };

  uint32_t num_images = 30;
  for (uint32_t i = 0; i < num_images; i++) {
    meta_array.push_back(meta);
  }

  for(uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
        test_info_->name(), i);
    ret = recorder_.CaptureImage(camera_id_, image_param, num_images, meta_array,
                               cb);
    ASSERT_TRUE(ret == NO_ERROR);

    sleep(5);

    ret = recorder_.CancelCaptureImage(camera_id_);
    ASSERT_TRUE(ret == NO_ERROR);
  }

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());
}

/*
* GetNumberOfCameras:
*     This test case will test GetNumberOfCameras Api.
*
* Api test sequence:
*   loop Start {
*   ------------------
*   - Connect
*   - GetNumberOfCameras
*   - Disconnect
*   ------------------
*   } loop End
*/
TEST_F(RecorderBaseGTest, GetNumberOfCameras) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  for (uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
        test_info_->name(), i);

    auto ret = recorder_.Connect(recorder_status_cb_);
    ASSERT_TRUE(ret == NO_ERROR);

    SupportedCameras supported_cameras;
    recorder_.GetNumberOfCameras(supported_cameras);
    ASSERT_TRUE(supported_cameras.size() > 0);

    for (auto camera : supported_cameras) {
      TEST_INFO("%s: camera_id %d camera_type %d ", __func__,
          camera.id, camera.type);
    }

    ret = recorder_.Disconnect();
    ASSERT_TRUE(ret == NO_ERROR);

    // Sleep for 3 seconds before next iteration, otherwise the test is
    // too fast and everything will be printed almost simultaneously.
    sleep(3);
  }
  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());
}

/*
* SingleSessionCameraParamTest: This test will test camera parameter setting.
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
TEST_F(RecorderBaseGTest, SingleSessionCameraParamTest) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  CameraResultCb result_cb = [&] (uint32_t camera_id,
      const CameraMetadata &result) {
    CameraResultCallbackHandler(camera_id, result); };
  ret = recorder_.StartCamera(camera_id_, camera_start_params_, result_cb);
  ASSERT_TRUE(ret == NO_ERROR);

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

  std::vector<uint32_t> track_ids;
  track_ids.push_back(video_track_id);
  sessions_.insert(std::make_pair(session_id, track_ids));

  ret = recorder_.StartSession(session_id);
  ASSERT_TRUE(ret == NO_ERROR);

  // run session for some time
  sleep(5);
  int dump_fd = open(DUMP_META_PATH, O_WRONLY|O_CREAT, 0644);
  ASSERT_TRUE(0 <= dump_fd);

  CameraMetadata meta;
  ret = recorder_.GetCameraParam(camera_id_, meta);
  ASSERT_TRUE(ret == NO_ERROR);

  meta.dump(dump_fd, 2);
  close(dump_fd);

  // Switch AWB mode
  uint8_t awb_mode = ANDROID_CONTROL_AWB_MODE_INCANDESCENT;
  ret = meta.update(ANDROID_CONTROL_AWB_MODE, &awb_mode, 1);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.SetCameraParam(camera_id_, meta);
  ASSERT_TRUE(ret == NO_ERROR);

  // run session for some time
  sleep(5);

  ret = recorder_.StopSession(session_id, true);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteVideoTrack(session_id, video_track_id);
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
* MultiSessionCameraParamTest: This testcase will verify camera parameter
* setting in multi session senarios.
* Api test sequence:
*  - StartCamera
*   - CreateSession
*   - CreateVideoTrack - 1080p
*   - StartSession
*   - SetCameraParameter - TNR
*   loop Start {
*   ------------------
*   - CreateSession
*   - CreateVideoTrack
*   - StartSession
*   - StopSession
*   - DeleteVideoTrack
*   - DeleteSession
*   - SetCameraParameter - TNR
*   ------------------
*   } loop End
*   - StopSession
*   - DeleteVideoTrack
*   - DeleteSession
*  - StopCamera
*/
TEST_F(RecorderBaseGTest, MultiSessionCameraParamTest) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  VideoFormat format_type = VideoFormat::kAVC;
  uint32_t width  = 1920;
  uint32_t height = 1080;
  ret = recorder_.StartCamera(camera_id_, camera_start_params_);
  ASSERT_TRUE(ret == NO_ERROR);

  SessionCb session_status_cb = CreateSessionStatusCb();
  uint32_t session_id1;
  ret = recorder_.CreateSession(session_status_cb, &session_id1);
  ASSERT_TRUE(session_id1 > 0);
  ASSERT_TRUE(ret == NO_ERROR);
  VideoTrackCreateParam video_track_param{camera_id_, format_type,
                                          width, height, 30};
  uint32_t video_track_id = 1;

  if (dump_bitstream_.IsEnabled()) {
    StreamDumpInfo dumpinfo = { format_type, session_id1, video_track_id,
                                width, height };
    ret = dump_bitstream_.SetUp(dumpinfo);
    ASSERT_TRUE(ret == NO_ERROR);
  }

  TrackCb video_track_cb;
  video_track_cb.data_cb = [&, session_id1] (uint32_t track_id,
      std::vector<BufferDescriptor> buffers,
      std::vector<MetaData> meta_buffers) {
        VideoTrackEncDataCb(session_id1, track_id, buffers, meta_buffers);
      };

  video_track_cb.event_cb = [&] (uint32_t track_id, EventType event_type,
      void *event_data, size_t event_data_size) { VideoTrackEventCb(track_id,
      event_type, event_data, event_data_size); };

  ret = recorder_.CreateVideoTrack(session_id1, video_track_id,
                                    video_track_param, video_track_cb);
  ASSERT_TRUE(ret == NO_ERROR);

  std::vector<uint32_t> track_ids;
  track_ids.push_back(video_track_id);
  sessions_.insert(std::make_pair(session_id1, track_ids));

  ret = recorder_.StartSession(session_id1);
  ASSERT_TRUE(ret == NO_ERROR);

  //Enable TNR - Fast mode.
  CameraMetadata meta;
  auto status = recorder_.GetCameraParam(camera_id_, meta);
  if (NO_ERROR == status) {
    if (meta.exists(ANDROID_NOISE_REDUCTION_MODE)) {
      const uint8_t tnr_mode = ANDROID_NOISE_REDUCTION_MODE_FAST;
      TEST_INFO("%s Enable TNR mode(%d)", __func__, tnr_mode);
      meta.update(ANDROID_NOISE_REDUCTION_MODE, &tnr_mode, 1);
      status = recorder_.SetCameraParam(camera_id_, meta);
      ASSERT_TRUE(ret == NO_ERROR);
    }
  }

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

    //Enable TNR - High quality mode.
    CameraMetadata meta;
    auto status = recorder_.GetCameraParam(camera_id_, meta);
    if (NO_ERROR == status) {
      if (meta.exists(ANDROID_NOISE_REDUCTION_MODE)) {
        const uint8_t tnr_mode = ANDROID_NOISE_REDUCTION_MODE_HIGH_QUALITY;
        TEST_INFO("%s Enable TNR mode(%d)", __func__, tnr_mode);
        meta.update(ANDROID_NOISE_REDUCTION_MODE, &tnr_mode, 1);
        status = recorder_.SetCameraParam(camera_id_, meta);
        ASSERT_TRUE(ret == NO_ERROR);
      }
    }
  }

  sleep(record_duration_);

  ret = recorder_.StopSession(session_id1, false);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteVideoTrack(session_id1, video_track_id);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteSession(session_id1);
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
