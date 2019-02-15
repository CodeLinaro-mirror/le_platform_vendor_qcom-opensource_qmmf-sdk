/*
* Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
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
      StreamDumpInfo dumpinfo = { format_type, session_id, video_track_id,
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
      StreamDumpInfo dumpinfo = { format_type, session_id, video_track_id_4k,
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
      StreamDumpInfo dumpinfo = { format_type, session_id, video_track_id_4k,
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
* SessionWith1080pEncLinked480pEncAndLinked480pYUVTrackWithTimelapse:
*     One session with EIS, TNR, 1440p 30fps master AVC track, 480p 30fps
*     linked AVC track and 480p 30fps linked YUV track.
*
* Api test sequence:
*  - StartCamera
*   loop Start {
*   ------------------
*   - CreateSession
*   - CreateVideoTrack - 1920x1080 @30 AVC - Master
*   - CreateVideoTrack - 848x480   @30 AVC - Linked
*   - CreateVideoTrack - 848x480   @30 YUV - Linked
*   - StartDisplay
*   - StartSession
*   - StopSession
*   - StopDisplay
*   - DeleteVideoTrack - 848x480   @30 YUV - Linked
*   - DeleteVideoTrack - 848x480   @30 AVC - Linked
*   - DeleteVideoTrack - 1920x1080 @30 AVC - Master
*   - DeleteSession
*   ------------------
*   } loop End
*  - StopCamera
*/
TEST_F(RecorderPostprocessVideoGTest,
    SessionWith1080pEncLinked480pEncAndLinked480pYUVTrackWithTimelapse) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.StartCamera(camera_id_, camera_start_params_);
  ASSERT_TRUE(ret == NO_ERROR);

  uint32_t track_id_1080p_avc = 1;
  uint32_t track_id_480p_avc  = 2;
  uint32_t track_id_480p_yuv  = 3;

  SessionCb session_status_cb = CreateSessionStatusCb();
  uint32_t session_id;
  ret = recorder_.CreateSession(session_status_cb, &session_id);
  ASSERT_TRUE(session_id > 0);
  ASSERT_TRUE(ret == NO_ERROR);

  FrameTrace track_trace(is_frame_debug_enabled_);

  if (dump_bitstream_.IsEnabled()) {
    StreamDumpInfo dump1080p = {
      VideoFormat::kAVC, session_id, track_id_1080p_avc, 1920, 1440
    };
    ret = dump_bitstream_.SetUp(dump1080p);
    ASSERT_TRUE(ret == NO_ERROR);

    StreamDumpInfo dump480p = {
      VideoFormat::kAVC, session_id, track_id_480p_avc, 848, 480
    };
    ret = dump_bitstream_.SetUp(dump480p);
    ASSERT_TRUE(ret == NO_ERROR);
  }

  TrackCb track_cb;
  track_cb.event_cb =
      [this] (uint32_t track_id, EventType type, void *data, size_t size)
      { VideoTrackEventCb(track_id, type, data, size); };

  for (uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
        test_info_->name(), i);

    VideoExtraParam extraparam;
    SourceVideoTrack source_video;
    VideoTimeLapse timelapse;

    /************************ 1080p @4 AVC ***********************************/

    VideoTrackCreateParam videoparam = { camera_id_, VideoFormat::kAVC, 1920, 1080, 4 };

    timelapse.time_interval = 33; //ms
    extraparam.Update(QMMF_VIDEO_TIMELAPSE_INTERVAL, timelapse);

    track_cb.data_cb = [&, session_id] (uint32_t track_id,
        std::vector<BufferDescriptor> buffers, std::vector<MetaData> meta) {
      VideoTrackEncDataCb(session_id, track_id, buffers, meta);
      track_trace.BufferAvailableCb(buffers.at(0));
    };

    ret = recorder_.CreateVideoTrack(session_id, track_id_1080p_avc,
                                     videoparam, extraparam, track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    extraparam.Clear();

    /************************ Linked 480p @4 AVC *****************************/

    videoparam = { camera_id_, VideoFormat::kAVC, 848, 480, 4 };

    source_video.source_track_id = track_id_1080p_avc;
    extraparam.Update(QMMF_SOURCE_VIDEO_TRACK_ID, source_video);

    timelapse.time_interval = 33; //ms
    extraparam.Update(QMMF_VIDEO_TIMELAPSE_INTERVAL, timelapse);

    track_cb.data_cb = [&, session_id] (uint32_t track_id,
        std::vector<BufferDescriptor> buffers, std::vector<MetaData> meta) {
      VideoTrackEncDataCb(session_id, track_id, buffers, meta);
    };

    ret = recorder_.CreateVideoTrack(session_id, track_id_480p_avc,
                                     videoparam, extraparam, track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    extraparam.Clear();

    /************************ Linked 480p @4 YUV *****************************/

    videoparam = { camera_id_, VideoFormat::kYUV, 848, 480, 4 };

    source_video.source_track_id = track_id_480p_avc;
    extraparam.Update(QMMF_SOURCE_VIDEO_TRACK_ID, source_video);

    track_cb.data_cb = [&, session_id] (uint32_t track_id,
        std::vector<BufferDescriptor> buffers, std::vector<MetaData> meta) {
      VideoTrackYUVDataCb(session_id, track_id, buffers, meta);
    };

    ret = recorder_.CreateVideoTrack(session_id, track_id_480p_yuv,
                                     videoparam, extraparam, track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    std::vector<uint32_t> track_ids;
    track_ids.push_back(track_id_1080p_avc);
    track_ids.push_back(track_id_480p_avc);
    track_ids.push_back(track_id_480p_yuv);
    sessions_.insert(std::make_pair(session_id, track_ids));
    extraparam.Clear();

    /************************ Set Video Parameters ***************************/

    CodecParamType type;
    type = CodecParamType::kFrameRateType;
    float fps = 1.0 / timelapse_interval_;

    ret = recorder_.SetVideoTrackParam(session_id, 1, type, &fps , sizeof(fps));
    ASSERT_TRUE(ret == NO_ERROR);

    /*********************** Start Recording  ********************************/

#ifndef DISABLE_DISPLAY
    if (use_display_) {
      ret = StartDisplay(DisplayType::kPrimary, 848, 480, 480, 360);
      if (ret != 0) {
        TEST_ERROR("%s: StartDisplay Failed!!", __func__);
      }
    }
#endif

    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    // Let session run for time record_duration_, during this time buffer with
    // valid data would be received in track callback (VideoTrackYUVDataCb).
    sleep(record_duration_);


    /*********************** Tear down  **************************************/

    ret = recorder_.StopSession(session_id, false);
    ASSERT_TRUE(ret == NO_ERROR);

#ifndef DISABLE_DISPLAY
    if (use_display_) {
      ret = StopDisplay(DisplayType::kPrimary);
      ASSERT_TRUE(ret == NO_ERROR);
    }
#endif

    ret = recorder_.DeleteVideoTrack(session_id, track_id_480p_yuv);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, track_id_480p_avc);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, track_id_1080p_avc);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    track_trace.Reset();

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
* SessionWith1440pEncLinked480pEncAndLinked480pYUVTrackWithTimelapse:
*     One session with EIS, TNR, 1440p 30fps master AVC track, 480p 30fps
*     linked AVC track and 480p 30fps linked YUV track.
*
* Api test sequence:
*  - StartCamera
*   loop Start {
*   ------------------
*   - CreateSession
*   - CreateVideoTrack - 1920x1440 @30 AVC - Master
*   - CreateVideoTrack - 848x480   @30 AVC - Linked
*   - CreateVideoTrack - 848x480   @30 YUV - Linked
*   - StartDisplay
*   - StartSession
*   - StopSession
*   - StopDisplay
*   - DeleteVideoTrack - 848x480   @30 YUV - Linked
*   - DeleteVideoTrack - 848x480   @30 AVC - Linked
*   - DeleteVideoTrack - 1920x1440 @30 AVC - Master
*   - DeleteSession
*   ------------------
*   } loop End
*  - StopCamera
*/
TEST_F(RecorderPostprocessVideoGTest,
    SessionWith1440pEncLinked480pEncAndLinked480pYUVTrackWithTimelapse) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.StartCamera(camera_id_, camera_start_params_);
  ASSERT_TRUE(ret == NO_ERROR);

  uint32_t track_id_1440p_avc = 1;
  uint32_t track_id_480p_avc  = 2;
  uint32_t track_id_480p_yuv  = 3;

  SessionCb session_status_cb = CreateSessionStatusCb();
  uint32_t session_id;
  ret = recorder_.CreateSession(session_status_cb, &session_id);
  ASSERT_TRUE(session_id > 0);
  ASSERT_TRUE(ret == NO_ERROR);

  FrameTrace track_trace(is_frame_debug_enabled_);

  if (dump_bitstream_.IsEnabled()) {
    StreamDumpInfo dump1440p = {
      VideoFormat::kAVC, session_id, track_id_1440p_avc, 1920, 1440
    };
    ret = dump_bitstream_.SetUp(dump1440p);
    ASSERT_TRUE(ret == NO_ERROR);

    StreamDumpInfo dump480p = {
      VideoFormat::kAVC, session_id, track_id_480p_avc, 848, 480
    };
    ret = dump_bitstream_.SetUp(dump480p);
    ASSERT_TRUE(ret == NO_ERROR);
  }

  TrackCb track_cb;
  track_cb.event_cb =
      [this] (uint32_t track_id, EventType type, void *data, size_t size)
      { VideoTrackEventCb(track_id, type, data, size); };

  for (uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
        test_info_->name(), i);

    VideoExtraParam extraparam;
    SourceVideoTrack source_video;
    VideoTimeLapse timelapse;

    /************************ 1440p @4 AVC ***********************************/

    VideoTrackCreateParam videoparam = { camera_id_, VideoFormat::kAVC, 1920, 1440, 4 };

    timelapse.time_interval = 33; //ms
    extraparam.Update(QMMF_VIDEO_TIMELAPSE_INTERVAL, timelapse);

    track_cb.data_cb = [&, session_id] (uint32_t track_id,
        std::vector<BufferDescriptor> buffers, std::vector<MetaData> meta) {
      VideoTrackEncDataCb(session_id, track_id, buffers, meta);
      track_trace.BufferAvailableCb(buffers.at(0));
    };

    ret = recorder_.CreateVideoTrack(session_id, track_id_1440p_avc,
                                     videoparam, extraparam, track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    extraparam.Clear();


    /************************ Linked 480p @4 AVC *****************************/

    videoparam = { camera_id_, VideoFormat::kAVC, 848, 480, 4 };

    source_video.source_track_id = track_id_1440p_avc;
    extraparam.Update(QMMF_SOURCE_VIDEO_TRACK_ID, source_video);

    timelapse.time_interval = 33; //ms
    extraparam.Update(QMMF_VIDEO_TIMELAPSE_INTERVAL, timelapse);

    track_cb.data_cb = [&, session_id] (uint32_t track_id,
        std::vector<BufferDescriptor> buffers, std::vector<MetaData> meta) {
      VideoTrackEncDataCb(session_id, track_id, buffers, meta);
    };

    ret = recorder_.CreateVideoTrack(session_id, track_id_480p_avc,
                                     videoparam, extraparam, track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    extraparam.Clear();


    /************************ Linked 480p @4 YUV *****************************/

    videoparam = { camera_id_, VideoFormat::kYUV, 848, 480, 4 };

    source_video.source_track_id = track_id_480p_avc;
    extraparam.Update(QMMF_SOURCE_VIDEO_TRACK_ID, source_video);

    track_cb.data_cb = [&, session_id] (uint32_t track_id,
        std::vector<BufferDescriptor> buffers, std::vector<MetaData> meta) {
      VideoTrackYUVDataCb(session_id, track_id, buffers, meta);
    };

    ret = recorder_.CreateVideoTrack(session_id, track_id_480p_yuv,
                                     videoparam, extraparam, track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    std::vector<uint32_t> track_ids;
    track_ids.push_back(track_id_1440p_avc);
    track_ids.push_back(track_id_480p_avc);
    track_ids.push_back(track_id_480p_yuv);
    sessions_.insert(std::make_pair(session_id, track_ids));
    extraparam.Clear();

    /************************ Set Video Parameters ***************************/
    CodecParamType type;
    type = CodecParamType::kFrameRateType;
    float fps = 1.0 / timelapse_interval_;
    ret = recorder_.SetVideoTrackParam(session_id, 1, type, &fps , sizeof(fps));
    ASSERT_TRUE(ret == NO_ERROR);

    /*********************** Start Recording  ********************************/

#ifndef DISABLE_DISPLAY
    if (use_display_) {
      ret = StartDisplay(DisplayType::kPrimary, 848, 480, 480, 360);
      if (ret != 0) {
        TEST_ERROR("%s: StartDisplay Failed!!", __func__);
      }
    }
#endif

    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    // Let session run for time record_duration_, during this time buffer with
    // valid data would be received in track callback (VideoTrackYUVDataCb).
    sleep(record_duration_);


    /*********************** Tear down  **************************************/

    ret = recorder_.StopSession(session_id, false);
    ASSERT_TRUE(ret == NO_ERROR);

#ifndef DISABLE_DISPLAY
    if (use_display_) {
      ret = StopDisplay(DisplayType::kPrimary);
      ASSERT_TRUE(ret == NO_ERROR);
    }
#endif

    ret = recorder_.DeleteVideoTrack(session_id, track_id_480p_yuv);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, track_id_480p_avc);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, track_id_1440p_avc);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    track_trace.Reset();

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
* SessionWith4kEncLinked480pEncAndLinked480pYUVTrackWithTimelapse:
*     One session with EIS, 4K 30fps master AVC track, 480p 30fps linked AVC
*     track and 480p 30fps linked YUV track.
*
* Api test sequence:
*  - StartCamera
*   loop Start {
*   ------------------
*   - CreateSession
*   - CreateVideoTrack - 3840x2160 @30 AVC - Master
*   - CreateVideoTrack - 848x480   @30 AVC - Linked
*   - CreateVideoTrack - 848x480   @30 YUV - Linked
*   - StartDisplay
*   - StartSession
*   - StopSession
*   - StopDisplay
*   - DeleteVideoTrack - 848x480   @30 YUV - Linked
*   - DeleteVideoTrack - 848x480   @30 AVC - Linked
*   - DeleteVideoTrack - 3840x2160 @30 AVC - Master
*   - DeleteSession
*   ------------------
*   } loop End
*  - StopCamera
*/
TEST_F(RecorderPostprocessVideoGTest,
    SessionWith4kEncLinked480pEncAndLinked480pYUVTrackWithTimelapse) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.StartCamera(camera_id_, camera_start_params_);
  ASSERT_TRUE(ret == NO_ERROR);

  uint32_t track_id_4k_avc   = 1;
  uint32_t track_id_480p_avc = 2;
  uint32_t track_id_480p_yuv = 3;

  SessionCb session_status_cb = CreateSessionStatusCb();
  uint32_t session_id;
  ret = recorder_.CreateSession(session_status_cb, &session_id);
  ASSERT_TRUE(session_id > 0);
  ASSERT_TRUE(ret == NO_ERROR);

  FrameTrace track_trace(is_frame_debug_enabled_);

  if (dump_bitstream_.IsEnabled()) {
    StreamDumpInfo dump4k = {
      VideoFormat::kAVC, session_id, track_id_4k_avc, 3840, 2160
    };
    ret = dump_bitstream_.SetUp(dump4k);
    ASSERT_TRUE(ret == NO_ERROR);

    StreamDumpInfo dump480p = {
      VideoFormat::kAVC, session_id, track_id_480p_avc, 848, 480
    };
    ret = dump_bitstream_.SetUp(dump480p);
    ASSERT_TRUE(ret == NO_ERROR);
  }

  TrackCb track_cb;
  track_cb.event_cb =
      [this] (uint32_t track_id, EventType type, void *data, size_t size)
      { VideoTrackEventCb(track_id, type, data, size); };

  for (uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
        test_info_->name(), i);

    VideoExtraParam extraparam;
    SourceVideoTrack source_video;
    VideoTimeLapse timelapse;

    /*************************** 4K @4 AVC **********************************/

    VideoTrackCreateParam videoparam = { camera_id_, VideoFormat::kAVC, 3840, 2160, 4 };
    track_trace.SetUp(session_id, track_id_4k_avc, 4);

    timelapse.time_interval = 33; //ms
    extraparam.Update(QMMF_VIDEO_TIMELAPSE_INTERVAL, timelapse);

    track_cb.data_cb = [&, session_id] (uint32_t track_id,
        std::vector<BufferDescriptor> buffers, std::vector<MetaData> meta) {
      VideoTrackEncDataCb(session_id, track_id, buffers, meta);
      track_trace.BufferAvailableCb(buffers.at(0));
    };

    ret = recorder_.CreateVideoTrack(session_id, track_id_4k_avc,
                                     videoparam, extraparam, track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    extraparam.Clear();


    /*********************** Linked 480p @4 AVC *****************************/

    videoparam = { camera_id_, VideoFormat::kAVC, 848, 480, 4 };

    source_video.source_track_id = track_id_4k_avc;
    extraparam.Update(QMMF_SOURCE_VIDEO_TRACK_ID, source_video);

    timelapse.time_interval = 33; //ms
    extraparam.Update(QMMF_VIDEO_TIMELAPSE_INTERVAL, timelapse);

    track_cb.data_cb = [&, session_id] (uint32_t track_id,
        std::vector<BufferDescriptor> buffers, std::vector<MetaData> meta) {
      VideoTrackEncDataCb(session_id, track_id, buffers, meta);
    };

    ret = recorder_.CreateVideoTrack(session_id, track_id_480p_avc,
                                     videoparam, extraparam, track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    extraparam.Clear();


    /*********************** Linked 480p @4 YUV *****************************/

    videoparam = { camera_id_, VideoFormat::kYUV, 848, 480, 4 };

    source_video.source_track_id = track_id_480p_avc;
    extraparam.Update(QMMF_SOURCE_VIDEO_TRACK_ID, source_video);

    track_cb.data_cb = [&, session_id] (uint32_t track_id,
        std::vector<BufferDescriptor> buffers, std::vector<MetaData> meta) {
      VideoTrackYUVDataCb(session_id, track_id, buffers, meta);
    };

    ret = recorder_.CreateVideoTrack(session_id, track_id_480p_yuv,
                                     videoparam, extraparam, track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    std::vector<uint32_t> track_ids;
    track_ids.push_back(track_id_4k_avc);
    track_ids.push_back(track_id_480p_avc);
    track_ids.push_back(track_id_480p_yuv);
    sessions_.insert(std::make_pair(session_id, track_ids));
    extraparam.Clear();

    /************************ Set Video Parameters ***************************/
    CodecParamType type;
    type = CodecParamType::kFrameRateType;
    float fps = 1.0 / timelapse_interval_;
    ret = recorder_.SetVideoTrackParam(session_id, 1, type, &fps , sizeof(fps));
    ASSERT_TRUE(ret == NO_ERROR);

    /*********************** Start Recording  ********************************/

#ifndef DISABLE_DISPLAY
    if (use_display_) {
      ret = StartDisplay(DisplayType::kPrimary, 848, 480, 480, 360);
      if (ret != 0) {
        TEST_ERROR("%s: StartDisplay Failed!!", __func__);
      }
    }
#endif

    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    // Let session run for time record_duration_, during this time buffer with
    // valid data would be received in track callback (VideoTrackYUVDataCb).
    sleep(record_duration_);


    /*********************** Tear down  **************************************/

    ret = recorder_.StopSession(session_id, false);
    ASSERT_TRUE(ret == NO_ERROR);

#ifndef DISABLE_DISPLAY
    if (use_display_) {
      ret = StopDisplay(DisplayType::kPrimary);
      ASSERT_TRUE(ret == NO_ERROR);
    }
#endif

    ret = recorder_.DeleteVideoTrack(session_id, track_id_480p_yuv);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, track_id_480p_avc);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, track_id_4k_avc);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    track_trace.Reset();

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
* SessionWith4kEncCopy480pEncAndLinked480pEISWIthSAR: This test will test
* session with one 4k30 Enc track, one copy 480p Enc Track and one 480p linked
* with YUV with SAR config.
* API test sequence:
*  - StartCamera
*  - CreateSession
*  - CreateVideoTrack - Master With SAR enabled
*  - CreateVideoTrack - Copy   With SAR enabled
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
TEST_F(RecorderPostprocessVideoGTest, SessionWith4kEncCopy480pEncAndLinked480pEISWithSAR) {
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
    StreamDumpInfo dumpinfo1 = {VideoFormat::kAVC, session_id,
                                video_track_id_4kp_avc, width, height};
    ret = dump_bitstream_.SetUp(dumpinfo1);
    ASSERT_TRUE(ret == NO_ERROR);

    StreamDumpInfo dumpinfo2 = {VideoFormat::kAVC, session_id,
                                video_track_id_480p_avc, 848, 480};
    ret = dump_bitstream_.SetUp(dumpinfo2);
    ASSERT_TRUE(ret == NO_ERROR);
  }

  VideoTrackCreateParam video_track_param{camera_id_, VideoFormat::kAVC, width,
                                          height, 30};

  video_track_param.codec_param.avc.bitrate = kBitRate4k30;
  video_track_param.codec_param.avc.ratecontrol_type =
      VideoRateControlType::kVariable;
  video_track_param.codec_param.avc.sar_enabled = true;
  video_track_param.codec_param.avc.sar_width = 1;
  video_track_param.codec_param.avc.sar_height = 1;

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
  video_track_param.codec_param.avc.bitrate = kBitRate480p;
  video_track_param.codec_param.avc.sar_enabled = true;
  video_track_param.codec_param.avc.sar_width = 1;
  video_track_param.codec_param.avc.sar_height = 1;

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
  video_track_param.codec_param.avc.sar_enabled = false;

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
    if (!default_eis_margins_) {
      // Video stabilization horizontal margin.
      float h_margin = 0.033;
      meta.update(QCAMERA3_IS_H_MARGIN_CFG, &h_margin, 1);

      // Video stabilization vertical margin.
      float v_margin = 0.033;
      meta.update(QCAMERA3_IS_V_MARGIN_CFG, &v_margin, 1);
    }

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
      StreamDumpInfo dumpinfo1 = { VideoFormat::kAVC, session_id,
                          video_track_id_1440p_avc, track_width, track_height};
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

/*
 * SessionWith4k_VHDR_PM_TNR_1080pLinked720p: This test will test session with
 *      4k h264 VHDR track and post processing. Post processing pipe is
 *      Privacy Mask.
 *
 * Api test sequence:
 *  - StartCamera
 *   loop Start {
 *   ------------------
 *   - CreateSession
 *   - CreateVideoTracks (4k 1080p 720p)
 *   - StartVideoTracks
 *   - StopSession
 *   - DeleteVideoTracks (4k 1080p 720p)
 *   - DeleteSession
 *   ------------------
 *   } loop End
 *  - StopCamera
 */
TEST_F(RecorderPostprocessVideoGTest, SessionWith4k_VHDR_PM_TNR_1080pLinked720p) {
  uint32_t w1 = 3840, h1 = 2160;
  uint32_t w2 = 1920, h2 = 1080;
  uint32_t w3 = 1280, h3 = 720;
  float fps = 24;

  uint32_t video_track1_id = 1;
  uint32_t video_track2_id = 2;
  uint32_t video_track3_id = 3;

  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  // Start
  ret = recorder_.StartCamera(camera_id_, camera_start_params_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.GetDefaultCaptureParam(camera_id_, static_info_);
  ASSERT_TRUE(ret == NO_ERROR);

  InitSupportedVHDRModes();
  auto is_shdr_supported = IsVHDRSupported();

  fprintf(stderr, "\nis_shdr_supported:%d\n", is_shdr_supported);

  SessionCb s1_status_cb;
  s1_status_cb.event_cb = [this] (EventType event_type, void *event_data,
                                  size_t event_data_size) -> void
      { SessionCallbackHandler(event_type, event_data, event_data_size); };

  uint32_t s1_id;
  ret = recorder_.CreateSession(s1_status_cb, &s1_id);
  ASSERT_TRUE(s1_id > 0);
  ASSERT_TRUE(ret == NO_ERROR);

  // Create 4MP AVC Stream
  VideoTrackCreateParam video_track{camera_id_, VideoFormat::kAVC, w1, h1,fps};
  video_track.low_power_mode = false;

  if (dump_bitstream_.IsEnabled()) {
    StreamDumpInfo dumpinfo = { VideoFormat::kAVC, s1_id, video_track1_id, w1, h1 };
    ret = dump_bitstream_.SetUp(dumpinfo);
    ASSERT_TRUE(ret == NO_ERROR);

    StreamDumpInfo dumpinfo2 = { VideoFormat::kAVC, s1_id, video_track2_id, w2, h2 };
    ret = dump_bitstream_.SetUp(dumpinfo2);
    ASSERT_TRUE(ret == NO_ERROR);

    StreamDumpInfo dumpinfo3 = { VideoFormat::kAVC, s1_id, video_track3_id, w3, h3 };
    ret = dump_bitstream_.SetUp(dumpinfo3);
    ASSERT_TRUE(ret == NO_ERROR);
  }

  TrackCb video_track1_cb;
  video_track1_cb.event_cb =
      [this] (uint32_t track_id, EventType event_type,
              void *event_data, size_t event_data_size) -> void {
      VideoTrackEventCb(track_id, event_type, event_data, event_data_size); };

   video_track1_cb.data_cb = [&, s1_id] (uint32_t track_id,
                                         std::vector<BufferDescriptor> buffers,
                                         std::vector<MetaData> meta_buffers) {
      VideoTrackEncDataCb(s1_id, track_id, buffers, meta_buffers);
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

        //Continius capture
        //30% = 1042,  7% = 1327
        std::string config = "{\"circles\": [ \
            {                                 \
            \"radius\": 1327,                 \
              \"centre\": {                   \
                \"x\": 1344,                  \
                \"y\": 760                    \
                },                            \
              \"color\": {                    \
                \"y\": 0,                     \
                \"u\": 128,                   \
                \"v\": 128                    \
              }                               \
            }                                 \
            ]                                 \
            }";

        fprintf(stderr,"--------- Test2 ConfigPlugin %s----\n", config.c_str());

        ret = recorder_.ConfigPlugin(pmr_plugin.uid, config);
        ASSERT_TRUE(ret == NO_ERROR);

        extra_param.Update(QMMF_POSTPROCESS_PLUGIN, pmr_plugin);
        found = true;

      }
    }
    ASSERT_TRUE(found == true);

  ret = recorder_.CreateVideoTrack(s1_id, video_track1_id,
                                   video_track,extra_param, video_track1_cb);
  ASSERT_TRUE(ret == NO_ERROR);

  std::vector<uint32_t> s1_track_ids;
  s1_track_ids.push_back(video_track1_id);

  // Create track2
  video_track.width  = w2;
  video_track.height = h2;

  TrackCb video_track_cb;
  video_track_cb.data_cb = [&, s1_id] (uint32_t track_id,
                                       std::vector<BufferDescriptor> buffers,
                                       std::vector<MetaData> meta_buffers) {
        VideoTrackEncDataCb(s1_id, track_id, buffers, meta_buffers);
  };

  video_track_cb.event_cb = [&] (uint32_t track_id, EventType event_type,
                                 void *event_data, size_t event_data_size) {
      VideoTrackEventCb(track_id, event_type, event_data, event_data_size);
  };

  ret = recorder_.CreateVideoTrack(s1_id, video_track2_id,
                                   video_track,
                                   video_track_cb);
  ASSERT_TRUE(ret == NO_ERROR);

  s1_track_ids.push_back(video_track2_id);

  // Create track3
  video_track.width  = w3;
  video_track.height = h3;

  VideoExtraParam extra_param2;
  SourceVideoTrack surface_video_copy2;
  surface_video_copy2.source_track_id = video_track2_id;
  extra_param2.Update(QMMF_SOURCE_VIDEO_TRACK_ID, surface_video_copy2);

  TrackCb video_track_cb3;
  video_track_cb3.data_cb = [&, s1_id] (uint32_t track_id,
                                        std::vector<BufferDescriptor> buffers,
                                        std::vector<MetaData> meta_buffers) {
        VideoTrackEncDataCb(s1_id, track_id, buffers, meta_buffers);
  };

  video_track_cb3.event_cb = [&] (uint32_t track_id, EventType event_type,
                                  void *event_data, size_t event_data_size) {
      VideoTrackEventCb(track_id, event_type, event_data, event_data_size);
  };

  ret = recorder_.CreateVideoTrack(s1_id, video_track3_id,
                                   video_track, extra_param2,
                                   video_track_cb3);
  ASSERT_TRUE(ret == NO_ERROR);

  s1_track_ids.push_back(video_track3_id);
  sessions_.insert(std::make_pair(s1_id, s1_track_ids));

  //Enable TNR - High quality mode.
  CameraMetadata meta;
  auto status = recorder_.GetCameraParam(camera_id_, meta);
  if (NO_ERROR == status) {
    if (meta.exists(ANDROID_NOISE_REDUCTION_MODE)) {
      const uint8_t tnr_mode = ANDROID_NOISE_REDUCTION_MODE_HIGH_QUALITY;
      TEST_INFO("%s Enable TNR mode(%d)", __func__, tnr_mode);
      meta.update(ANDROID_NOISE_REDUCTION_MODE, &tnr_mode, 1);
    }
  }

  // Start Session1
  ret = recorder_.StartSession(s1_id);
  ASSERT_TRUE(ret == NO_ERROR);

  sleep(2);

  // Camera Params
  // CameraMetadata meta;
  ret = recorder_.GetCameraParam(camera_id_, meta);
  ASSERT_TRUE(NO_ERROR == ret);

  // SHDR
  if (is_shdr_supported) {
      fprintf(stderr, "\nSetting shdr ON..\n");
      const int32_t vhdrMode =  QCAMERA3_VIDEO_HDR_MODE_ON;
      meta.update( QCAMERA3_VIDEO_HDR_MODE, &vhdrMode, 1);
  }

  ret = recorder_.SetCameraParam(camera_id_, meta);
  ASSERT_TRUE(NO_ERROR == ret);

  sleep(record_duration_);

  ret = recorder_.StopSession(s1_id, false);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteVideoTrack(s1_id, video_track1_id);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteVideoTrack(s1_id, video_track2_id);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteVideoTrack(s1_id, video_track3_id);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeletePlugin(pmr_plugin.uid);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteSession(s1_id);
  ASSERT_TRUE(ret == NO_ERROR);

  ClearSessions();

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  dump_bitstream_.CloseAll();

  fprintf(stderr, "---------- Test Completed %s.%s ----------\n",
          test_info_->test_case_name(), test_info_->name());
}

#ifndef DISABLE_DISPLAY
/*
* SessionWith1440p60FPSEncCopy480pEncAndLinked480pDisplayEISLCACTNR:
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
       SessionWith1440p60FPSEncCopy480pEncAndLinked480pDisplayEISLCACTNR) {
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
      StreamDumpInfo dumpinfo1 = {VideoFormat::kAVC, session_id,
                                  video_track_id_1440p_avc, width, height};
      ret = dump_bitstream_.SetUp(dumpinfo1);
      ASSERT_TRUE(ret == NO_ERROR);

      StreamDumpInfo dumpinfo2 = {VideoFormat::kAVC, session_id,
                                  video_track_id_480p_avc, 640, 480};
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
* SessionWith960p90FPSEncCopy480pEncAndLinked480pDisplayEISLCACTNR:
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
       SessionWith960p90FPSEncCopy480pEncAndLinked480pDisplayEISLCACTNR) {
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
      StreamDumpInfo dumpinfo1 = { VideoFormat::kAVC, session_id,
                                 video_track_id_960p_avc, width, height };
      ret = dump_bitstream_.SetUp(dumpinfo1);
      ASSERT_TRUE(ret == NO_ERROR);

      StreamDumpInfo dumpinfo2 = { VideoFormat::kAVC, session_id,
                                 video_track_id_480p_avc, 640, 480 };
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
* SessionWith4k30FPSEncCopy480pEncAndLinked480pDisplayEISLCAC:
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
       SessionWith4k30FPSEncCopy480pEncAndLinked480pDisplayEISLCAC) {
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
    StreamDumpInfo dumpinfo1 = {VideoFormat::kAVC, session_id,
                                video_track_id_4kp_avc, width, height};
    ret = dump_bitstream_.SetUp(dumpinfo1);
    ASSERT_TRUE(ret == NO_ERROR);

    StreamDumpInfo dumpinfo2 = {VideoFormat::kAVC, session_id,
                                video_track_id_480p_avc, 848, 480};
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

#endif

#ifdef CAM_ARCH_V2

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
      StreamDumpInfo dumpinfo1 = { VideoFormat::kAVC, session_id,
                                  video_track_id_4k_avc, 4096, 2048 };
      ret = dump_bitstream_.SetUp(dumpinfo1);
      ASSERT_TRUE(ret == NO_ERROR);

      StreamDumpInfo dumpinfo2 = { VideoFormat::kAVC, session_id,
                                 video_track_id_1080p_avc, 2160, 1080 };
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
    if (ubwc_stream_enable_) {
      video_track_param.low_power_mode = true;
    }

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
    if (ubwc_stream_enable_) {
      video_track_param.low_power_mode = true;
    }

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
    if (ubwc_stream_enable_) {
      video_track_param.low_power_mode = true;
    }

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
    if (ubwc_stream_enable_) {
      video_track_param.low_power_mode = true;
    }

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
* SessionWith4KEncWithSHDR: This test will verify single session containing
* Camera 4K AVC track with SHDR. Only for hardware supporting SHDR.
* Api test sequence:
*  - StartCamera
*   - CreateSession
*   - Add ExtraParam for SHDR
*   - CreateVideoTrack
*   - StartSession
*   - StopSession
*   - DeleteVideoTrack
*   - DeleteSession
*  - StopCamera
*/
TEST_F(RecorderPostprocessVideoGTest, SessionWith4KEncWithSHDR) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  VideoFormat format_type = VideoFormat::kAVC;
  uint32_t width  = 3840;
  uint32_t height = 2160;
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
* SessionWith4KEncWithSHDRInAecWait: This test will verify single session
* containing Camera 4K AVC track with SHDR in AEC wait Mode. Only for hardware
* supporting SHDR.
* Api test sequence:
*  - StartCamera
*   - CreateSession
*   - CreateVideoTrack with extra param for SHDR and AEC wait
*   - StartSession
*   - StopSession
*   - DeleteVideoTrack
*   - DeleteSession
*  - StopCamera
*/
TEST_F(RecorderPostprocessVideoGTest, SessionWith4KEncWithSHDRInAecWait) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  VideoFormat format_type = VideoFormat::kHEVC;
  uint32_t width  = 3840;
  uint32_t height = 2160;
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

  // Set HDR Extra Param
  VideoExtraParam extra_param;
  VideoHDRMode vid_hdr_mode;
  vid_hdr_mode.enable = true;
  extra_param.Update(QMMF_VIDEO_HDR_MODE, vid_hdr_mode);

  // Set AEC Extra Param
  VideoWaitAECMode wait_aec;
  wait_aec.enable = true;
  extra_param.Update(QMMF_VIDEO_WAIT_AEC_MODE, wait_aec);

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
    if (ubwc_stream_enable_) {
      video_track_param.low_power_mode = true;
    }

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
TEST_F(RecorderPostprocessVideoGTest,
    SessionWithDualCam4k60Enc1080p30EncAndLinked1080p30YUVWithTNRAndZZHDR) {
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
    if (ubwc_stream_enable_) {
      video_track_param.low_power_mode = true;
    }

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
TEST_F(RecorderPostprocessVideoGTest,
    SessionWithDualCam5_7k30Enc1080p30EncAndLinked1080p30YUVWithTNRAndZZHDR) {
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
      StreamDumpInfo dumpinfo1 = { VideoFormat::kAVC, session_id,
                                   video_track_id_5_7k_avc, 5760, 2880 };
      ret = dump_bitstream_.SetUp(dumpinfo1);
      ASSERT_TRUE(ret == NO_ERROR);

      StreamDumpInfo dumpinfo2 = { VideoFormat::kAVC, session_id,
                                   video_track_id_1080p_avc, 2160, 1080 };
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
    if (ubwc_stream_enable_) {
      video_track_param.low_power_mode = true;
    }

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
