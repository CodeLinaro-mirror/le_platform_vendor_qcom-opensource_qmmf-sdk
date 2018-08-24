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
#define LOG_TAG "RecorderPostprocessVideoGTest"

#include "recorder/test/gtest/qmmf_recorder_postprocessing_video_gtest.h"

using namespace qcamera;

/*
* HFRModeSwitch: This test will test switching between HFR/non-HFR mode.
* Api test sequence:
*
*   loop Start {
*   ------------------
*   - StartCamera
*   - CreateSession
*   - CreateVideoTrack
*   - StartSession
*   - recording @30fps
*   ---------------
*   - StopSession
*   - DeleteVideoTrack
*   - SetCameraParam (change fps to 60)
*   - CreateVideoTrack
*   - StartSession
*   - recording @120fps
*   ---------------
*   - StopSession
*   - DeleteVideoTrack
*   - SetCameraParam (change fps to 30)
*   - CreateVideoTrack
*   - StartSession
*   - recording @30fps
*   ---------------
*   - StopSession
*   - DeleteVideoTrack
*   - DeleteSession
*   - StopCamera
*   ------------------
*   } loop End
*
*/
TEST_F(RecorderPostprocessVideoGTest, HFRModeSwitch) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  float fps = 30;
  int32_t fps_range[2] = {0, 0};
  CameraMetadata meta;
  uint32_t width  = 1920;
  uint32_t height = 1080;
  VideoFormat format_type = VideoFormat::kYUV;
  camera_start_params_.frame_rate = fps;

  for(uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
        test_info_->name(), i);

    auto ret = Init();
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.StartCamera(camera_id_, camera_start_params_);
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
      StreamDumpInfo dumpinfo = { format_type, video_track_id, session_id,
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

    sleep(5);

    ret = recorder_.GetCameraParam(camera_id_, meta);
    ASSERT_TRUE(ret == NO_ERROR);

    if (meta.exists(ANDROID_CONTROL_AE_TARGET_FPS_RANGE)) {
      fps_range[0] = 60;
      fps_range[1] = 60;

      ret = recorder_.StopSession(session_id, false);
      ASSERT_TRUE(ret == NO_ERROR);

      ret = recorder_.DeleteVideoTrack(session_id, video_track_id);
      ASSERT_TRUE(ret == NO_ERROR);


      ret = meta.update(ANDROID_CONTROL_AE_TARGET_FPS_RANGE, fps_range, 2);
      ASSERT_TRUE(ret == NO_ERROR);
      ret = recorder_.SetCameraParam(camera_id_, meta);
      ASSERT_TRUE(ret == NO_ERROR);

      video_track_param.frame_rate = fps_range[1];


      ret = recorder_.CreateVideoTrack(session_id, video_track_id,
                                        video_track_param, video_track_cb);

      ret = recorder_.StartSession(session_id);
      ASSERT_TRUE(ret == NO_ERROR);

      sleep(10);

      fps_range[0] = 30;
      fps_range[1] = 30;

      ret = recorder_.StopSession(session_id, false);
      ASSERT_TRUE(ret == NO_ERROR);

      ret = recorder_.DeleteVideoTrack(session_id, video_track_id);
      ASSERT_TRUE(ret == NO_ERROR);

      ret = meta.update(ANDROID_CONTROL_AE_TARGET_FPS_RANGE, fps_range, 2);
      ASSERT_TRUE(ret == NO_ERROR);
      ret = recorder_.SetCameraParam(camera_id_, meta);
      ASSERT_TRUE(ret == NO_ERROR);

      video_track_param.frame_rate = fps_range[1];

      ret = recorder_.CreateVideoTrack(session_id, video_track_id,
                                       video_track_param, video_track_cb);

      ret = recorder_.StartSession(session_id);
      ASSERT_TRUE(ret == NO_ERROR);

      sleep(10);
    }

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

  }



  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());

}

/*
* SessionWith4kp30fps480p30fpsVSTABEncTrack: This test will test session with one 4k
* 30fps h264 track and one 480p 30fps h264 track with VSTAB enabled.
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
TEST_F(RecorderPostprocessVideoGTest, SessionWith4kp30fps480p30fpsVSTABEncTrack) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  VideoFormat format_type = VideoFormat::kAVC;
  uint32_t width  = 3840;
  uint32_t height = 2160;
  float fps = 30;

  camera_start_params_.frame_rate = fps;
  ret = recorder_.StartCamera(camera_id_, camera_start_params_);
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
    StreamDumpInfo dumpinfo = { format_type, video_track_id, session_id,
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
    StreamDumpInfo dumpinfo = { format_type, video_track480p_id, session_id,
                                width, height };
    ret = dump_bitstream_.SetUp(dumpinfo);
  }


  video_track_param.width       = width;
  video_track_param.height      = height;


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

  sleep(10);
  //Enable VSTAB
  CameraMetadata meta;
  ret = recorder_.GetCameraParam(camera_id_, meta);
  ASSERT_TRUE(ret == NO_ERROR);

  uint8_t vstab_mode = ANDROID_CONTROL_VIDEO_STABILIZATION_MODE_ON;
  meta.update(ANDROID_CONTROL_VIDEO_STABILIZATION_MODE, &vstab_mode, 1);
  ret = recorder_.SetCameraParam(camera_id_, meta);
  ASSERT_TRUE(ret == NO_ERROR);

  //Continue recording
  sleep(20);

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
* LowResVideo10MPContinuousSnapshotWithLCACAndCdsOff: This gtest will test Continuous
*    10MP JPEG snapshot with Bayer LCAC.
* Api test sequence:
*  - StartCamera
*  - Low resolution video 640x480@30fps
*  - Disable CDS
*  - Continuous CaptureImage - BayerLcac + JPEG (Continius capture)
*  - CancelCaptureImage
*  - StopCamera
*/
TEST_F(RecorderPostprocessVideoGTest, LowResVideo10MPContinuousSnapshotWithLCACAndCdsOff) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.StartCamera(camera_id_, camera_start_params_);
  ASSERT_TRUE(ret == NO_ERROR);

  SessionCb session_status_cb = CreateSessionStatusCb();
  uint32_t session_id;
  ret = recorder_.CreateSession(session_status_cb, &session_id);
  ASSERT_TRUE(session_id > 0);
  ASSERT_TRUE(ret == NO_ERROR);

  if (dump_bitstream_.IsEnabled()) {
    StreamDumpInfo dumpinfo = {
      VideoFormat::kAVC,
      session_id,
      1, 640, 480
    };
    ret = dump_bitstream_.SetUp(dumpinfo);
    ASSERT_TRUE(ret == NO_ERROR);
  }

  TrackCb video_track_cb;
  video_track_cb.event_cb =
      [this] (uint32_t track_id, EventType event_type,
              void *event_data, size_t event_data_size) -> void {
      VideoTrackEventCb(track_id, event_type, event_data, event_data_size); };

  uint32_t video_track_id = 1;
  VideoTrackCreateParam video_track_param{camera_id_, VideoFormat::kAVC,
                                          640,
                                          480,
                                          30};

  video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
      std::vector<BufferDescriptor> buffers,
      std::vector<MetaData> meta_buffers) {
        VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
      };

  ret = recorder_.CreateVideoTrack(session_id, video_track_id,
                                   video_track_param, video_track_cb);
  ASSERT_TRUE(ret == NO_ERROR);

  std::vector<uint32_t> track_ids = {video_track_id};
  sessions_.insert(std::make_pair(session_id, track_ids));

  ret = recorder_.StartSession(session_id);
  ASSERT_TRUE(ret == NO_ERROR);

  // Record for sometime
  sleep(5);

  ImageParam image_param{};
  image_param.width         = 3872;
  image_param.height        = 2592;
  image_param.image_format  = ImageFormat::kJPEG;
  image_param.image_quality = default_jpeg_quality_;

  std::vector<CameraMetadata> meta_array;
  CameraMetadata meta;
  ret = recorder_.GetDefaultCaptureParam(camera_id_, meta);
  ASSERT_TRUE(ret == NO_ERROR);

  bool res_supported = Common::ValidateResFromJpegSizes(meta,
      image_param.width, image_param.height);
  ASSERT_TRUE (res_supported != false);

  ImageCaptureCb cb = [this] (uint32_t camera_id, uint32_t image_count,
                              BufferDescriptor buffer,
                              MetaData meta_data) -> void
      { SnapshotCb(camera_id, image_count, buffer, meta_data);
      };

  ImageConfigParam image_config;
  PostprocPlugin bayer_lcac_plugin, edge_smooth_plugin;

  SupportedPlugins supported_plugins;
  ret = recorder_.GetSupportedPlugins(&supported_plugins);
  ASSERT_TRUE(ret == NO_ERROR);

  bool found = false;
  for (auto const& plugin_info : supported_plugins) {
    if (plugin_info.name == "BayerLcac") {
      ret = recorder_.CreatePlugin(&bayer_lcac_plugin.uid, plugin_info);
      ASSERT_TRUE(ret == NO_ERROR);

      image_config.Update(QMMF_POSTPROCESS_PLUGIN, bayer_lcac_plugin, 0);
      found = true;
    }
  }
  ASSERT_TRUE(found == true);

  found = false;

  // Update same focal length to streaming meta.
  float focal_length = 8.0; // 4 fps mode.
  ret = SetCameraFocalLength(focal_length);
  ASSERT_TRUE(ret == NO_ERROR);


  SnapshotType snapshot_type;
  snapshot_type.type = SnapshotMode::kContinuous;
  image_config.Update(QMMF_SNAPSHOT_TYPE, snapshot_type, 0);

  ret = recorder_.ConfigImageCapture(camera_id_, image_config);
  ASSERT_TRUE(ret == NO_ERROR);

  int32_t cds_mode = 0; // 0-Off, 1-On, 2-Auto
  TEST_INFO("%s: Disable CDS", __func__);
  meta.update( QCAMERA3_CDS_MODE, &cds_mode, 1);

  meta_array.clear();
  meta_array.push_back(meta);
  ret = recorder_.CaptureImage(camera_id_, image_param, 1, meta_array, cb);
  ASSERT_TRUE(ret == NO_ERROR);

  // take continuous snapshots till 10 secs to simulate long press.
  sleep(10);

  focal_length = 6.0;
  ret = SetCameraFocalLength(focal_length);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.CancelCaptureImage(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  //preview
  sleep(5);

  ret = recorder_.DeletePlugin(bayer_lcac_plugin.uid);
  ASSERT_TRUE(ret == NO_ERROR);

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
* LowResVideo10MPJpeg422ContinuousCaptureWithLCACandEdgeSmooth:
*     This test will test 10MP JPEG single snapshot with reprocessing
*     (bayer LCAC, edge smooth) followed by Continuous capture with same
*     configuration.
*
* Api test sequence:
*  - StartCamera
*  - Low resolution video 640x480@30fps
*  - Single Capture - BayerLcac + EdgeSmooth + JPEG
*  - Continuous Capture - BayerLcac + JPEG (Continius capture)
*  - CancelCaptureImage
*  - StopCamera
*/
TEST_F(RecorderPostprocessVideoGTest,
       LowResVideo10MPJpeg422ContinuousCaptureWithLCACandEdgeSmooth) {

  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.StartCamera(camera_id_, camera_start_params_);
  ASSERT_TRUE(ret == NO_ERROR);

  SessionCb session_status_cb = CreateSessionStatusCb();
  uint32_t session_id;
  ret = recorder_.CreateSession(session_status_cb, &session_id);
  ASSERT_TRUE(session_id > 0);
  ASSERT_TRUE(ret == NO_ERROR);

  if (dump_bitstream_.IsEnabled()) {
    StreamDumpInfo dumpinfo = {
      VideoFormat::kAVC,
      session_id,
      1, 640, 480
    };
    ret = dump_bitstream_.SetUp(dumpinfo);
    ASSERT_TRUE(ret == NO_ERROR);
  }

  TrackCb video_track_cb;
  video_track_cb.event_cb =
      [this] (uint32_t track_id, EventType event_type,
              void *event_data, size_t event_data_size) -> void {
      VideoTrackEventCb(track_id, event_type, event_data, event_data_size); };

  uint32_t video_track_id = 1;
  VideoTrackCreateParam video_track_param{camera_id_, VideoFormat::kAVC,
                                          640,
                                          480,
                                          30};

  video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
      std::vector<BufferDescriptor> buffers,
      std::vector<MetaData> meta_buffers) {
        VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
      };

  ret = recorder_.CreateVideoTrack(session_id, video_track_id,
                                   video_track_param, video_track_cb);
  ASSERT_TRUE(ret == NO_ERROR);

  std::vector<uint32_t> track_ids = {video_track_id};
  sessions_.insert(std::make_pair(session_id, track_ids));

  ret = recorder_.StartSession(session_id);
  ASSERT_TRUE(ret == NO_ERROR);

  // Record for sometime
  sleep(5);

  ImageParam image_param{};
  image_param.width         = 3872;
  image_param.height        = 2592;
  image_param.image_format  = ImageFormat::kJPEG;
  image_param.image_quality = default_jpeg_quality_;

  std::vector<CameraMetadata> meta_array;
  CameraMetadata meta;
  ret = recorder_.GetDefaultCaptureParam(camera_id_, meta);
  ASSERT_TRUE(ret == NO_ERROR);

  // Update focal length to capture meta to select 4fps sensor mode.
  float focal_length = 8.0;
  meta.update(ANDROID_LENS_FOCAL_LENGTH, &focal_length, 1);

  bool res_supported = Common::ValidateResFromJpegSizes(meta,
      image_param.width, image_param.height);
  ASSERT_TRUE(res_supported != false);

  // number of frames 1. Timeout 5s (4fps snapshot).
  test_wait_.Reset(1, 5);

  ImageCaptureCb cb = [this] (uint32_t camera_id, uint32_t image_count,
                              BufferDescriptor buffer,
                              MetaData meta_data) -> void
      { SnapshotCb(camera_id, image_count, buffer, meta_data);
        test_wait_.Done();
      };

  ImageConfigParam image_config;
  PostprocPlugin bayer_lcac_plugin, edge_smooth_plugin;

  SupportedPlugins supported_plugins;
  ret = recorder_.GetSupportedPlugins(&supported_plugins);
  ASSERT_TRUE(ret == NO_ERROR);

  bool found = false;
  for (auto const& plugin_info : supported_plugins) {
    if (plugin_info.name == "BayerLcac") {
      ret = recorder_.CreatePlugin(&bayer_lcac_plugin.uid, plugin_info);
      ASSERT_TRUE(ret == NO_ERROR);

      image_config.Update(QMMF_POSTPROCESS_PLUGIN, bayer_lcac_plugin, 0);
      found = true;
    }
  }
  ASSERT_TRUE(found == true);

  found = false;
  for (auto const& plugin_info : supported_plugins) {
    if (plugin_info.name == "EdgeSmooth") {
      ret = recorder_.CreatePlugin(&edge_smooth_plugin.uid, plugin_info);
      ASSERT_TRUE(ret == NO_ERROR);

      image_config.Update(QMMF_POSTPROCESS_PLUGIN, edge_smooth_plugin, 1);
      found = true;
    }
  }
  ASSERT_TRUE(found == true);

  HighQualityCaptureSetup high_quality_setup;
  high_quality_setup.jpeg_input_format = BufferFormat::kNV16;
  image_config.Update(QMMF_JPEG_CAPTURE_SETUP, high_quality_setup);

  // Update same focal length to streaming meta.
  focal_length = 8.0;
  ret = SetCameraFocalLength(focal_length);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.ConfigImageCapture(camera_id_, image_config);
  ASSERT_TRUE(ret == NO_ERROR);

  meta_array.push_back(meta);
  ret = recorder_.CaptureImage(camera_id_, image_param, 1, meta_array, cb);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = test_wait_.Wait();
  ASSERT_TRUE(ret == NO_ERROR);

  //Continius capture
  std::string config = "{\"EdgeSmooth\" : false }";
  ret = recorder_.ConfigPlugin(edge_smooth_plugin.uid, config);
  ASSERT_TRUE(ret == NO_ERROR);

  ImageConfigParam image_config_continius;
  SnapshotType snapshot_type;
  snapshot_type.type = SnapshotMode::kContinuous;
  image_config_continius.Update(QMMF_SNAPSHOT_TYPE, snapshot_type, 0);

  ret = recorder_.ConfigImageCapture(camera_id_, image_config_continius);
  ASSERT_TRUE(ret == NO_ERROR);

  // Lock AE for snapshot
  uint8_t ae_lock = ANDROID_CONTROL_AE_LOCK_ON;
  ret = meta.update(ANDROID_CONTROL_AE_LOCK, &ae_lock, 1);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.SetCameraParam(camera_id_, meta);
  ASSERT_TRUE(ret == NO_ERROR);

  meta_array.clear();
  meta_array.push_back(meta);
  ret = recorder_.CaptureImage(camera_id_, image_param, 1, meta_array, cb);
  ASSERT_TRUE(ret == NO_ERROR);

  sleep(6);

  focal_length = 6.0;
  ret = SetCameraFocalLength(focal_length);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.CancelCaptureImage(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  // Unlock AE
  ae_lock = ANDROID_CONTROL_AE_LOCK_OFF;
  ret = meta.update(ANDROID_CONTROL_AE_LOCK, &ae_lock, 1);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.SetCameraParam(camera_id_, meta);
  ASSERT_TRUE(ret == NO_ERROR);

  //preview
  sleep(5);

  ret = recorder_.DeletePlugin(bayer_lcac_plugin.uid);
  ASSERT_TRUE(ret == NO_ERROR);

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
* SessionWith27Kp60fps480p30fpsVSTABEncTrack: This test will test session with
*  one 2.7K 60fps h264 track and one 480p 30 fps h264 track with VSTAB enabled.
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
TEST_F(RecorderPostprocessVideoGTest, SessionWith27Kp60fps480p30fpsVSTABEncTrack) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  VideoFormat format_type = VideoFormat::kAVC;
  uint32_t width  = 2704;
  uint32_t height = 1520;
  float fps = 60;

  camera_start_params_.frame_rate = fps;
  ret = recorder_.StartCamera(camera_id_, camera_start_params_);
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
    StreamDumpInfo dumpinfo = { format_type, video_track_id, session_id,
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
    StreamDumpInfo dumpinfo = { format_type, video_track480p_id, session_id,
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

  sleep(10);
  //Enable VSTAB
  CameraMetadata meta;
  ret = recorder_.GetCameraParam(camera_id_, meta);
  ASSERT_TRUE(ret == NO_ERROR);

  uint8_t vstab_mode = ANDROID_CONTROL_VIDEO_STABILIZATION_MODE_ON;
  meta.update(ANDROID_CONTROL_VIDEO_STABILIZATION_MODE, &vstab_mode, 1);
  ret = recorder_.SetCameraParam(camera_id_, meta);
  ASSERT_TRUE(ret == NO_ERROR);

  //Continue recording
  sleep(20);

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
* SessionWith4KEncWithLCACYUV: This test will test session with one
*                              4K h264 track and LCAC YUV.
* API test sequence:
*  - StartCamera
*   loop Start {
*   ------------------
*   - CreateSession
*   - CreateVideoTrack
*   - Enable LCAC YUV
*   - StartVideoTrack
*   - StopSession
*   - DeleteVideoTrack
*   - DeleteSession
*   ------------------
*   } loop End
*  - StopCamera
*/
TEST_F(RecorderPostprocessVideoGTest, SessionWith4KEncWithLCACYUV) {
  fprintf(stderr, "\n---------- Run Test %s.%s ------------\n",
          test_info_->test_case_name(), test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  VideoFormat format_type = VideoFormat::kAVC;
  uint32_t width = 3840;
  uint32_t height = 2160;

  ret = recorder_.StartCamera(camera_id_, camera_start_params_);
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

    VideoTrackCreateParam video_track_param{camera_id_, format_type, width,
                                            height, 30};
    uint32_t video_track_id = 1;

    if (dump_bitstream_.IsEnabled()) {
      StreamDumpInfo dumpinfo = { format_type, video_track_id, session_id,
                                  width, height};
      ret = dump_bitstream_.SetUp(dumpinfo);
      ASSERT_TRUE(ret == NO_ERROR);
    }

    TrackCb video_track_cb;
    video_track_cb.data_cb = [&, session_id](
        uint32_t track_id, std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
      VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
    };

    video_track_cb.event_cb = [this](uint32_t track_id, EventType event_type,
                                     void *event_data,
                                     size_t event_data_size) -> void {
      VideoTrackEventCb(track_id, event_type, event_data, event_data_size);
    };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id,
                                     video_track_param, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    std::vector<uint32_t> track_ids;
    track_ids.push_back(video_track_id);
    sessions_.insert(std::make_pair(session_id, track_ids));

    CameraMetadata meta;
    // Enable YUV LCAC
    ret = recorder_.GetCameraParam(camera_id_, meta);
    if (NO_ERROR == ret) {
      uint8_t enable_lcac = 1;
      ret = meta.update( QCAMERA3_LCAC_PROCESSING_ENABLE, &enable_lcac, 1);
      ASSERT_TRUE(ret == NO_ERROR);
      ret = recorder_.SetCameraParam(camera_id_, meta);
      ASSERT_TRUE(ret == NO_ERROR);
    }
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

  fprintf(stderr, "---------- Test Completed %s.%s ----------\n",
          test_info_->test_case_name(), test_info_->name());
}

/*
* SessionWith4KEnc1080pYUVSwTnrTrack: This test will test session with
*        4K h264 track and post processing. Post processing pipe is
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
TEST_F(RecorderPostprocessVideoGTest, SessionWith4KEnc1080pYUVSwTnrTrack) {
  fprintf(stderr, "\n---------- Run Test %s.%s ------------\n",
          test_info_->test_case_name(), test_info_->name());

  auto ret = Init();
  assert(ret == NO_ERROR);

  VideoFormat format_type = VideoFormat::kAVC;
  uint32_t width = 3840;
  uint32_t height = 2160;

  ret = recorder_.StartCamera(camera_id_, camera_start_params_);
  assert(ret == NO_ERROR);

  for (uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr, "test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
              test_info_->name(), i);

    SessionCb session_status_cb = CreateSessionStatusCb();
    uint32_t session_id;
    ret = recorder_.CreateSession(session_status_cb, &session_id);
    assert(session_id > 0);
    assert(ret == NO_ERROR);
    VideoTrackCreateParam video_track_param{camera_id_, format_type, width,
                                            height, 30};
    uint32_t video_track_id_4k = 1;
    if (dump_bitstream_.IsEnabled()) {
      StreamDumpInfo dumpinfo = { format_type, video_track_id_4k, session_id,
                                  width, height};
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
    PostprocPlugin sw_tnr_plugin4k;

    SupportedPlugins supported_plugins;
    ret = recorder_.GetSupportedPlugins(&supported_plugins);
    assert(ret == NO_ERROR);

    for (auto const &plugin_info : supported_plugins) {
      if (plugin_info.name == "SwTnr") {
        ret = recorder_.CreatePlugin(&sw_tnr_plugin4k.uid, plugin_info);
        assert(ret == NO_ERROR);

        extra_param.Update(QMMF_POSTPROCESS_PLUGIN, sw_tnr_plugin4k);
      }
    }

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_4k,
                                     video_track_param, extra_param,
                                     video_track_cb);

    assert(ret == NO_ERROR);

    std::vector<uint32_t> track_ids;
    track_ids.push_back(video_track_id_4k);

    uint32_t video_trackid_1080p = 2;
    video_track_param.width = 1920;
    video_track_param.height = 1080;
    video_track_param.frame_rate = 30;
    video_track_param.format_type = VideoFormat::kYUV;

    video_track_cb.data_cb = [&, session_id](
        uint32_t track_id, std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
      VideoTrackYUVDataCb(session_id, track_id, buffers, meta_buffers); };

    PostprocPlugin sw_tnr_plugin1080p;
    for (auto const &plugin_info : supported_plugins) {
      if (plugin_info.name == "SwTnr") {
        ret = recorder_.CreatePlugin(&sw_tnr_plugin1080p.uid, plugin_info);
        assert(ret == NO_ERROR);

        extra_param.Update(QMMF_POSTPROCESS_PLUGIN, sw_tnr_plugin1080p);
      }
    }

    ret = recorder_.CreateVideoTrack(session_id, video_trackid_1080p,
                                     video_track_param, extra_param,
                                     video_track_cb);
    assert(ret == NO_ERROR);

    track_ids.push_back(video_trackid_1080p);

    sessions_.insert(std::make_pair(session_id, track_ids));

    ret = recorder_.StartSession(session_id);
    assert(ret == NO_ERROR);

    // Let session run for record_duration_, during this time buffer with valid
    // data would be received in track callback (VideoTrackDataCb).
    sleep(record_duration_);

    ret = recorder_.StopSession(session_id, false);
    assert(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_4k);
    assert(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_trackid_1080p);
    assert(ret == NO_ERROR);

    ret = recorder_.DeletePlugin(sw_tnr_plugin4k.uid);
    assert(ret == NO_ERROR);

    ret = recorder_.DeletePlugin(sw_tnr_plugin1080p.uid);
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
* SessionWith4KEncWithLCACYUVEIS: This test will test session with one
*                                 4K h264 track and LCAC YUV, EIS.
* API test sequence:
*  - StartCamera
*   loop Start {
*   ------------------
*   - CreateSession
*   - CreateVideoTrack
*   - Enable LCAC YUV
*   - Enable EIS
*   - StartVideoTrack
*   - StopSession
*   - DeleteVideoTrack
*   - DeleteSession
*   ------------------
*   } loop End
*  - StopCamera
*/
TEST_F(RecorderPostprocessVideoGTest, SessionWith4KEncWithLCACYUVEIS) {
  fprintf(stderr, "\n---------- Run Test %s.%s ------------\n",
          test_info_->test_case_name(), test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  VideoFormat format_type = VideoFormat::kAVC;
  uint32_t width = 3840;
  uint32_t height = 2160;

  ret = recorder_.StartCamera(camera_id_, camera_start_params_);
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

    VideoTrackCreateParam video_track_param{camera_id_, format_type, width,
                                            height, 30};
    uint32_t video_track_id = 1;

    if (dump_bitstream_.IsEnabled()) {
      StreamDumpInfo dumpinfo = { format_type, video_track_id, session_id,
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

    video_track_cb.event_cb = [this](uint32_t track_id, EventType event_type,
                                     void *event_data,
                                     size_t event_data_size) -> void {
      VideoTrackEventCb(track_id, event_type, event_data, event_data_size);
    };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id,
                                     video_track_param, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    std::vector<uint32_t> track_ids;
    track_ids.push_back(video_track_id);
    sessions_.insert(std::make_pair(session_id, track_ids));

    CameraMetadata meta;
    // Enable YUV LCAC
    ret = recorder_.GetCameraParam(camera_id_, meta);
    if (NO_ERROR == ret) {
      uint8_t enable_lcac = 1;
      ret = meta.update( QCAMERA3_LCAC_PROCESSING_ENABLE, &enable_lcac, 1);
      ASSERT_TRUE(ret == NO_ERROR);
      ret = recorder_.SetCameraParam(camera_id_, meta);
      ASSERT_TRUE(ret == NO_ERROR);
      // Enable EIS
      uint8_t vstab_mode = ANDROID_CONTROL_VIDEO_STABILIZATION_MODE_ON;
      ret = meta.update(ANDROID_CONTROL_VIDEO_STABILIZATION_MODE,
                        &vstab_mode, 1);
      ASSERT_TRUE(ret == NO_ERROR);
      ret = recorder_.SetCameraParam(camera_id_, meta);
      ASSERT_TRUE(ret == NO_ERROR);
    }
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

  fprintf(stderr, "---------- Test Completed %s.%s ----------\n",
          test_info_->test_case_name(), test_info_->name());
}

/*
* SessionWith1080pEncTrackWithHazeBuster: This test will test session with
*        1080p h264 track and post processing. Post processing pipe is
*        Haze buster.
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
TEST_F(RecorderPostprocessVideoGTest, SessionWith4KHazeBusterEncTrack) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  VideoFormat format_type = VideoFormat::kAVC;
  uint32_t width  = 3840;
  uint32_t height = 2160;

  ret = recorder_.StartCamera(camera_id_, camera_start_params_);
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
      StreamDumpInfo dumpinfo = { format_type, video_track_id, session_id,
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
    PostprocPlugin haze_buster_plugin;

    SupportedPlugins supported_plugins;
    ret = recorder_.GetSupportedPlugins(&supported_plugins);
    ASSERT_TRUE(ret == NO_ERROR);

    bool found = false;
    for (auto const& plugin_info : supported_plugins) {
      if (plugin_info.name == "HazeBuster") {
        ret = recorder_.CreatePlugin(&haze_buster_plugin.uid, plugin_info);
        ASSERT_TRUE(ret == NO_ERROR);

        extra_param.Update(QMMF_POSTPROCESS_PLUGIN, haze_buster_plugin);
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

    ret = recorder_.DeletePlugin(haze_buster_plugin.uid);
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
* SessionWith4KEncTrackWithHazeBuster: This test will test session with
*        4K h264 track and post processing. Post processing pipe is
*        Haze buster.
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
TEST_F(RecorderPostprocessVideoGTest, SessionWith4KEnc1080pYUVHazeBusterTrack) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  VideoFormat format_type = VideoFormat::kAVC;
  uint32_t width  = 3840;
  uint32_t height = 2160;

  ret = recorder_.StartCamera(camera_id_, camera_start_params_);
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
    uint32_t video_track_id_4k = 1;
    if (dump_bitstream_.IsEnabled()) {
      StreamDumpInfo dumpinfo = { format_type, video_track_id_4k, session_id,
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
    PostprocPlugin haze_buster_plugin4k;

    SupportedPlugins supported_plugins;
    ret = recorder_.GetSupportedPlugins(&supported_plugins);
    ASSERT_TRUE(ret == NO_ERROR);

    bool found = false;
    for (auto const& plugin_info : supported_plugins) {
      if (plugin_info.name == "HazeBuster") {
        ret = recorder_.CreatePlugin(&haze_buster_plugin4k.uid, plugin_info);
        ASSERT_TRUE(ret == NO_ERROR);

        extra_param.Update(QMMF_POSTPROCESS_PLUGIN, haze_buster_plugin4k);
        found = true;
      }
    }
    ASSERT_TRUE(found == true);

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_4k,
                                     video_track_param, extra_param,
                                     video_track_cb);

    ASSERT_TRUE(ret == NO_ERROR);

    std::vector<uint32_t> track_ids;
    track_ids.push_back(video_track_id_4k);

    uint32_t video_trackid_1080p = 2;
    video_track_param.width       = 1920;
    video_track_param.height      = 1080;
    video_track_param.frame_rate  = 30;
    video_track_param.format_type = VideoFormat::kYUV;

    video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
                                std::vector<BufferDescriptor> buffers,
                                std::vector<MetaData> meta_buffers) {
      VideoTrackYUVDataCb(session_id, track_id, buffers, meta_buffers); };

    PostprocPlugin haze_buster_plugin1080p;
    found = false;
    for (auto const& plugin_info : supported_plugins) {
      if (plugin_info.name == "HazeBuster") {
        ret = recorder_.CreatePlugin(&haze_buster_plugin1080p.uid, plugin_info);
        ASSERT_TRUE(ret == NO_ERROR);

        extra_param.Update(QMMF_POSTPROCESS_PLUGIN, haze_buster_plugin1080p);
        found = true;
      }
    }
    ASSERT_TRUE(found == true);

    ret = recorder_.CreateVideoTrack(session_id, video_trackid_1080p,
                                     video_track_param, extra_param,
                                     video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    track_ids.push_back(video_trackid_1080p);

    sessions_.insert(std::make_pair(session_id, track_ids));

    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    // Let session run for record_duration_, during this time buffer with valid
    // data would be received in track callback (VideoTrackDataCb).
    sleep(record_duration_);

    ret = recorder_.StopSession(session_id, false);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_4k);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_trackid_1080p);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeletePlugin(haze_buster_plugin4k.uid);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeletePlugin(haze_buster_plugin1080p.uid);
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
TEST_F(RecorderPostprocessVideoGTest, SingleSessionCameraParamTest) {
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
TEST_F(RecorderPostprocessVideoGTest, MultiSessionCameraParamTest) {
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
    StreamDumpInfo dumpinfo = { format_type, video_track_id, session_id1,
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

/*
* DynamicSessionAndTracksUpdateWithCamParams: This is a usecase which will test
*                                    adding or deleting tracks and sessions,
*                                    along with changing camera parameters such
                                     as TNR and SHDR.
*   Properties              : [Default Values]
*   -------------------------------------------
*   PROP_TRACK1_WIDTH       : [3840]
*   PROP_TRACK1_HEIGHT      : [2160]
*   PROP_TRACK1_FPS         : [30]
*   PROP_TRACK2_WIDTH       : [1920]
*   PROP_TRACK2_HEIGHT      : [1080]
*   PROP_TRACK2_FPS         : [24]
*   PROP_CAM_PARAMS1        : bit 0: SHDR [0], bit 1: TNR [0]
*   PROP_CAM_PARAMS2        : bit 0: SHDR [0], bit 1: TNR [0]
*   PROP_TRACK1_DELETE      : [0]
*   PROP_SESSION2_CREATE    : [0]
*
* Api test sequence:
*  - StartCamera
*   ------------------
*   - StartSession - 4k AVC
*   - [Enable/Disable SHDR, TNR]
*   - [StartSession - 1080p AVC]
*   - [Enable/Disable SHDR, TNR]
*   - [StopSession  - 1080p]
*   - StopSession - 4k
*   ------------------
*   - Stop Camera
*/
TEST_F(RecorderPostprocessVideoGTest, DynamicSessionAndTracksUpdateWithCamParams) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  // Init
  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  // Fetch Properties
  uint32_t w1, h1, w2, h2;
  float fps1, fps2;
  bool shdr1, shdr2, is_shdr_supported;
  bool tnr1, tnr2, is_tnr_supported;
  bool is_t1_delete, is_s2_create;
  char prop_val[PROPERTY_VALUE_MAX];

  property_get(PROP_TRACK1_WIDTH, prop_val, "3840");
  w1 = atoi(prop_val);
  ASSERT_TRUE(0 != w1);
  property_get(PROP_TRACK1_HEIGHT, prop_val, "2160");
  h1 = atoi(prop_val);
  ASSERT_TRUE(0 != h1);
  property_get(PROP_TRACK2_WIDTH, prop_val, "1920");
  w2 = atoi(prop_val);
  ASSERT_TRUE(0 != w2);
  property_get(PROP_TRACK2_HEIGHT, prop_val, "1080");
  h2 = atoi(prop_val);
  ASSERT_TRUE(0 != h2);

  property_get(PROP_TRACK1_FPS, prop_val, "30");
  fps1 = atoi(prop_val);
  ASSERT_TRUE(0 != fps1);
  property_get(PROP_TRACK2_FPS, prop_val, "24");
  fps2 = atoi(prop_val);
  ASSERT_TRUE(0 != fps2);

  property_get(PROP_CAM_PARAMS1, prop_val, "0");
  shdr1 = atoi(prop_val) & 0x1;
  tnr1 = atoi(prop_val) & 0x2;
  property_get(PROP_CAM_PARAMS2, prop_val, "0");
  shdr2 = atoi(prop_val) & 0x1;
  tnr2 = atoi(prop_val) & 0x2;

  property_get(PROP_TRACK1_DELETE, prop_val, "0");
  is_t1_delete = (atoi(prop_val) == 0) ? false : true;
  property_get(PROP_SESSION2_CREATE, prop_val, "0");
  is_s2_create = (atoi(prop_val) == 0) ? false : true;

  fprintf(stderr, "\nw1:%d h1:%d fps1:%f w2:%d h2:%d fps2:%f "
          "shdr1:%d tnr1:%d shdr2:%d tnr2:%d\n",
          w1, h1, fps1, w2, h2, fps2, shdr1, tnr1, shdr2, tnr2);

  bool is_allowed = (!(is_t1_delete && is_s2_create));
  ASSERT_TRUE(true == is_allowed);

  // Start
  ret = recorder_.StartCamera(camera_id_, camera_start_params_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.GetDefaultCaptureParam(camera_id_, static_info_);
  ASSERT_TRUE(ret == NO_ERROR);

  InitSupportedVHDRModes();
  is_shdr_supported = IsVHDRSupported();
  InitSupportedNRModes();
  is_tnr_supported = IsNRSupported();

  fprintf(stderr, "\nis_shdr_supported:%d is_tnr_supported:%d\n",
          is_shdr_supported, is_tnr_supported);

  // Create Session1
  SessionCb s1_status_cb = CreateSessionStatusCb();
  uint32_t s1_id;
  ret = recorder_.CreateSession(s1_status_cb, &s1_id);
  ASSERT_TRUE(s1_id > 0);
  ASSERT_TRUE(ret == NO_ERROR);

  // Create 4k AVC Stream
  VideoTrackCreateParam video_track1{camera_id_, VideoFormat::kAVC,
                                     w1,
                                     h1,
                                     fps1};
    video_track1.low_power_mode = false;

  TrackCb video_track1_cb;
  video_track1_cb.event_cb =
      [this] (uint32_t track_id, EventType event_type,
              void *event_data, size_t event_data_size) -> void {
      VideoTrackEventCb(track_id, event_type, event_data, event_data_size); };

  video_track1_cb.data_cb = [&, s1_id] (uint32_t track_id,
                                std::vector<BufferDescriptor> buffers,
                                std::vector<MetaData> meta_buffers) {
      VideoTrackEncDataCb(s1_id, track_id, buffers, meta_buffers); };

  uint32_t video_track1_id = 1;
  ret = recorder_.CreateVideoTrack(s1_id, video_track1_id,
                                   video_track1, video_track1_cb);
  ASSERT_TRUE(ret == NO_ERROR);

  std::vector<uint32_t> s1_track_ids;
  s1_track_ids.push_back(video_track1_id);
  sessions_.insert(std::make_pair(s1_id, s1_track_ids));

  // Start Session1
  ret = recorder_.StartSession(s1_id);
  ASSERT_TRUE(ret == NO_ERROR);
  sleep(5);

  // Camera Params
  CameraMetadata meta;
  ret = recorder_.GetCameraParam(camera_id_, meta);
  ASSERT_TRUE(NO_ERROR == ret);

  // SHDR
  if (is_shdr_supported) {
    if (shdr1) {
      fprintf(stderr, "\nSetting shdr1 ON..\n");
      const int32_t vhdrMode =  QCAMERA3_VIDEO_HDR_MODE_ON;
      meta.update( QCAMERA3_VIDEO_HDR_MODE, &vhdrMode, 1);
    } else {
      fprintf(stderr, "\nSetting shdr1 OFF..\n");
      const int32_t vhdrMode =  QCAMERA3_VIDEO_HDR_MODE_OFF;
      meta.update( QCAMERA3_VIDEO_HDR_MODE, &vhdrMode, 1);
    }
  }

  // TNR
  if (is_tnr_supported) {
    if (tnr1) {
      fprintf(stderr, "\nSetting TNR1 ON..\n");
      const uint8_t tnrMode = ANDROID_NOISE_REDUCTION_MODE_HIGH_QUALITY;
      meta.update(ANDROID_NOISE_REDUCTION_MODE, &tnrMode, 1);
    } else {
      fprintf(stderr, "\nSetting TNR1 OFF..\n");
      const uint8_t tnrMode = ANDROID_NOISE_REDUCTION_MODE_OFF;
      meta.update(ANDROID_NOISE_REDUCTION_MODE, &tnrMode, 1);
    }
  }

  ret = recorder_.SetCameraParam(camera_id_, meta);
  ASSERT_TRUE(NO_ERROR == ret);

  sleep(15);

  if (!is_s2_create) {
    ret = recorder_.StopSession(s1_id, false);
    ASSERT_TRUE(ret == NO_ERROR);
  }

  if (is_t1_delete) {
    ret = recorder_.DeleteVideoTrack(s1_id, video_track1_id);
    ASSERT_TRUE(ret == NO_ERROR);
  }

  // Create 1080p AVC Stream
  VideoTrackCreateParam video_track2{camera_id_, VideoFormat::kAVC,
                                     w2,
                                     h2,
                                     fps2};
  video_track2.low_power_mode = false;

  TrackCb video_track2_cb;
  uint32_t video_track2_id = 2;
  video_track2_cb.event_cb = [&] (uint32_t track_id, EventType event_type,
      void *event_data, size_t event_data_size) {
          VideoTrackEventCb(track_id, event_type, event_data, event_data_size);
      };

  // Create Session2
  uint32_t s2_id;
  if (is_s2_create) {
    SessionCb s2_status_cb = CreateSessionStatusCb();
    ret = recorder_.CreateSession(s2_status_cb, &s2_id);
    ASSERT_TRUE(s2_id > 0);
    ASSERT_TRUE(ret == NO_ERROR);

    video_track2_cb.data_cb = [&, s2_id] (uint32_t track_id,
        std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
      VideoTrackEncDataCb(s2_id, track_id, buffers, meta_buffers); };

    ret = recorder_.CreateVideoTrack(s2_id, video_track2_id,
                                     video_track2, video_track2_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    std::vector<uint32_t> s2_track_ids;
    s2_track_ids.push_back(video_track2_id);
    sessions_.insert(std::make_pair(s2_id, s2_track_ids));

    ret = recorder_.StartSession(s2_id);
    ASSERT_TRUE(ret == NO_ERROR);

  } else {

    video_track2_cb.data_cb = [&, s1_id] (uint32_t track_id,
      std::vector<BufferDescriptor> buffers,
      std::vector<MetaData> meta_buffers) {
          VideoTrackEncDataCb(s1_id, track_id, buffers, meta_buffers);
      };

    // Add T2 to the existing Session and restart Session
    ret = recorder_.CreateVideoTrack(s1_id, video_track2_id,
                                     video_track2, video_track2_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    ClearSessions();
    std::vector<uint32_t> tracks;
    if (!is_t1_delete) {
      tracks.push_back(video_track1_id);
    }
    tracks.push_back(video_track2_id);
    sessions_.insert(std::make_pair(s1_id, tracks));

    ret = recorder_.StartSession(s1_id);
    ASSERT_TRUE(ret == NO_ERROR);
  }

  // Camera Params
  ret = recorder_.GetCameraParam(camera_id_, meta);
  ASSERT_TRUE(NO_ERROR == ret);

  // SHDR
  if (is_shdr_supported) {
    if (shdr2) {
      fprintf(stderr, "\nSetting shdr2 ON..\n");
      const int32_t vhdrMode =  QCAMERA3_VIDEO_HDR_MODE_ON;
      meta.update( QCAMERA3_VIDEO_HDR_MODE, &vhdrMode, 1);
    } else {
      fprintf(stderr, "\nSetting shdr2 OFF..\n");
      const int32_t vhdrMode =  QCAMERA3_VIDEO_HDR_MODE_OFF;
      meta.update( QCAMERA3_VIDEO_HDR_MODE, &vhdrMode, 1);
    }
  }

  // TNR
  if (is_tnr_supported) {
    if (tnr2) {
      fprintf(stderr, "\nSetting TNR2 ON..\n");
      const uint8_t tnrMode = ANDROID_NOISE_REDUCTION_MODE_HIGH_QUALITY;
      meta.update(ANDROID_NOISE_REDUCTION_MODE, &tnrMode, 1);
    } else {
      fprintf(stderr, "\nSetting TNR2 OFF..\n");
      const uint8_t tnrMode = ANDROID_NOISE_REDUCTION_MODE_OFF;
      meta.update(ANDROID_NOISE_REDUCTION_MODE, &tnrMode, 1);
    }
  }

  ret = recorder_.SetCameraParam(camera_id_, meta);
  ASSERT_TRUE(NO_ERROR == ret);
  sleep(15);

  // Stop Session-2
  if (is_s2_create) {
    ret = recorder_.StopSession(s2_id, false);
    ASSERT_TRUE(ret == NO_ERROR);
    ret = recorder_.DeleteVideoTrack(s2_id, video_track2_id);
    ASSERT_TRUE(ret == NO_ERROR);
    ret = recorder_.DeleteSession(s2_id);
    ASSERT_TRUE(ret == NO_ERROR);
  }

  // Stop Session-1
  ret = recorder_.StopSession(s1_id, false);
  ASSERT_TRUE(ret == NO_ERROR);

  if (!is_t1_delete) {
    ret = recorder_.DeleteVideoTrack(s1_id, video_track1_id);
    ASSERT_TRUE(ret == NO_ERROR);
  }

  if (!is_s2_create) {
    ret = recorder_.DeleteVideoTrack(s1_id, video_track2_id);
    ASSERT_TRUE(ret == NO_ERROR);
  }

  ret = recorder_.DeleteSession(s1_id);
  ASSERT_TRUE(ret == NO_ERROR);

  ClearSessions();

  // Deinit and Stop
  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
          test_info_->test_case_name(), test_info_->name());
}

/*
* SessionWith1440EncWithEISAndLCACEnable: This test will test session with 1440p
*                                         30fps h264 track and EIS and LCAC is
*                                         enable.
* API test sequence:
*  - StartCamera
*  - CreateSession
*  - CreateVideoTrack
*  - Enable EIS
*  - Start Session
*  - Enable LCAC
*  - StopSession
*  - DeleteVideoTrack
*  - DeleteSession
*  - StopCamera
*/
TEST_F(RecorderPostprocessVideoGTest, SessionWith1440EncWithEISAndLCACEnable) {
  fprintf(stderr, "\n---------- Run Test %s.%s ------------\n",
          test_info_->test_case_name(), test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  VideoFormat format_type = VideoFormat::kAVC;
  uint32_t width = 1920;
  uint32_t height = 1440;
  float fps = 30;

  camera_start_params_.frame_rate = fps;
  ret = recorder_.StartCamera(camera_id_, camera_start_params_);
  ASSERT_TRUE(ret == NO_ERROR);
  for (uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    SessionCb session_status_cb = CreateSessionStatusCb();
    uint32_t session_id;
    ret = recorder_.CreateSession(session_status_cb, &session_id);
    ASSERT_TRUE(session_id > 0);
    ASSERT_TRUE(ret == NO_ERROR);
    VideoTrackCreateParam video_track_param{camera_id_, format_type, width,
                                            height, fps};
    uint32_t video_track_id = 1;

    if (dump_bitstream_.IsEnabled()) {
      StreamDumpInfo dumpinfo = { format_type, video_track_id, session_id,
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

    video_track_cb.event_cb = [this](uint32_t track_id, EventType event_type,
                                     void *event_data,
                                     size_t event_data_size) -> void {
      VideoTrackEventCb(track_id, event_type, event_data, event_data_size);
    };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id,
                                     video_track_param, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    std::vector<uint32_t> track_ids;
    track_ids.push_back(video_track_id);
    sessions_.insert(std::make_pair(session_id, track_ids));
    // Enable EIS before Start Session
    CameraMetadata meta;
    ret = recorder_.GetCameraParam(camera_id_, meta);
    ASSERT_TRUE(ret == NO_ERROR);

    uint8_t vstab_mode = ANDROID_CONTROL_VIDEO_STABILIZATION_MODE_ON;
    meta.update(ANDROID_CONTROL_VIDEO_STABILIZATION_MODE, &vstab_mode, 1);
    ret = recorder_.SetCameraParam(camera_id_, meta);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);
    sleep(record_duration_ / 2);

    ret = recorder_.GetCameraParam(camera_id_, meta);

    if (NO_ERROR == ret) {
      uint8_t enable_lcac = 1;
      ret = meta.update( QCAMERA3_LCAC_PROCESSING_ENABLE, &enable_lcac, 1);
      ASSERT_TRUE(ret == NO_ERROR);
      ret = recorder_.SetCameraParam(camera_id_, meta);
      ASSERT_TRUE(ret == NO_ERROR);
    }

    sleep(record_duration_ / 2);

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

  fprintf(stderr, "---------- Test Completed %s.%s ----------\n",
          test_info_->test_case_name(), test_info_->name());
}

#ifndef DISABLE_DISPLAY
/*
* SessionWith1440pEnc480pEnc480pDisplayTrack4FPSVideoTimeLapse:
*    This test will create three concurrent sessions 1440p Enc track for storage,
*    480p Enc track for streaming, 480p YUV track for display with  YUV LCAC And
*    TNR is enabled
* API test sequence:
*  - StartCamera
*  - CreateSession1
*  - CreateVideoTrack
*  - Enable LCAC
*  - Start Session1
*  - CreateSession2
*  - CreateVideoTrack
*  - Start Session2
*  - CreateSession3
*  - CreateVideoTrack
*  - Start Session3
*  - StopSessions
*  - DeleteVideoTracks
*  - DeleteSessions
*  - StopCamera
*/
TEST_F(RecorderPostprocessVideoGTest, SessionWith1440pEnc480pEnc480pDisplayTrack4FPSVideoTimeLapse) {
  fprintf(stderr, "\n---------- Run Test %s.%s ------------\n",
          test_info_->test_case_name(), test_info_->name());

  float stream_fps = 4;
  uint32_t width = 1920;
  uint32_t height = 1440;
  VideoExtraParam extra_param;
  VideoTimeLapse timelapse;
  VideoRotate rotate_param;
  CameraMetadata meta;
  TrackCb video_track_cb;
  uint8_t enable_lcac;

  uint32_t session_id1;
  uint32_t session1_video_trackid = 1;
  std::vector<uint32_t> session1_track_ids;

  uint32_t session_id2;
  uint32_t session2_video_trackid = 2;
  std::vector<uint32_t> session2_track_ids;

  uint32_t session_id3;
  uint32_t session3_yuv_trackid = 3;
  std::vector<uint32_t> session3_track_ids;

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  camera_start_params_.frame_rate = stream_fps;
  ret = recorder_.StartCamera(camera_id_, camera_start_params_);
  ASSERT_TRUE(ret == NO_ERROR);

  SessionCb session_status_cb = CreateSessionStatusCb();
  for (uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr, "test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
        test_info_->name(), i);

    fprintf(stderr, "Time Lapse LandScape Mode \n");
    // Landscape Mode
    // Session 1 Track: 1920x1440p @4 AVC
    ret = recorder_.CreateSession(session_status_cb, &session_id1);
    ASSERT_TRUE(session_id1 > 0);
    ASSERT_TRUE(ret == NO_ERROR);

    width = 1920;
    height = 1440;

    if (dump_bitstream_.IsEnabled()) {
      StreamDumpInfo dumpinfo = { VideoFormat::kAVC, session1_video_trackid,
                                  session_id1, width, height };
      ret = dump_bitstream_.SetUp(dumpinfo);
      ASSERT_TRUE(ret == NO_ERROR);
    }

    video_track_cb.data_cb = [&, session_id1] (uint32_t track_id,
      std::vector<BufferDescriptor> buffers,
      std::vector<MetaData> meta_buffers) {
        VideoTrackEncDataCb(session_id1, track_id, buffers, meta_buffers);
      };

    video_track_cb.event_cb = [&] (uint32_t track_id, EventType event_type,
        void *event_data, size_t event_data_size) { VideoTrackEventCb(track_id,
        event_type, event_data, event_data_size); };

    VideoTrackCreateParam video_track_param{camera_id_, VideoFormat::kAVC, width,
                                            height, stream_fps};

    video_track_param.codec_param.avc.bitrate = 18000000;

    timelapse.time_interval = 33; //ms
    extra_param.Update(QMMF_VIDEO_TIMELAPSE_INTERVAL, timelapse);

    ret = recorder_.CreateVideoTrack(session_id1, session1_video_trackid,
                                     video_track_param, extra_param,
                                     video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    // Enable YUV LCAC
    auto status = recorder_.GetCameraParam(camera_id_, meta);
    if (NO_ERROR == status) {
      enable_lcac = 1;
      ret = meta.update( QCAMERA3_LCAC_PROCESSING_ENABLE, &enable_lcac, 1);
      ASSERT_TRUE(ret == NO_ERROR);
      ret = recorder_.SetCameraParam(camera_id_, meta);
      ASSERT_TRUE(ret == NO_ERROR);
    }

    // Store Session 1 tracks
    session1_track_ids.push_back(session1_video_trackid);
    sessions_.insert(std::make_pair(session_id1, session1_track_ids));

    ret = recorder_.StartSession(session_id1);
    ASSERT_TRUE(ret == NO_ERROR);

    // Session 2 Track: 640x480p @4 AVC
    ret = recorder_.CreateSession(session_status_cb, &session_id2);
    ASSERT_TRUE(session_id2 > 0);
    ASSERT_TRUE(ret == NO_ERROR);
    width = 640;
    height = 480;
    video_track_param.codec_param.avc.bitrate = 1000000;
    video_track_param.camera_id = camera_id_;
    video_track_param.width = width;
    video_track_param.height = height;
    video_track_param.frame_rate = stream_fps;
    video_track_param.format_type = VideoFormat::kAVC;

    if (dump_bitstream_.IsEnabled()) {
      StreamDumpInfo dumpinfo = { VideoFormat::kAVC, session2_video_trackid,
                                  session_id2, width, height };
      ret = dump_bitstream_.SetUp(dumpinfo);
      ASSERT_TRUE(ret == NO_ERROR);
    }

    video_track_cb.data_cb = [&, session_id2](uint32_t track_id,
                                 std::vector<BufferDescriptor> buffers,
                                 std::vector<MetaData> meta_buffers) {
      VideoTrackEncDataCb(session_id2, track_id, buffers, meta_buffers);
    };

    timelapse.time_interval = 33; //ms
    extra_param.Update(QMMF_VIDEO_TIMELAPSE_INTERVAL, timelapse);

    ret = recorder_.CreateVideoTrack(session_id2, session2_video_trackid,
                                     video_track_param, extra_param,
                                     video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    // Store Session 2 tracks
    session2_track_ids.push_back(session2_video_trackid);
    sessions_.insert(std::make_pair(session_id2, session1_track_ids));

    ret = recorder_.StartSession(session_id2);
    ASSERT_TRUE(ret == NO_ERROR);

    // Session 3 Track: 640x480p @4 YUV
    ret = recorder_.CreateSession(session_status_cb, &session_id3);
    ASSERT_TRUE(session_id3 > 0);
    ASSERT_TRUE(ret == NO_ERROR);

    width = 640;
    height = 480;
    video_track_param.camera_id = camera_id_;
    video_track_param.width = width;
    video_track_param.height = height;
    video_track_param.frame_rate = stream_fps;
    video_track_param.format_type = VideoFormat::kYUV;

    video_track_cb.data_cb = [&, session_id3](uint32_t track_id,
                                 std::vector<BufferDescriptor> buffers,
                                 std::vector<MetaData> meta_buffers) {
      VideoTrackYUVDataCb(session_id3, track_id, buffers, meta_buffers); };

    ret = recorder_.CreateVideoTrack(session_id3, session3_yuv_trackid,
                                     video_track_param, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    // Store Session 3 tracks
    session3_track_ids.push_back(session3_yuv_trackid);
    sessions_.insert(std::make_pair(session_id3, session3_track_ids));

    use_display_ = true;

    if (use_display_) {
      ret = StartDisplay(DisplayType::kPrimary, width, height, height, width/2);
      if (ret != 0) {
        TEST_ERROR("%s: StartDisplay Failed!!", __func__);
      }
    }

    ret = recorder_.StartSession(session_id3);
    ASSERT_TRUE(ret == NO_ERROR);

    // Let session run for record_duration_, during this time buffer with valid
    // data would be received in track callback (VideoTrackDataCb).
    sleep(record_duration_);

    // Stop and delete Session 1
    ret = recorder_.StopSession(session_id1, false);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id1, session1_video_trackid);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteSession(session_id1);
    ASSERT_TRUE(ret == NO_ERROR);

    // Stop and delete Session 2
    ret = recorder_.StopSession(session_id2, false);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id2, session2_video_trackid);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteSession(session_id2);
    ASSERT_TRUE(ret == NO_ERROR);

    // Stop and delete Session 3
    ret = recorder_.StopSession(session_id3, false);
    ASSERT_TRUE(ret == NO_ERROR);

    if (use_display_) {
      ret = StopDisplay(DisplayType::kPrimary);
      if (ret != 0) {
        TEST_ERROR("%s: StopDisplay Failed!!", __func__);
      }
    }

    ret = recorder_.DeleteVideoTrack(session_id3, session3_yuv_trackid);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteSession(session_id3);
    ASSERT_TRUE(ret == NO_ERROR);

    ClearSessions();

    fprintf(stderr, "Time Lapse Portrait Mode \n");
    // Portrait Mode
    // Session 1 Track: 1440x1920p @4 AVC
    ret = recorder_.CreateSession(session_status_cb, &session_id1);
    ASSERT_TRUE(session_id1 > 0);
    ASSERT_TRUE(ret == NO_ERROR);

    video_track_cb.data_cb = [&, session_id1] (uint32_t track_id,
      std::vector<BufferDescriptor> buffers,
      std::vector<MetaData> meta_buffers) {
        VideoTrackEncDataCb(session_id1, track_id, buffers, meta_buffers);
      };

    video_track_cb.event_cb = [&] (uint32_t track_id, EventType event_type,
        void *event_data, size_t event_data_size) { VideoTrackEventCb(track_id,
        event_type, event_data, event_data_size); };

    video_track_param.camera_id = camera_id_;
    video_track_param.width = 1440;
    video_track_param.height = 1920;
    video_track_param.frame_rate = stream_fps;
    video_track_param.format_type = VideoFormat::kAVC;
    video_track_param.codec_param.avc.bitrate = 18000000;

    timelapse.time_interval = 33; //ms
    extra_param.Update(QMMF_VIDEO_TIMELAPSE_INTERVAL, timelapse);

    rotate_param.flags = RotationFlags::kRotate90; //90 Degree
    extra_param.Update(QMMF_VIDEO_ROTATE, rotate_param);

    ret = recorder_.CreateVideoTrack(session_id1, session1_video_trackid,
                                     video_track_param, extra_param,
                                     video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    // Enable YUV LCAC
    status = recorder_.GetCameraParam(camera_id_, meta);
    if (NO_ERROR == status) {
      enable_lcac = 1;
      ret = meta.update( QCAMERA3_LCAC_PROCESSING_ENABLE, &enable_lcac, 1);
      ASSERT_TRUE(ret == NO_ERROR);
      ret = recorder_.SetCameraParam(camera_id_, meta);
      ASSERT_TRUE(ret == NO_ERROR);
    }
    // Store Session 1 tracks
    session1_track_ids.push_back(session1_video_trackid);
    sessions_.insert(std::make_pair(session_id1, session1_track_ids));

    ret = recorder_.StartSession(session_id1);
    ASSERT_TRUE(ret == NO_ERROR);

    // Session 2 Track: 480x640p @4 AVC
    ret = recorder_.CreateSession(session_status_cb, &session_id2);
    ASSERT_TRUE(session_id2 > 0);
    ASSERT_TRUE(ret == NO_ERROR);
    width = 480;
    height = 640;
    video_track_param.codec_param.avc.bitrate = 1000000;
    video_track_param.camera_id = camera_id_;
    video_track_param.width = width;
    video_track_param.height = height;
    video_track_param.frame_rate = stream_fps;
    video_track_param.format_type = VideoFormat::kAVC;

    video_track_cb.data_cb = [&, session_id2](uint32_t track_id,
                                 std::vector<BufferDescriptor> buffers,
                                 std::vector<MetaData> meta_buffers) {
      VideoTrackEncDataCb(session_id2, track_id, buffers, meta_buffers);
    };

    timelapse.time_interval = 33; //ms
    extra_param.Update(QMMF_VIDEO_TIMELAPSE_INTERVAL, timelapse);

    rotate_param.flags = RotationFlags::kRotate90; //90 Degree
    extra_param.Update(QMMF_VIDEO_ROTATE, rotate_param);

    ret = recorder_.CreateVideoTrack(session_id2, session2_video_trackid,
                                     video_track_param, extra_param,
                                     video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    // Store Session 2 tracks
    session2_track_ids.push_back(session2_video_trackid);
    sessions_.insert(std::make_pair(session_id2, session1_track_ids));

    ret = recorder_.StartSession(session_id2);
    ASSERT_TRUE(ret == NO_ERROR);

    // Session 3 Track: 640x480 @4 YUV
    ret = recorder_.CreateSession(session_status_cb, &session_id3);
    ASSERT_TRUE(session_id3 > 0);
    ASSERT_TRUE(ret == NO_ERROR);

    width = 640;
    height = 480;
    video_track_param.camera_id = camera_id_;
    video_track_param.width = width;
    video_track_param.height = height;
    video_track_param.frame_rate = stream_fps;
    video_track_param.format_type = VideoFormat::kYUV;

    video_track_cb.data_cb = [&, session_id3](uint32_t track_id,
                                 std::vector<BufferDescriptor> buffers,
                                 std::vector<MetaData> meta_buffers) {
      VideoTrackYUVDataCb(session_id3, track_id, buffers, meta_buffers); };

    ret = recorder_.CreateVideoTrack(session_id3, session3_yuv_trackid,
                                     video_track_param, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    // Store Session 3 tracks
    session3_track_ids.push_back(session3_yuv_trackid);
    sessions_.insert(std::make_pair(session_id3, session3_track_ids));

    use_display_ = true;

    if (use_display_) {
      ret = StartDisplay(DisplayType::kPrimary, width, height, width/2, height);
      if (ret != 0) {
        TEST_ERROR("%s: StartDisplay Failed!!", __func__);
      }
    }

    ret = recorder_.StartSession(session_id3);
    ASSERT_TRUE(ret == NO_ERROR);

    // Let session run for record_duration_, during this time buffer with valid
    // data would be received in track callback (VideoTrackDataCb).
    sleep(record_duration_);

    // Stop and delete Session 1
    ret = recorder_.StopSession(session_id1, false);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id1, session1_video_trackid);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteSession(session_id1);
    ASSERT_TRUE(ret == NO_ERROR);

    // Stop and delete Session 2
    ret = recorder_.StopSession(session_id2, false);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id2, session2_video_trackid);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteSession(session_id2);
    ASSERT_TRUE(ret == NO_ERROR);

    // Stop and delete Session 3
    ret = recorder_.StopSession(session_id3, false);
    ASSERT_TRUE(ret == NO_ERROR);

    if (use_display_) {
      ret = StopDisplay(DisplayType::kPrimary);
      if (ret != 0) {
        TEST_ERROR("%s: StopDisplay Failed!!", __func__);
      }
    }

    ret = recorder_.DeleteVideoTrack(session_id3, session3_yuv_trackid);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteSession(session_id3);
    ASSERT_TRUE(ret == NO_ERROR);

    ClearSessions();

    // for next iteration rotation param shold be set to 0 degree
    // else stream configuration error will come.
    rotate_param.flags = RotationFlags::kNone; //0 Degree
    extra_param.Update(QMMF_VIDEO_ROTATE, rotate_param);

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
* SessionWith4kEnc480pEnc480pDisplayTrack4FPSVideoTimeLapse:
*    This test will create three concurrent sessions 4k Enc track for storage,
*    480p Enc track for streaming, 480p YUV track for display with  YUV LCAC
*    and TNR.here camera starting at 4fps then skipping frame to get 4FPS.
* API test sequence:
*  - StartCamera
*  - CreateSession1
*  - CreateVideoTrack
*  - Enable LCAC
*  - Start Session1
*  - CreateSession2
*  - CreateVideoTrack
*  - Start Session2
*  - CreateSession3
*  - CreateVideoTrack
*  - Start Session3
*  - StopSessions
*  - DeleteVideoTracks
*  - DeleteSessions
*  - StopCamera
*/
TEST_F(RecorderPostprocessVideoGTest, SessionWith4kEnc480pEnc480pDisplayTrack4FPSVideoTimeLapse) {
  fprintf(stderr, "\n---------- Run Test %s.%s ------------\n",
          test_info_->test_case_name(), test_info_->name());

  float stream_fps = 4;
  uint32_t width = 3840;
  uint32_t height = 2160;

  VideoExtraParam extra_param;
  VideoTimeLapse timelapse;
  VideoRotate rotate_param;
  CameraMetadata meta;
  TrackCb video_track_cb;
  uint8_t enable_lcac;

  uint32_t session_id1;
  uint32_t session1_video_trackid = 1;
  std::vector<uint32_t> session1_track_ids;

  uint32_t session_id2;
  uint32_t session2_video_trackid = 2;
  std::vector<uint32_t> session2_track_ids;

  uint32_t session_id3;
  uint32_t session3_yuv_trackid = 3;
  std::vector<uint32_t> session3_track_ids;

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  camera_start_params_.frame_rate = stream_fps;
  ret = recorder_.StartCamera(camera_id_, camera_start_params_);
  ASSERT_TRUE(ret == NO_ERROR);

  SessionCb session_status_cb = CreateSessionStatusCb();
  for (uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr, "test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
        test_info_->name(), i);

    fprintf(stderr, "Time Lapse LandScape Mode \n");
    // Landscape Mode
    // Session 1 Track: 3840X2160 @4 AVC
    ret = recorder_.CreateSession(session_status_cb, &session_id1);
    ASSERT_TRUE(session_id1 > 0);
    ASSERT_TRUE(ret == NO_ERROR);

    width = 3840;
    height = 2160;

    if (dump_bitstream_.IsEnabled()) {
      StreamDumpInfo dumpinfo = { VideoFormat::kAVC, session1_video_trackid,
                                  session_id1, width, height };
      ret = dump_bitstream_.SetUp(dumpinfo);
      ASSERT_TRUE(ret == NO_ERROR);
    }

    video_track_cb.data_cb = [&, session_id1] (uint32_t track_id,
      std::vector<BufferDescriptor> buffers,
      std::vector<MetaData> meta_buffers) {
        VideoTrackEncDataCb(session_id1, track_id, buffers, meta_buffers);
      };

    video_track_cb.event_cb = [&] (uint32_t track_id, EventType event_type,
        void *event_data, size_t event_data_size) { VideoTrackEventCb(track_id,
        event_type, event_data, event_data_size); };

    VideoTrackCreateParam video_track_param{camera_id_, VideoFormat::kAVC, width,
                                            height, stream_fps};

    video_track_param.codec_param.avc.bitrate = 18000000;

    timelapse.time_interval = 33; //ms
    extra_param.Update(QMMF_VIDEO_TIMELAPSE_INTERVAL, timelapse);

    ret = recorder_.CreateVideoTrack(session_id1, session1_video_trackid,
                                     video_track_param, extra_param,
                                     video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    // Enable YUV LCAC
    auto status = recorder_.GetCameraParam(camera_id_, meta);
    if (NO_ERROR == status) {
      enable_lcac = 1;
      ret = meta.update( QCAMERA3_LCAC_PROCESSING_ENABLE, &enable_lcac, 1);
      ASSERT_TRUE(ret == NO_ERROR);
      ret = recorder_.SetCameraParam(camera_id_, meta);
      ASSERT_TRUE(ret == NO_ERROR);
    }
    // Store Session 1 tracks
    session1_track_ids.push_back(session1_video_trackid);
    sessions_.insert(std::make_pair(session_id1, session1_track_ids));

    ret = recorder_.StartSession(session_id1);
    ASSERT_TRUE(ret == NO_ERROR);


    // Session 2 Track: 640x480p @4 AVC
    ret = recorder_.CreateSession(session_status_cb, &session_id2);
    ASSERT_TRUE(session_id2 > 0);
    ASSERT_TRUE(ret == NO_ERROR);
    width = 640;
    height = 480;
    video_track_param.codec_param.avc.bitrate = 1000000;
    video_track_param.camera_id = camera_id_;
    video_track_param.width = width;
    video_track_param.height = height;
    video_track_param.frame_rate = stream_fps;
    video_track_param.format_type = VideoFormat::kAVC;

    if (dump_bitstream_.IsEnabled()) {
      StreamDumpInfo dumpinfo = { VideoFormat::kAVC, session2_video_trackid,
                                  session_id2, width, height };
      ret = dump_bitstream_.SetUp(dumpinfo);
      ASSERT_TRUE(ret == NO_ERROR);
    }

    video_track_cb.data_cb = [&, session_id2](uint32_t track_id,
                                 std::vector<BufferDescriptor> buffers,
                                 std::vector<MetaData> meta_buffers) {
      VideoTrackEncDataCb(session_id2, track_id, buffers, meta_buffers);
    };

    timelapse.time_interval = 33; //ms
    extra_param.Update(QMMF_VIDEO_TIMELAPSE_INTERVAL, timelapse);

    ret = recorder_.CreateVideoTrack(session_id2, session2_video_trackid,
                                     video_track_param, extra_param,
                                     video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    // Store Session 2 tracks
    session2_track_ids.push_back(session2_video_trackid);
    sessions_.insert(std::make_pair(session_id2, session1_track_ids));

    ret = recorder_.StartSession(session_id2);
    ASSERT_TRUE(ret == NO_ERROR);


    // Session 3 Track: 640x480p @4 YUV
    ret = recorder_.CreateSession(session_status_cb, &session_id3);
    ASSERT_TRUE(session_id3 > 0);
    ASSERT_TRUE(ret == NO_ERROR);

    width = 640;
    height = 480;
    video_track_param.camera_id = camera_id_;
    video_track_param.width = width;
    video_track_param.height = height;
    video_track_param.frame_rate = stream_fps;
    video_track_param.format_type = VideoFormat::kYUV;

    video_track_cb.data_cb = [&, session_id3](uint32_t track_id,
                                 std::vector<BufferDescriptor> buffers,
                                 std::vector<MetaData> meta_buffers) {
      VideoTrackYUVDataCb(session_id3, track_id, buffers, meta_buffers); };

    ret = recorder_.CreateVideoTrack(session_id3, session3_yuv_trackid,
                                     video_track_param, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    // Store Session 3 tracks
    session3_track_ids.push_back(session3_yuv_trackid);
    sessions_.insert(std::make_pair(session_id3, session3_track_ids));

    use_display_ = true;

    if (use_display_) {
      ret = StartDisplay(DisplayType::kPrimary, width, height, height, width/2);
      if (ret != 0) {
        TEST_ERROR("%s: StartDisplay Failed!!", __func__);
      }
    }

    ret = recorder_.StartSession(session_id3);
    ASSERT_TRUE(ret == NO_ERROR);


    // Let session run for record_duration_, during this time buffer with valid
    // data would be received in track callback (VideoTrackDataCb).
    sleep(record_duration_);

    // Stop and delete Session 1
    ret = recorder_.StopSession(session_id1, false);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id1, session1_video_trackid);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteSession(session_id1);
    ASSERT_TRUE(ret == NO_ERROR);

    // Stop and delete Session 2
    ret = recorder_.StopSession(session_id2, false);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id2, session2_video_trackid);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteSession(session_id2);
    ASSERT_TRUE(ret == NO_ERROR);

    // Stop and delete Session 3
    ret = recorder_.StopSession(session_id3, false);
    ASSERT_TRUE(ret == NO_ERROR);

    if (use_display_) {
      ret = StopDisplay(DisplayType::kPrimary);
      if (ret != 0) {
        TEST_ERROR("%s: StopDisplay Failed!!", __func__);
      }
    }

    ret = recorder_.DeleteVideoTrack(session_id3, session3_yuv_trackid);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteSession(session_id3);
    ASSERT_TRUE(ret == NO_ERROR);

    ClearSessions();

    fprintf(stderr, "Time Lapse Portrait Mode \n");
    // Portrait Mode
    // Session 1 Track: 2160X3840p @4 AVC
    ret = recorder_.CreateSession(session_status_cb, &session_id1);
    ASSERT_TRUE(session_id1 > 0);
    ASSERT_TRUE(ret == NO_ERROR);

    video_track_cb.data_cb = [&, session_id1] (uint32_t track_id,
      std::vector<BufferDescriptor> buffers,
      std::vector<MetaData> meta_buffers) {
        VideoTrackEncDataCb(session_id1, track_id, buffers, meta_buffers);
      };

    video_track_cb.event_cb = [&] (uint32_t track_id, EventType event_type,
        void *event_data, size_t event_data_size) { VideoTrackEventCb(track_id,
        event_type, event_data, event_data_size); };

    width = 2160;
    height = 3840;
    video_track_param.camera_id = camera_id_;
    video_track_param.width = width;
    video_track_param.height = height;
    video_track_param.frame_rate = stream_fps;
    video_track_param.format_type = VideoFormat::kAVC;
    video_track_param.codec_param.avc.bitrate = 18000000;

    timelapse.time_interval = 33; //ms
    extra_param.Update(QMMF_VIDEO_TIMELAPSE_INTERVAL, timelapse);

    rotate_param.flags = RotationFlags::kRotate90; //90 Degree
    extra_param.Update(QMMF_VIDEO_ROTATE, rotate_param);

    ret = recorder_.CreateVideoTrack(session_id1, session1_video_trackid,
                                     video_track_param, extra_param,
                                     video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    // Enable YUV LCAC
    status = recorder_.GetCameraParam(camera_id_, meta);
    if (NO_ERROR == status) {
      enable_lcac = 1;
      ret = meta.update( QCAMERA3_LCAC_PROCESSING_ENABLE, &enable_lcac, 1);
      ASSERT_TRUE(ret == NO_ERROR);
      ret = recorder_.SetCameraParam(camera_id_, meta);
      ASSERT_TRUE(ret == NO_ERROR);
    }
    // Store Session 1 tracks
    session1_track_ids.push_back(session1_video_trackid);
    sessions_.insert(std::make_pair(session_id1, session1_track_ids));

    ret = recorder_.StartSession(session_id1);
    ASSERT_TRUE(ret == NO_ERROR);

    // Session 2 Track: 480x640p @4 AVC
    ret = recorder_.CreateSession(session_status_cb, &session_id2);
    ASSERT_TRUE(session_id2 > 0);
    ASSERT_TRUE(ret == NO_ERROR);
    width = 480;
    height = 640;
    video_track_param.codec_param.avc.bitrate = 1000000;
    video_track_param.camera_id = camera_id_;
    video_track_param.width = width;
    video_track_param.height = height;
    video_track_param.frame_rate = stream_fps;
    video_track_param.format_type = VideoFormat::kAVC;

    video_track_cb.data_cb = [&, session_id2](uint32_t track_id,
                                 std::vector<BufferDescriptor> buffers,
                                 std::vector<MetaData> meta_buffers) {
      VideoTrackEncDataCb(session_id2, track_id, buffers, meta_buffers);
    };

    timelapse.time_interval = 33; //ms
    extra_param.Update(QMMF_VIDEO_TIMELAPSE_INTERVAL, timelapse);

    rotate_param.flags = RotationFlags::kRotate90; //90 Degree
    extra_param.Update(QMMF_VIDEO_ROTATE, rotate_param);

    ret = recorder_.CreateVideoTrack(session_id2, session2_video_trackid,
                                     video_track_param, extra_param,
                                     video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    // Store Session 2 tracks
    session2_track_ids.push_back(session2_video_trackid);
    sessions_.insert(std::make_pair(session_id2, session1_track_ids));

    ret = recorder_.StartSession(session_id2);
    ASSERT_TRUE(ret == NO_ERROR);

    // Session 3 Track: 640x480p @4 YUV
    ret = recorder_.CreateSession(session_status_cb, &session_id3);
    ASSERT_TRUE(session_id3 > 0);
    ASSERT_TRUE(ret == NO_ERROR);

    width = 480;
    height = 640;
    video_track_param.camera_id = camera_id_;
    video_track_param.width = width;
    video_track_param.height = height;
    video_track_param.frame_rate = stream_fps;
    video_track_param.format_type = VideoFormat::kYUV;

    video_track_cb.data_cb = [&, session_id3](uint32_t track_id,
                                 std::vector<BufferDescriptor> buffers,
                                 std::vector<MetaData> meta_buffers) {
      VideoTrackYUVDataCb(session_id3, track_id, buffers, meta_buffers); };

    ret = recorder_.CreateVideoTrack(session_id3, session3_yuv_trackid,
                                     video_track_param, extra_param,
                                     video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    // Store Session 3 tracks
    session3_track_ids.push_back(session3_yuv_trackid);
    sessions_.insert(std::make_pair(session_id3, session3_track_ids));

    use_display_ = true;

    if (use_display_) {
      ret = StartDisplay(DisplayType::kPrimary, width, height, 480, 320);
      if (ret != 0) {
        TEST_ERROR("%s: StartDisplay Failed!!", __func__);
      }
    }

    ret = recorder_.StartSession(session_id3);
    ASSERT_TRUE(ret == NO_ERROR);

    // Let session run for record_duration_, during this time buffer with valid
    // data would be received in track callback (VideoTrackDataCb).
    sleep(record_duration_);

    // Stop and delete Session 1
    ret = recorder_.StopSession(session_id1, false);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id1, session1_video_trackid);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteSession(session_id1);
    ASSERT_TRUE(ret == NO_ERROR);

    // Stop and delete Session 2
    ret = recorder_.StopSession(session_id2, false);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id2, session2_video_trackid);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteSession(session_id2);
    ASSERT_TRUE(ret == NO_ERROR);

    // Stop and delete Session 3
    ret = recorder_.StopSession(session_id3, false);
    ASSERT_TRUE(ret == NO_ERROR);

    if (use_display_) {
      ret = StopDisplay(DisplayType::kPrimary);
      if (ret != 0) {
        TEST_ERROR("%s: StopDisplay Failed!!", __func__);
      }
    }

    ret = recorder_.DeleteVideoTrack(session_id3, session3_yuv_trackid);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteSession(session_id3);
    ASSERT_TRUE(ret == NO_ERROR);

    ClearSessions();

    // for next iteration rotation param shold be set to 0 degree
    // else stream configuration error will come.
    rotate_param.flags = RotationFlags::kNone; //0 Degree
    extra_param.Update(QMMF_VIDEO_ROTATE, rotate_param);

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

/*
* SessionWith4kEncCopy480pEncAndLinked480pYUVLCAC: This test will test session
*     with one 4k30 Enc track, one copy 480p Enc Track and one 480p linked.
* Api test sequence:
*  - StartCamera
*  - CreateSession
*  - CreateVideoTrack - Master
*  - CreateVideoTrack - Copy
*  - CreateVideoTrack - Linked
*   loop Start {
*   -----------------
*   - StartSession
*   - StopSession
*   ------------------
*   } loop End
*  - DeleteVideoTrack - Linked
*  - DeleteVideoTrack - Copy
*  - DeleteVideoTrack - Master
*  - DeleteSession
*  - StopCamera
*/
TEST_F(RecorderPostprocessVideoGTest,
       SessionWith4kEncCopy480pEncAndLinked480pYUVLCAC) {
  fprintf(stderr, "\n---------- Run Test %s.%s ------------\n",
          test_info_->test_case_name(), test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.StartCamera(camera_id_, camera_start_params_);
  ASSERT_TRUE(ret == NO_ERROR);

  uint32_t video_track_id_4kp_avc = 1;
  uint32_t video_track_id_480p_avc = 2;
  uint32_t video_track_id_480p_yuv = 3;
  CameraMetadata meta;
  uint8_t enable_lcac;
  uint32_t session_id;
  SessionCb session_status_cb = CreateSessionStatusCb();
  ret = recorder_.CreateSession(session_status_cb, &session_id);
  ASSERT_TRUE(session_id > 0);
  ASSERT_TRUE(ret == NO_ERROR);

  uint32_t width = 3840;
  uint32_t height = 2160;
  if (dump_bitstream_.IsEnabled()) {
    StreamDumpInfo dumpinfo1 = { VideoFormat::kAVC, video_track_id_4kp_avc,
                                 session_id, width, height};
    ret = dump_bitstream_.SetUp(dumpinfo1);
    ASSERT_TRUE(ret == NO_ERROR);

    StreamDumpInfo dumpinfo2 = { VideoFormat::kAVC, video_track_id_480p_avc,
                                 session_id, 848, 480 };
    ret = dump_bitstream_.SetUp(dumpinfo2);
    ASSERT_TRUE(ret == NO_ERROR);
  }
  VideoTrackCreateParam video_track_param{camera_id_, VideoFormat::kAVC, width,
                                          height, 30};
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

  ret = recorder_.CreateVideoTrack(session_id, video_track_id_4kp_avc,
                                   video_track_param, video_track_cb);
  ASSERT_TRUE(ret == NO_ERROR);

  std::vector<uint32_t> track_ids;
  track_ids.push_back(video_track_id_4kp_avc);

  VideoExtraParam extra_param;
  SourceVideoTrack surface_video_copy;
  surface_video_copy.source_track_id = video_track_id_4kp_avc;
  extra_param.Update(QMMF_SOURCE_VIDEO_TRACK_ID, surface_video_copy);

  width = 848;
  height = 480;

  video_track_param.width = width;
  video_track_param.height = height;
  video_track_param.frame_rate = 30;

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

  // Enable YUV LCAC
  auto status = recorder_.GetCameraParam(camera_id_, meta);
  if (NO_ERROR == status) {
    enable_lcac = 1;
    ret = meta.update( QCAMERA3_LCAC_PROCESSING_ENABLE, &enable_lcac, 1);
    ASSERT_TRUE(ret == NO_ERROR);
    ret = recorder_.SetCameraParam(camera_id_, meta);
    ASSERT_TRUE(ret == NO_ERROR);

  }

  for (uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr, "test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
              test_info_->name(), i);
    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    // Let session run for time record_duration_, during this time buffer with
    // valid data would be received in track callback (VideoTrackYUVDataCb).
    sleep(record_duration_);

    ret = recorder_.StopSession(session_id, false);
    ASSERT_TRUE(ret == NO_ERROR);
  }

  ret = recorder_.DeleteVideoTrack(session_id, video_track_id_480p_yuv);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteVideoTrack(session_id, video_track_id_480p_avc);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteVideoTrack(session_id, video_track_id_4kp_avc);
  ASSERT_TRUE(ret == NO_ERROR);

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
* SessionWith4kEncCopy480pEncAndLinked480pEISYUVLCAC: This test will test session
*     with one 4k30 Enc track, one copy 480p Enc Track and one 480p linked with
*     EIS and YUV CAC.
* Api test sequence:
*  - StartCamera
*  - CreateSession
*  - CreateVideoTrack - Master
*  - CreateVideoTrack - Copy
*  - CreateVideoTrack - Linked
*   loop Start {
*   -----------------
*   - StartSession
*   - StopSession
*   ------------------
*   } loop End
*  - DeleteVideoTrack - Linked
*  - DeleteVideoTrack - Copy
*  - DeleteVideoTrack - Master
*  - DeleteSession
*  - StopCamera
*/
TEST_F(RecorderPostprocessVideoGTest,
       SessionWith4kEncCopy480pEncAndLinked480pEISYUVLCAC) {
  fprintf(stderr, "\n---------- Run Test %s.%s ------------\n",
          test_info_->test_case_name(), test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.StartCamera(camera_id_, camera_start_params_);
  ASSERT_TRUE(ret == NO_ERROR);

  uint32_t video_track_id_4kp_avc = 1;
  uint32_t video_track_id_480p_avc = 2;
  uint32_t video_track_id_480p_yuv = 3;
  CameraMetadata meta;
  uint8_t enable_lcac;
  uint8_t vstab_mode;

  uint32_t width = 3840;
  uint32_t height = 2160;

  SessionCb session_status_cb = CreateSessionStatusCb();
  uint32_t session_id;
  ret = recorder_.CreateSession(session_status_cb, &session_id);
  ASSERT_TRUE(session_id > 0);
  ASSERT_TRUE(ret == NO_ERROR);

  if (dump_bitstream_.IsEnabled()) {
    StreamDumpInfo dumpinfo1 = { VideoFormat::kAVC, video_track_id_4kp_avc,
                                 session_id, width, height};
    ret = dump_bitstream_.SetUp(dumpinfo1);
    ASSERT_TRUE(ret == NO_ERROR);

    StreamDumpInfo dumpinfo2 = { VideoFormat::kAVC, video_track_id_480p_avc,
                                  session_id, 848, 480};
    ret = dump_bitstream_.SetUp(dumpinfo2);
    ASSERT_TRUE(ret == NO_ERROR);
  }

  VideoTrackCreateParam video_track_param{camera_id_, VideoFormat::kAVC, width,
                                          height, 30};
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

  ret = recorder_.CreateVideoTrack(session_id, video_track_id_4kp_avc,
                                   video_track_param, video_track_cb);
  ASSERT_TRUE(ret == NO_ERROR);

  std::vector<uint32_t> track_ids;
  track_ids.push_back(video_track_id_4kp_avc);

  VideoExtraParam extra_param;
  SourceVideoTrack surface_video_copy;
  surface_video_copy.source_track_id = video_track_id_4kp_avc;
  extra_param.Update(QMMF_SOURCE_VIDEO_TRACK_ID, surface_video_copy);

  width  = 848;
  height = 480;

  video_track_param.width = width;
  video_track_param.height = height;
  video_track_param.frame_rate = 30;

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
    // Enable YUV LCAC
    enable_lcac = 1;
    ret = meta.update( QCAMERA3_LCAC_PROCESSING_ENABLE, &enable_lcac, 1);
    ASSERT_TRUE(ret == NO_ERROR);
    ret = recorder_.SetCameraParam(camera_id_, meta);
    ASSERT_TRUE(ret == NO_ERROR);

    // Enable EIS
    vstab_mode = 1;
    vstab_mode = ANDROID_CONTROL_VIDEO_STABILIZATION_MODE_ON;
    meta.update(ANDROID_CONTROL_VIDEO_STABILIZATION_MODE, &vstab_mode, 1);
    ret = recorder_.SetCameraParam(camera_id_, meta);
    ASSERT_TRUE(ret == NO_ERROR);
  }

  for (uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr, "test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
              test_info_->name(), i);
    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    // Let session run for time record_duration_, during this time buffer with
    // valid data would be received in track callback (VideoTrackYUVDataCb).
    sleep(record_duration_);

    ret = recorder_.StopSession(session_id, false);
    ASSERT_TRUE(ret == NO_ERROR);
  }

  ret = recorder_.DeleteVideoTrack(session_id, video_track_id_480p_yuv);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteVideoTrack(session_id, video_track_id_480p_avc);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteVideoTrack(session_id, video_track_id_4kp_avc);
  ASSERT_TRUE(ret == NO_ERROR);

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
* SessionWith4kEncCopy480pEncAndLinked480pEIS: This test will test session
*     with one 4k30 Enc track, one copy 480p Enc Track and one 480p linked with
*     EIS and YUV CAC.
* Api test sequence:
*  - StartCamera
*  - CreateSession
*  - CreateVideoTrack - Master
*  - CreateVideoTrack - Copy
*  - CreateVideoTrack - Linked
*   loop Start {
*   -----------------
*   - StartSession
*   - StopSession
*   ------------------
*   } loop End
*  - DeleteVideoTrack - Linked
*  - DeleteVideoTrack - Copy
*  - DeleteVideoTrack - Master
*  - DeleteSession
*  - StopCamera
*/
TEST_F(RecorderPostprocessVideoGTest,
       SessionWith4kEncCopy480pEncAndLinked480pEIS) {
  fprintf(stderr, "\n---------- Run Test %s.%s ------------\n",
          test_info_->test_case_name(), test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.StartCamera(camera_id_, camera_start_params_);
  ASSERT_TRUE(ret == NO_ERROR);

  uint32_t video_track_id_4kp_avc = 1;
  uint32_t video_track_id_480p_avc = 2;
  uint32_t video_track_id_480p_yuv = 3;
  CameraMetadata meta;
  uint8_t vstab_mode;

  SessionCb session_status_cb = CreateSessionStatusCb();
  uint32_t session_id;
  ret = recorder_.CreateSession(session_status_cb, &session_id);
  ASSERT_TRUE(session_id > 0);
  ASSERT_TRUE(ret == NO_ERROR);

  uint32_t width = 3840;
  uint32_t height = 2160;

  if (dump_bitstream_.IsEnabled()) {
    StreamDumpInfo dumpinfo1 = {VideoFormat::kAVC, video_track_id_4kp_avc,
                                session_id, width, height};
    ret = dump_bitstream_.SetUp(dumpinfo1);
    ASSERT_TRUE(ret == NO_ERROR);

    StreamDumpInfo dumpinfo2 = {VideoFormat::kAVC, video_track_id_480p_avc,
                                session_id, 848, 480};
    ret = dump_bitstream_.SetUp(dumpinfo2);
    ASSERT_TRUE(ret == NO_ERROR);
  }

  VideoTrackCreateParam video_track_param{camera_id_, VideoFormat::kAVC, width,
                                          height, 30};
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

  ret = recorder_.CreateVideoTrack(session_id, video_track_id_4kp_avc,
                                   video_track_param, video_track_cb);
  ASSERT_TRUE(ret == NO_ERROR);

  std::vector<uint32_t> track_ids;
  track_ids.push_back(video_track_id_4kp_avc);

  VideoExtraParam extra_param;
  SourceVideoTrack surface_video_copy;
  surface_video_copy.source_track_id = video_track_id_4kp_avc;
  extra_param.Update(QMMF_SOURCE_VIDEO_TRACK_ID, surface_video_copy);

  width  = 848;
  height = 480;

  video_track_param.width = width;
  video_track_param.height = height;
  video_track_param.frame_rate = 30;

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
    // Enable EIS
    vstab_mode = 1;
    vstab_mode = ANDROID_CONTROL_VIDEO_STABILIZATION_MODE_ON;
    meta.update(ANDROID_CONTROL_VIDEO_STABILIZATION_MODE, &vstab_mode, 1);
    ret = recorder_.SetCameraParam(camera_id_, meta);
    ASSERT_TRUE(ret == NO_ERROR);
  }

  for (uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr, "test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
              test_info_->name(), i);
    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    // Let session run for time record_duration_, during this time buffer with
    // valid data would be received in track callback (VideoTrackYUVDataCb).
    sleep(record_duration_);

    ret = recorder_.StopSession(session_id, false);
    ASSERT_TRUE(ret == NO_ERROR);
  }

  ret = recorder_.DeleteVideoTrack(session_id, video_track_id_480p_yuv);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteVideoTrack(session_id, video_track_id_480p_avc);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteVideoTrack(session_id, video_track_id_4kp_avc);
  ASSERT_TRUE(ret == NO_ERROR);

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
* SessionWith1440p60FPSEncCopy480pEncAndLinked480pEISLCACTNR:
*                                          This test will test session with
*                                          one 1440p60 Enc track, one Copy 480
*                                          Enc Track and one linked with EIS,
*                                          YUV CAC and TNR.
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
TEST_F(RecorderPostprocessVideoGTest,
       SessionWith1440p60FPSEncCopy480pEncAndLinked480pEISLCACTNR) {
  fprintf(stderr, "\n---------- Run Test %s.%s ------------\n",
          test_info_->test_case_name(), test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  camera_start_params_.frame_rate = 60;
  ret = recorder_.StartCamera(camera_id_, camera_start_params_);
  ASSERT_TRUE(ret == NO_ERROR);

  uint32_t video_track_id_1440p_avc = 1;
  uint32_t video_track_id_480p_avc = 2;
  uint32_t video_track_id_480p_yuv = 3;

  CameraMetadata meta;
  uint8_t enable_lcac;
  uint8_t vstab_mode;
  uint8_t tnr_mode;

  for (uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr, "test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
              test_info_->name(), i);

    SessionCb session_status_cb = CreateSessionStatusCb();
    uint32_t session_id;
    ret = recorder_.CreateSession(session_status_cb, &session_id);
    ASSERT_TRUE(session_id > 0);
    ASSERT_TRUE(ret == NO_ERROR);

    uint32_t width = 1920;
    uint32_t height = 1440;
    if (dump_bitstream_.IsEnabled()) {
      StreamDumpInfo dumpinfo1 = { VideoFormat::kAVC, video_track_id_1440p_avc,
                                   session_id, width, height };
      ret = dump_bitstream_.SetUp(dumpinfo1);
      ASSERT_TRUE(ret == NO_ERROR);

      StreamDumpInfo dumpinfo2 = { VideoFormat::kAVC, video_track_id_480p_avc,
                                   session_id, 640, 480};
      ret = dump_bitstream_.SetUp(dumpinfo2);
      ASSERT_TRUE(ret == NO_ERROR);
    }

    VideoTrackCreateParam video_track_param{camera_id_, VideoFormat::kAVC,
                                            width, height, 60};
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

    VideoExtraParam extra_param;
    SourceVideoTrack surface_video_copy;
    surface_video_copy.source_track_id = video_track_id_1440p_avc;
    extra_param.Update(QMMF_SOURCE_VIDEO_TRACK_ID, surface_video_copy);

    width = 640;
    height = 480;
    video_track_param.width = width;
    video_track_param.height = height;
    video_track_param.frame_rate = 60;

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
      // Enable YUV LCAC
      enable_lcac = 1;
      ret = meta.update( QCAMERA3_LCAC_PROCESSING_ENABLE, &enable_lcac, 1);
      ASSERT_TRUE(ret == NO_ERROR);
      ret = recorder_.SetCameraParam(camera_id_, meta);
      ASSERT_TRUE(ret == NO_ERROR);

      // Enable EIS
      vstab_mode = ANDROID_CONTROL_VIDEO_STABILIZATION_MODE_ON;
      meta.update(ANDROID_CONTROL_VIDEO_STABILIZATION_MODE, &vstab_mode, 1);
      ret = recorder_.SetCameraParam(camera_id_, meta);
      ASSERT_TRUE(ret == NO_ERROR);

      tnr_mode = ANDROID_NOISE_REDUCTION_MODE_HIGH_QUALITY;
      TEST_INFO("%s: Enable TNR mode(%d)", __func__, tnr_mode);
      meta.update(ANDROID_NOISE_REDUCTION_MODE, &tnr_mode, 1);
      status = recorder_.SetCameraParam(camera_id_, meta);
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
* SessionWith1440p60FPSEncCopy480pEncAndLinked480pEISLCAC:
*                                          This test will test session with
*                                          one 1440p60 Enc track, one Copy 480
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
TEST_F(RecorderPostprocessVideoGTest,
       SessionWith1440p60FPSEncCopy480pEncAndLinked480pEISLCAC) {
  fprintf(stderr, "\n---------- Run Test %s.%s ------------\n",
          test_info_->test_case_name(), test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  camera_start_params_.frame_rate = 60;
  ret = recorder_.StartCamera(camera_id_, camera_start_params_);
  ASSERT_TRUE(ret == NO_ERROR);

  uint32_t video_track_id_1440p_avc = 1;
  uint32_t video_track_id_480p_avc = 2;
  uint32_t video_track_id_480p_yuv = 3;

  CameraMetadata meta;
  uint8_t enable_lcac;
  uint8_t vstab_mode;

  for (uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr, "test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
              test_info_->name(), i);
    SessionCb session_status_cb = CreateSessionStatusCb();
    uint32_t session_id;
    ret = recorder_.CreateSession(session_status_cb, &session_id);
    ASSERT_TRUE(session_id > 0);
    ASSERT_TRUE(ret == NO_ERROR);

    uint32_t width = 1920;
    uint32_t height = 1440;
    if (dump_bitstream_.IsEnabled()) {
      StreamDumpInfo dumpinfo1 = { VideoFormat::kAVC, video_track_id_1440p_avc,
                                   session_id, width, height};
      ret = dump_bitstream_.SetUp(dumpinfo1);
      ASSERT_TRUE(ret == NO_ERROR);

      StreamDumpInfo dumpinfo2 = { VideoFormat::kAVC, video_track_id_480p_avc,
                                   session_id, 640, 480};
      ret = dump_bitstream_.SetUp(dumpinfo2);
      ASSERT_TRUE(ret == NO_ERROR);
    }
    VideoTrackCreateParam video_track_param{camera_id_, VideoFormat::kAVC,
                                            width, height, 60};
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

    VideoExtraParam extra_param;
    SourceVideoTrack surface_video_copy;
    surface_video_copy.source_track_id = video_track_id_1440p_avc;
    extra_param.Update(QMMF_SOURCE_VIDEO_TRACK_ID, surface_video_copy);

    width = 640;
    height = 480;
    video_track_param.width = width;
    video_track_param.height = height;
    video_track_param.frame_rate = 60;

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

    // Enable YUV LCAC
    auto status = recorder_.GetCameraParam(camera_id_, meta);
    if (NO_ERROR == status) {
      enable_lcac = 1;
      ret = meta.update( QCAMERA3_LCAC_PROCESSING_ENABLE, &enable_lcac, 1);
      ASSERT_TRUE(ret == NO_ERROR);
      ret = recorder_.SetCameraParam(camera_id_, meta);
      ASSERT_TRUE(ret == NO_ERROR);

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
* SessionWith1440p60FPSEncCopy480pEncAndLinked480pEIS:
*                                          This test will test session with
*                                          one 1440p60 Enc track, one Copy 480
*                                          Enc Track and one linked with EIS,
*                                          YUV CAC and TNR.
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
TEST_F(RecorderPostprocessVideoGTest,
       SessionWith1440p60FPSEncCopy480pEncAndLinked480pEIS) {
  fprintf(stderr, "\n---------- Run Test %s.%s ------------\n",
          test_info_->test_case_name(), test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  camera_start_params_.frame_rate = 60;
  ret = recorder_.StartCamera(camera_id_, camera_start_params_);
  ASSERT_TRUE(ret == NO_ERROR);

  uint32_t video_track_id_1440p_avc = 1;
  uint32_t video_track_id_480p_avc = 2;
  uint32_t video_track_id_480p_yuv = 3;

  CameraMetadata meta;
  uint8_t vstab_mode;

  for (uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr, "test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
              test_info_->name(), i);
    SessionCb session_status_cb = CreateSessionStatusCb();
    uint32_t session_id;
    ret = recorder_.CreateSession(session_status_cb, &session_id);
    ASSERT_TRUE(session_id > 0);
    ASSERT_TRUE(ret == NO_ERROR);

    uint32_t width = 1920;
    uint32_t height = 1440;
    if (dump_bitstream_.IsEnabled()) {
      StreamDumpInfo dumpinfo1 = { VideoFormat::kAVC, video_track_id_1440p_avc,
                                   session_id, width, height};
      ret = dump_bitstream_.SetUp(dumpinfo1);
      ASSERT_TRUE(ret == NO_ERROR);

      StreamDumpInfo dumpinfo2 = { VideoFormat::kAVC, video_track_id_480p_avc,
                                   session_id, 640, 480};
      ret = dump_bitstream_.SetUp(dumpinfo2);
      ASSERT_TRUE(ret == NO_ERROR);
    }
    VideoTrackCreateParam video_track_param{camera_id_, VideoFormat::kAVC,
                                            width, height, 60};
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

    VideoExtraParam extra_param;
    SourceVideoTrack surface_video_copy;
    surface_video_copy.source_track_id = video_track_id_1440p_avc;
    extra_param.Update(QMMF_SOURCE_VIDEO_TRACK_ID, surface_video_copy);

    width = 640;
    height = 480;
    video_track_param.width = width;
    video_track_param.height = height;
    video_track_param.frame_rate = 60;

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
* SessionWith1440p60FPSEncCopy480pEncAndLinked480p:
*                                          This test will test session with
*                                          one 1440p60 Enc track, one Copy 480
*                                          Enc Track and one linked with EIS,
*                                          YUV CAC and TNR.
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
TEST_F(RecorderPostprocessVideoGTest,
       SessionWith1440p60FPSEncCopy480pEncAndLinked480p) {
  fprintf(stderr, "\n---------- Run Test %s.%s ------------\n",
          test_info_->test_case_name(), test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  camera_start_params_.frame_rate = 60;
  ret = recorder_.StartCamera(camera_id_, camera_start_params_);
  ASSERT_TRUE(ret == NO_ERROR);

  uint32_t video_track_id_1440p_avc = 1;
  uint32_t video_track_id_480p_avc = 2;
  uint32_t video_track_id_480p_yuv = 3;

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

    uint32_t width = 1920;
    uint32_t height = 1440;
    if (dump_bitstream_.IsEnabled()) {
      StreamDumpInfo dumpinfo1 = { VideoFormat::kAVC, video_track_id_1440p_avc,
                                   session_id, width, height};
      ret = dump_bitstream_.SetUp(dumpinfo1);
      ASSERT_TRUE(ret == NO_ERROR);

      StreamDumpInfo dumpinfo2 = { VideoFormat::kAVC, video_track_id_480p_avc,
                                   session_id, 640, 480};
      ret = dump_bitstream_.SetUp(dumpinfo2);
      ASSERT_TRUE(ret == NO_ERROR);
    }

    VideoTrackCreateParam video_track_param{camera_id_, VideoFormat::kAVC,
                                            width, height, 60};
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

    VideoExtraParam extra_param;
    SourceVideoTrack surface_video_copy;
    surface_video_copy.source_track_id = video_track_id_1440p_avc;
    extra_param.Update(QMMF_SOURCE_VIDEO_TRACK_ID, surface_video_copy);

    width = 640;
    height = 480;
    video_track_param.width = width;
    video_track_param.height = height;
    video_track_param.frame_rate = 60;

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
* SessionWith1440p60FPSEncEIS: This test will test session with
*                                  one 1440p60 Enc track with EIS, YUVCAC
*                                  and TNR
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
TEST_F(RecorderPostprocessVideoGTest, SessionWith1440p60FPSEncEIS) {
  fprintf(stderr, "\n---------- Run Test %s.%s ------------\n",
          test_info_->test_case_name(), test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  camera_start_params_.frame_rate = 60;
  ret = recorder_.StartCamera(camera_id_, camera_start_params_);
  ASSERT_TRUE(ret == NO_ERROR);

  uint32_t video_track_id_1440p_avc = 1;

  CameraMetadata meta;
  uint8_t vstab_mode;

  for (uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr, "test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
              test_info_->name(), i);
    SessionCb session_status_cb = CreateSessionStatusCb();
    uint32_t session_id;
    ret = recorder_.CreateSession(session_status_cb, &session_id);
    ASSERT_TRUE(session_id > 0);
    ASSERT_TRUE(ret == NO_ERROR);

    uint32_t width = 1920;
    uint32_t height = 1440;
    if (dump_bitstream_.IsEnabled()) {
      StreamDumpInfo dumpinfo1 = { VideoFormat::kAVC, video_track_id_1440p_avc,
                                   session_id, width, height };
      ret = dump_bitstream_.SetUp(dumpinfo1);
      ASSERT_TRUE(ret == NO_ERROR);
    }
    VideoTrackCreateParam video_track_param{camera_id_, VideoFormat::kAVC,
                                            width, height, 60};
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

    auto status = recorder_.GetCameraParam(camera_id_, meta);
    if (NO_ERROR == status) {
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
* SessionWith1440p60FPSEncEISLCAC: This test will test session with
*                                  one 1440p60 Enc track with EIS and YUVCAC
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
TEST_F(RecorderPostprocessVideoGTest, SessionWith1440p60FPSEncEISLCAC) {
  fprintf(stderr, "\n---------- Run Test %s.%s ------------\n",
          test_info_->test_case_name(), test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  camera_start_params_.frame_rate = 60;
  ret = recorder_.StartCamera(camera_id_, camera_start_params_);
  ASSERT_TRUE(ret == NO_ERROR);

  uint32_t video_track_id_1440p_avc = 1;

  CameraMetadata meta;
  uint8_t enable_lcac;
  uint8_t vstab_mode;

  for (uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr, "test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
              test_info_->name(), i);
    SessionCb session_status_cb = CreateSessionStatusCb();
    uint32_t session_id;
    ret = recorder_.CreateSession(session_status_cb, &session_id);
    ASSERT_TRUE(session_id > 0);
    ASSERT_TRUE(ret == NO_ERROR);

    uint32_t width = 1920;
    uint32_t height = 1440;
    if (dump_bitstream_.IsEnabled()) {
      StreamDumpInfo dumpinfo1 = { VideoFormat::kAVC, video_track_id_1440p_avc,
                                   session_id, width, height };
      ret = dump_bitstream_.SetUp(dumpinfo1);
      ASSERT_TRUE(ret == NO_ERROR);
    }
    VideoTrackCreateParam video_track_param{camera_id_, VideoFormat::kAVC,
                                            width, height, 60};
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

    auto status = recorder_.GetCameraParam(camera_id_, meta);
    if (NO_ERROR == status) {
      // Enable YUV LCAC
      enable_lcac = 1;
      ret = meta.update( QCAMERA3_LCAC_PROCESSING_ENABLE, &enable_lcac, 1);
      ASSERT_TRUE(ret == NO_ERROR);
      ret = recorder_.SetCameraParam(camera_id_, meta);
      ASSERT_TRUE(ret == NO_ERROR);

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
* SessionWith1440p60FPSEncEISLCACTNR: This test will test session with
*                                  one 1440p60 Enc track with EIS and YUVCAC
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
TEST_F(RecorderPostprocessVideoGTest, SessionWith1440p60FPSEncEISLCACTNR) {
  fprintf(stderr, "\n---------- Run Test %s.%s ------------\n",
          test_info_->test_case_name(), test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  camera_start_params_.frame_rate = 60;
  ret = recorder_.StartCamera(camera_id_, camera_start_params_);
  ASSERT_TRUE(ret == NO_ERROR);

  uint32_t video_track_id_1440p_avc = 1;

  CameraMetadata meta;
  uint8_t enable_lcac;
  uint8_t vstab_mode;
  uint8_t tnr_mode;

  for (uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr, "test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
              test_info_->name(), i);
    SessionCb session_status_cb = CreateSessionStatusCb();
    uint32_t session_id;
    ret = recorder_.CreateSession(session_status_cb, &session_id);
    ASSERT_TRUE(session_id > 0);
    ASSERT_TRUE(ret == NO_ERROR);

    uint32_t width = 1920;
    uint32_t height = 1440;
    if (dump_bitstream_.IsEnabled()) {
      StreamDumpInfo dumpinfo1 = { VideoFormat::kAVC, video_track_id_1440p_avc,
                                   session_id, width, height};
      ret = dump_bitstream_.SetUp(dumpinfo1);
      ASSERT_TRUE(ret == NO_ERROR);
    }
    VideoTrackCreateParam video_track_param{camera_id_, VideoFormat::kAVC,
                                            width, height, 60};
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

    auto status = recorder_.GetCameraParam(camera_id_, meta);
    if (NO_ERROR == status) {
      // Enable YUV LCAC
      enable_lcac = 1;
      ret = meta.update( QCAMERA3_LCAC_PROCESSING_ENABLE, &enable_lcac, 1);
      ASSERT_TRUE(ret == NO_ERROR);
      ret = recorder_.SetCameraParam(camera_id_, meta);
      ASSERT_TRUE(ret == NO_ERROR);

      // Enable EIS
      vstab_mode = ANDROID_CONTROL_VIDEO_STABILIZATION_MODE_ON;
      meta.update(ANDROID_CONTROL_VIDEO_STABILIZATION_MODE, &vstab_mode, 1);
      ret = recorder_.SetCameraParam(camera_id_, meta);
      ASSERT_TRUE(ret == NO_ERROR);

      // Enable TNR.
      tnr_mode = ANDROID_NOISE_REDUCTION_MODE_HIGH_QUALITY;
      meta.update(ANDROID_NOISE_REDUCTION_MODE, &tnr_mode, 1);
      status = recorder_.SetCameraParam(camera_id_, meta);
      ASSERT_TRUE(ret == NO_ERROR);
    }

    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    // Let session run for time record_duration_, during this time buffer with
    // valid data would be received in track callback (VideoTrackYUVDataCb).
    sleep(record_duration_);

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
* SessionWith1440p60FPSSmoothZoom: This test will test session with
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
TEST_F(RecorderPostprocessVideoGTest, SessionWith1440p60FPSSmoothZoom) {
  fprintf(stderr, "\n---------- Run Test %s.%s ------------\n",
          test_info_->test_case_name(), test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  camera_start_params_.frame_rate = 60;
  ret = recorder_.StartCamera(camera_id_, camera_start_params_);
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
      StreamDumpInfo dumpinfo1 = { VideoFormat::kAVC, video_track_id_1440p_avc,
                                   session_id, track_width, track_height };
      ret = dump_bitstream_.SetUp(dumpinfo1);
      ASSERT_TRUE(ret == NO_ERROR);
    }
    VideoTrackCreateParam video_track_param{camera_id_, VideoFormat::kAVC,
                                            track_width, track_height, 60};
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

    float zoom = 2.0f;
    camera_metadata_entry active_array_size;
    if (meta.exists(ANDROID_SENSOR_INFO_ACTIVE_ARRAY_SIZE)) {
      active_array_size = meta.find(ANDROID_SENSOR_INFO_ACTIVE_ARRAY_SIZE);
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

#ifndef DISABLE_DISPLAY
/*
* SessionWith1440p60FPSEncCopy480pEncAndLinked480pDisplayEISLCACTNRLandscape:
*                                          This test will test session with
*                                          one 1440p60 Enc track, one Copy 480
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
TEST_F(RecorderPostprocessVideoGTest,
       SessionWith1440p60FPSEncCopy480pEncAndLinked480pDisplayEISLCACTNRLandscape) {
  fprintf(stderr, "\n---------- Run Test %s.%s ------------\n",
          test_info_->test_case_name(), test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  camera_start_params_.frame_rate = 60;
  ret = recorder_.StartCamera(camera_id_, camera_start_params_);
  ASSERT_TRUE(ret == NO_ERROR);

  uint32_t video_track_id_1440p_avc = 1;
  uint32_t video_track_id_480p_avc = 2;
  uint32_t video_track_id_480p_yuv = 3;

  CameraMetadata meta;
  uint8_t enable_lcac;
  uint8_t vstab_mode;
  uint8_t tnr_mode;

  for (uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr, "test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
              test_info_->name(), i);

    SessionCb session_status_cb = CreateSessionStatusCb();
    uint32_t session_id;
    ret = recorder_.CreateSession(session_status_cb, &session_id);
    ASSERT_TRUE(session_id > 0);
    ASSERT_TRUE(ret == NO_ERROR);

    uint32_t width = 1920;
    uint32_t height = 1440;
    if (dump_bitstream_.IsEnabled()) {
      StreamDumpInfo dumpinfo1 = {VideoFormat::kAVC, video_track_id_1440p_avc,
                                  session_id, width, height};
      ret = dump_bitstream_.SetUp(dumpinfo1);
      ASSERT_TRUE(ret == NO_ERROR);

      StreamDumpInfo dumpinfo2 = {VideoFormat::kAVC, video_track_id_480p_avc,
                                  session_id, 640, 480};
      ret = dump_bitstream_.SetUp(dumpinfo2);
      ASSERT_TRUE(ret == NO_ERROR);
    }
    VideoTrackCreateParam video_track_param{camera_id_, VideoFormat::kAVC,
                                            width, height, 60};
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

    VideoExtraParam extra_param;
    SourceVideoTrack surface_video_copy;
    surface_video_copy.source_track_id = video_track_id_1440p_avc;
    extra_param.Update(QMMF_SOURCE_VIDEO_TRACK_ID, surface_video_copy);

    width = 640;
    height = 480;
    video_track_param.width = width;
    video_track_param.height = height;
    video_track_param.frame_rate = 60;

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

    // Enable YUV LCAC
    auto status = recorder_.GetCameraParam(camera_id_, meta);
    if (NO_ERROR == status) {
      enable_lcac = 1;
      ret = meta.update( QCAMERA3_LCAC_PROCESSING_ENABLE, &enable_lcac, 1);
      ASSERT_TRUE(ret == NO_ERROR);
      ret = recorder_.SetCameraParam(camera_id_, meta);
      ASSERT_TRUE(ret == NO_ERROR);

      // Enable EIS
      vstab_mode = ANDROID_CONTROL_VIDEO_STABILIZATION_MODE_ON;
      meta.update(ANDROID_CONTROL_VIDEO_STABILIZATION_MODE, &vstab_mode, 1);
      ret = recorder_.SetCameraParam(camera_id_, meta);
      ASSERT_TRUE(ret == NO_ERROR);

      tnr_mode = ANDROID_NOISE_REDUCTION_MODE_HIGH_QUALITY;
      TEST_INFO("%s: Enable TNR mode(%d)", __func__, tnr_mode);
      meta.update(ANDROID_NOISE_REDUCTION_MODE, &tnr_mode, 1);
      status = recorder_.SetCameraParam(camera_id_, meta);
      ASSERT_TRUE(ret == NO_ERROR);
    }

    use_display_ = true;

    if (use_display_) {
      ret = StartDisplay(DisplayType::kPrimary, width, height, 480, 320);
      if (ret != 0) {
        TEST_ERROR("%s: StartDisplay Failed!!", __func__);
      }
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
      if (ret != 0) {
        TEST_ERROR("%s: StopDisplay Failed!!", __func__);
      }
    }

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_480p_yuv);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_480p_avc);
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
* SessionWith960p90FPSEncCopy480pEncAndLinked480pDisplayEISLCACTNRLandscape:
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
TEST_F(RecorderPostprocessVideoGTest,
       SessionWith960p90FPSEncCopy480pEncAndLinked480pDisplayEISLCACTNRLandscape) {
  fprintf(stderr, "\n---------- Run Test %s.%s ------------\n",
          test_info_->test_case_name(), test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  camera_start_params_.frame_rate = 90;
  ret = recorder_.StartCamera(camera_id_, camera_start_params_);
  ASSERT_TRUE(ret == NO_ERROR);

  uint32_t video_track_id_960p_avc = 1;
  uint32_t video_track_id_480p_avc = 2;
  uint32_t video_track_id_480p_yuv = 3;

  CameraMetadata meta;
  uint8_t enable_lcac;
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
      StreamDumpInfo dumpinfo1 = { VideoFormat::kAVC, video_track_id_960p_avc,
                                   session_id, width, height };
      ret = dump_bitstream_.SetUp(dumpinfo1);
      ASSERT_TRUE(ret == NO_ERROR);

      StreamDumpInfo dumpinfo2 = { VideoFormat::kAVC, video_track_id_480p_avc,
                                   session_id, 640, 480 };
      ret = dump_bitstream_.SetUp(dumpinfo2);
      ASSERT_TRUE(ret == NO_ERROR);
    }

    VideoTrackCreateParam video_track_param{camera_id_, VideoFormat::kAVC,
                                            width, height, 90};
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

    // Enable YUV LCAC
    auto status = recorder_.GetCameraParam(camera_id_, meta);
    if (NO_ERROR == status) {
      enable_lcac = 1;
      ret = meta.update( QCAMERA3_LCAC_PROCESSING_ENABLE, &enable_lcac, 1);
      ASSERT_TRUE(ret == NO_ERROR);
      ret = recorder_.SetCameraParam(camera_id_, meta);
      ASSERT_TRUE(ret == NO_ERROR);

      // Enable EIS
      vstab_mode = ANDROID_CONTROL_VIDEO_STABILIZATION_MODE_ON;
      meta.update(ANDROID_CONTROL_VIDEO_STABILIZATION_MODE, &vstab_mode, 1);
      ret = recorder_.SetCameraParam(camera_id_, meta);
      ASSERT_TRUE(ret == NO_ERROR);

      tnr_mode = ANDROID_NOISE_REDUCTION_MODE_HIGH_QUALITY;
      TEST_INFO("%s: Enable TNR mode(%d)", __func__, tnr_mode);
      meta.update(ANDROID_NOISE_REDUCTION_MODE, &tnr_mode, 1);
      status = recorder_.SetCameraParam(camera_id_, meta);
      ASSERT_TRUE(ret == NO_ERROR);

    }

    use_display_ = true;

    if (use_display_) {
      ret = StartDisplay(DisplayType::kPrimary, width, height, 480, 320);
      if (ret != 0) {
        TEST_ERROR("%s: StartDisplay Failed!!", __func__);
      }
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
      if (ret != 0) {
        TEST_ERROR("%s: StopDisplay Failed!!", __func__);
      }
    }

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
* SessionWith4k30FPSEncCopy480pEncAndLinked480pDisplayEISLCACLandscape:
*                                          This test will test session with
*                                          one 4k30 Enc track, one Copy 480 Enc
*                                          Track and one linked.
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
TEST_F(RecorderPostprocessVideoGTest,
       SessionWith4k30FPSEncCopy480pEncAndLinked480pDisplayEISLCACLandscape) {
  fprintf(stderr, "\n---------- Run Test %s.%s ------------\n",
          test_info_->test_case_name(), test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.StartCamera(camera_id_, camera_start_params_);
  ASSERT_TRUE(ret == NO_ERROR);

  uint32_t video_track_id_4kp_avc = 1;
  uint32_t video_track_id_480p_avc = 2;
  uint32_t video_track_id_480p_yuv = 3;
  CameraMetadata meta;
  uint8_t enable_lcac;
  uint8_t vstab_mode;

  SessionCb session_status_cb = CreateSessionStatusCb();
  uint32_t session_id;
  ret = recorder_.CreateSession(session_status_cb, &session_id);
  ASSERT_TRUE(session_id > 0);
  ASSERT_TRUE(ret == NO_ERROR);

  uint32_t width = 3840;
  uint32_t height = 2160;
  if (dump_bitstream_.IsEnabled()) {
    StreamDumpInfo dumpinfo1 = {VideoFormat::kAVC, video_track_id_4kp_avc,
                                session_id, width, height};
    ret = dump_bitstream_.SetUp(dumpinfo1);
    ASSERT_TRUE(ret == NO_ERROR);

    StreamDumpInfo dumpinfo2 = {VideoFormat::kAVC, video_track_id_480p_avc,
                                session_id, 848, 480};
    ret = dump_bitstream_.SetUp(dumpinfo2);
    ASSERT_TRUE(ret == NO_ERROR);
  }

  VideoTrackCreateParam video_track_param{camera_id_, VideoFormat::kAVC, width,
                                          height, 30};
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

  ret = recorder_.CreateVideoTrack(session_id, video_track_id_4kp_avc,
                                   video_track_param, video_track_cb);
  ASSERT_TRUE(ret == NO_ERROR);

  std::vector<uint32_t> track_ids;
  track_ids.push_back(video_track_id_4kp_avc);

  VideoExtraParam extra_param;
  SourceVideoTrack surface_video_copy;
  surface_video_copy.source_track_id = video_track_id_4kp_avc;
  extra_param.Update(QMMF_SOURCE_VIDEO_TRACK_ID, surface_video_copy);

  width = 848;
  height = 480;

  video_track_param.width = width;
  video_track_param.height = height;
  video_track_param.frame_rate = 30;

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

  // Enable YUV LCAC
  auto status = recorder_.GetCameraParam(camera_id_, meta);
  if (NO_ERROR == status) {
    enable_lcac = 1;
    ret = meta.update( QCAMERA3_LCAC_PROCESSING_ENABLE, &enable_lcac, 1);
    ASSERT_TRUE(ret == NO_ERROR);
    ret = recorder_.SetCameraParam(camera_id_, meta);
    ASSERT_TRUE(ret == NO_ERROR);

    // Enable EIS
    vstab_mode = ANDROID_CONTROL_VIDEO_STABILIZATION_MODE_ON;
    meta.update(ANDROID_CONTROL_VIDEO_STABILIZATION_MODE, &vstab_mode, 1);
    ret = recorder_.SetCameraParam(camera_id_, meta);
    ASSERT_TRUE(ret == NO_ERROR);
  }

  use_display_ = true;

  if (use_display_) {
    ret = StartDisplay(DisplayType::kPrimary, width, height, 480, 320);
    if (ret != 0) {
      TEST_ERROR("%s: StartDisplay Failed!!", __func__);
    }
  }
  for (uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr, "test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
              test_info_->name(), i);
    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    // Let session run for time record_duration_, during this time buffer with
    // valid data would be received in track callback (VideoTrackYUVDataCb).
    sleep(record_duration_);

    ret = recorder_.StopSession(session_id, false);
    ASSERT_TRUE(ret == NO_ERROR);
  }
  if (use_display_) {
    ret = StopDisplay(DisplayType::kPrimary);
    if (ret != 0) {
      TEST_ERROR("%s: StopDisplay Failed!!", __func__);
    }
  }

  ret = recorder_.DeleteVideoTrack(session_id, video_track_id_480p_yuv);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteVideoTrack(session_id, video_track_id_480p_avc);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteVideoTrack(session_id, video_track_id_4kp_avc);
  ASSERT_TRUE(ret == NO_ERROR);

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
* SessionWith1440p30FPSEncCopy480pEncAndLinked480pDisplayEISLCACTNR:
*                                          This test will test session with
*                                          one 1440p30 Enc track, one Copy 480
*                                          Enc Track and one linked.
* API test sequence:
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
TEST_F(RecorderPostprocessVideoGTest,
       SessionWith1440p30FPSEncCopy480pEncAndLinked480pDisplayEISLCACTNR) {
  fprintf(stderr, "\n---------- Run Test %s.%s ------------\n",
          test_info_->test_case_name(), test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  camera_start_params_.frame_rate = 30;
  ret = recorder_.StartCamera(camera_id_, camera_start_params_);
  ASSERT_TRUE(ret == NO_ERROR);

  uint32_t video_track_id_1440p_avc = 1;
  uint32_t video_track_id_480p_avc = 2;
  uint32_t video_track_id_480p_yuv = 3;

  CameraMetadata meta;
  uint8_t enable_lcac;
  uint8_t vstab_mode;
  uint8_t tnr_mode;

  for (uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr, "test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
              test_info_->name(), i);

    SessionCb session_status_cb = CreateSessionStatusCb();
    uint32_t session_id;
    ret = recorder_.CreateSession(session_status_cb, &session_id);
    ASSERT_TRUE(session_id > 0);
    ASSERT_TRUE(ret == NO_ERROR);

    uint32_t width = 1920;
    uint32_t height = 1440;
    if (dump_bitstream_.IsEnabled()) {
      StreamDumpInfo dumpinfo1 = {VideoFormat::kAVC, video_track_id_1440p_avc,
                                  session_id, width, height};
      ret = dump_bitstream_.SetUp(dumpinfo1);
      ASSERT_TRUE(ret == NO_ERROR);

      StreamDumpInfo dumpinfo2 = {VideoFormat::kAVC, video_track_id_480p_avc,
                                  session_id, 640, 480};
      ret = dump_bitstream_.SetUp(dumpinfo2);
      ASSERT_TRUE(ret == NO_ERROR);
    }

    VideoTrackCreateParam video_track_param{camera_id_, VideoFormat::kAVC,
                                            width, height, 30};
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

    VideoExtraParam extra_param;
    SourceVideoTrack surface_video_copy;
    surface_video_copy.source_track_id = video_track_id_1440p_avc;
    extra_param.Update(QMMF_SOURCE_VIDEO_TRACK_ID, surface_video_copy);

    width = 640;
    height = 480;
    video_track_param.width = width;
    video_track_param.height = height;
    video_track_param.frame_rate = 30;

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

    // Enable YUV LCAC
    auto status = recorder_.GetCameraParam(camera_id_, meta);
    if (NO_ERROR == status) {
      enable_lcac = 1;
      ret = meta.update( QCAMERA3_LCAC_PROCESSING_ENABLE, &enable_lcac, 1);
      ASSERT_TRUE(ret == NO_ERROR);
      ret = recorder_.SetCameraParam(camera_id_, meta);
      ASSERT_TRUE(ret == NO_ERROR);

      // Enable EIS
      vstab_mode = ANDROID_CONTROL_VIDEO_STABILIZATION_MODE_ON;
      meta.update(ANDROID_CONTROL_VIDEO_STABILIZATION_MODE, &vstab_mode, 1);
      ret = recorder_.SetCameraParam(camera_id_, meta);
      ASSERT_TRUE(ret == NO_ERROR);

      // Enable TNR
      tnr_mode = ANDROID_NOISE_REDUCTION_MODE_HIGH_QUALITY;
      TEST_INFO("%s: Enable TNR mode(%d)", __func__, tnr_mode);
      meta.update(ANDROID_NOISE_REDUCTION_MODE, &tnr_mode, 1);
      status = recorder_.SetCameraParam(camera_id_, meta);
      ASSERT_TRUE(ret == NO_ERROR);
    }

    use_display_ = true;

    if (use_display_) {
      ret = StartDisplay(DisplayType::kPrimary, width, height, 480, 320);
      if (ret != 0) {
        TEST_ERROR("%s: StartDisplay Failed!!", __func__);
      }
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
      if (ret != 0) {
        TEST_ERROR("%s: StopDisplay Failed!!", __func__);
      }
    }

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_480p_yuv);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_480p_avc);
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
* SessionWith1440p30FPSEncCopy480pEncAndLinked480pDisplayEISLCAC:
*                                          This test will test session with
*                                          one 1440p30 Enc track, one Copy 480
*                                          Enc Track and one linked.
* API test sequence:
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
TEST_F(RecorderPostprocessVideoGTest,
       SessionWith1440p30FPSEncCopy480pEncAndLinked480pDisplayEISLCAC) {
  fprintf(stderr, "\n---------- Run Test %s.%s ------------\n",
          test_info_->test_case_name(), test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  camera_start_params_.frame_rate = 30;
  ret = recorder_.StartCamera(camera_id_, camera_start_params_);
  ASSERT_TRUE(ret == NO_ERROR);

  uint32_t video_track_id_1440p_avc = 1;
  uint32_t video_track_id_480p_avc = 2;
  uint32_t video_track_id_480p_yuv = 3;

  CameraMetadata meta;
  uint8_t enable_lcac;
  uint8_t vstab_mode;

  for (uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr, "test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
              test_info_->name(), i);

    SessionCb session_status_cb = CreateSessionStatusCb();
    uint32_t session_id;
    ret = recorder_.CreateSession(session_status_cb, &session_id);
    ASSERT_TRUE(session_id > 0);
    ASSERT_TRUE(ret == NO_ERROR);

    uint32_t width = 1920;
    uint32_t height = 1440;
    if (dump_bitstream_.IsEnabled()) {
      StreamDumpInfo dumpinfo1 = {VideoFormat::kAVC, video_track_id_1440p_avc,
                                  session_id, width, height};
      ret = dump_bitstream_.SetUp(dumpinfo1);
      ASSERT_TRUE(ret == NO_ERROR);

      StreamDumpInfo dumpinfo2 = {VideoFormat::kAVC, video_track_id_480p_avc,
                                  session_id, 640, 480};
      ret = dump_bitstream_.SetUp(dumpinfo2);
      ASSERT_TRUE(ret == NO_ERROR);
    }

    VideoTrackCreateParam video_track_param{camera_id_, VideoFormat::kAVC,
                                            width, height, 30};
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

    VideoExtraParam extra_param;
    SourceVideoTrack surface_video_copy;
    surface_video_copy.source_track_id = video_track_id_1440p_avc;
    extra_param.Update(QMMF_SOURCE_VIDEO_TRACK_ID, surface_video_copy);

    width = 640;
    height = 480;
    video_track_param.width = width;
    video_track_param.height = height;
    video_track_param.frame_rate = 30;

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

    // Enable YUV LCAC
    auto status = recorder_.GetCameraParam(camera_id_, meta);
    if (NO_ERROR == status) {
      enable_lcac = 1;
      ret = meta.update( QCAMERA3_LCAC_PROCESSING_ENABLE, &enable_lcac, 1);
      ASSERT_TRUE(ret == NO_ERROR);
      ret = recorder_.SetCameraParam(camera_id_, meta);
      ASSERT_TRUE(ret == NO_ERROR);

      // Enable EIS
      vstab_mode = ANDROID_CONTROL_VIDEO_STABILIZATION_MODE_ON;
      meta.update(ANDROID_CONTROL_VIDEO_STABILIZATION_MODE, &vstab_mode, 1);
      ret = recorder_.SetCameraParam(camera_id_, meta);
      ASSERT_TRUE(ret == NO_ERROR);
    }

    use_display_ = true;

    if (use_display_) {
      ret = StartDisplay(DisplayType::kPrimary, width, height, 480, 320);
      if (ret != 0) {
        TEST_ERROR("%s: StartDisplay Failed!!", __func__);
      }
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
      if (ret != 0) {
        TEST_ERROR("%s: StopDisplay Failed!!", __func__);
      }
    }

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_480p_yuv);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id_480p_avc);
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

#endif

#ifdef ANDROID_O_OR_ABOVE
/*
* SessionWithDualCam4KEncAllISOModes: This case will test a dual cam session with
*                 4096x2048 h264 encoded track, during which in regular intervals
*                 ISO modes will change.
*                 Note: camera_id_ for dual cam to be set using adb property.
* Api test sequence:
*  - StartCamera
*   loop Start {
*   --------------------------
*   - CreateSession
*   - CreateVideoTrack
*   - StartVideoTrack
*   - StartSession
*   - Set ISO mode
*   - StopSession
*   - DeleteVideoTrack
*   - DeleteSession
*   --------------------------
*   } loop End
*  - StopCamera
*/
TEST_F(RecorderPostprocessVideoGTest, SessionWithDualCam4KEncAllISOModes) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  uint32_t record_dur = MAX(record_duration_, kISOModeEnd * 10);

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.StartCamera(camera_id_, camera_start_params_);
  ASSERT_TRUE(ret == NO_ERROR);

  VideoFormat format_type = VideoFormat::kAVC;
  uint32_t stream_width  = 4096;
  uint32_t stream_height = 2048;

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
      StreamDumpInfo dumpinfo = { video_track_param.format_type, video_track_id,
                                  session_id, stream_width, stream_height };
      ret = dump_bitstream_.SetUp(dumpinfo);
      ASSERT_TRUE(ret == NO_ERROR);
    }

    TrackCb video_track_cb;
    video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
                              std::vector<BufferDescriptor> buffers,
                              std::vector<MetaData> meta_buffers) {
    VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers); };

    video_track_cb.event_cb = [&] (uint32_t track_id, EventType event_type,
        void *event_data, size_t event_data_size) { VideoTrackEventCb(track_id,
        event_type, event_data, event_data_size); };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id,
                                      video_track_param, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    CameraMetadata meta;
    ret = recorder_.GetCameraParam(camera_id_, meta);
    ASSERT_TRUE(ret == NO_ERROR);

    uint32_t select_iso_priority_vtag;
    uint32_t use_iso_priority_vtag;
    if (!VendorTagSupported(String8("select_priority"),
        String8("org.codeaurora.qcamera3.iso_exp_priority"),
        &select_iso_priority_vtag)) {
      TEST_WARN("%s: select_priority is not supported", __func__);
      ASSERT_TRUE(0);
    }
    if (!VendorTagSupported(String8("use_iso_exp_priority"),
        String8("org.codeaurora.qcamera3.iso_exp_priority"),
        &use_iso_priority_vtag)) {
      TEST_WARN("%s: use_iso_exp_priority is not supported", __func__);
      ASSERT_TRUE(0);
    }

    // Setting tag to iso
    int32_t select_iso_priority = 0;
    ret = meta.update(select_iso_priority_vtag, &select_iso_priority, 1);

    for (int32_t count = kISOModeAuto; count < kISOModeEnd; count++) {
      int64_t iso_mode = count;
      ret = meta.update(use_iso_priority_vtag, &iso_mode, 1);
      ASSERT_TRUE(ret == NO_ERROR);
      fprintf(stderr, "ISO switched to mode[%d]\n", count);
      ret = recorder_.SetCameraParam(camera_id_, meta);
      ASSERT_TRUE(ret == NO_ERROR);
      sleep(record_dur/kISOModeEnd);
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
* SessionWithDualCam4KEncExposureTime: This case will test a dual cam session with
*                 4096x2048 h264 encoded track, during which in regular intervals
*                 shutter (exposure) time will change.
*                 Note: camera_id_ for dual cam to be set using adb property.
* Api test sequence:
*  - StartCamera
*   loop Start {
*   --------------------------
*   - CreateSession
*   - CreateVideoTrack
*   - StartVideoTrack
*   - StartSession
*   - Set exposure time
*   - StopSession
*   - DeleteVideoTrack
*   - DeleteSession
*   --------------------------
*   } loop End
*  - StopCamera
*/
TEST_F(RecorderPostprocessVideoGTest, SessionWithDualCam4KEncExposureTime) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  // Speeds are in nano-secs
  int64_t shutter_speed[] = {
    500000000,  // 1/2 fps
    200000000,  // 1/5 fps
    100000000,  // 1/10 fps
    50000000,   // 1/20 fps
    33000000    // 1/30 fps
  };
  uint32_t num_samples = sizeof(shutter_speed)/sizeof(shutter_speed[0]);
  uint32_t record_dur = MAX(record_duration_, num_samples * 10);

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.StartCamera(camera_id_, camera_start_params_);
  ASSERT_TRUE(ret == NO_ERROR);

  VideoFormat format_type = VideoFormat::kAVC;
  uint32_t stream_width  = 4096;
  uint32_t stream_height = 2048;

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
      StreamDumpInfo dumpinfo = { video_track_param.format_type, video_track_id,
                                  session_id, stream_width, stream_height };
      ret = dump_bitstream_.SetUp(dumpinfo);
      ASSERT_TRUE(ret == NO_ERROR);
    }

    TrackCb video_track_cb;
    video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
                              std::vector<BufferDescriptor> buffers,
                              std::vector<MetaData> meta_buffers) {
    VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers); };

    video_track_cb.event_cb = [&] (uint32_t track_id, EventType event_type,
        void *event_data, size_t event_data_size) { VideoTrackEventCb(track_id,
        event_type, event_data, event_data_size); };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id,
                                     video_track_param, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    CameraMetadata meta;
    ret = recorder_.GetCameraParam(camera_id_, meta);
    ASSERT_TRUE(ret == NO_ERROR);

    uint32_t select_exp_priority_vtag;
    uint32_t use_exp_priority_vtag;
    uint32_t exp_time_range_vtag;
    camera_metadata_entry_t entry;
    int64_t min_exp_time = 0, max_exp_time = 0;
    if (!VendorTagSupported(String8("select_priority"),
        String8("org.codeaurora.qcamera3.iso_exp_priority"),
        &select_exp_priority_vtag)) {
      TEST_WARN("%s: select_priority is not supported", __func__);
      ASSERT_TRUE(0);
    }
    if (!VendorTagSupported(String8("use_iso_exp_priority"),
        String8("org.codeaurora.qcamera3.iso_exp_priority"),
        &use_exp_priority_vtag)) {
      TEST_WARN("%s: use_iso_exp_priority is not supported", __func__);
      ASSERT_TRUE(0);
    }
    if (VendorTagExistsInMeta(meta, String8("exposure_time_range"),
          String8("org.codeaurora.qcamera3.iso_exp_priority"),
          &exp_time_range_vtag)) {
      entry = meta.find(exp_time_range_vtag);
      min_exp_time = entry.data.i64[0];
      max_exp_time = entry.data.i64[1];
      if (max_exp_time <= min_exp_time) {
        TEST_ERROR("%s: min_exp_time = %lld, max_exp_time = %lld, aborting.",
                   __func__, min_exp_time, max_exp_time);
        ASSERT_TRUE(0);
      }
      fprintf(stderr, "min_exp_time = %lld ns, max_exp_time = %lld ns\n",
              min_exp_time, max_exp_time);
    }

    // Setting tag to exposure time
    int32_t select_exp_priority = 1;
    ret = meta.update(select_exp_priority_vtag, &select_exp_priority, 1);

    int64_t exp_val = 0;
    int32_t expected_fps = 0;
    for (uint32_t count = 0; count < num_samples; count++) {
      exp_val = shutter_speed[count];
      // Frames-per-sec = {1 / (frame-time-in-ns / 10^9)}
      expected_fps = 1000000000 / exp_val;
      if ((exp_val >= min_exp_time) && (exp_val <= max_exp_time)) {
        ret = meta.update(use_exp_priority_vtag, &exp_val, 1);
        ASSERT_TRUE(ret == NO_ERROR);
        fprintf(stderr, "Applying Exposure time: %lld ns, "
                "expected: %d fps when applied..\n", exp_val, expected_fps);
        ret = recorder_.SetCameraParam(camera_id_, meta);
        ASSERT_TRUE(ret == NO_ERROR);
      } else {
        fprintf(stderr, "Holding on to previous Exposure time: %lld ns\n",
                exp_val);
      }
      sleep(record_dur/num_samples);
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
* SessionWithDualCam4KEncAllAWBModes: This case will test a Dual Cam session
*                 with 4096x2048 h264 encoded track, during which in regular
*                 intervals AWB modes will change.
* Note: camera_id_ for dual cam to be set using adb property.
* Api test sequence:
*  - StartCamera
*   loop Start {
*   ------------------
*   - CreateSession
*   - CreateVideoTrack
*   - StartVideoTrack
*   - StartSession
*   - Set AWB mode
*   - StopSession
*   - DeleteVideoTrack
*   - DeleteSession
*   ------------------
*   } loop End
*  - StopCamera
*/
TEST_F(RecorderPostprocessVideoGTest, SessionWithDualCam4KEncAllAWBModes) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  uint32_t record_dur = MAX(record_duration_, kAWBModeEnd * 10);

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  VideoFormat format_type = VideoFormat::kAVC;
  uint32_t stream_width  = 4096;
  uint32_t stream_height = 2048;

  ret = recorder_.StartCamera(camera_id_, camera_start_params_);
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
                                            stream_width, stream_height, 30};

    uint32_t video_track_id = 1;
    if (dump_bitstream_.IsEnabled()) {
      StreamDumpInfo dumpinfo = { video_track_param.format_type, video_track_id,
                                  session_id, stream_width, stream_height };
      ret = dump_bitstream_.SetUp(dumpinfo);
      ASSERT_TRUE(ret == NO_ERROR);
    }

    TrackCb video_track_cb;
    video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
                              std::vector<BufferDescriptor> buffers,
                              std::vector<MetaData> meta_buffers) {
    VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers); };

    video_track_cb.event_cb = [&] (uint32_t track_id, EventType event_type,
        void *event_data, size_t event_data_size) { VideoTrackEventCb(track_id,
        event_type, event_data, event_data_size); };

    ret = recorder_.CreateVideoTrack(session_id, video_track_id,
                                      video_track_param, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    CameraMetadata meta;
    ret = recorder_.GetCameraParam(camera_id_, meta);
    ASSERT_TRUE(ret == NO_ERROR);


    for (uint32_t count = kAWBModeOff; count < kAWBModeEnd; count++) {
      uint8_t awb_mode = count;
      ret = meta.update(ANDROID_CONTROL_AWB_MODE, &awb_mode, 1);
      ASSERT_TRUE(ret == NO_ERROR);
      fprintf(stderr, "AWB switched to mode[%d]\n", count);
      ret = recorder_.SetCameraParam(camera_id_, meta);
      ASSERT_TRUE(ret == NO_ERROR);
      sleep(record_dur/kAWBModeEnd);
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
* SessionWithDualCam4kEncCopy1080EncAndLinked1080YUV: This test will test
*                           Dual cam session with one 4kp Enc track,
*                           one Copy 1080 Enc Track and one linked.
*                           Note: camera_id_ for dual cam to be set using
*                           adb property.
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
TEST_F(RecorderPostprocessVideoGTest, SessionWithDualCam4kEncCopy1080EncAndLinked1080YUV) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.StartCamera(camera_id_, camera_start_params_);
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
      StreamDumpInfo dumpinfo1 = { VideoFormat::kAVC, video_track_id_4k_avc,
                                   session_id, 4096, 2048 };
      ret = dump_bitstream_.SetUp(dumpinfo1);
      ASSERT_TRUE(ret == NO_ERROR);

      StreamDumpInfo dumpinfo2 = { VideoFormat::kAVC, video_track_id_1080p_avc,
                                   session_id, 2160, 1080 };
      ret = dump_bitstream_.SetUp(dumpinfo2);
      ASSERT_TRUE(ret == NO_ERROR);
    }
    VideoTrackCreateParam video_track_param{ camera_id_, VideoFormat::kAVC,
                                            4096, 2048, 30 };
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

    video_track_param.width  = 2160;
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

    video_track_param.width   = 2160;
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
* SessionWithDualCam4kEncCopy1080EncAndLinked1080YUVWithTNR: This test will test
*                           Dual cam session with one 4kp Enc track,
*                           one Copy 1080 Enc Track and one linked.
*                           Note: camera_id_ for dual cam to be set using
*                           adb property.
* Api test sequence:
*  - StartCamera
*   loop Start {
*   ------------------
*   - CreateSession
*   - CreateVideoTrack - Master
*   - CreateVideoTrack - Copy
*   - CreateVideoTrack - Linked
*   - StartSession
*   - Enable TNR
*   - Disable TNR
*   - StopSession
*   - DeleteVideoTrack - Linked
*   - DeleteVideoTrack - Copy
*   - DeleteVideoTrack - Master
*   - DeleteSession
*   ------------------
*   } loop End
*  - StopCamera
*/
TEST_F(RecorderPostprocessVideoGTest, SessionWithDualCam4kEncCopy1080EncAndLinked1080YUVWithTNR) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.StartCamera(camera_id_, camera_start_params_);
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
      StreamDumpInfo dumpinfo1 = { VideoFormat::kAVC, video_track_id_4k_avc,
                                   session_id, 4096, 2048 };
      ret = dump_bitstream_.SetUp(dumpinfo1);
      ASSERT_TRUE(ret == NO_ERROR);

      StreamDumpInfo dumpinfo2 = { VideoFormat::kAVC, video_track_id_1080p_avc,
                                   session_id, 2160, 1080 };
      ret = dump_bitstream_.SetUp(dumpinfo2);
      ASSERT_TRUE(ret == NO_ERROR);
    }
    VideoTrackCreateParam video_track_param{camera_id_, VideoFormat::kAVC,
                                            4096, 2048, 30};
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

    video_track_param.width  = 2160;
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

    video_track_param.width   = 2160;
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

    sleep(record_duration_/3);

    //Enable TNR - High quality mode.
    CameraMetadata meta;
    ret= recorder_.GetCameraParam(camera_id_, meta);
    if (NO_ERROR == ret) {
      if (meta.exists(ANDROID_NOISE_REDUCTION_MODE)) {
        const uint8_t tnr_mode = ANDROID_NOISE_REDUCTION_MODE_HIGH_QUALITY;
        TEST_INFO("%s Enable TNR mode(%d)", __func__, tnr_mode);
        meta.update(ANDROID_NOISE_REDUCTION_MODE, &tnr_mode, 1);
        ret= recorder_.SetCameraParam(camera_id_, meta);
        ASSERT_TRUE(ret == NO_ERROR);
      }
    }

    sleep(record_duration_/3);

    //Enable TNR - High quality mode.
    ret = recorder_.GetCameraParam(camera_id_, meta);
    if (NO_ERROR == ret) {
      if (meta.exists(ANDROID_NOISE_REDUCTION_MODE)) {
        const uint8_t tnr_mode = ANDROID_NOISE_REDUCTION_MODE_OFF;
        TEST_INFO("%s Enable TNR mode(%d)", __func__, tnr_mode);
        meta.update(ANDROID_NOISE_REDUCTION_MODE, &tnr_mode, 1);
        ret= recorder_.SetCameraParam(camera_id_, meta);
        ASSERT_TRUE(ret == NO_ERROR);
      }
    }

    sleep(record_duration_/3);

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
* SessionWithSingleCam4kEncCopy1080EncAndCopy720YUV: This test will test
*                                          single camera session with
*                                          one 4kp Enc track, one Copy 1080p Enc
                                           Track and one 720p Copy YUV.
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
*   - DeleteVideoTrack - Copy
*   - DeleteVideoTrack - Copy
*   - DeleteVideoTrack - Master
*   - DeleteSession
*   ------------------
*   } loop End
*  - StopCamera
*/
TEST_F(RecorderPostprocessVideoGTest, SessionWithSingleCam4kEncCopy1080EncAndCopy720YUV) {
  fprintf(stderr, "\n---------- Run Test %s.%s ------------\n",
          test_info_->test_case_name(), test_info_->name());
  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);
  uint32_t track1_width  = 3840;
  uint32_t track1_height = 2160;
  uint32_t track2_width  = 1920;
  uint32_t track2_height = 1080;
  uint32_t track3_width  = 1280;
  uint32_t track3_height = 720;

  ret = recorder_.StartCamera(camera_id_, camera_start_params_);
  ASSERT_TRUE(ret == NO_ERROR);

  uint32_t video_track_id_4k_avc = 1;
  uint32_t video_track_id_1080p_avc = 2;
  uint32_t video_track_id_720p_yuv = 3;

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
      StreamDumpInfo dumpinfo1 = { VideoFormat::kAVC, video_track_id_4k_avc,
                                  session_id, track1_width, track1_height };
      ret = dump_bitstream_.SetUp(dumpinfo1);
      ASSERT_TRUE(ret == NO_ERROR);

      StreamDumpInfo dumpinfo2 = { VideoFormat::kAVC, video_track_id_1080p_avc,
                                  session_id, track2_width, track2_height };
      ret = dump_bitstream_.SetUp(dumpinfo2);
      ASSERT_TRUE(ret == NO_ERROR);
    }
    VideoTrackCreateParam video_track_param{camera_id_, VideoFormat::kAVC,
                                            track1_width, track1_height, 30};
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

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_4k_avc,
                                     video_track_param, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    std::vector<uint32_t> track_ids;
    track_ids.push_back(video_track_id_4k_avc);

    VideoExtraParam extra_param;
    SourceVideoTrack surface_video_copy;
    surface_video_copy.source_track_id = video_track_id_4k_avc;
    extra_param.Update(QMMF_SOURCE_VIDEO_TRACK_ID, surface_video_copy);

    video_track_param.width = track2_width;
    video_track_param.height = track2_height;

    video_track_cb.data_cb = [&, session_id](
        uint32_t track_id, std::vector<BufferDescriptor> buffers,
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

    video_track_param.width = track3_width;
    video_track_param.height = track3_height;
    video_track_param.format_type = VideoFormat::kYUV;

    video_track_cb.data_cb = [&, session_id](
        uint32_t track_id, std::vector<BufferDescriptor> buffers,
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
* SessionWithDualCam4kEncCopy1080EncAndCopy720YUV: This test will test
*                                          Dual Cam session with
*                                          one 4kp Enc track, one Copy 1080p Enc
                                           Track and one 720p Copy YUV.
* Note: camera_id_ for dual cam to be set using adb property.
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
*   - DeleteVideoTrack - Copy
*   - DeleteVideoTrack - Copy
*   - DeleteVideoTrack - Master
*   - DeleteSession
*   ------------------
*   } loop End
*  - StopCamera
*/
TEST_F(RecorderPostprocessVideoGTest, SessionWithDualCam4kEncCopy1080EncAndCopy720YUV) {
  fprintf(stderr, "\n---------- Run Test %s.%s ------------\n",
          test_info_->test_case_name(), test_info_->name());
  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);
  uint32_t track1_width  = 4096;
  uint32_t track1_height = 2048;
  uint32_t track2_width  = 2160;
  uint32_t track2_height = 1080;
  uint32_t track3_width  = 1440;
  uint32_t track3_height = 720;

  ret = recorder_.StartCamera(camera_id_, camera_start_params_);
  ASSERT_TRUE(ret == NO_ERROR);

  uint32_t video_track_id_4k_avc = 1;
  uint32_t video_track_id_1080p_avc = 2;
  uint32_t video_track_id_720p_yuv = 3;

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
      StreamDumpInfo dumpinfo1 = { VideoFormat::kAVC, video_track_id_4k_avc,
                                  session_id, track1_width, track1_height };
      ret = dump_bitstream_.SetUp(dumpinfo1);
      ASSERT_TRUE(ret == NO_ERROR);

      StreamDumpInfo dumpinfo2 = { VideoFormat::kAVC, video_track_id_1080p_avc,
                                  session_id, track2_width, track2_height };
      ret = dump_bitstream_.SetUp(dumpinfo2);
      ASSERT_TRUE(ret == NO_ERROR);
    }
    VideoTrackCreateParam video_track_param{camera_id_, VideoFormat::kAVC,
                                            track1_width, track1_height, 30};
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

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_4k_avc,
                                     video_track_param, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    std::vector<uint32_t> track_ids;
    track_ids.push_back(video_track_id_4k_avc);

    VideoExtraParam extra_param;
    SourceVideoTrack surface_video_copy;
    surface_video_copy.source_track_id = video_track_id_4k_avc;
    extra_param.Update(QMMF_SOURCE_VIDEO_TRACK_ID, surface_video_copy);

    video_track_param.width = track2_width;
    video_track_param.height = track2_height;

    video_track_cb.data_cb = [&, session_id](
        uint32_t track_id, std::vector<BufferDescriptor> buffers,
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

    video_track_param.width = track3_width;
    video_track_param.height = track3_height;
    video_track_param.format_type = VideoFormat::kYUV;

    video_track_cb.data_cb = [&, session_id](
        uint32_t track_id, std::vector<BufferDescriptor> buffers,
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
* SessionWithDualCam4kEncCopy1080EncAndCopy720YUVWithTNR: This test will test
*                                          Dual Cam session with
*                                          one 4kp Enc track, one Copy 1080p Enc
                                           Track and one 720p Copy YUV.
* Note: camera_id_ for dual cam to be set using adb property.
* Api test sequence:
*  - StartCamera
*   loop Start {
*   ------------------
*   - CreateSession
*   - CreateVideoTrack - Master
*   - CreateVideoTrack - Copy
*   - CreateVideoTrack - Copy
*   - StartSession
*   - Enable TNR
*   - Disable TNR
*   - StopSession
*   - DeleteVideoTrack - Copy
*   - DeleteVideoTrack - Copy
*   - DeleteVideoTrack - Master
*   - DeleteSession
*   ------------------
*   } loop End
*  - StopCamera
*/
TEST_F(RecorderPostprocessVideoGTest, SessionWithDualCam4kEncCopy1080EncAndCopy720YUVWithTNR) {
  fprintf(stderr, "\n---------- Run Test %s.%s ------------\n",
          test_info_->test_case_name(), test_info_->name());
  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);
  uint32_t track1_width  = 4096;
  uint32_t track1_height = 2048;
  uint32_t track2_width  = 2160;
  uint32_t track2_height = 1080;
  uint32_t track3_width  = 1440;
  uint32_t track3_height = 720;

  ret = recorder_.StartCamera(camera_id_, camera_start_params_);
  ASSERT_TRUE(ret == NO_ERROR);

  uint32_t video_track_id_4k_avc = 1;
  uint32_t video_track_id_1080p_avc = 2;
  uint32_t video_track_id_720p_yuv = 3;

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
      StreamDumpInfo dumpinfo1 = { VideoFormat::kAVC, video_track_id_4k_avc,
                                  session_id, track1_width, track1_height };
      ret = dump_bitstream_.SetUp(dumpinfo1);
      ASSERT_TRUE(ret == NO_ERROR);

      StreamDumpInfo dumpinfo2 = { VideoFormat::kAVC, video_track_id_1080p_avc,
                                  session_id, track2_width, track2_height };
      ret = dump_bitstream_.SetUp(dumpinfo2);
      ASSERT_TRUE(ret == NO_ERROR);
    }
    VideoTrackCreateParam video_track_param{camera_id_, VideoFormat::kAVC,
                                            track1_width, track1_height, 30};
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

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_4k_avc,
                                     video_track_param, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    std::vector<uint32_t> track_ids;
    track_ids.push_back(video_track_id_4k_avc);

    VideoExtraParam extra_param;
    SourceVideoTrack surface_video_copy;
    surface_video_copy.source_track_id = video_track_id_4k_avc;
    extra_param.Update(QMMF_SOURCE_VIDEO_TRACK_ID, surface_video_copy);

    video_track_param.width = track2_width;
    video_track_param.height = track2_height;

    video_track_cb.data_cb = [&, session_id](
        uint32_t track_id, std::vector<BufferDescriptor> buffers,
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

    video_track_param.width = track3_width;
    video_track_param.height = track3_height;
    video_track_param.format_type = VideoFormat::kYUV;

    video_track_cb.data_cb = [&, session_id](
        uint32_t track_id, std::vector<BufferDescriptor> buffers,
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

    sleep(record_duration_/3);

    //Enable TNR - High quality mode.
    CameraMetadata meta;
    ret = recorder_.GetCameraParam(camera_id_, meta);
    if (NO_ERROR == ret) {
      if (meta.exists(ANDROID_NOISE_REDUCTION_MODE)) {
        const uint8_t tnr_mode = ANDROID_NOISE_REDUCTION_MODE_HIGH_QUALITY;
        TEST_INFO("%s Enable TNR mode(%d)", __func__, tnr_mode);
        meta.update(ANDROID_NOISE_REDUCTION_MODE, &tnr_mode, 1);
        ret = recorder_.SetCameraParam(camera_id_, meta);
        ASSERT_TRUE(ret == NO_ERROR);
      }
    }

    sleep(record_duration_/3);

    //Enable TNR - OFF.
    ret = recorder_.GetCameraParam(camera_id_, meta);
    if (NO_ERROR == ret) {
      if (meta.exists(ANDROID_NOISE_REDUCTION_MODE)) {
        const uint8_t tnr_mode = ANDROID_NOISE_REDUCTION_MODE_OFF;
        TEST_INFO("%s Enable TNR mode(%d)", __func__, tnr_mode);
        meta.update(ANDROID_NOISE_REDUCTION_MODE, &tnr_mode, 1);
        ret = recorder_.SetCameraParam(camera_id_, meta);
        ASSERT_TRUE(ret == NO_ERROR);
      }
    }

    sleep(record_duration_/3);

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
* SessionsWith4KEncTrackZZHDR: This test will verify multiple sessions with
* Camera 4K tracks. Only for hardware supporting zzHDR
* Api test sequence:
*  - StartCamera
*   - CreateSession
*   - Add ExtraParam for zzHDR
*   - CreateVideoTrack
*   - StartSession
*   - StopSession
*   - DeleteVideoTrack
*   - DeleteSession
*  - StopCamera
*/
TEST_F(RecorderPostprocessVideoGTest, SessionsWith4KEncTrackZZHDR) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  VideoFormat format_type = VideoFormat::kAVC;
  uint32_t width  = 4096;
  uint32_t height = 2048;
  ret = recorder_.StartCamera(camera_id_, camera_start_params_);
  ASSERT_TRUE(ret == NO_ERROR);

  SessionCb session_status_cb = CreateSessionStatusCb();
  uint32_t session_id;
  ret = recorder_.CreateSession(session_status_cb, &session_id);
  ASSERT_TRUE(session_id > 0);
  ASSERT_TRUE(ret == NO_ERROR);
  VideoTrackCreateParam video_track_param{camera_id_, format_type, width,
                                          height, 30};
  uint32_t video_track_id = 1;

  // Setting Enable HDR Extra Param
  VideoExtraParam extra_param;
  VideoHDRMode vid_hdr_mode;
  vid_hdr_mode.enable = true;
  extra_param.Update(QMMF_VIDEO_HDR_MODE, vid_hdr_mode);

  if (dump_bitstream_.IsEnabled()) {
    StreamDumpInfo dumpinfo = { format_type, video_track_id, session_id,
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
                                   video_track_param, extra_param,
                                   video_track_cb);
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
* SessionWithDualCam4k30Enc1080p30EncAndLinked1080p30YUVWithTNRAndZZHDR: This
*                           test will test Dual cam session with one 4kp Enc
*                           track, one 1080 Enc Track and one linked.
*                           Note: camera_id_ for dual cam to be set using
*                           adb property.
* Api test sequence:
*  - StartCamera
*   loop Start {
*   ------------------
*   - CreateSession
*   - CreateVideoTrack with zzHDR
*   - CreateVideoTrack - Master
*   - CreateVideoTrack - Linked
*   - StartSession
*   - Enable TNR
*   - Enable Overlay
*   - Disable Overlay
*   - Disable TNR
*   - StopSession
*   - DeleteVideoTrack - Linked
*   - DeleteVideoTrack - Master
*   - DeleteVideoTrack
*   - DeleteSession
*   ------------------
*   } loop End
*  - StopCamera
*/
TEST_F(RecorderPostprocessVideoGTest, SessionWithDualCam4k30Enc1080p30EncAndLinked1080p30YUVWithTNRAndZZHDR) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.StartCamera(camera_id_, camera_start_params_);
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
      StreamDumpInfo dumpinfo1 = { VideoFormat::kAVC, video_track_id_4k_avc,
                                   session_id, 4096, 2048 };
      ret = dump_bitstream_.SetUp(dumpinfo1);
      ASSERT_TRUE(ret == NO_ERROR);

      StreamDumpInfo dumpinfo2 = { VideoFormat::kAVC, video_track_id_1080p_avc,
                                   session_id, 2160, 1080 };
      ret = dump_bitstream_.SetUp(dumpinfo2);
      ASSERT_TRUE(ret == NO_ERROR);
    }

    VideoTrackCreateParam video_track_param{camera_id_, VideoFormat::kAVC,
                                            4096, 2048, 30};
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

    // Setting Enable HDR Extra Param
    VideoExtraParam extra_param_hdr;
    VideoHDRMode vid_hdr_mode;
    vid_hdr_mode.enable = true;
    extra_param_hdr.Update(QMMF_VIDEO_HDR_MODE, vid_hdr_mode);

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_4k_avc,
                                     video_track_param, extra_param_hdr,
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

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_1080p_avc,
                                     video_track_param, extra_param_hdr,
                                     video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    track_ids.push_back(video_track_id_1080p_avc);

    video_track_param.format_type = VideoFormat::kYUV;

    video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
        std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
          VideoTrackYUVDataCb(session_id, track_id, buffers, meta_buffers); };

    VideoExtraParam extra_param;
    SourceVideoTrack surface_video_linked;
    surface_video_linked.source_track_id = video_track_id_1080p_avc;
    extra_param.Update(QMMF_SOURCE_VIDEO_TRACK_ID, surface_video_linked);

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_1080p_yuv,
                                     video_track_param, extra_param,
                                     video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    track_ids.push_back(video_track_id_1080p_yuv);
    sessions_.insert(std::make_pair(session_id, track_ids));

    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    //Enable TNR - High quality mode.
    CameraMetadata meta;
    ret= recorder_.GetCameraParam(camera_id_, meta);
    if (NO_ERROR == ret) {
      if (meta.exists(ANDROID_NOISE_REDUCTION_MODE)) {
        const uint8_t tnr_mode = ANDROID_NOISE_REDUCTION_MODE_HIGH_QUALITY;
        TEST_INFO("%s Enable TNR mode(%d)", __func__, tnr_mode);
        meta.update(ANDROID_NOISE_REDUCTION_MODE, &tnr_mode, 1);
        ret= recorder_.SetCameraParam(camera_id_, meta);
        ASSERT_TRUE(ret == NO_ERROR);
      }
    }

    // Enable overlay
    uint32_t mask_id_4k ;
    CreatePrivacyMaskOverlay(video_track_id_4k_avc, 4096, 2048, &mask_id_4k);

    sleep(record_duration_);

    DestroyPrivacyMaskOverlay(video_track_id_4k_avc, mask_id_4k);

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
* SessionWithDualCam4k60Enc1080p30EncAndLinked1080p30YUVWithTNRAndZZHDR: This test will test
*                           Dual cam session with one 4kp Enc track,
*                           one 1080 Enc Track and one linked.
*                           Note: camera_id_ for dual cam to be set using
*                           adb property.
* Api test sequence:
*  - StartCamera
*   loop Start {
*   ------------------
*   - CreateSession
*   - CreateVideoTrack with zzHDR
*   - CreateVideoTrack - Master
*   - CreateVideoTrack - Linked
*   - StartSession
*   - Enable TNR
*   - Enable Overlay
*   - Disable Overlay
*   - Disable TNR
*   - StopSession
*   - DeleteVideoTrack - Linked
*   - DeleteVideoTrack - Master
*   - DeleteVideoTrack
*   - DeleteSession
*   ------------------
*   } loop End
*  - StopCamera
*/
TEST_F(RecorderPostprocessVideoGTest, SessionWithDualCam4k60Enc1080p30EncAndLinked1080p30YUVWithTNRAndZZHDR) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.StartCamera(camera_id_, camera_start_params_);
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
                                   video_track_id_4k_avc, 4096, 2048 };
      ret = dump_bitstream_.SetUp(dumpinfo1);
      ASSERT_TRUE(ret == NO_ERROR);

      StreamDumpInfo dumpinfo2 = { VideoFormat::kAVC, session_id,
                                   video_track_id_1080p_avc, 2160, 1080 };
      ret = dump_bitstream_.SetUp(dumpinfo2);
      ASSERT_TRUE(ret == NO_ERROR);
    }
    VideoTrackCreateParam video_track_param{camera_id_, VideoFormat::kAVC,
                                            4096, 2048, 60};
    video_track_param.codec_param.avc.bitrate = 160000000;
    video_track_param.codec_param.avc.ratecontrol_type =
      VideoRateControlType::kVariable;

    TrackCb video_track_cb;
    video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
        std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
          VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
        };

    video_track_cb.event_cb = [&] (uint32_t track_id, EventType event_type,
        void *event_data, size_t event_data_size) { VideoTrackEventCb(track_id,
        event_type, event_data, event_data_size); };

    // Setting Enable HDR Extra Param
    VideoExtraParam extra_param_hdr;
    VideoHDRMode vid_hdr_mode;
    vid_hdr_mode.enable = true;
    extra_param_hdr.Update(QMMF_VIDEO_HDR_MODE, vid_hdr_mode);

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_4k_avc,
                                     video_track_param, extra_param_hdr,
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

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_1080p_avc,
                                     video_track_param, extra_param_hdr,
                                     video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    track_ids.push_back(video_track_id_1080p_avc);

    video_track_param.format_type = VideoFormat::kYUV;

    video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
        std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
          VideoTrackYUVDataCb(session_id, track_id, buffers, meta_buffers); };

    VideoExtraParam extra_param;
    SourceVideoTrack surface_video_linked;
    surface_video_linked.source_track_id = video_track_id_1080p_avc;
    extra_param.Update(QMMF_SOURCE_VIDEO_TRACK_ID, surface_video_linked);

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_1080p_yuv,
                                     video_track_param, extra_param,
                                     video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    track_ids.push_back(video_track_id_1080p_yuv);
    sessions_.insert(std::make_pair(session_id, track_ids));

    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    //Enable TNR - High quality mode.
    CameraMetadata meta;
    auto ret= recorder_.GetCameraParam(camera_id_, meta);
    if (NO_ERROR == ret) {
      if (meta.exists(ANDROID_NOISE_REDUCTION_MODE)) {
        const uint8_t tnr_mode = ANDROID_NOISE_REDUCTION_MODE_HIGH_QUALITY;
        TEST_INFO("%s Enable TNR mode(%d)", __func__, tnr_mode);
        meta.update(ANDROID_NOISE_REDUCTION_MODE, &tnr_mode, 1);
        ret= recorder_.SetCameraParam(camera_id_, meta);
        ASSERT_TRUE(ret == NO_ERROR);
      }
    }

    // Enable overlay
    uint32_t mask_id_4k;
    CreatePrivacyMaskOverlay(video_track_id_4k_avc, 4096, 2048, &mask_id_4k);

    sleep(record_duration_);

    DestroyPrivacyMaskOverlay(video_track_id_4k_avc, mask_id_4k);

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
* SessionWithDualCam5_7k30Enc1080p30EncAndLinked1080p30YUVWithTNRAndZZHDR:
*                           This test will test Dual cam session with one
*                           5.7k Enc track, one 1080 Enc Track and one linked.
*                           Note: camera_id_ for dual cam to be set using
*                           adb property.
* Api test sequence:
*  - StartCamera
*   loop Start {
*   ------------------
*   - CreateSession
*   - CreateVideoTrack with zzHDR
*   - CreateVideoTrack - Master
*   - CreateVideoTrack - Linked
*   - StartSession
*   - Enable TNR
*   - Enable Overlay
*   - Disable Overlay
*   - Disable TNR
*   - StopSession
*   - DeleteVideoTrack - Linked
*   - DeleteVideoTrack - Master
*   - DeleteVideoTrack
*   - DeleteSession
*   ------------------
*   } loop End
*  - StopCamera
*/
TEST_F(RecorderPostprocessVideoGTest, SessionWithDualCam5_7k30Enc1080p30EncAndLinked1080p30YUVWithTNRAndZZHDR) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.StartCamera(camera_id_, camera_start_params_);
  ASSERT_TRUE(ret == NO_ERROR);

  uint32_t video_track_id_5_7k_avc     = 1;
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
      StreamDumpInfo dumpinfo1 = { VideoFormat::kAVC, video_track_id_5_7k_avc,
                                   session_id, 5760, 2880 };
      ret = dump_bitstream_.SetUp(dumpinfo1);
      ASSERT_TRUE(ret == NO_ERROR);

      StreamDumpInfo dumpinfo2 = { VideoFormat::kAVC, video_track_id_1080p_avc,
                                   session_id, 2160, 1080 };
      ret = dump_bitstream_.SetUp(dumpinfo2);
      ASSERT_TRUE(ret == NO_ERROR);
    }
    VideoTrackCreateParam video_track_param{camera_id_, VideoFormat::kAVC,
                                            5760, 2880, 30};
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

    // Setting Enable HDR Extra Param
    VideoExtraParam extra_param_hdr;
    VideoHDRMode vid_hdr_mode;
    vid_hdr_mode.enable = true;
    extra_param_hdr.Update(QMMF_VIDEO_HDR_MODE, vid_hdr_mode);

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_5_7k_avc,
                                     video_track_param, extra_param_hdr,
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

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_1080p_avc,
                                     video_track_param, extra_param_hdr,
                                     video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    track_ids.push_back(video_track_id_1080p_avc);

    video_track_param.format_type = VideoFormat::kYUV;

    video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
        std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
          VideoTrackYUVDataCb(session_id, track_id, buffers, meta_buffers); };

    VideoExtraParam extra_param;
    SourceVideoTrack surface_video_linked;
    surface_video_linked.source_track_id = video_track_id_1080p_avc;
    extra_param.Update(QMMF_SOURCE_VIDEO_TRACK_ID, surface_video_linked);

    ret = recorder_.CreateVideoTrack(session_id, video_track_id_1080p_yuv,
                                     video_track_param, extra_param,
                                     video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    track_ids.push_back(video_track_id_1080p_yuv);
    sessions_.insert(std::make_pair(session_id, track_ids));

    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    //Enable TNR - High quality mode.
    CameraMetadata meta;
    auto ret= recorder_.GetCameraParam(camera_id_, meta);
    if (NO_ERROR == ret) {
      if (meta.exists(ANDROID_NOISE_REDUCTION_MODE)) {
        const uint8_t tnr_mode = ANDROID_NOISE_REDUCTION_MODE_HIGH_QUALITY;
        TEST_INFO("%s Enable TNR mode(%d)", __func__, tnr_mode);
        meta.update(ANDROID_NOISE_REDUCTION_MODE, &tnr_mode, 1);
        ret= recorder_.SetCameraParam(camera_id_, meta);
        ASSERT_TRUE(ret == NO_ERROR);
      }
    }

    // Enable overlay
    uint32_t mask_id_4k ;
    CreatePrivacyMaskOverlay(video_track_id_5_7k_avc, 5760, 2880, &mask_id_4k);

    sleep(record_duration_);

    DestroyPrivacyMaskOverlay(video_track_id_5_7k_avc, mask_id_4k);

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
#endif
