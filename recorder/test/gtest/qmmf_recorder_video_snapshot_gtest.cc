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
#define LOG_TAG "RecorderVideoSnapshotGTest"

#include "recorder/test/gtest/qmmf_recorder_video_snapshot_gtest.h"

using namespace qcamera;

/*
* 1080pZSL1080pVideo: This case will test 1080p ZSL along with 1080p AVC video.
* Api test sequence:
*   ------------------
*  - StartCamera
*  - CreateSession
*  - CreateVideoTrack
*  *   loop Start {
*   ------------------
*  - CaptureImage (ZSL)
*  *   ------------------
*   } loop End
*  - StopSession
*  - DeleteVideoTrack
*  - DeleteSession
*  - CaptureImage
*  - StopCamera
*   ------------------
*/
TEST_F(RecorderVideoSnapshotGTest, 1080pZSL1080pVideo) {

  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  camera_start_params_.zsl_mode = true;
  camera_start_params_.zsl_width = 1920;
  camera_start_params_.zsl_height = 1080;
  camera_start_params_.zsl_queue_depth = 4;
  camera_start_params_.frame_rate = 30;
  ret = recorder_.StartCamera(camera_id_, camera_start_params_);
  ASSERT_TRUE(ret == NO_ERROR);

  SessionCb session_status_cb = CreateSessionStatusCb();
  uint32_t session_id;
  ret = recorder_.CreateSession(session_status_cb, &session_id);
  ASSERT_TRUE(session_id > 0);
  ASSERT_TRUE(ret == NO_ERROR);
  VideoTrackCreateParam video_track_param{camera_id_, VideoFormat::kAVC,
                                          1920, 1080, 30};
  uint32_t video_track_id = 1;
  if (dump_bitstream_.IsEnabled()) {
    StreamDumpInfo dumpinfo = { video_track_param.format_type, video_track_id,
      session_id, video_track_param.width, video_track_param.height };
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

  sleep(3);
  ret = recorder_.CreateVideoTrack(session_id, video_track_id,
                                    video_track_param, video_track_cb);
  ASSERT_TRUE(ret == NO_ERROR);

  std::vector<uint32_t> track_ids;
  track_ids.push_back(video_track_id);
  sessions_.insert(std::make_pair(session_id, track_ids));

  ret = recorder_.StartSession(session_id);
  ASSERT_TRUE(ret == NO_ERROR);

  sleep(3);

  ImageParam image_param{};
  image_param.width         = camera_start_params_.zsl_width;
  image_param.height        = camera_start_params_.zsl_height;
  image_param.image_format  = ImageFormat::kJPEG;
  image_param.image_quality = default_jpeg_quality_;

  std::vector<CameraMetadata> meta_array;

  ImageCaptureCb cb = [this] (uint32_t camera_id, uint32_t image_count,
                              BufferDescriptor buffer,
                              MetaData meta_data) -> void
      { SnapshotCb(camera_id, image_count, buffer, meta_data); };

  for(uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
        test_info_->name(), i);

    ret = recorder_.CaptureImage(camera_id_, image_param, 1, meta_array,
                                 cb);
    ASSERT_TRUE(ret == NO_ERROR);
    sleep(3);
  }

  ret = recorder_.StopSession(session_id, false);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteVideoTrack(session_id, video_track_id);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteSession(session_id);
  ASSERT_TRUE(ret == NO_ERROR);

  ClearSessions();

  ret = recorder_.CaptureImage(camera_id_, image_param, 1, meta_array,
                               cb);
  ASSERT_TRUE(ret == NO_ERROR);
  sleep(3);

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  dump_bitstream_.CloseAll();
  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());
}

/*
* 4KZSL1080pYUVPreview: This case will test ZSL at 4K along with 1080p
* YUV preview.
* Api test sequence:
*   ------------------
*  - StartCamera
*  - CreateSession
*  - CreateVideoTrack
*  *   loop Start {
*   ------------------
*  - CaptureImage (ZSL)
*  *   ------------------
*   } loop End
*  - StopSession
*  - DeleteVideoTrack
*  - DeleteSession
*  - CaptureImage
*  - StopCamera
*   ------------------
*/
TEST_F(RecorderVideoSnapshotGTest, 4KZSL1080pYUVPreview) {

  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  camera_start_params_.zsl_mode = true;
  camera_start_params_.zsl_width = 3840;
  camera_start_params_.zsl_height = 2160;
  camera_start_params_.zsl_queue_depth = 4;
  camera_start_params_.frame_rate = 30;
  ret = recorder_.StartCamera(camera_id_, camera_start_params_);
  ASSERT_TRUE(ret == NO_ERROR);

  SessionCb session_status_cb = CreateSessionStatusCb();
  uint32_t session_id;
  ret = recorder_.CreateSession(session_status_cb, &session_id);
  ASSERT_TRUE(session_id > 0);
  ASSERT_TRUE(ret == NO_ERROR);
  VideoTrackCreateParam preview_track_param{camera_id_, VideoFormat::kYUV,
                                            1920, 1080, 30};
  preview_track_param.low_power_mode = true;
  uint32_t preview_track_id          = 1;

  TrackCb video_track_cb;
  video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
      std::vector<BufferDescriptor> buffers, std::vector<MetaData> meta_buffers)
      { VideoTrackYUVDataCb(session_id, track_id, buffers, meta_buffers); };

  video_track_cb.event_cb = [&] (uint32_t track_id, EventType event_type,
      void *event_data, size_t event_data_size) { VideoTrackEventCb(track_id,
      event_type, event_data, event_data_size); };

  ret = recorder_.CreateVideoTrack(session_id, preview_track_id,
                                    preview_track_param, video_track_cb);
  ASSERT_TRUE(ret == NO_ERROR);

  std::vector<uint32_t> track_ids;
  track_ids.push_back(preview_track_id);
  sessions_.insert(std::make_pair(session_id, track_ids));

  ret = recorder_.StartSession(session_id);
  ASSERT_TRUE(ret == NO_ERROR);

  sleep(5);

  ImageParam image_param{};
  image_param.width         = camera_start_params_.zsl_width;
  image_param.height        = camera_start_params_.zsl_height;
  image_param.image_format  = ImageFormat::kJPEG;
  image_param.image_quality = default_jpeg_quality_;

  std::vector<CameraMetadata> meta_array;

  ImageCaptureCb cb = [this] (uint32_t camera_id, uint32_t image_count,
                              BufferDescriptor buffer,
                              MetaData meta_data) -> void
      { SnapshotCb(camera_id, image_count, buffer, meta_data); };

  for(uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
        test_info_->name(), i);

    ret = recorder_.CaptureImage(camera_id_, image_param, 1, meta_array,
                                 cb);
    ASSERT_TRUE(ret == NO_ERROR);
    sleep(3);
  }

  ret = recorder_.StopSession(session_id, false);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteVideoTrack(session_id, preview_track_id);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteSession(session_id);
  ASSERT_TRUE(ret == NO_ERROR);

  ClearSessions();

  ret = recorder_.CaptureImage(camera_id_, image_param, 1, meta_array,
                               cb);
  ASSERT_TRUE(ret == NO_ERROR);
  sleep(3);

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());
}

/*
* 4KZSL1080p480pYUVPreview: This case will test ZSL at 4K along with 1080p
* and 480p YUV preview.
* Api test sequence:
*   ------------------
*  - StartCamera
*  - CreateSession
*  - CreateVideoTrack
*  *   loop Start {
*   ------------------
*  - CaptureImage (ZSL)
*  *   ------------------
*   } loop End
*  - StopSession
*  - DeleteVideoTrack
*  - DeleteSession
*  - CaptureImage
*  - StopCamera
*   ------------------
*/
TEST_F(RecorderVideoSnapshotGTest, 4KZSL1080p480pYUVPreview) {

  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  camera_start_params_.zsl_mode = true;
  camera_start_params_.zsl_width = 3840;
  camera_start_params_.zsl_height = 2160;
  camera_start_params_.zsl_queue_depth = 4;
  camera_start_params_.frame_rate = 30;
  ret = recorder_.StartCamera(camera_id_, camera_start_params_);
  ASSERT_TRUE(ret == NO_ERROR);

  SessionCb session_status_cb = CreateSessionStatusCb();
  uint32_t session_id;
  ret = recorder_.CreateSession(session_status_cb, &session_id);
  ASSERT_TRUE(session_id > 0);
  ASSERT_TRUE(ret == NO_ERROR);
  VideoTrackCreateParam preview_track_param{camera_id_, VideoFormat::kYUV,
                                            1920, 1080, 30};
  preview_track_param.low_power_mode = false;
  uint32_t preview_track_id          = 1;

  TrackCb video_track_cb;
  video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
      std::vector<BufferDescriptor> buffers, std::vector<MetaData> meta_buffers)
      { VideoTrackYUVDataCb(session_id, track_id, buffers, meta_buffers); };

  video_track_cb.event_cb = [&] (uint32_t track_id, EventType event_type,
      void *event_data, size_t event_data_size) { VideoTrackEventCb(track_id,
      event_type, event_data, event_data_size); };

  ret = recorder_.CreateVideoTrack(session_id, preview_track_id,
                                    preview_track_param, video_track_cb);
  ASSERT_TRUE(ret == NO_ERROR);

  std::vector<uint32_t> track_ids;
  track_ids.push_back(preview_track_id);

  preview_track_param.width         = 720;
  preview_track_param.height        = 480;
  uint32_t preview_track480p_id = 2;

  ret = recorder_.CreateVideoTrack(session_id, preview_track480p_id,
                                    preview_track_param, video_track_cb);
  ASSERT_TRUE(ret == NO_ERROR);

  track_ids.push_back(preview_track480p_id);
  sessions_.insert(std::make_pair(session_id, track_ids));

  ret = recorder_.StartSession(session_id);
  ASSERT_TRUE(ret == NO_ERROR);

  sleep(5);

  ImageParam image_param{};
  image_param.width         = camera_start_params_.zsl_width;
  image_param.height        = camera_start_params_.zsl_height;
  image_param.image_format  = ImageFormat::kJPEG;
  image_param.image_quality = default_jpeg_quality_;

  std::vector<CameraMetadata> meta_array;

  ImageCaptureCb cb = [this] (uint32_t camera_id, uint32_t image_count,
                              BufferDescriptor buffer,
                              MetaData meta_data) -> void
      { SnapshotCb(camera_id, image_count, buffer, meta_data); };

  for(uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
        test_info_->name(), i);

    ret = recorder_.CaptureImage(camera_id_, image_param, 1, meta_array,
                                 cb);
    ASSERT_TRUE(ret == NO_ERROR);
    sleep(3);
  }

  ret = recorder_.StopSession(session_id, false);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteVideoTrack(session_id, preview_track_id);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteVideoTrack(session_id, preview_track480p_id);
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
* 4KZSLTwo1080pVideo: This case will test ZSL at 4K along with two 1080p
* videos.
* Api test sequence:
*   ------------------
*  - StartCamera
*  - CreateSession
*  - CreateVideoTrack
*  *   loop Start {
*   ------------------
*  - CaptureImage (ZSL)
*  *   ---------------
*   } loop End
*  - StopSession
*  - DeleteVideoTrack
*  - DeleteSession
*  - CaptureImage
*  - StopCamera
*   ------------------
*/
TEST_F(RecorderVideoSnapshotGTest, 4KZSLTwo1080pVideo) {

  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  camera_start_params_.zsl_mode = true;
  camera_start_params_.zsl_width = 3840;
  camera_start_params_.zsl_height = 2160;
  camera_start_params_.zsl_queue_depth = 4;
  camera_start_params_.frame_rate = 30;
  ret = recorder_.StartCamera(camera_id_, camera_start_params_);
  ASSERT_TRUE(ret == NO_ERROR);

  SessionCb session_status_cb = CreateSessionStatusCb();
  uint32_t session_id;
  ret = recorder_.CreateSession(session_status_cb, &session_id);
  ASSERT_TRUE(session_id > 0);
  ASSERT_TRUE(ret == NO_ERROR);
  VideoTrackCreateParam video_track_param{ camera_id_, VideoFormat::kAVC,
                                          1920, 1080, 30 };
  uint32_t video_track_id_1 = 1;
  if (dump_bitstream_.IsEnabled()) {
    StreamDumpInfo dumpinfo = { video_track_param.format_type, video_track_id_1,
      session_id, video_track_param.width, video_track_param.height };
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

  ret = recorder_.CreateVideoTrack(session_id, video_track_id_1,
                                   video_track_param, video_track_cb);
  ASSERT_TRUE(ret == NO_ERROR);

  std::vector<uint32_t> track_ids;
  track_ids.push_back(video_track_id_1);

  uint32_t video_track_id_2 = 2;
  if (dump_bitstream_.IsEnabled()) {
    StreamDumpInfo dumpinfo = { video_track_param.format_type, video_track_id_2,
      session_id, video_track_param.width, video_track_param.height };
    ret = dump_bitstream_.SetUp(dumpinfo);
  }

  video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
      std::vector<BufferDescriptor> buffers,
      std::vector<MetaData> meta_buffers) {
        VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
      };

  ret = recorder_.CreateVideoTrack(session_id, video_track_id_2,
                                   video_track_param, video_track_cb);
  ASSERT_TRUE(ret == NO_ERROR);

  track_ids.push_back(video_track_id_2);
  sessions_.insert(std::make_pair(session_id, track_ids));

  ret = recorder_.StartSession(session_id);
  ASSERT_TRUE(ret == NO_ERROR);

  sleep(5);

  ImageParam image_param{};
  image_param.width         = camera_start_params_.zsl_width;
  image_param.height        = camera_start_params_.zsl_height;
  image_param.image_format  = ImageFormat::kJPEG;
  image_param.image_quality = default_jpeg_quality_;

  std::vector<CameraMetadata> meta_array;

  ImageCaptureCb cb = [this] (uint32_t camera_id, uint32_t image_count,
                              BufferDescriptor buffer,
                              MetaData meta_data) -> void
      { SnapshotCb(camera_id, image_count, buffer, meta_data); };

  for(uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
        test_info_->name(), i);

    ret = recorder_.CaptureImage(camera_id_, image_param, 1, meta_array,
                                 cb);
    ASSERT_TRUE(ret == NO_ERROR);
    sleep(3);
  }

  ret = recorder_.StopSession(session_id, false);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteVideoTrack(session_id, video_track_id_1);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteVideoTrack(session_id, video_track_id_2);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteSession(session_id);
  ASSERT_TRUE(ret == NO_ERROR);

  ClearSessions();

  ret = recorder_.CaptureImage(camera_id_, image_param, 1, meta_array,
                               cb);
  ASSERT_TRUE(ret == NO_ERROR);
  sleep(3);

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  dump_bitstream_.CloseAll();
  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());
}

/*
* SessionWith1080Enc30fps1080pMJpeg10fps1080pJpegEnc1fps:
*                            This test will test session with one 1080p H264 30
*                            fps, 1080p Mjpeg 10 fps and 1080p Jpeg encode with
*                            one fps
* API test sequence:
*  - StartCamera
*  - CreateSession
*  - CreateVideoTrack for h264,mjpeg
*  - StartVideoTrack
*  - Take snapshot
*  - StopSession
*  - DeleteVideoTrack
*  - DeleteSession
*  - StopCamera
*/
TEST_F(RecorderVideoSnapshotGTest, SessionWith1080Enc30fps1080pMJpeg10fps1080pJpegEnc1fps) {
  fprintf(stderr, "\n---------- Run Test %s.%s ------------\n",
          test_info_->test_case_name(), test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  VideoFormat format_type = VideoFormat::kAVC;
  uint32_t width = 1920;
  uint32_t height = 1080;

  ret = recorder_.StartCamera(camera_id_, camera_start_params_);
  ASSERT_TRUE(ret == NO_ERROR);

  SessionCb session_status_cb = CreateSessionStatusCb();
  uint32_t session_id;
  ret = recorder_.CreateSession(session_status_cb, &session_id);
  ASSERT_TRUE(session_id > 0);
  ASSERT_TRUE(ret == NO_ERROR);
  VideoTrackCreateParam video_track_param_1{camera_id_, format_type, 1920, 1080,
                                            30};
  uint32_t video_track_id_1 = 1;

  if (dump_bitstream_.IsEnabled()) {
    StreamDumpInfo dumpinfo = { format_type, video_track_id_1, session_id,
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
    StreamDumpInfo dumpinfo = { VideoFormat::kJPEG, video_track_id_mjpeg,
                                session_id, width, height};
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
  sleep(2);
  CameraMetadata meta;
  ret = recorder_.GetDefaultCaptureParam(camera_id_, meta);
  ASSERT_TRUE(ret == NO_ERROR);
  std::vector<CameraMetadata> meta_array;
  meta_array.push_back(meta);
  for (uint32_t i = 1; i <= record_duration_; i++) {
    ImageParam image_param{};
    image_param.width = 1920;
    image_param.height = 1080;
    image_param.image_format = ImageFormat::kJPEG;
    image_param.image_quality = default_jpeg_quality_;

    ImageCaptureCb cb = [this](uint32_t camera_id, uint32_t image_count,
                               BufferDescriptor buffer,
                               MetaData meta_data) -> void {
      SnapshotCb(camera_id, image_count, buffer, meta_data);
    };

    ret = recorder_.CaptureImage(camera_id_, image_param, 1, meta_array, cb);
    sleep(1);
  }
  ret = recorder_.StopSession(session_id, false);
  ASSERT_TRUE(ret == NO_ERROR);
  ret = recorder_.DeleteVideoTrack(session_id, video_track_id_1);
  ASSERT_TRUE(ret == NO_ERROR);
  ret = recorder_.DeleteVideoTrack(session_id, video_track_id_mjpeg);
  ASSERT_TRUE(ret == NO_ERROR);
  ret = recorder_.DeleteSession(session_id);
  ASSERT_TRUE(ret == NO_ERROR);
  dump_bitstream_.CloseAll();

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  fprintf(stderr, "---------- Test Completed %s.%s ----------\n",
          test_info_->test_case_name(), test_info_->name());
}

/*
* SessionWith4kp30fps4K1fpsSnapshotEncTrack: This test will test session with
*  one 4k 30fps h264 track and one 4K 1fps h264 track and snapshot.
* Api test sequence:
*  - StartCamera
*  - CreateSession
*  - CreateVideoTrack
*  - StartVideoTrack
*  - CaptureImage
*  - StopSession
*  - DeleteVideoTrack
*  - DeleteSession
*  - StopCamera
*/
TEST_F(RecorderVideoSnapshotGTest, SessionWith4kp30fps4K1fpsSnapshotEncTrack) {
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

  video_track_param.setAVCVariableFramerateVideoParam();

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

  uint32_t video_track4K1fps_id = 2;
  if (dump_bitstream_.IsEnabled()) {
    StreamDumpInfo dumpinfo = { format_type, video_track_id, session_id,
                                width, height };
    ret = dump_bitstream_.SetUp(dumpinfo);
  }

  video_track_param.frame_rate  = 1;
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

  ret = recorder_.CreateVideoTrack(session_id, video_track4K1fps_id,
                                   video_track_param, video_track_cb);
  ASSERT_TRUE(ret == NO_ERROR);

  track_ids.push_back(video_track4K1fps_id);
  sessions_.insert(std::make_pair(session_id, track_ids));

  ret = recorder_.StartSession(session_id);
  ASSERT_TRUE(ret == NO_ERROR);

  sleep(30);

  //Take snapshot
  ImageParam image_param{};
  image_param.width         = 1920;
  image_param.height        = 1080;
  image_param.image_format  = ImageFormat::kJPEG;
  image_param.image_quality = default_jpeg_quality_;

  std::vector<CameraMetadata> meta_array;
  CameraMetadata meta;

  ret = recorder_.GetDefaultCaptureParam(camera_id_, meta);
  ASSERT_TRUE(ret == NO_ERROR);

  bool res_supported = GtestCommon::ValidateResFromJpegSizes(meta,
      image_param.width, image_param.height);
  ASSERT_TRUE (res_supported != false);

  ImageCaptureCb cb = [this] (uint32_t camera_id, uint32_t image_count,
                              BufferDescriptor buffer,
                              MetaData meta_data) -> void
      { SnapshotCb(camera_id, image_count, buffer, meta_data); };

  meta_array.push_back(meta);
  ret = recorder_.CaptureImage(camera_id_, image_param, 1, meta_array,
                               cb);
  ASSERT_TRUE(ret == NO_ERROR);

  sleep(5);

  ret = recorder_.StopSession(session_id, false);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteVideoTrack(session_id, video_track_id);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteVideoTrack(session_id, video_track4K1fps_id);
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
* SessionWith4kp30fps4K1fps240p30fpsSnapshotEncTrack: This test will test
* session with one 4k 30fps h264 track, one 4K 1fps h264 track, one 432x240
*  30fps h264 track and snapshot.
* Api test sequence:
*  - StartCamera
*  - CreateSession
*  - CreateVideoTrack
*  - StartVideoTrack
*  - CaptureImage
*  - StopSession
*  - DeleteVideoTrack
*  - DeleteSession
*  - StopCamera
*/
TEST_F(RecorderVideoSnapshotGTest, SessionWith4kp30fps4K1fps240p30fpsSnapshotEncTrack) {
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
  for (uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
        test_info_->name(), i);
    uint32_t session_id;
    ret = recorder_.CreateSession(session_status_cb, &session_id);
    ASSERT_TRUE(session_id > 0);
    ASSERT_TRUE(ret == NO_ERROR);
    VideoTrackCreateParam video_track_param{camera_id_, format_type,
                                            width, height, fps};

    video_track_param.setAVCVariableFramerateVideoParam();

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

    uint32_t video_track4K1fps_id = 2;
    if (dump_bitstream_.IsEnabled()) {
      StreamDumpInfo dumpinfo = { format_type, video_track4K1fps_id,
                                  session_id, width, height };
      ret = dump_bitstream_.SetUp(dumpinfo);
    }

  video_track_param.frame_rate  = 1;
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

    ret = recorder_.CreateVideoTrack(session_id, video_track4K1fps_id,
                                     video_track_param, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    track_ids.push_back(video_track4K1fps_id);

    uint32_t video_track240p_id = 3;
    width = 480;
    height = 320;
    if (dump_bitstream_.IsEnabled()) {
      StreamDumpInfo dumpinfo = { format_type, video_track240p_id, session_id,
                                  width, height };
      ret = dump_bitstream_.SetUp(dumpinfo);
    }

    video_track_param.width = width;
    video_track_param.height = height;
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

    ret = recorder_.CreateVideoTrack(session_id, video_track240p_id,
                                     video_track_param, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    track_ids.push_back(video_track240p_id);

    sessions_.insert(std::make_pair(session_id, track_ids));

    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    sleep(30);

    //Take snapshot
    ImageParam image_param{};
    image_param.width         = 1920;
    image_param.height        = 1080;
    image_param.image_format  = ImageFormat::kJPEG;
    image_param.image_quality = default_jpeg_quality_;

    std::vector<CameraMetadata> meta_array;
    CameraMetadata meta;

    ret = recorder_.GetDefaultCaptureParam(camera_id_, meta);
    ASSERT_TRUE(ret == NO_ERROR);

    bool res_supported = GtestCommon::ValidateResFromJpegSizes(meta,
      image_param.width, image_param.height);
    ASSERT_TRUE (res_supported != false);

    ImageCaptureCb cb = [this] (uint32_t camera_id, uint32_t image_count,
                                BufferDescriptor buffer,
                                MetaData meta_data) -> void
        { SnapshotCb(camera_id, image_count, buffer, meta_data); };

    meta_array.push_back(meta);
    ret = recorder_.CaptureImage(camera_id_, image_param, 1, meta_array,
                                 cb);
    ASSERT_TRUE(ret == NO_ERROR);

    sleep(5);

    ret = recorder_.StopSession(session_id, false);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_id);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track4K1fps_id);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track240p_id);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    ClearSessions();
    dump_bitstream_.CloseAll();
  } // End iteration count loop.
  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
          test_info_->test_case_name(), test_info_->name());
}

/*
 * SessionWith1080p120fpsSnapshotEISEncTrack: This test will test session with one 1080p
 * 120fps h264 track.
 * Api test sequence:
 *  - StartCamera
 *  - CreateSession
 *  - CreateVideoTrack
 *  - StartVideoTrack
 *  - Snapshot
 *  - StopSession
 *  - DeleteVideoTrack
 *  - DeleteSession
 *  - StopCamera
 */
TEST_F(RecorderVideoSnapshotGTest, SessionWith1080p120fpsSnapshotEISEncTrack) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  VideoFormat format_type = VideoFormat::kAVC;
  uint32_t width  = 1920;
  uint32_t height = 1080;
  float fps = 120;

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
  sessions_.insert(std::make_pair(session_id, track_ids));

  ret = recorder_.StartSession(session_id);
  ASSERT_TRUE(ret == NO_ERROR);

  sleep(record_duration_);
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

  //Take snapshot
  ImageParam image_param{};
  image_param.width         = 3840;
  image_param.height        = 2160;
  image_param.image_format  = ImageFormat::kJPEG;
  image_param.image_quality = default_jpeg_quality_;

  std::vector<CameraMetadata> meta_array;

  ret = recorder_.GetDefaultCaptureParam(camera_id_, meta);
  ASSERT_TRUE(ret == NO_ERROR);

  bool res_supported = GtestCommon::ValidateResFromJpegSizes(meta,
    image_param.width, image_param.height);
  ASSERT_TRUE (res_supported != false);

  ImageCaptureCb cb = [this] (uint32_t camera_id, uint32_t image_count,
                              BufferDescriptor buffer,
                              MetaData meta_data) -> void
      { SnapshotCb(camera_id, image_count, buffer, meta_data); };

  meta_array.push_back(meta);
  ret = recorder_.CaptureImage(camera_id_, image_param, 1, meta_array,
                               cb);
  ASSERT_TRUE(ret == NO_ERROR);

  sleep(5);

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
 * SessionWith1080p120fps480p30fpsSnapshotEncTrack: This test will test session with one 1080p
 * 120fps h264 track.
 * Api test sequence:
 *  - StartCamera
 *  - CreateSession
 *  - CreateVideoTrack
 *  - StartVideoTrack
 *  - Snapshot
 *  - StopSession
 *  - DeleteVideoTrack
 *  - DeleteSession
 *  - StopCamera
 */
TEST_F(RecorderVideoSnapshotGTest, SessionWith1080p120fps480p30fpsSnapshotEncTrack) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  VideoFormat format_type = VideoFormat::kAVC;
  uint32_t width  = 1920;
  uint32_t height = 1080;
  float fps = 120;

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

  //Take snapshot
  ImageParam image_param{};
  image_param.width         = 3840;
  image_param.height        = 2160;
  image_param.image_format  = ImageFormat::kJPEG;
  image_param.image_quality = default_jpeg_quality_;

  std::vector<CameraMetadata> meta_array;
  CameraMetadata meta;

  ret = recorder_.GetDefaultCaptureParam(camera_id_, meta);
  ASSERT_TRUE(ret == NO_ERROR);

  bool res_supported = GtestCommon::ValidateResFromJpegSizes(meta,
    image_param.width, image_param.height);
  ASSERT_TRUE (res_supported != false);

  ImageCaptureCb cb = [this] (uint32_t camera_id, uint32_t image_count,
                              BufferDescriptor buffer,
                              MetaData meta_data) -> void
      { SnapshotCb(camera_id, image_count, buffer, meta_data); };

  meta_array.push_back(meta);
  ret = recorder_.CaptureImage(camera_id_, image_param, 1, meta_array,
                               cb);
  ASSERT_TRUE(ret == NO_ERROR);

  sleep(5);

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
* SessionWith1080p60fps480p30fps4KSnapshotEncTrack: This test will test session
* with one 1080p 60fps h264 track and one 480p 30fps h264 track and a snapshot.
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
TEST_F(RecorderVideoSnapshotGTest, SessionWith1080p60fps480p30fpsSnapshotEncTrack) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  VideoFormat format_type = VideoFormat::kAVC;
  uint32_t width  = 1920;
  uint32_t height = 1080;
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
    StreamDumpInfo dumpinfo = { format_type, video_track_id, session_id,
                                width, height };
    ret = dump_bitstream_.SetUp(dumpinfo);
    ASSERT_TRUE(ret == NO_ERROR);
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

  //Take snapshot
  ImageParam image_param{};
  image_param.width         = 1920;
  image_param.height        = 1080;
  image_param.image_format  = ImageFormat::kJPEG;
  image_param.image_quality = default_jpeg_quality_;

  std::vector<CameraMetadata> meta_array;
  CameraMetadata meta;

  ret = recorder_.GetDefaultCaptureParam(camera_id_, meta);
  ASSERT_TRUE(ret == NO_ERROR);

  bool res_supported = GtestCommon::ValidateResFromJpegSizes(meta,
    image_param.width, image_param.height);
  ASSERT_TRUE (res_supported != false);

  ImageCaptureCb cb = [this] (uint32_t camera_id, uint32_t image_count,
                              BufferDescriptor buffer,
                              MetaData meta_data) -> void
      { SnapshotCb(camera_id, image_count, buffer, meta_data); };

  meta_array.push_back(meta);
  ret = recorder_.CaptureImage(camera_id_, image_param, 1, meta_array,
                               cb);
  ASSERT_TRUE(ret == NO_ERROR);

  sleep(5);

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
* 4KEncCancelCaptureImage: This test will exercise CancelCapture Api during 4K video
* record.
* Api test sequence:
*  - StartCamera
*  - CreateSession
*  - CreateVideoTrack - 4K
*  - StartSession
*   loop Start {
*   --------------------
*   - CaptureImage
*   - CancelCaptureImage
*   ---------------------
*   } loop End
*  - StopSession
*  - DeleteVideoTrack
*  - DeleteSession
*  - StopCamera
*/
TEST_F(RecorderVideoSnapshotGTest, 4KEncCancelCaptureImage) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  camera_start_params_.frame_rate = 30;
  ret = recorder_.StartCamera(camera_id_, camera_start_params_);
  ASSERT_TRUE(ret == NO_ERROR);

  VideoFormat format_type = VideoFormat::kAVC;
  uint32_t width  = 3840;
  uint32_t height = 2160;
  float fps = 30;

  SessionCb session_status_cb = CreateSessionStatusCb();
  uint32_t session_id;
  ret = recorder_.CreateSession(session_status_cb, &session_id);
  ASSERT_TRUE(session_id > 0);
  ASSERT_TRUE(ret == NO_ERROR);
  VideoTrackCreateParam video_track_param{camera_id_, format_type,
                                          width, height, fps};
  video_track_param.low_power_mode = false;
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
                                 void *event_data, size_t event_data_size) {
      VideoTrackEventCb(track_id, event_type, event_data, event_data_size); };

  ret = recorder_.CreateVideoTrack(session_id, video_track_id,
                                   video_track_param, video_track_cb);
  ASSERT_TRUE(ret == NO_ERROR);

  std::vector<uint32_t> track_ids;
  track_ids.push_back(video_track_id);
  sessions_.insert(std::make_pair(session_id, track_ids));

  ret = recorder_.StartSession(session_id);
  ASSERT_TRUE(ret == NO_ERROR);

  sleep(3);

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

  bool res_supported = false;
  // Check Supported Raw YUV snapshot resolutions.
  if (meta.exists(ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS)) {
    entry = meta.find(ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS);
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

  ImageCaptureCb cb = [this] (uint32_t camera_id, uint32_t image_count,
                                BufferDescriptor buffer,
                                MetaData meta_data) -> void
      { SnapshotCb(camera_id, image_count, buffer, meta_data); };

  meta_array.push_back(meta);

  // take snapshot and cancel it in loop.
  for(uint32_t i = 1; i <= iteration_count_; i++) {

    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
        test_info_->name(), i);

    ret = recorder_.CaptureImage(camera_id_, image_param, 1, meta_array, cb);
    ASSERT_TRUE(ret == NO_ERROR);

    auto ran = std::rand() % 3;
    auto sleep_time = (ran == 0) ? ran : ran + 1;
    sleep(sleep_time);

    ret = recorder_.CancelCaptureImage(camera_id_);
    ASSERT_TRUE(ret == NO_ERROR);
  }

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

  dump_bitstream_.CloseAll();
  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());
}

/*
* 4KVideo480pVideoAnd4KSnapshot: This test will test session with 4K and 480p
*     Video tracks and parallel 4K Snapshot.
* Api test sequence:
*  - StartCamera
*  - CreateSession
*  - CreateVideoTrack : 4K
*  - CreateVideoTrack : 480p
*  - StartSession
*   loop Start {
*   ------------------
*   - CaptureImage : 4K
*   ------------------
*   } loop End
*  - StopSession
*  - DeleteVideoTrack
*  - DeleteVideoTrack
*  - DeleteSession
*  - DeleteSession
*  - StopCamera
*/
TEST_F(RecorderVideoSnapshotGTest, 4KVideo480pVideoAnd4KSnapshot) {
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

  uint32_t video_track_id1 = 1;
  VideoTrackCreateParam video_track_param{camera_id_, format_type,
                                          width,
                                          height,
                                          30};
   video_track_param.low_power_mode = false;

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
  std::vector<uint32_t> track_ids;

  // Create 4K encode track.
  ret = recorder_.CreateVideoTrack(session_id, video_track_id1,
                                     video_track_param, video_track_cb);
  ASSERT_TRUE(ret == NO_ERROR);

  if (dump_bitstream_.IsEnabled()) {
    StreamDumpInfo dumpinfo = { format_type, video_track_id1, session_id,
      video_track_param.width, video_track_param.height };
    ret = dump_bitstream_.SetUp(dumpinfo);
    ASSERT_TRUE(ret == NO_ERROR);
  }

  track_ids.push_back(video_track_id1);

  uint32_t video_track_id2 = 2;
  video_track_param.width         = 640;
  video_track_param.height        = 480;
  video_track_param.low_power_mode = false;

  video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
      std::vector<BufferDescriptor> buffers,
      std::vector<MetaData> meta_buffers) {
        VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
      };

  // Create 480p encode track.
  ret = recorder_.CreateVideoTrack(session_id, video_track_id2,
                                   video_track_param, video_track_cb);
  ASSERT_TRUE(ret == NO_ERROR);

  if (dump_bitstream_.IsEnabled()) {
    StreamDumpInfo dumpinfo = { format_type, video_track_id2, session_id,
      video_track_param.width, video_track_param.height };
    ret = dump_bitstream_.SetUp(dumpinfo);
  }

  track_ids.push_back(video_track_id2);

  sessions_.insert(std::make_pair(session_id, track_ids));

  ret = recorder_.StartSession(session_id);
  ASSERT_TRUE(ret == NO_ERROR);

  // Record for sometime
  sleep(5);

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

  bool res_supported = false;

  // Check Supported Raw YUV snapshot resolutions.
  if (meta.exists(ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS)) {
    entry = meta.find(ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS);
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

  ImageCaptureCb cb = [this] (uint32_t camera_id, uint32_t image_count,
                              BufferDescriptor buffer,
                              MetaData meta_data) -> void
    { SnapshotCb(camera_id, image_count, buffer, meta_data); };

  meta_array.push_back(meta);

  // take 4K snapshots after every 2 seconds while 4K & 480p recording
  // is going on.
  for(uint32_t i = 1; i <= iteration_count_; i++) {

    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
        test_info_->name(), i);

    ret = recorder_.CaptureImage(camera_id_, image_param, 1, meta_array,
                               cb);
    ASSERT_TRUE(ret == NO_ERROR);

    sleep(2);
  }

  ret = recorder_.StopSession(session_id, false);
  ASSERT_TRUE(ret == NO_ERROR);

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
* EcodingPreBuffer1080p: This test will start caching 5 seconds
* of 1080p video ES. After an event(image capture) is triggered it will
* store the accumulated history along with 5 seconds of video after
* the event.
* Api test sequence:
*  - StartCamera
*  - CreateSession
*  - CreateVideoTrack
*  - StartVideoTrack
*  - CaptureImage
*  - StopSession
*  - DeleteVideoTrack
*  - DeleteSession
*  - StopCamera
*/
TEST_F(RecorderVideoSnapshotGTest, EncodingPreBuffer1080p) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  VideoFormat format_type = VideoFormat::kAVC;
  uint32_t width  = 1920;
  uint32_t height = 1080;
  float fps = 30;
  AVQueue *av_queue = NULL;
  size_t history_length_ms = 5000;
  size_t frame_duration_ms = 1000 / fps;
  size_t queue_size = (history_length_ms / frame_duration_ms) * 2;

  ASSERT_TRUE(0 < AVQueueInit(&av_queue, REALTIME, queue_size + 1, queue_size));

  camera_start_params_.frame_rate = fps;
  ret = recorder_.StartCamera(camera_id_, camera_start_params_);
  ASSERT_TRUE(ret == NO_ERROR);

  SessionCb session_status_cb = CreateSessionStatusCb();
  uint32_t session_id;
  ret = recorder_.CreateSession(session_status_cb, &session_id);
  ASSERT_TRUE(session_id > 0);
  ASSERT_TRUE(ret == NO_ERROR);

  ImageParam image_param{};
  image_param.width         = width;
  image_param.height        = height;
  image_param.image_format  = ImageFormat::kJPEG;
  image_param.image_quality = default_jpeg_quality_;

  std::vector<CameraMetadata> meta_array;
  ImageCaptureCb cb = [this] (uint32_t camera_id, uint32_t image_count,
                              BufferDescriptor buffer, MetaData meta_data) ->
                              void { SnapshotCb(camera_id, image_count,
                                                buffer, meta_data); };
  VideoTrackCreateParam video_track_param{camera_id_, format_type,
                                          width, height, fps};
  uint32_t video_track_id = 1;
  video_track_param.codec_param.avc.insert_aud_delimiter = false;
  StreamDumpInfo dumpinfo = { format_type, video_track_id, session_id,
                              width, height };
  ret = dump_bitstream_.SetUp(dumpinfo);
  ASSERT_TRUE(ret == NO_ERROR);

  TrackCb video_track_cb;
  video_track_cb.data_cb = [&, session_id] (uint32_t track_id,
      std::vector<BufferDescriptor> buffers,
      std::vector<MetaData> meta_buffers) {
        VideoCachedDataCb(session_id, track_id, buffers, meta_buffers,
                          format_type, av_queue);
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

  // Start filling in video cache
  sleep((history_length_ms / 1000) * 2);

  //Event trigger
  ret = recorder_.CaptureImage(camera_id_, image_param, 1, meta_array,
                               cb);
  ASSERT_TRUE(ret == NO_ERROR);

  //Cache history length of video after event trigger
  sleep(history_length_ms / 1000);

  ret = recorder_.StopSession(session_id, false);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteVideoTrack(session_id, video_track_id);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteSession(session_id);
  ASSERT_TRUE(ret == NO_ERROR);

  ClearSessions();

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DumpQueue(av_queue, dump_bitstream_.GetFileFd(video_track_id,
    session_id));
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  dump_bitstream_.CloseAll();

  if (NULL != av_queue) {
    AVQueueFree(&av_queue, AVFreePacket);
    av_queue = NULL;
  }
  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
          test_info_->test_case_name(), test_info_->name());
}

/*
* SessionsWith1440pEncAndLinked1440pYUVTrackAndSessionWith1440Enc:
*   This test will test one session with one 1440p Enc track & one linked
*   1440p YUV track and second session with one 1440p Enc track.
* Api test sequence:
*  - StartCamera
*   loop Start {
*   ------------------
*   - CreateSession
*   - CreateVideoTrack - AVC Master
*   - CreateVideoTrack - YUV Linked
*   - StartSession
*   - CreateSession
*   - CreateVideoTrack - AVC
*   - StartSession
*   - TakeSnapshot
*   - StopSession
*   - DeleteVideoTrack - AVC
*   - DeleteSession
*   - StopSession
*   - DeleteVideoTrack - Linked
*   - DeleteVideoTrack - Master
*   - DeleteSession
*   ------------------
*   } loop End
*  - StopCamera
*/
TEST_F(RecorderVideoSnapshotGTest,
    SessionsWith1440pEncAndLinked1440pYUVTrackAndSessionWith1440Enc) {

  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  uint32_t width  = 1920;
  uint32_t height = 1440;

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.StartCamera(camera_id_, camera_start_params_);
  ASSERT_TRUE(ret == NO_ERROR);

  VideoTrackCreateParam video_track_param{camera_id_, VideoFormat::kAVC,
                                          1920, 1440, 30};
  uint32_t session1_trackid_avc = 1;
  uint32_t session1_trackid_yuv = 2;
  uint32_t session2_trackid_avc = 3;

  for (uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
        test_info_->name(), i);

    // Session 1 Track: 1920x1440p @30 AVC
    uint32_t session_id1;
    SessionCb session_status_cb = CreateSessionStatusCb();
    ret = recorder_.CreateSession(session_status_cb, &session_id1);
    ASSERT_TRUE(session_id1 > 0);
    ASSERT_TRUE(ret == NO_ERROR);

    if (dump_bitstream_.IsEnabled()) {
      StreamDumpInfo dumpinfo_track1 = { VideoFormat::kAVC, session1_trackid_avc,
                                         session_id1, width, height };
      ret = dump_bitstream_.SetUp(dumpinfo_track1);
      ASSERT_TRUE(ret == NO_ERROR);

      StreamDumpInfo dumpinfo_track2 = { VideoFormat::kAVC, session2_trackid_avc,
                                         session_id1, width, height };
      ret = dump_bitstream_.SetUp(dumpinfo_track2);
      ASSERT_TRUE(ret == NO_ERROR);
    }
    TrackCb video_track_cb;
    video_track_cb.data_cb = [&] (uint32_t track_id,
                              std::vector<BufferDescriptor> buffers,
                              std::vector<MetaData> meta_buffers) {
        VideoTrackEncDataCb(session_id1, track_id, buffers,
                               meta_buffers);
     };

    video_track_cb.event_cb = [&] (uint32_t track_id, EventType event_type,
        void *event_data, size_t event_data_size) { VideoTrackEventCb(track_id,
        event_type, event_data, event_data_size); };

    video_track_param.camera_id     = camera_id_;
    video_track_param.width         = width;
    video_track_param.height        = height;
    video_track_param.frame_rate    = 30;
    video_track_param.format_type   = VideoFormat::kAVC;

    ret = recorder_.CreateVideoTrack(session_id1, session1_trackid_avc,
                                     video_track_param, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    // Session 1 Track: 1920x1440p @30 Linked YUV
    VideoExtraParam extra_param;
    SourceVideoTrack surface_video_copy;
    surface_video_copy.source_track_id = session1_trackid_avc;
    extra_param.Update(QMMF_SOURCE_VIDEO_TRACK_ID, surface_video_copy);

    video_track_param.camera_id     = camera_id_;
    video_track_param.width         = width;
    video_track_param.height        = height;
    video_track_param.frame_rate    = 30;
    video_track_param.format_type   = VideoFormat::kYUV;

    video_track_cb.data_cb = [&] (uint32_t track_id,
                              std::vector<BufferDescriptor> buffers,
                              std::vector<MetaData> meta_buffers) {
        VideoTrackYUVDataCb(session_id1, track_id, buffers, meta_buffers); };

    ret = recorder_.CreateVideoTrack(session_id1, session1_trackid_yuv,
                                     video_track_param, extra_param,
                                     video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    // Store Session 1 tracks
    std::vector<uint32_t> session1_track_ids;
    session1_track_ids.push_back(session1_trackid_avc);
    session1_track_ids.push_back(session1_trackid_yuv);
    sessions_.insert(std::make_pair(session_id1, session1_track_ids));

    ret = recorder_.StartSession(session_id1);
    ASSERT_TRUE(ret == NO_ERROR);

    // Session 2 Track: 1920x1440p @30 AVC
    uint32_t session_id2;
    ret = recorder_.CreateSession(session_status_cb, &session_id2);
    ASSERT_TRUE(session_id2 > 0);
    ASSERT_TRUE(ret == NO_ERROR);

    video_track_param.camera_id     = camera_id_;
    video_track_param.width         = width;
    video_track_param.height        = height;
    video_track_param.frame_rate    = 30;
    video_track_param.format_type   = VideoFormat::kAVC;

    video_track_cb.data_cb = [&] (uint32_t track_id,
                              std::vector<BufferDescriptor> buffers,
                              std::vector<MetaData> meta_buffers) {
        VideoTrackEncDataCb(session_id2, track_id, buffers, meta_buffers);
    };

    ret = recorder_.CreateVideoTrack(session_id2, session2_trackid_avc,
                                     video_track_param, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    // Store Session 2 tracks
    std::vector<uint32_t> session2_track_ids;
    session2_track_ids.push_back(session2_trackid_avc);
    sessions_.insert(std::make_pair(session_id2, session2_track_ids));

    ret = recorder_.StartSession(session_id2);
    ASSERT_TRUE(ret == NO_ERROR);

    // Let session run for record_duration_, during this time buffer with valid
    // data would be received in track callback (VideoTrackDataCb).
    sleep(record_duration_ / 2);

    std::vector<CameraMetadata> meta_array;
    CameraMetadata meta;

    ret = recorder_.GetDefaultCaptureParam(camera_id_, meta);
    ASSERT_TRUE(ret == NO_ERROR);

    // Take a snapshot in the middle of recording time
    ImageParam image_param = {};
    image_param.image_format  = ImageFormat::kJPEG;
    image_param.image_quality = default_jpeg_quality_;
    GtestCommon::GetMaxSupportedCameraRes(meta, image_param.width,
        image_param.height);

    ImageCaptureCb cb = [this] (uint32_t camera_id, uint32_t image_count,
                                BufferDescriptor buffer,
                                MetaData meta_data) -> void
        { SnapshotCb(camera_id, image_count, buffer, meta_data); };

    meta_array.push_back(meta);
    ret = recorder_.CaptureImage(camera_id_, image_param, 1, meta_array, cb);
    ASSERT_TRUE(ret == NO_ERROR);

    sleep(record_duration_ / 2);

    ret = recorder_.StopSession(session_id2, false);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id2, session2_trackid_avc);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteSession(session_id2);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.StopSession(session_id1, false);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id1, session1_trackid_yuv);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id1, session1_trackid_avc);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteSession(session_id1);
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
* SessionsWith1440pEncAndLinked1440pYUVTrackAndSessionWith1440EncWithEISAndLCACEnable:
*   This test will test one session with one 1440p Enc track & one linked
*   1440p YUV track and second session with one 1440p Enc track with LCAC
*   YUV and EIS Enable.
* API test sequence:
*  - StartCamera
*   loop Start {
*   ------------------
*   - CreateSession
*   - CreateVideoTrack - AVC Master
*   - CreateVideoTrack - YUV Linked
*   - StartSession
*   - CreateSession
*   - CreateVideoTrack - AVC
*   - StartSession
*   - Enable LCAC and EIS
*   - TakeSnapshot
*   - StopSession
*   - DeleteVideoTrack - AVC
*   - DeleteSession
*   - StopSession
*   - DeleteVideoTrack - Linked
*   - DeleteVideoTrack - Master
*   - DeleteSession
*   ------------------
*   } loop End
*  - StopCamera
*/
TEST_F(RecorderVideoSnapshotGTest,
       SessionsWith1440pEncAndLinked1440pYUVTrackAndSessionWith1440EncWithEISAndLCACEnable) {
  fprintf(stderr, "\n---------- Run Test %s.%s ------------\n",
          test_info_->test_case_name(), test_info_->name());

  uint32_t width = 1920;
  uint32_t height = 1440;

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.StartCamera(camera_id_, camera_start_params_);
  ASSERT_TRUE(ret == NO_ERROR);
  VideoTrackCreateParam video_track_param{camera_id_, VideoFormat::kAVC, 1920,
                                          1440, 30};

  uint32_t session1_trackid_avc = 1;
  uint32_t session1_trackid_yuv = 2;
  uint32_t session2_trackid_avc = 3;

  for (uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr, "test iteration = %d/%d\n", i, iteration_count_);

    // Session 1 Track: 1920x1440p @30 AVC
    uint32_t session_id1;
    SessionCb session_status_cb = CreateSessionStatusCb();
    ret = recorder_.CreateSession(session_status_cb, &session_id1);
    ASSERT_TRUE(session_id1 > 0);
    ASSERT_TRUE(ret == NO_ERROR);

    if (dump_bitstream_.IsEnabled()) {
      StreamDumpInfo dumpinfo_track1 = {VideoFormat::kAVC, session_id1, session1_trackid_avc,
                                        width, height};
      ret = dump_bitstream_.SetUp(dumpinfo_track1);
      ASSERT_TRUE(ret == NO_ERROR);
    }
    TrackCb video_track_cb;
    video_track_cb.data_cb = [&](uint32_t track_id,
                                 std::vector<BufferDescriptor> buffers,
                                 std::vector<MetaData> meta_buffers) {
      VideoTrackEncDataCb(session_id1, track_id, buffers, meta_buffers);
    };

    video_track_cb.event_cb = [&](uint32_t track_id, EventType event_type,
                                  void *event_data, size_t event_data_size) {
      VideoTrackEventCb(track_id, event_type, event_data, event_data_size);
    };

    video_track_param.camera_id = camera_id_;
    video_track_param.width = width;
    video_track_param.height = height;
    video_track_param.frame_rate = 30;
    video_track_param.format_type = VideoFormat::kAVC;

    ret = recorder_.CreateVideoTrack(session_id1, session1_trackid_avc,
                                     video_track_param, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    // Session 1 Track: 1920x1440p @30 Linked YUV
    VideoExtraParam extra_param;
    SourceVideoTrack surface_video_copy;
    surface_video_copy.source_track_id = session1_trackid_avc;
    extra_param.Update(QMMF_SOURCE_VIDEO_TRACK_ID, surface_video_copy);

    video_track_param.camera_id = camera_id_;
    video_track_param.width = width;
    video_track_param.height = height;
    video_track_param.frame_rate = 30;
    video_track_param.format_type = VideoFormat::kYUV;

    video_track_cb.data_cb = [&](uint32_t track_id,
                                 std::vector<BufferDescriptor> buffers,
                                 std::vector<MetaData> meta_buffers) {
      VideoTrackYUVDataCb(session_id1, track_id, buffers, meta_buffers); };

    ret = recorder_.CreateVideoTrack(session_id1, session1_trackid_yuv,
                                     video_track_param, extra_param,
                                     video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);
    CameraMetadata meta;
    // Enable EIS before Start Session
    ret = recorder_.GetCameraParam(camera_id_, meta);
    ASSERT_TRUE(ret == NO_ERROR);
    uint8_t vstab_mode = ANDROID_CONTROL_VIDEO_STABILIZATION_MODE_ON;
    meta.update(ANDROID_CONTROL_VIDEO_STABILIZATION_MODE, &vstab_mode, 1);
    ret = recorder_.SetCameraParam(camera_id_, meta);
    ASSERT_TRUE(ret == NO_ERROR);

    // Enable YUV LCAC
    uint8_t enable_lcac = 1;
    ret = meta.update(  QCAMERA3_LCAC_PROCESSING_ENABLE, &enable_lcac, 1);
    ASSERT_TRUE(ret == NO_ERROR);
    ret = recorder_.SetCameraParam(camera_id_, meta);
    ASSERT_TRUE(ret == NO_ERROR);

    // Store Session 1 tracks
    std::vector<uint32_t> session1_track_ids;
    session1_track_ids.push_back(session1_trackid_avc);
    session1_track_ids.push_back(session1_trackid_yuv);
    sessions_.insert(std::make_pair(session_id1, session1_track_ids));

    ret = recorder_.StartSession(session_id1);
    ASSERT_TRUE(ret == NO_ERROR);
    // Let session run for 5 sec, during this time buffer with valid
    // data would be received in track callback (VideoTrackDataCb).
    sleep(5);
    // Session 2 Track: 1920x1440p @30 AVC
    uint32_t session_id2;
    ret = recorder_.CreateSession(session_status_cb, &session_id2);
    ASSERT_TRUE(session_id2 > 0);
    ASSERT_TRUE(ret == NO_ERROR);

    if (dump_bitstream_.IsEnabled()) {
      StreamDumpInfo dumpinfo_track2 = {VideoFormat::kAVC, session_id2, session2_trackid_avc,
                                        width, height};
      ret = dump_bitstream_.SetUp(dumpinfo_track2);
      ASSERT_TRUE(ret == NO_ERROR);
    }

    video_track_param.camera_id = camera_id_;
    video_track_param.width = width;
    video_track_param.height = height;
    video_track_param.frame_rate = 30;
    video_track_param.format_type = VideoFormat::kAVC;

    video_track_cb.data_cb = [&](uint32_t track_id,
                                 std::vector<BufferDescriptor> buffers,
                                 std::vector<MetaData> meta_buffers) {
      VideoTrackEncDataCb(session_id2, track_id, buffers, meta_buffers);
    };

    ret = recorder_.CreateVideoTrack(session_id2, session2_trackid_avc,
                                     video_track_param, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);
    // Store Session 2 tracks
    std::vector<uint32_t> session2_track_ids;
    session2_track_ids.push_back(session2_trackid_avc);
    sessions_.insert(std::make_pair(session_id2, session2_track_ids));
    ret = recorder_.StartSession(session_id2);
    ASSERT_TRUE(ret == NO_ERROR);

    // Let session run for record_duration_, during this time buffer with valid
    // data would be received in track callback (VideoTrackDataCb).
    sleep(record_duration_ / 2);

    std::vector<CameraMetadata> meta_array;

    ret = recorder_.GetDefaultCaptureParam(camera_id_, meta);
    ASSERT_TRUE(ret == NO_ERROR);

    // Take a snapshot in the middle of recording time
    ImageParam image_param = {};
    image_param.image_format = ImageFormat::kJPEG;
    image_param.image_quality = default_jpeg_quality_;
    GtestCommon::GetMaxSupportedCameraRes(meta, image_param.width,
      image_param.height);

    ImageCaptureCb cb = [this](uint32_t camera_id, uint32_t image_count,
                               BufferDescriptor buffer,
                               MetaData meta_data) -> void {
      SnapshotCb(camera_id, image_count, buffer, meta_data);
    };

    meta_array.push_back(meta);
    ret = recorder_.CaptureImage(camera_id_, image_param, 1, meta_array, cb);
    ASSERT_TRUE(ret == NO_ERROR);

    sleep(record_duration_ / 2);

    ret = recorder_.StopSession(session_id2, false);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id2, session2_trackid_avc);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteSession(session_id2);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.StopSession(session_id1, false);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id1, session1_trackid_yuv);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id1, session1_trackid_avc);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteSession(session_id1);
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
* ThreeSessionsWith1440pEncAnd1440pYUVTrack: This test will verify
*                                            three sessions each with 1440p
*                                             track.
* Api test sequence:
*  - StartCamera
*   loop Start {
*   ------------------
*   - CreateSession
*   - CreateVideoTrack
*   - StartSession
*   - CreateSession
*   - CreateVideoTrack
*   - StartSession
*   - CreateSession
*   - CreateVideoTrack
*   - StartSession
*   - TakeSnapshot
*   - StopSession
*   - DeleteVideoTrack
*   - DeleteSession
*   - StopSession
*   - DeleteVideoTrack
*   - DeleteSession
*   - StopSession
*   - DeleteVideoTrack
*   - DeleteSession
*   ------------------
*   } loop End
*  - StopCamera
*/
TEST_F(RecorderVideoSnapshotGTest, ThreeSessionsWith1440pEncAnd1440pYUVTrack) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  uint32_t width  = 1920;
  uint32_t height = 1440;

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.StartCamera(camera_id_, camera_start_params_);
  ASSERT_TRUE(ret == NO_ERROR);

  SessionCb session_status_cb = CreateSessionStatusCb();
  VideoTrackCreateParam video_track_param{camera_id_, VideoFormat::kAVC,
                                          1920, 1440, 30};
  uint32_t session1_track_id = 1;
  uint32_t session2_track_id = 2;
  uint32_t session3_track_id = 3;

  for (uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
        test_info_->name(), i);

    // Session 1 Track: 1920x1440p @30 Encode
    uint32_t session_id1;
    ret = recorder_.CreateSession(session_status_cb, &session_id1);
    ASSERT_TRUE(session_id1 > 0);
    ASSERT_TRUE(ret == NO_ERROR);

    if (dump_bitstream_.IsEnabled()) {
        StreamDumpInfo dumpinfo_track1 = { VideoFormat::kAVC, session1_track_id,
                                           session_id1, width, height };
        ret = dump_bitstream_.SetUp(dumpinfo_track1);
        ASSERT_TRUE(ret == NO_ERROR);

        StreamDumpInfo dumpinfo_track2 = { VideoFormat::kAVC, session1_track_id,
                                           session_id1, width, height };
        ret = dump_bitstream_.SetUp(dumpinfo_track2);
        ASSERT_TRUE(ret == NO_ERROR);
    }
    TrackCb video_track_cb;
    video_track_cb.data_cb = [&] (uint32_t track_id,
                              std::vector<BufferDescriptor> buffers,
                              std::vector<MetaData> meta_buffers) {
        VideoTrackEncDataCb(session_id1, track_id, buffers, meta_buffers);
    };

    video_track_cb.event_cb = [&] (uint32_t track_id, EventType event_type,
        void *event_data, size_t event_data_size) { VideoTrackEventCb(track_id,
        event_type, event_data, event_data_size); };

    video_track_param.camera_id     = camera_id_;
    video_track_param.width         = width;
    video_track_param.height        = height;
    video_track_param.frame_rate    = 30;
    video_track_param.format_type   = VideoFormat::kAVC;

    ret = recorder_.CreateVideoTrack(session_id1, session1_track_id,
                                      video_track_param, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    std::vector<uint32_t> session1_track_ids;
    session1_track_ids.push_back(session1_track_id);
    sessions_.insert(std::make_pair(session_id1, session1_track_ids));

    ret = recorder_.StartSession(session_id1);
    ASSERT_TRUE(ret == NO_ERROR);

    // Session 2 Track: 1920x1440p @30 YUV
    uint32_t session_id2;
    ret = recorder_.CreateSession(session_status_cb, &session_id2);
    ASSERT_TRUE(session_id2 > 0);
    ASSERT_TRUE(ret == NO_ERROR);

    video_track_cb.data_cb = [&] (uint32_t track_id,
                              std::vector<BufferDescriptor> buffers,
                              std::vector<MetaData> meta_buffers) {
        VideoTrackYUVDataCb(session_id2, track_id, buffers, meta_buffers); };

    video_track_param.camera_id     = camera_id_;
    video_track_param.width         = width;
    video_track_param.height        = height;
    video_track_param.frame_rate    = 30;
    video_track_param.format_type   = VideoFormat::kYUV;

    ret = recorder_.CreateVideoTrack(session_id2, session2_track_id,
                                     video_track_param, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    std::vector<uint32_t> session2_track_ids;
    session2_track_ids.push_back(session2_track_id);
    sessions_.insert(std::make_pair(session_id2, session2_track_ids));

    ret = recorder_.StartSession(session_id2);
    ASSERT_TRUE(ret == NO_ERROR);

    // Session 3 Track: 1920x1440p @30 Encode
    uint32_t session_id3;
    ret = recorder_.CreateSession(session_status_cb, &session_id3);
    ASSERT_TRUE(session_id3 > 0);
    ASSERT_TRUE(ret == NO_ERROR);

    video_track_cb.data_cb = [&] (uint32_t track_id,
                              std::vector<BufferDescriptor> buffers,
                              std::vector<MetaData> meta_buffers) {
        VideoTrackEncDataCb(session_id3, track_id, buffers, meta_buffers);
    };

    video_track_param.camera_id     = camera_id_;
    video_track_param.width         = width;
    video_track_param.height        = height;
    video_track_param.frame_rate    = 30;
    video_track_param.format_type   = VideoFormat::kAVC;

    ret = recorder_.CreateVideoTrack(session_id3, session3_track_id,
                                      video_track_param, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    std::vector<uint32_t> session3_track_ids;
    session3_track_ids.push_back(session3_track_id);
    sessions_.insert(std::make_pair(session_id3, session3_track_ids));

    ret = recorder_.StartSession(session_id3);
    ASSERT_TRUE(ret == NO_ERROR);

    // Let session run for record_duration_, during this time buffer with valid
    // data would be received in track callback (VideoTrackDataCb).
    sleep(record_duration_ / 2);

    std::vector<CameraMetadata> meta_array;
    CameraMetadata meta;

    ret = recorder_.GetDefaultCaptureParam(camera_id_, meta);
    ASSERT_TRUE(ret == NO_ERROR);

    // Take a snapshot in the middle of recording time
    ImageParam image_param = {};
    image_param.image_format  = ImageFormat::kJPEG;
    image_param.image_quality = default_jpeg_quality_;
    GtestCommon::GetMaxSupportedCameraRes(meta, image_param.width,
      image_param.height);

    ImageCaptureCb cb = [this] (uint32_t camera_id, uint32_t image_count,
                                BufferDescriptor buffer,
                                MetaData meta_data) -> void
        { SnapshotCb(camera_id, image_count, buffer, meta_data); };

    meta_array.push_back(meta);
    ret = recorder_.CaptureImage(camera_id_, image_param, 1, meta_array, cb);
    ASSERT_TRUE(ret == NO_ERROR);

    sleep(record_duration_ / 2);

    ret = recorder_.StopSession(session_id3, false);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id3, session3_track_id);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteSession(session_id3);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.StopSession(session_id2, false);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id2, session2_track_id);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteSession(session_id2);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.StopSession(session_id1, false);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id1, session1_track_id);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteSession(session_id1);
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
* SessionWith1440EncWithEISAndLCACEnableAnd12MPSnapshot:
*                                This test will test session with one 1440p
*                                30fps h264 track and EIS and LCAC is enable and
*                                12 MP snapshot.
* API test sequence:
*  - StartCamera
*  - CreateSession
*  - CreateVideoTrack
*  - Enable EIS
*  - Start Session
*  - Enable LCAC
*  - Take 12 MP snapshot
*  - StopSession
*  - DeleteVideoTrack
*  - DeleteSession
*  - StopCamera
*/
TEST_F(RecorderVideoSnapshotGTest, SessionWith1440EncWithEISAndLCACEnableAnd12MPSnapshot) {
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
    auto ret = recorder_.GetCameraParam(camera_id_, meta);
    ASSERT_TRUE(ret == NO_ERROR);

    uint8_t vstab_mode = ANDROID_CONTROL_VIDEO_STABILIZATION_MODE_ON;
    meta.update(ANDROID_CONTROL_VIDEO_STABILIZATION_MODE, &vstab_mode, 1);
    ret = recorder_.SetCameraParam(camera_id_, meta);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    // Let session run for record_duration_, during this time buffer with valid
    // data would be received in track callback (VideoTrackDataCb).
    sleep(record_duration_ / 2);

    auto status = recorder_.GetCameraParam(camera_id_, meta);
    if (NO_ERROR == status) {
      uint8_t enable_lcac = 1;
      ret = meta.update(  QCAMERA3_LCAC_PROCESSING_ENABLE, &enable_lcac, 1);
      ASSERT_TRUE(ret == NO_ERROR);
      ret = recorder_.SetCameraParam(camera_id_, meta);
      ASSERT_TRUE(ret == NO_ERROR);
    }
    ret = recorder_.GetDefaultCaptureParam(camera_id_, meta);
    ASSERT_TRUE(ret == NO_ERROR);

    // Take a snapshot in the middle of recording time
    ImageParam image_param = {};
    image_param.image_format = ImageFormat::kJPEG;
    image_param.image_quality = default_jpeg_quality_;
    GtestCommon::GetMaxSupportedCameraRes(meta, image_param.width,
      image_param.height);

    std::vector<CameraMetadata> meta_array;

    ImageCaptureCb cb = [this](uint32_t camera_id, uint32_t image_count,
                               BufferDescriptor buffer,
                               MetaData meta_data) -> void {
      SnapshotCb(camera_id, image_count, buffer, meta_data);
    };
    meta_array.push_back(meta);

    ret = recorder_.CaptureImage(camera_id_, image_param, 1, meta_array, cb);

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

/*
* 1080pVideo4KVideoTypeSnapshot: This test will test session with 1080p
*     Video track and parallel 4K Snapshot with included active video requests.
* Api test sequence:
*  - StartCamera
*  - CreateSession
*  - CreateVideoTrack : 1080p
*  - StartSession
*  - Set video snapshot mode
*   loop Start {
*   ------------------
*   - CaptureImage : 4K
*   ------------------
*   } loop End
*  - StopSession
*  - DeleteVideoTrack
*  - DeleteSession
*  - StopCamera
*/
TEST_F(RecorderVideoSnapshotGTest, 1080pVideo4KVideoTypeSnapshot) {
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
  uint32_t session_id;
  ret = recorder_.CreateSession(session_status_cb, &session_id);
  ASSERT_TRUE(session_id > 0);
  ASSERT_TRUE(ret == NO_ERROR);

  uint32_t video_track_id1 = 1;
  VideoTrackCreateParam video_track_param{camera_id_, format_type,
                                          width, height, 30};
   video_track_param.low_power_mode = false;

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
  std::vector<uint32_t> track_ids;

  // Create 1080p encode track.
  ret = recorder_.CreateVideoTrack(session_id, video_track_id1,
                                     video_track_param, video_track_cb);
  ASSERT_TRUE(ret == NO_ERROR);

  if (dump_bitstream_.IsEnabled()) {
    StreamDumpInfo dumpinfo = { format_type, video_track_id1, session_id,
      video_track_param.width, video_track_param.height };
    ret = dump_bitstream_.SetUp(dumpinfo);
    ASSERT_TRUE(ret == NO_ERROR);
  }

  track_ids.push_back(video_track_id1);

  sessions_.insert(std::make_pair(session_id, track_ids));

  ret = recorder_.StartSession(session_id);
  ASSERT_TRUE(ret == NO_ERROR);

  // Record for sometime
  sleep(1);

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

  bool res_supported = false;

  // Check Supported Raw YUV snapshot resolutions.
  if (meta.exists(ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS)) {
    entry = meta.find(ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS);
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

  ImageCaptureCb cb = [this] (uint32_t camera_id, uint32_t image_count,
                              BufferDescriptor buffer,
                              MetaData meta_data) -> void
    { SnapshotCb(camera_id, image_count, buffer, meta_data); };

  meta_array.push_back(meta);

  ImageConfigParam image_config;
  SnapshotType snapshot_type;
  snapshot_type.type = SnapshotMode::kVideo;
  image_config.Update(QMMF_SNAPSHOT_TYPE, snapshot_type);

  ret = recorder_.ConfigImageCapture(camera_id_, image_config);
  ASSERT_TRUE(ret == NO_ERROR);

  // take 4K snapshots every second while 1080p video recording
  // is going on.
  for(uint32_t i = 0; i < iteration_count_; i++) {

    fprintf(stderr,"Snapshot iteration = %d/%d\n", i, iteration_count_);
    ret = recorder_.CaptureImage(camera_id_, image_param, 1, meta_array, cb);
    ASSERT_TRUE(ret == NO_ERROR);

    sleep(1);
  }

  ret = recorder_.StopSession(session_id, false);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteVideoTrack(session_id, video_track_id1);
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
* SmoothZoomWith1080pEncTrack4KSnapshotFullFOV: This test will test SmoothZoom
*     with 1080p h264 track.
* Api test sequence:
*  - StartCamera
*   loop Start {
*   ------------------
*   - CreateSession
*   - CreateVideoTrack
*   - StartVideoTrack
*   - CaptureImage
*   - SmoothZoom apply
*   - CaptureImage
*   - StopSession
*   - DeleteVideoTrack
*   - DeleteSession
*   ------------------
*   } loop End
*  - StopCamera
*/
TEST_F(RecorderVideoSnapshotGTest, SmoothZoomWith1080pEncTrack4KSnapshotFullFOV) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  float zoom = 2.5f;

  VideoFormat format_type = VideoFormat::kAVC;
  uint32_t width  = 1920;
  uint32_t height = 1080;

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

    ret = recorder_.CreateVideoTrack(session_id, video_track_id,
                                      video_track_param, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    std::vector<uint32_t> track_ids;
    track_ids.push_back(video_track_id);
    sessions_.insert(std::make_pair(session_id, track_ids));

    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    // Take snapshot before zoom is applied on video track.
    ImageParam image_param{};
    image_param.width         = 3840;
    image_param.height        = 2160;
    image_param.image_format  = ImageFormat::kJPEG;
    image_param.image_quality = 95;

    CameraMetadata meta;
    ret = recorder_.GetDefaultCaptureParam(camera_id_, meta);
    ASSERT_TRUE(ret == NO_ERROR);

    std::vector<CameraMetadata> meta_array;
    meta_array.push_back(meta);

    ImageCaptureCb cb = [this] (uint32_t camera_id, uint32_t image_count,
                              BufferDescriptor buffer,
                              MetaData meta_data) -> void
      { SnapshotCb(camera_id, image_count, buffer, meta_data); };


    ret = recorder_.CaptureImage(camera_id_, image_param, 1, meta_array,
                                 cb);
    ASSERT_TRUE(ret == NO_ERROR);

    // Record for few seconds.
    sleep(5);

    // Apply zoom on video track.
    CameraMetadata video_meta;
    ret = recorder_.GetCameraParam(camera_id_, video_meta);
    ASSERT_TRUE(ret == NO_ERROR);

    int32_t crop[4];
    int32_t width = 0;
    int32_t height = 0;

    if (video_meta.exists(ANDROID_SCALER_AVAILABLE_RAW_SIZES)) {
      width =
        video_meta.find(ANDROID_SCALER_AVAILABLE_RAW_SIZES).data.i32[0];
      height =
        video_meta.find(ANDROID_SCALER_AVAILABLE_RAW_SIZES).data.i32[1];
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

    // Take full FOV snapshot after zoom is applied on video track.
    ret = recorder_.CaptureImage(camera_id_, image_param, 1, meta_array,
                                 cb);
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
* SmoothZoomWith1080pEncTrack4KSnapshot: This test will test SmoothZoom with
*    1080p h264 track.
* Api test sequence:
*  - StartCamera
*   loop Start {
*   ------------------
*   - CreateSession
*   - CreateVideoTrack
*   - StartVideoTrack
*   - CaptureImage
*   - SmoothZoom apply
*   - CaptureImage - same zoom effect as video
*   - StopSession
*   - DeleteVideoTrack
*   - DeleteSession
*   ------------------
*   } loop End
*  - StopCamera
*/
TEST_F(RecorderVideoSnapshotGTest, SmoothZoomWith1080pEncTrack4KSnapshot) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  float zoom = 2.5f;

  VideoFormat format_type = VideoFormat::kAVC;
  uint32_t width  = 1920;
  uint32_t height = 1080;

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

    ret = recorder_.CreateVideoTrack(session_id, video_track_id,
                                      video_track_param, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    std::vector<uint32_t> track_ids;
    track_ids.push_back(video_track_id);
    sessions_.insert(std::make_pair(session_id, track_ids));

    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    // Take snapshot before zoom is applied on video track.
    ImageParam image_param{};
    image_param.width         = 3840;
    image_param.height        = 2160;
    image_param.image_format  = ImageFormat::kJPEG;
    image_param.image_quality = 95;

    CameraMetadata meta;
    ret = recorder_.GetDefaultCaptureParam(camera_id_, meta);
    ASSERT_TRUE(ret == NO_ERROR);

    std::vector<CameraMetadata> meta_array;
    meta_array.push_back(meta);

    ImageCaptureCb cb = [this] (uint32_t camera_id, uint32_t image_count,
                              BufferDescriptor buffer,
                              MetaData meta_data) -> void
      { SnapshotCb(camera_id, image_count, buffer, meta_data); };


    ret = recorder_.CaptureImage(camera_id_, image_param, 1, meta_array,
                                 cb);
    ASSERT_TRUE(ret == NO_ERROR);

    // Record for few seconds.
    sleep(5);

    // Apply zoom on video track.
    CameraMetadata video_meta;
    ret = recorder_.GetCameraParam(camera_id_, video_meta);
    ASSERT_TRUE(ret == NO_ERROR);

    int32_t crop[4];
    int32_t width = 0;
    int32_t height = 0;

    if (video_meta.exists(ANDROID_SCALER_AVAILABLE_RAW_SIZES)) {
      width =
        video_meta.find(ANDROID_SCALER_AVAILABLE_RAW_SIZES).data.i32[0];
      height =
        video_meta.find(ANDROID_SCALER_AVAILABLE_RAW_SIZES).data.i32[1];
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

    // Apply zoom to snapshot meta.
    ret = meta.update(ANDROID_SCALER_CROP_REGION, crop, 4);
    ASSERT_TRUE(ret == NO_ERROR);

    meta_array.clear();
    meta_array.push_back(meta);

    // Take snapshot after zoom is applied on video track.
    ret = recorder_.CaptureImage(camera_id_, image_param, 1, meta_array,
                                 cb);
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

/* SessionWith1080pEncTrackCaptureChangeFocalLength: This test will test
 * session with one 1080p Enc track and 4K Snapshot. Api test sequence:
 *  - StartCamera
 *   loop Start {
 *   ------------------
 *   - CreateSession
 *   - CreateVideoTrack
 *   - SetCameraFocalLength - 5.0
 *   - StartSession
 *   - SetCameraFocalLength - 4.71
 *   - CaptureImage
 *   - CancelCaptureImage
 *   - StopSession
 *   - DeleteVideoTrack
 *   - DeleteSession
 *   ------------------
 *   } loop End
 *  - StopCamera
 */
TEST_F(RecorderVideoSnapshotGTest, SessionWith1080pEncTrackCaptureChangeFocalLength) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.StartCamera(camera_id_, camera_start_params_);
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
    VideoTrackCreateParam video_track_param{camera_id_, VideoFormat::kAVC,
                                            1920, 1080, 15};
    uint32_t video_track_id = 1;

    if (dump_bitstream_.IsEnabled()) {
      StreamDumpInfo dumpinfo = { VideoFormat::kAVC, video_track_id,
                                  session_id, 1920, 1080 };
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

    ret = SetCameraFocalLength(5.00);
    ASSERT_TRUE(ret == NO_ERROR);

    std::vector<uint32_t> track_ids;
    track_ids.push_back(video_track_id);
    sessions_.insert(std::make_pair(session_id, track_ids));

    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    // Let session run for time record_duration_, during this time buffer with
    // valid data would be received in track callback (VideoTrackYUVDataCb).
    sleep(record_duration_);

    //Take snapshot
    ImageParam image_param{};
    image_param.width         = 3840;
    image_param.height        = 2160;
    image_param.image_format  = ImageFormat::kJPEG;
    image_param.image_quality = 95;

    std::vector<CameraMetadata> meta_array;
    CameraMetadata meta;

    ret = recorder_.GetDefaultCaptureParam(camera_id_, meta);
    ASSERT_TRUE(ret == NO_ERROR);

    bool res_supported = GtestCommon::ValidateResFromJpegSizes(meta,
      image_param.width, image_param.height);
    ASSERT_TRUE (res_supported != false);

    ImageCaptureCb cb = [this] (uint32_t camera_id, uint32_t image_count,
                                BufferDescriptor buffer,
                                MetaData meta_data) -> void
        { SnapshotCb(camera_id, image_count, buffer, meta_data);
          TEST_INFO("%s: Snapshot done!!!", __func__);
          test_wait_.Done();
        };

    uint32_t num_snapshots = 1;
    test_wait_.Reset(num_snapshots);
    float focal_length = 4.83;
    meta.update(ANDROID_LENS_FOCAL_LENGTH, &focal_length, 1);

    meta_array.clear();
    for (uint32_t r = 0; r < num_snapshots; r++) {
      meta_array.push_back(meta);
    }
    {
      std::lock_guard<std::mutex> lock(error_lock_);
      camera_error_ = false;
    }
    ret = recorder_.CaptureImage(camera_id_, image_param, num_snapshots,
                                 meta_array, cb);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = test_wait_.Wait();
    ASSERT_TRUE(ret == NO_ERROR);

    // wait for capture done and remove capture stream
    ret = recorder_.CancelCaptureImage(camera_id_);
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
} //SessionWith1080pYUVCaptureTrackFocalLength
