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
#define LOG_TAG "RecorderImageGTest"

#include "recorder/test/gtest/qmmf_recorder_image_gtest.h"

using namespace qcamera;

/*
* 1080pZSLCapture: This case will test 1080p ZSL capture.
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
TEST_F(RecorderImageGTest, 1080pZSLCapture) {

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
* 4KSnapshotDisableEXIF: This test will test 4K JPEG snapshot.
* Api test sequence:
*  - StartCamera
*  - ConfigImageCapture - Disable EXIF
*   loop Start {
*   ------------------
*   - CaptureImage - JPEG
*   ------------------
*   } loop End
*  - StopCamera
*/
TEST_F(RecorderImageGTest, 4KSnapshotDisableEXIF) {
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

  TrackCb preview_track_cb;
  preview_track_cb.event_cb =
      [this] (uint32_t track_id, EventType event_type,
              void *event_data, size_t event_data_size) -> void {
      VideoTrackEventCb(track_id, event_type, event_data, event_data_size); };

  uint32_t preview_track_id = 1;

  VideoTrackCreateParam preview_track_param{camera_id_, VideoFormat::kYUV,
                                          640, 480, 30};
    preview_track_param.low_power_mode = true;

  preview_track_cb.data_cb = [&, session_id] (uint32_t track_id,
      std::vector<BufferDescriptor> buffers,
      std::vector<MetaData> meta_buffers) {
          VideoTrackYUVDataCb(session_id, track_id, buffers, meta_buffers);
      };

  ret = recorder_.CreateVideoTrack(session_id, preview_track_id,
                                   preview_track_param, preview_track_cb);
  ASSERT_TRUE(ret == NO_ERROR);

  std::vector<uint32_t> track_ids = {preview_track_id};
  sessions_.insert(std::make_pair(session_id, track_ids));

  ret = recorder_.StartSession(session_id);
  ASSERT_TRUE(ret == NO_ERROR);

  // Record for sometime
  sleep(1);

  ImageParam image_param{};
  image_param.width         = 3840;
  image_param.height        = 2160;
  image_param.image_format  = ImageFormat::kJPEG;
  image_param.image_quality = 95;

  std::vector<CameraMetadata> meta_array;
  camera_metadata_entry_t entry;
  CameraMetadata meta;

  ret = recorder_.GetDefaultCaptureParam(camera_id_, meta);
  ASSERT_TRUE(ret == NO_ERROR);

  meta_array.push_back(meta);

  bool res_supported = false;
  // Check Supported JPEG snapshot resolutions.
  if (meta.exists(ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS)) {
    entry = meta.find(ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS);
    for (uint32_t i = 0 ; i < entry.count; i += 4) {
      if (HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED == entry.data.i32[i]) {
        if (ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT ==
            entry.data.i32[i+3]) {
          if (image_param.width == static_cast<uint32_t>(entry.data.i32[i+1])
              && image_param.height ==
                  static_cast<uint32_t>(entry.data.i32[i+2])) {
            res_supported = true; // 3840x2160 JPEG supported.
          }
        }
      }
    }
  }
  ASSERT_TRUE (res_supported != false);

  ImageCaptureCb cb = [this] (uint32_t camera_id, uint32_t image_count,
                              BufferDescriptor buffer,
                              MetaData meta_data) -> void
      { SnapshotCb(camera_id, image_count, buffer, meta_data); };

  ImageConfigParam image_config;
  ImageExif exif;
  exif.enable = false;
  image_config.Update(QMMF_EXIF, exif);

  ret = recorder_.ConfigImageCapture(camera_id_, image_config);
  ASSERT_TRUE(ret == NO_ERROR);

  for (uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
        test_info_->name(), i);
    ret = recorder_.CaptureImage(camera_id_, image_param, 1, meta_array, cb);
    ASSERT_TRUE(ret == NO_ERROR);
    // Take snapshot after every 5 sec.
    sleep(5);
  }

  ret = recorder_.CancelCaptureImage(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.StopSession(session_id, false);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteVideoTrack(session_id, preview_track_id);
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
* 10MPSnapshotDisableEXIF: This test will test 10MP JPEG snapshot.
* Api test sequence:
*  - StartCamera
*  - ConfigImageCapture - Disable EXIF
*   loop Start {
*   ------------------
*   - CaptureImage - JPEG
*   ------------------
*   } loop End
*  - StopCamera
*/
TEST_F(RecorderImageGTest, 10MPSnapshotDisableEXIF) {
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

  TrackCb preview_track_cb;
  preview_track_cb.event_cb =
      [this] (uint32_t track_id, EventType event_type,
              void *event_data, size_t event_data_size) -> void {
      VideoTrackEventCb(track_id, event_type, event_data, event_data_size); };

  uint32_t preview_track_id = 1;

  VideoTrackCreateParam preview_track_param{camera_id_, VideoFormat::kYUV,
                                          640, 480, 30};
    preview_track_param.low_power_mode = true;

  preview_track_cb.data_cb = [&, session_id] (uint32_t track_id,
      std::vector<BufferDescriptor> buffers,
      std::vector<MetaData> meta_buffers) {
          VideoTrackYUVDataCb(session_id, track_id, buffers, meta_buffers);
      };

  ret = recorder_.CreateVideoTrack(session_id, preview_track_id,
                                   preview_track_param, preview_track_cb);
  ASSERT_TRUE(ret == NO_ERROR);

  std::vector<uint32_t> track_ids = {preview_track_id};
  sessions_.insert(std::make_pair(session_id, track_ids));

  ret = recorder_.StartSession(session_id);
  ASSERT_TRUE(ret == NO_ERROR);

  // Record for sometime
  sleep(1);

  ImageParam image_param{};
  image_param.width         = 3872;
  image_param.height        = 2592;
  image_param.image_format  = ImageFormat::kJPEG;
  image_param.image_quality = default_jpeg_quality_;

  std::vector<CameraMetadata> meta_array;
  camera_metadata_entry_t entry;
  CameraMetadata meta;

  ret = recorder_.GetDefaultCaptureParam(camera_id_, meta);
  ASSERT_TRUE(ret == NO_ERROR);

  meta_array.push_back(meta);

  bool res_supported = false;
  // Check Supported JPEG snapshot resolutions.
  if (meta.exists(ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS)) {
    entry = meta.find(ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS);
    for (uint32_t i = 0 ; i < entry.count; i += 4) {
      if (HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED == entry.data.i32[i]) {
        if (ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT ==
            entry.data.i32[i+3]) {
          if (image_param.width == static_cast<uint32_t>(entry.data.i32[i+1])
              && image_param.height ==
                  static_cast<uint32_t>(entry.data.i32[i+2])) {
            res_supported = true; // 3840x2160 JPEG supported.
          }
        }
      }
    }
  }
  ASSERT_TRUE (res_supported != false);

  ImageCaptureCb cb = [this] (uint32_t camera_id, uint32_t image_count,
                              BufferDescriptor buffer,
                              MetaData meta_data) -> void
      { SnapshotCb(camera_id, image_count, buffer, meta_data); };

  ImageConfigParam image_config;
  ImageExif exif;
  exif.enable = false;
  image_config.Update(QMMF_EXIF, exif);

  ret = recorder_.ConfigImageCapture(camera_id_, image_config);
  ASSERT_TRUE(ret == NO_ERROR);

  for (uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
        test_info_->name(), i);
    ret = recorder_.CaptureImage(camera_id_, image_param, 1, meta_array, cb);
    ASSERT_TRUE(ret == NO_ERROR);
    // Take snapshot after every 5 sec.
    sleep(5);
  }

  ret = recorder_.CancelCaptureImage(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.StopSession(session_id, false);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteVideoTrack(session_id, preview_track_id);
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
* 10MPSnapshotDisableEXIFUpdateFocalLength: This test will test 10MP JPEG
*                                           snapshot.
* Api test sequence:
*  - StartCamera
*  - CreateVideoTrack
*  - Set Focal length to capture and streaming meta
*  - ConfigImageCapture - Disable EXIF
*   loop Start {
*   ------------------
*   - CaptureImage - JPEG
*   ------------------
*  - Reset focal length to go back to preview mode.
*  - CancelCaptureImage
*   } loop End
*  - StopCamera
*/
TEST_F(RecorderImageGTest, 10MPSnapshotDisableEXIFUpdateFocalLength) {
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

  TrackCb preview_track_cb;
  preview_track_cb.event_cb =
      [this] (uint32_t track_id, EventType event_type,
              void *event_data, size_t event_data_size) -> void {
      VideoTrackEventCb(track_id, event_type, event_data, event_data_size); };

  uint32_t preview_track_id = 1;

  VideoTrackCreateParam preview_track_param{camera_id_, VideoFormat::kYUV,
                                          640, 480, 30};
    preview_track_param.low_power_mode = true;

  preview_track_cb.data_cb = [&, session_id] (uint32_t track_id,
      std::vector<BufferDescriptor> buffers,
      std::vector<MetaData> meta_buffers) {
          VideoTrackYUVDataCb(session_id, track_id, buffers, meta_buffers);
      };

  ret = recorder_.CreateVideoTrack(session_id, preview_track_id,
                                   preview_track_param, preview_track_cb);
  ASSERT_TRUE(ret == NO_ERROR);

  std::vector<uint32_t> track_ids = {preview_track_id};
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
  camera_metadata_entry_t entry;
  CameraMetadata meta;

  ret = recorder_.GetDefaultCaptureParam(camera_id_, meta);
  ASSERT_TRUE(ret == NO_ERROR);

  // Update focal length to capture meta to select 4fps sensor mode.
  float focal_length = 18;
  meta.update(ANDROID_LENS_FOCAL_LENGTH, &focal_length, 1);

  meta_array.push_back(meta);

  bool res_supported = false;
  // Check Supported JPEG snapshot resolutions.
  if (meta.exists(ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS)) {
    entry = meta.find(ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS);
    for (uint32_t i = 0 ; i < entry.count; i += 4) {
      if (HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED == entry.data.i32[i]) {
        if (ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT ==
            entry.data.i32[i+3]) {
          if (image_param.width == static_cast<uint32_t>(entry.data.i32[i+1])
              && image_param.height ==
                  static_cast<uint32_t>(entry.data.i32[i+2])) {
            res_supported = true; // 3840x2160 JPEG supported.
          }
        }
      }
    }
  }
  ASSERT_TRUE (res_supported != false);

  ImageCaptureCb cb = [this] (uint32_t camera_id, uint32_t image_count,
                              BufferDescriptor buffer,
                              MetaData meta_data) -> void
      { SnapshotCb(camera_id, image_count, buffer, meta_data); };

  ImageConfigParam image_config;
  ImageExif exif;
  exif.enable = false;
  image_config.Update(QMMF_EXIF, exif);

  ret = SetCameraFocalLength(focal_length);

  ret = recorder_.ConfigImageCapture(camera_id_, image_config);
  ASSERT_TRUE(ret == NO_ERROR);

  for (uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
        test_info_->name(), i);
    ret = recorder_.CaptureImage(camera_id_, image_param, 1, meta_array, cb);
    ASSERT_TRUE(ret == NO_ERROR);
    // Take snapshot after every 5 sec.
    sleep(5);
  }

  // Record for some more time.
  sleep(5);

  // Reset focal length corresponding to preview sensor mode.
  focal_length = 16;
  ret = SetCameraFocalLength(focal_length);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.CancelCaptureImage(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  // Record for some more time.
  sleep(5);
  ret = recorder_.StopSession(session_id, false);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteVideoTrack(session_id, preview_track_id);
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
* 4KSnapshot: This test will test 4K JPEG snapshot.
* Api test sequence:
*  - StartCamera
*   loop Start {
*   ------------------
*   - CaptureImage - JPEG
*   ------------------
*   } loop End
*  - StopCamera
*/
TEST_F(RecorderImageGTest, 4KSnapshot) {
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

  TrackCb preview_track_cb;
  preview_track_cb.event_cb =
      [this] (uint32_t track_id, EventType event_type,
              void *event_data, size_t event_data_size) -> void {
      VideoTrackEventCb(track_id, event_type, event_data, event_data_size); };

  uint32_t preview_track_id = 1;

  VideoTrackCreateParam preview_track_param{camera_id_, VideoFormat::kYUV,
                                          640, 480, 30};
    preview_track_param.low_power_mode = true;

  preview_track_cb.data_cb = [&, session_id] (uint32_t track_id,
      std::vector<BufferDescriptor> buffers,
      std::vector<MetaData> meta_buffers) {
          VideoTrackYUVDataCb(session_id, track_id, buffers, meta_buffers);
      };

  ret = recorder_.CreateVideoTrack(session_id, preview_track_id,
                                   preview_track_param, preview_track_cb);
  ASSERT_TRUE(ret == NO_ERROR);

  std::vector<uint32_t> track_ids = {preview_track_id};
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

  meta_array.push_back(meta);

  bool res_supported = false;
  // Check Supported JPEG snapshot resolutions.
  if (meta.exists(ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS)) {
    entry = meta.find(ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS);
    for (uint32_t i = 0 ; i < entry.count; i += 4) {
      if (HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED == entry.data.i32[i]) {
        if (ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT ==
            entry.data.i32[i+3]) {
          if (image_param.width == static_cast<uint32_t>(entry.data.i32[i+1])
              && image_param.height ==
                  static_cast<uint32_t>(entry.data.i32[i+2])) {
            res_supported = true; // 3840x2160 JPEG supported.
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

  for (uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
        test_info_->name(), i);
    ret = recorder_.CaptureImage(camera_id_, image_param, 1, meta_array, cb);
    ASSERT_TRUE(ret == NO_ERROR);
    // Take snapshot after every 5 sec.
    sleep(5);
  }

  ret = recorder_.CancelCaptureImage(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.StopSession(session_id, false);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteVideoTrack(session_id, preview_track_id);
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
* 4KSnapshotStillPlusRaw: This test will test 4K JPEG and RAW snapshot
*                    on the same time.
* Api test sequence:
*  - StartCamera
*   loop Start {
*   ------------------
*   - CaptureImage - RAW+JPEG
*   ------------------
*   } loop End
*  - StopCamera
*/
TEST_F(RecorderImageGTest, 4KSnapshotStillPlusRaw) {
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

  TrackCb video_track_cb;
  video_track_cb.event_cb =
      [this] (uint32_t track_id, EventType event_type,
              void *event_data, size_t event_data_size) -> void {
      VideoTrackEventCb(track_id, event_type, event_data, event_data_size); };

  uint32_t video_track_id = 1;
  VideoTrackCreateParam video_track_param{camera_id_, VideoFormat::kAVC,
                                          640, 480, 30};
  video_track_param.low_power_mode = false;

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
  sleep(1);

  ImageParam image_param{};
  image_param.width         = 3840;
  image_param.height        = 2160;
  image_param.image_format  = ImageFormat::kJPEG;
  image_param.image_quality = 95;

  std::vector<CameraMetadata> meta_array;
  camera_metadata_entry_t entry;
  CameraMetadata meta;

  ret = recorder_.GetDefaultCaptureParam(camera_id_, meta);
  ASSERT_TRUE(ret == NO_ERROR);

  bool res_supported = false;
  // Check Supported JPEG snapshot resolutions.
  if (meta.exists(ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS)) {
    entry = meta.find(ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS);
    for (uint32_t i = 0 ; i < entry.count; i += 4) {
      if (HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED == entry.data.i32[i]) {
        if (ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT ==
            entry.data.i32[i+3]) {
          if (image_param.width == static_cast<uint32_t>(entry.data.i32[i+1])
              && image_param.height ==
                  static_cast<uint32_t>(entry.data.i32[i+2])) {
            res_supported = true; // 3840x2160 JPEG supported.
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

  ImageConfigParam image_config;

  SnapshotType snapshot_type;
  snapshot_type.type = SnapshotMode::kStillPlusRaw;
  image_config.Update(QMMF_SNAPSHOT_TYPE, snapshot_type, 0);

  ret = recorder_.ConfigImageCapture(camera_id_, image_config);
  ASSERT_TRUE(ret == NO_ERROR);

  meta_array.push_back(meta);
  for (uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
        test_info_->name(), i);
    ret = recorder_.CaptureImage(camera_id_, image_param, 1, meta_array, cb);
    ASSERT_TRUE(ret == NO_ERROR);
    // Take snapshot after every 5 sec.
    sleep(5);
  }
  meta_array.clear();

  ret = recorder_.CancelCaptureImage(camera_id_);
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

  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());
}

/*
* 10MPSnapshot: This test will test 10MP JPEG snapshot.
* Api test sequence:
*  - StartCamera
*   loop Start {
*   ------------------
*   - CaptureImage - JPEG
*   ------------------
*   } loop End
*  - StopCamera
*/
TEST_F(RecorderImageGTest, 10MPSnapshot) {
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

  TrackCb preview_track_cb;
  preview_track_cb.event_cb =
      [this] (uint32_t track_id, EventType event_type,
              void *event_data, size_t event_data_size) -> void {
      VideoTrackEventCb(track_id, event_type, event_data, event_data_size); };

  uint32_t preview_track_id = 1;

  VideoTrackCreateParam preview_track_param{camera_id_, VideoFormat::kYUV,
                                          640, 480, 30};
    preview_track_param.low_power_mode = true;

  preview_track_cb.data_cb = [&, session_id] (uint32_t track_id,
      std::vector<BufferDescriptor> buffers,
      std::vector<MetaData> meta_buffers) {
          VideoTrackYUVDataCb(session_id, track_id, buffers, meta_buffers);
      };

  ret = recorder_.CreateVideoTrack(session_id, preview_track_id,
                                   preview_track_param, preview_track_cb);
  ASSERT_TRUE(ret == NO_ERROR);

  std::vector<uint32_t> track_ids = {preview_track_id};
  sessions_.insert(std::make_pair(session_id, track_ids));

  ret = recorder_.StartSession(session_id);
  ASSERT_TRUE(ret == NO_ERROR);

  // Record for sometime
  sleep(1);

  ImageParam image_param{};
  image_param.width         = 3872;
  image_param.height        = 2592;
  image_param.image_format  = ImageFormat::kJPEG;
  image_param.image_quality = 95;

  std::vector<CameraMetadata> meta_array;
  camera_metadata_entry_t entry;
  CameraMetadata meta;

  ret = recorder_.GetDefaultCaptureParam(camera_id_, meta);
  ASSERT_TRUE(ret == NO_ERROR);

  meta_array.push_back(meta);

  bool res_supported = false;
  // Check Supported JPEG snapshot resolutions.
  if (meta.exists(ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS)) {
    entry = meta.find(ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS);
    for (uint32_t i = 0 ; i < entry.count; i += 4) {
      if (HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED == entry.data.i32[i]) {
        if (ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT ==
            entry.data.i32[i+3]) {
          if (image_param.width == static_cast<uint32_t>(entry.data.i32[i+1])
              && image_param.height ==
                  static_cast<uint32_t>(entry.data.i32[i+2])) {
            res_supported = true; // 3840x2160 JPEG supported.
          }
        }
      }
    }
  }
  ASSERT_TRUE (res_supported != false);

  ImageCaptureCb cb = [this] (uint32_t camera_id, uint32_t image_count,
                              BufferDescriptor buffer,
                              MetaData meta_data) -> void
      { SnapshotCb(camera_id, image_count, buffer, meta_data); };

  for (uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
        test_info_->name(), i);
    ret = recorder_.CaptureImage(camera_id_, image_param, 1, meta_array, cb);
    ASSERT_TRUE(ret == NO_ERROR);
    // Take snapshot after every 5 sec.
    sleep(5);
  }

  ret = recorder_.CancelCaptureImage(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.StopSession(session_id, false);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteVideoTrack(session_id, preview_track_id);
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
* 10MPJPEG422Snapshot: This test will test 10MP JPEG snapshot with two
* thumbnails.
* Api test sequence:
*  - StartCamera
*   loop Start {
*   ------------------
*   - CaptureImage - JPEG
*   ------------------
*   } loop End
*  - StopCamera
*/
TEST_F(RecorderImageGTest, 10MPJPEG422Snapshot) {
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

  TrackCb preview_track_cb;
  preview_track_cb.event_cb =
      [this] (uint32_t track_id, EventType event_type,
              void *event_data, size_t event_data_size) -> void {
      VideoTrackEventCb(track_id, event_type, event_data, event_data_size); };

  uint32_t preview_track_id = 1;

  VideoTrackCreateParam preview_track_param{camera_id_, VideoFormat::kYUV,
                                          640,
                                          480,
                                          30};
    preview_track_param.low_power_mode = true;

  preview_track_cb.data_cb = [&, session_id] (uint32_t track_id,
      std::vector<BufferDescriptor> buffers,
      std::vector<MetaData> meta_buffers) {
          VideoTrackYUVDataCb(session_id, track_id, buffers, meta_buffers);
      };

  ret = recorder_.CreateVideoTrack(session_id, preview_track_id,
                                   preview_track_param, preview_track_cb);
  ASSERT_TRUE(ret == NO_ERROR);

  std::vector<uint32_t> track_ids = {preview_track_id};
  sessions_.insert(std::make_pair(session_id, track_ids));

  ret = recorder_.StartSession(session_id);
  ASSERT_TRUE(ret == NO_ERROR);

  // Record for sometime
  sleep(1);

  ImageParam image_param{};
  image_param.width         = 3872;
  image_param.height        = 2592;
  image_param.image_format  = ImageFormat::kJPEG;
  image_param.image_quality = 95;

  std::vector<CameraMetadata> meta_array;
  CameraMetadata meta;

  ret = recorder_.GetDefaultCaptureParam(camera_id_, meta);
  ASSERT_TRUE(ret == NO_ERROR);

  meta_array.push_back(meta);

  bool res_supported = GtestCommon::ValidateResFromJpegSizes(
      meta, image_param.width, image_param.height);
  ASSERT_TRUE (res_supported != false);

  ImageCaptureCb cb = [this] (uint32_t camera_id, uint32_t image_count,
                              BufferDescriptor buffer,
                              MetaData meta_data) -> void
      { SnapshotCb(camera_id, image_count, buffer, meta_data); };

  ImageConfigParam image_config;
  ImageThumbnail thumbnail;
  HighQualityCaptureSetup high_quality_setup;

  // Primary thumbnail parameters.
  thumbnail.width = 960;
  thumbnail.height = 480;
  thumbnail.quality = 90;
  image_config.Update(QMMF_IMAGE_THUMBNAIL, thumbnail, 0);

  for (uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
        test_info_->name(), i);

    high_quality_setup.jpeg_input_format = BufferFormat::kNV12;
    image_config.Update(QMMF_JPEG_CAPTURE_SETUP,
      high_quality_setup);

    ret = recorder_.ConfigImageCapture(camera_id_, image_config);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.CaptureImage(camera_id_, image_param, 1, meta_array, cb);
    ASSERT_TRUE(ret == NO_ERROR);
    // Take snapshot after every 5 sec.
    sleep(5);

    high_quality_setup.jpeg_input_format = BufferFormat::kNV16;
    image_config.Update(QMMF_JPEG_CAPTURE_SETUP,
      high_quality_setup);

    ret = recorder_.ConfigImageCapture(camera_id_, image_config);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.CaptureImage(camera_id_, image_param, 1, meta_array, cb);
    ASSERT_TRUE(ret == NO_ERROR);
    // Take snapshot after every 5 sec.
    sleep(5);
  }

  ret = recorder_.CancelCaptureImage(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);
  ret = recorder_.StopSession(session_id, false);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteVideoTrack(session_id, preview_track_id);
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
* Jpeg422BurstSnapshotWithBayerLCAC15fps:
*     This test will test burst snapshot with ost processing. Post processing
*     pipe is Bayer LCAC, Bayer to YUV reprocessing and YUV422 JPEG with two
*     thumbnails.
*
* Api test sequence:
*  - StartCamera
*  - CreateSession
*  - CreateVideoTrack
*  - StartSesion
*  - GetSupportedPlugins
*  - CreatePlugin - Bayer LCAC
*  - ConfigImageCapture - Add LCAC and two thumbnails
*  - Enable CDS if needed based on lux index
*   loop Start {
*   ------------------
*   - Lock AE
*   - CaptureImage - Burst With Bayer LCAC
*   - Unlock AE
*   ------------------
*   } loop End
*  - CancelCaptureImage
*  - DeletePlugin
*  - StopSession
*  - DeleteVideoTrack
*  - DeleteSession
*  - StopCamera
*/
TEST_F(RecorderImageGTest, Jpeg422BurstSnapshotWithBayerLCAC15fps) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  std::condition_variable  ae_converge_signal;
  std::mutex ae_converge_mutex;
  bool ae_converged = false;
  float lux_idx = 0.0f;

  CameraResultCb result_cb = [&] (uint32_t camera_id,
      const CameraMetadata &result) {
        if (result.exists(ANDROID_CONTROL_AE_STATE)) {
          uint8_t aec = result.find(ANDROID_CONTROL_AE_STATE).data.u8[0];
          if (((aec == ANDROID_CONTROL_AE_STATE_CONVERGED) ||
            (aec == ANDROID_CONTROL_AE_STATE_LOCKED))) {
            TEST_INFO("%s: AE is converged!!!", __func__);
            std::unique_lock<std::mutex> ae_converge_lock(ae_converge_mutex);
            ae_converged = true;
            ae_converge_signal.notify_one();
          }
        }
        if (result.exists(  QCAMERA3_CURRENT_LUX_IDX)) {
          lux_idx = result.find(  QCAMERA3_CURRENT_LUX_IDX).data.f[0];
          TEST_DBG("%s: lux_idx: %f", __func__, lux_idx);
        }
      };

  const uint32_t frame_rate = 15;
  camera_start_params_.frame_rate = frame_rate;
  ret = recorder_.StartCamera(camera_id_, camera_start_params_, result_cb);
  ASSERT_TRUE(ret == NO_ERROR);

  SessionCb session_status_cb = CreateSessionStatusCb();
  uint32_t session_id;
  ret = recorder_.CreateSession(session_status_cb, &session_id);
  ASSERT_TRUE(session_id > 0);
  ASSERT_TRUE(ret == NO_ERROR);

  TrackCb video_track_cb;
  video_track_cb.event_cb =
      [this] (uint32_t track_id, EventType event_type,
              void *event_data, size_t event_data_size) -> void {
      VideoTrackEventCb(track_id, event_type, event_data, event_data_size); };

  uint32_t video_track_id = 1;
  VideoTrackCreateParam video_track_param{camera_id_, VideoFormat::kAVC,
                                          640,
                                          480,
                                          frame_rate};
  video_track_param.low_power_mode = false;

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
  sleep(2);

  ImageParam image_param{};
  image_param.width         = 3872;
  image_param.height        = 2592;
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
      {
        SnapshotCb(camera_id, image_count, buffer, meta_data);
        test_wait_.Done();
      };

  ImageConfigParam image_config;
  PostprocPlugin bayer_lcac_plugin;

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

  ImageThumbnail thumbnail;

  // Secondary thumbnail(Screennail) parameters.
  thumbnail.width = 320;
  thumbnail.height = 240;
  thumbnail.quality = 85;
  image_config.Update(QMMF_IMAGE_THUMBNAIL, thumbnail, 0);

  // Primary thumbnail parameters.
  thumbnail.width = 960;
  thumbnail.height = 480;
  thumbnail.quality = 90;
  image_config.Update(QMMF_IMAGE_THUMBNAIL, thumbnail, 1);

  HighQualityCaptureSetup high_quality_setup;
  high_quality_setup.jpeg_input_format = BufferFormat::kNV16;
  image_config.Update(QMMF_JPEG_CAPTURE_SETUP, high_quality_setup);

  ret = recorder_.ConfigImageCapture(camera_id_, image_config);
  ASSERT_TRUE(ret == NO_ERROR);

  CameraMetadata video_meta;
  int32_t cds_mode = 0; // 0-Off, 1-On, 2-Auto
  if (lux_idx > default_cds_threshold_) {
    TEST_INFO("%s: Enable CDS", __func__);
    cds_mode = 1;
    meta.update(  QCAMERA3_CDS_MODE, &cds_mode, 1);

    ret = recorder_.GetCameraParam(camera_id_, video_meta);
    ASSERT_TRUE(ret == NO_ERROR);
    video_meta.update(  QCAMERA3_CDS_MODE, &cds_mode, 1);

    ret = recorder_.SetCameraParam(camera_id_, video_meta);
    ASSERT_TRUE(ret == NO_ERROR);
  }

  // Set frame rate otherwise default value is used
  int32_t fps_range[2];
  fps_range[0] = frame_rate;
  fps_range[1] = frame_rate;
  ret = meta.update(ANDROID_CONTROL_AE_TARGET_FPS_RANGE, fps_range, 2);
  ASSERT_TRUE(ret == NO_ERROR);

  uint32_t num_images = burst_image_count_;
  for (uint32_t num = 0; num < num_images; num++) {
    meta_array.push_back(meta);
  }

  for (uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
        test_info_->name(), i);

    {
      // Wait for AE convergence
      std::unique_lock<std::mutex> ae_converge_lock(ae_converge_mutex);
      if (!ae_converged) {
        TEST_INFO("%s: Wait for AE to Converged!", __func__);
        auto status = ae_converge_signal.wait_for(ae_converge_lock,
        std::chrono::seconds(30 / frame_rate + 1));
        ASSERT_TRUE(status == std::cv_status::no_timeout);
        TEST_INFO("%s: AE Converged succesfuly", __func__);
      } else {
        TEST_INFO("%s: AE is already converged!", __func__);
      }
    }

    ret = recorder_.GetCameraParam(camera_id_, video_meta);
    ASSERT_TRUE(ret == NO_ERROR);

    // Lock AE
    uint8_t ae_lock = ANDROID_CONTROL_AE_LOCK_ON;
    ret = video_meta.update(ANDROID_CONTROL_AE_LOCK, &ae_lock, 1);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.SetCameraParam(camera_id_, video_meta);
    ASSERT_TRUE(ret == NO_ERROR);

    test_wait_.Reset(num_images, 15);
    ret = recorder_.CaptureImage(camera_id_, image_param, num_images,
                                 meta_array, cb);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = test_wait_.Wait();
    ASSERT_TRUE(ret == NO_ERROR);

    {
      std::unique_lock<std::mutex> ae_converge_lock(ae_converge_mutex);
      ae_converged = false;
    }
    // Unlock AE
    ae_lock = ANDROID_CONTROL_AE_LOCK_OFF;
    ret = video_meta.update(ANDROID_CONTROL_AE_LOCK, &ae_lock, 1);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.SetCameraParam(camera_id_, video_meta);
    ASSERT_TRUE(ret == NO_ERROR);
  }

  //Disable CDS
  cds_mode = 0;
  video_meta.update(  QCAMERA3_CDS_MODE, &cds_mode, 1);

  ret = recorder_.SetCameraParam(camera_id_, video_meta);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.CancelCaptureImage(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

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

  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());
}

/*
* 10MPSnapshotMultiThumbnails: This test will test 10MP JPEG snapshot with two
* thumbnails.
* Api test sequence:
*  - StartCamera
*   loop Start {
*   ------------------
*   - CaptureImage - JPEG
*   ------------------
*   } loop End
*  - StopCamera
*/
TEST_F(RecorderImageGTest, 10MPSnapshotMultiThumbnails) {
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

  TrackCb preview_track_cb;
  preview_track_cb.event_cb =
      [this] (uint32_t track_id, EventType event_type,
              void *event_data, size_t event_data_size) -> void {
      VideoTrackEventCb(track_id, event_type, event_data, event_data_size); };

  uint32_t preview_track_id = 1;

  VideoTrackCreateParam preview_track_param{camera_id_, VideoFormat::kYUV,
                                          640,
                                          480,
                                          30};
    preview_track_param.low_power_mode = true;

  preview_track_cb.data_cb = [&, session_id] (uint32_t track_id,
      std::vector<BufferDescriptor> buffers,
      std::vector<MetaData> meta_buffers) {
          VideoTrackYUVDataCb(session_id, track_id, buffers, meta_buffers);
      };

  ret = recorder_.CreateVideoTrack(session_id, preview_track_id,
                                   preview_track_param, preview_track_cb);
  ASSERT_TRUE(ret == NO_ERROR);

  std::vector<uint32_t> track_ids = {preview_track_id};
  sessions_.insert(std::make_pair(session_id, track_ids));

  ret = recorder_.StartSession(session_id);
  ASSERT_TRUE(ret == NO_ERROR);

  // Record for sometime
  sleep(1);

  ImageParam image_param{};
  image_param.width         = 3872;
  image_param.height        = 2592;
  image_param.image_format  = ImageFormat::kJPEG;
  image_param.image_quality = 95;

  std::vector<CameraMetadata> meta_array;
  camera_metadata_entry_t entry;
  CameraMetadata meta;

  ret = recorder_.GetDefaultCaptureParam(camera_id_, meta);
  ASSERT_TRUE(ret == NO_ERROR);

  meta_array.push_back(meta);

  bool res_supported = false;
  // Check Supported JPEG snapshot resolutions.
  if (meta.exists(ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS)) {
    entry = meta.find(ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS);
    for (uint32_t i = 0 ; i < entry.count; i += 4) {
      if (HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED == entry.data.i32[i]) {
        if (ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT ==
            entry.data.i32[i+3]) {
          if (image_param.width == static_cast<uint32_t>(entry.data.i32[i+1])
              && image_param.height ==
                  static_cast<uint32_t>(entry.data.i32[i+2])) {
            res_supported = true; // 3840x2160 JPEG supported.
          }
        }
      }
    }
  }
  ASSERT_TRUE (res_supported != false);

  ImageConfigParam image_config;
  ImageThumbnail thumbnail;

  // Secondary thumbnail(Screennail) parameters.
  thumbnail.width = 320;
  thumbnail.height = 240;
  thumbnail.quality = 85;
  image_config.Update(QMMF_IMAGE_THUMBNAIL, thumbnail, 0);

  // Primary thumbnail parameters.
  thumbnail.width = 960;
  thumbnail.height = 480;
  thumbnail.quality = 90;
  image_config.Update(QMMF_IMAGE_THUMBNAIL, thumbnail, 1);

  ret = recorder_.ConfigImageCapture(camera_id_, image_config);
  ASSERT_TRUE(ret == NO_ERROR);

  ImageCaptureCb cb = [this] (uint32_t camera_id, uint32_t image_count,
                              BufferDescriptor buffer,
                              MetaData meta_data) -> void
      { SnapshotCb(camera_id, image_count, buffer, meta_data); };

  for (uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
        test_info_->name(), i);
    ret = recorder_.CaptureImage(camera_id_, image_param, 1, meta_array, cb);
    ASSERT_TRUE(ret == NO_ERROR);
    // Take snapshot after every 5 sec.
    sleep(5);
  }

  ret = recorder_.CancelCaptureImage(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.StopSession(session_id, false);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteVideoTrack(session_id, preview_track_id);
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
* 10MPSnapshotWithEdgeSmooth: This test will test 10MP JPEG snapshot with
*                       reprocessing. Reprocessing pipe is EdgeSmooth and JPEG.
* Api test sequence:
*  - StartCamera
*   loop Start {
*   ------------------
*   - CaptureImage - JPEG
*   ------------------
*   } loop End
*  - StopCamera
*/
TEST_F(RecorderImageGTest, 10MPSnapshotWithEdgeSmooth) {
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
  video_track_param.low_power_mode = false;

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
  sleep(1);

  ImageParam image_param{};
  image_param.width         = 3872;
  image_param.height        = 2592;
  image_param.image_format  = ImageFormat::kJPEG;
  image_param.image_quality = default_jpeg_quality_;

  std::vector<CameraMetadata> meta_array;
  camera_metadata_entry_t entry;
  CameraMetadata meta;

  ret = recorder_.GetDefaultCaptureParam(camera_id_, meta);
  ASSERT_TRUE(ret == NO_ERROR);
  meta_array.push_back(meta);

  bool res_supported = false;
  // Check Supported JPEG snapshot resolutions.
  if (meta.exists(ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS)) {
    entry = meta.find(ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS);
    for (uint32_t i = 0 ; i < entry.count; i += 4) {
      if (HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED == entry.data.i32[i]) {
        if (ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT ==
            entry.data.i32[i+3]) {
          if (image_param.width == static_cast<uint32_t>(entry.data.i32[i+1])
              && image_param.height ==
                  static_cast<uint32_t>(entry.data.i32[i+2])) {
            res_supported = true; // 3840x2160 JPEG supported.
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

  ImageConfigParam image_config;
  PostprocPlugin edge_smooth_plugin;

  SupportedPlugins supported_plugins;
  ret = recorder_.GetSupportedPlugins(&supported_plugins);
  ASSERT_TRUE(ret == NO_ERROR);

  bool found = false;
  for (auto const& plugin_info : supported_plugins) {
    if (plugin_info.name == "EdgeSmooth") {
      ret = recorder_.CreatePlugin(&edge_smooth_plugin.uid, plugin_info);
      ASSERT_TRUE(ret == NO_ERROR);

      image_config.Update(QMMF_POSTPROCESS_PLUGIN, edge_smooth_plugin);
      found = true;
    }
  }
  ASSERT_TRUE(found == true);

  ret = recorder_.ConfigImageCapture(camera_id_, image_config);
  ASSERT_TRUE(ret == NO_ERROR);

  for (uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
        test_info_->name(), i);

    ret = recorder_.CaptureImage(camera_id_, image_param, 1, meta_array, cb);
    ASSERT_TRUE(ret == NO_ERROR);
    // Take snapshot after every 5 sec.
    sleep(5);
  }

  ret = recorder_.CancelCaptureImage(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeletePlugin(edge_smooth_plugin.uid);
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

  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());
}

/*
* 10MPSnapshotWithLCAC: This test will test 10MP JPEG snapshot with
*                     reprocessing. Reprocessing pipe is bayer LCAC,
*                     bayer to you reprocessing and JPEG.
* Api test sequence:
*  - StartCamera
*   loop Start {
*   ------------------
*   - CaptureImage - JPEG
*   ------------------
*   } loop End
*  - StopCamera
*/
TEST_F(RecorderImageGTest, 10MPSnapshotWithLCAC) {
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

  video_track_param.low_power_mode = false;

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
  sleep(1);

  ImageParam image_param{};
  image_param.width         = 3872;
  image_param.height        = 2592;
  image_param.image_format  = ImageFormat::kJPEG;
  image_param.image_quality = default_jpeg_quality_;

  std::vector<CameraMetadata> meta_array;
  camera_metadata_entry_t entry;
  CameraMetadata meta;

  ret = recorder_.GetDefaultCaptureParam(camera_id_, meta);
  ASSERT_TRUE(ret == NO_ERROR);
  meta_array.push_back(meta);

  bool res_supported = false;
  // Check Supported JPEG snapshot resolutions.
  if (meta.exists(ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS)) {
    entry = meta.find(ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS);
    for (uint32_t i = 0 ; i < entry.count; i += 4) {
      if (HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED == entry.data.i32[i]) {
        if (ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT ==
            entry.data.i32[i+3]) {
          if (image_param.width == static_cast<uint32_t>(entry.data.i32[i+1])
              && image_param.height ==
                  static_cast<uint32_t>(entry.data.i32[i+2])) {
            res_supported = true; // 3840x2160 JPEG supported.
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

  ImageConfigParam image_config;
  PostprocPlugin bayer_lcac_plugin;

  SupportedPlugins supported_plugins;
  ret = recorder_.GetSupportedPlugins(&supported_plugins);
  ASSERT_TRUE(ret == NO_ERROR);

  bool found = false;
  for (auto const& plugin_info : supported_plugins) {
    if (plugin_info.name == "BayerLcac") {
      ret = recorder_.CreatePlugin(&bayer_lcac_plugin.uid, plugin_info);
      ASSERT_TRUE(ret == NO_ERROR);

      image_config.Update(QMMF_POSTPROCESS_PLUGIN, bayer_lcac_plugin);
      found = true;
    }
  }
  ASSERT_TRUE(found == true);

  ret = recorder_.ConfigImageCapture(camera_id_, image_config);
  ASSERT_TRUE(ret == NO_ERROR);

  for (uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
        test_info_->name(), i);

    ret = recorder_.CaptureImage(camera_id_, image_param, 1, meta_array, cb);
    ASSERT_TRUE(ret == NO_ERROR);
    sleep(5);
  }

  ret = recorder_.CancelCaptureImage(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeletePlugin(bayer_lcac_plugin.uid);
  ASSERT_TRUE(ret == NO_ERROR);

  sleep(10);

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

  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());
}

/*
* 10MPSnapshotWithLCACandEdgeSmooth: This test will test 10MP JPEG snapshot with
*                     reprocessing. Reprocessing pipe is bayer LCAC,
*                     bayer to you reprocessing, edge smooth and JPEG.
* Api test sequence:
*  - StartCamera
*   loop Start {
*   ------------------
*   - CaptureImage - JPEG
*   ------------------
*   } loop End
*  - StopCamera
*/
TEST_F(RecorderImageGTest, 10MPSnapshotWithLCACandEdgeSmooth) {
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
  video_track_param.low_power_mode = false;

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
  sleep(1);

  ImageParam image_param{};
  image_param.width         = 3872;
  image_param.height        = 2592;
  image_param.image_format  = ImageFormat::kJPEG;
  image_param.image_quality = default_jpeg_quality_;

  std::vector<CameraMetadata> meta_array;
  camera_metadata_entry_t entry;
  CameraMetadata meta;

  ret = recorder_.GetDefaultCaptureParam(camera_id_, meta);
  ASSERT_TRUE(ret == NO_ERROR);
  meta_array.push_back(meta);

  bool res_supported = false;
  // Check Supported JPEG snapshot resolutions.
  if (meta.exists(ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS)) {
    entry = meta.find(ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS);
    for (uint32_t i = 0 ; i < entry.count; i += 4) {
      if (HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED == entry.data.i32[i]) {
        if (ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT ==
            entry.data.i32[i+3]) {
          if (image_param.width == static_cast<uint32_t>(entry.data.i32[i+1])
              && image_param.height ==
                  static_cast<uint32_t>(entry.data.i32[i+2])) {
            res_supported = true; // 3840x2160 JPEG supported.
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

  ImageConfigParam image_config;
  PostprocPlugin bayer_lcac_plugin, edge_smooth_plugin, haze;

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

  ret = recorder_.ConfigImageCapture(camera_id_, image_config);
  ASSERT_TRUE(ret == NO_ERROR);

  for (uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
        test_info_->name(), i);

    ret = recorder_.CaptureImage(camera_id_, image_param, 1, meta_array, cb);
    ASSERT_TRUE(ret == NO_ERROR);
    sleep(5);

  }

  ret = recorder_.CancelCaptureImage(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

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

  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());
}

/*
* LowResVideo10MPSnapshotWithLCACandEdgeSmoothContinuousCapture: This test will
*       test 10MP JPEG single snapshot with reprocessing (bayer LCAC, edge
*       smooth) followed by Continuous capture with same configuration.
* Api test sequence:
*  - StartCamera
*  - Low resolution video 640x480@30fps
*  - Single Capture - BayerLcac + EdgeSmooth + JPEG
*  - Continuous Capture - BayerLcac + JPEG (Continius capture)
*  - CancelCaptureImage
*  - StopCamera
*/
TEST_F(RecorderImageGTest, LowResVideo10MPSnapshotWithLCACandEdgeSmoothContinuousCapture) {
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
    StreamDumpInfo dumpinfo = { VideoFormat::kAVC, session_id, 1, 640, 480 };
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
                                          640, 480, 30};

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

  bool res_supported = GtestCommon::ValidateResFromJpegSizes(meta,
      image_param.width, image_param.height);
  ASSERT_TRUE (res_supported != false);

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

  // Update same focal length to streaming meta.
  float focal_length = 8.0;
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
* LowResVideo10MPContinuousSnapshotWithLCAC: This gtest will test Continuous
*    10MP JPEG snapshot with Bayer LCAC.
* Api test sequence:
*  - StartCamera
*  - Low resolution video 640x480@30fps
*  - Continuous CaptureImage - BayerLcac + JPEG (Continius capture)
*  - CancelCaptureImage
*  - StopCamera
*/
TEST_F(RecorderImageGTest, LowResVideo10MPContinuousSnapshotWithLCAC) {
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
    StreamDumpInfo dumpinfo = { VideoFormat::kAVC, session_id, 1, 640, 480 };
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
                                          640, 480, 30};

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

  bool res_supported = GtestCommon::ValidateResFromJpegSizes(meta,
      image_param.width, image_param.height);
  ASSERT_TRUE (res_supported != false);

  ImageCaptureCb cb = [this] (uint32_t camera_id, uint32_t image_count,
                              BufferDescriptor buffer,
                              MetaData meta_data) -> void
      { SnapshotCb(camera_id, image_count, buffer, meta_data); };

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
* LowResVideo10MPContinuousSnapshotWithLCACandEdgeSmooth: This gtest will
*       test Continuous 10MP JPEG snapshot with Bayer LCAC.
* Api test sequence:
*  - StartCamera
*  - Low resolution video 640x480@30fps
*  - Continuous CaptureImage - BayerLcac + JPEG (Continius capture)
*  - StopCamera
*/
TEST_F(RecorderImageGTest, LowResVideo10MPContinuousSnapshotWithLCACandEdgeSmooth) {
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
    StreamDumpInfo dumpinfo = { VideoFormat::kAVC, session_id, 1, 640, 480 };
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
                                          640, 480, 30};

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

    bool res_supported = GtestCommon::ValidateResFromJpegSizes(meta,
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
  for (auto const& plugin_info : supported_plugins) {
    if (plugin_info.name == "EdgeSmooth") {
      ret = recorder_.CreatePlugin(&edge_smooth_plugin.uid, plugin_info);
      ASSERT_TRUE(ret == NO_ERROR);

      image_config.Update(QMMF_POSTPROCESS_PLUGIN, edge_smooth_plugin, 1);
      found = true;
    }
  }
  ASSERT_TRUE(found == true);

  // Update same focal length to streaming meta.
  focal_length = 8.0;
  ret = SetCameraFocalLength(focal_length);
  ASSERT_TRUE(ret == NO_ERROR);

  SnapshotType snapshot_type;
  snapshot_type.type = SnapshotMode::kContinuous;
  image_config.Update(QMMF_SNAPSHOT_TYPE, snapshot_type, 0);

  ret = recorder_.ConfigImageCapture(camera_id_, image_config);
  ASSERT_TRUE(ret == NO_ERROR);

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

  ret = recorder_.DeletePlugin(edge_smooth_plugin.uid);
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
* BurstSnapshotWithThumbnails: This test will test 1080p Burst jpg snapshot
*                              with enabled first and secondary thumbnails.
* Api test sequence:
*  - StartCamera
*  - CaptureImage - Burst 2 Jpgs
*  - StopCamera
*/
TEST_F(RecorderImageGTest, BurstSnapshotWithThumbnails) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  camera_start_params_.frame_rate = 30;
  ret = recorder_.StartCamera(camera_id_, camera_start_params_);
  ASSERT_TRUE(ret == NO_ERROR);

  ImageParam image_param{};
  image_param.width         = 1920;
  image_param.height        = 1080;
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
            res_supported = true; // 1920x1080 YUV res supported.
          }
        }
      }
    }
  }
  ASSERT_TRUE (res_supported != false);

  TEST_INFO("%s: Running Test(%s)", __func__,
    test_info_->name());

  ImageCaptureCb cb = [this] (uint32_t camera_id, uint32_t image_count,
                                BufferDescriptor buffer,
                                MetaData meta_data) -> void
      { SnapshotCb(camera_id, image_count, buffer, meta_data); };

  ImageConfigParam image_config;
  ImageThumbnail thumbnail;

  // Secondary thumbnail(Screennail) parameters.
  thumbnail.width = 320;
  thumbnail.height = 240;
  thumbnail.quality = 85;
  image_config.Update(QMMF_IMAGE_THUMBNAIL, thumbnail, 0);

  // Primary thumbnail parameters.
  thumbnail.width = 960;
  thumbnail.height = 480;
  thumbnail.quality = 90;
  image_config.Update(QMMF_IMAGE_THUMBNAIL, thumbnail, 1);

  ret = recorder_.ConfigImageCapture(camera_id_, image_config);
  ASSERT_TRUE(ret == NO_ERROR);

  uint32_t num_images = burst_image_count_;
  for (uint32_t i = 0; i < num_images; i++) {
    meta_array.push_back(meta);
  }

  for (uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr, "test iteration = %d/%d\n", i, iteration_count_);
    int32_t repeat = num_images;
    do {
      {
        std::lock_guard<std::mutex> lock(error_lock_);
        camera_error_ = false;
      }
      ret = recorder_.CaptureImage(camera_id_, image_param, num_images,
                                   meta_array, cb);
      ASSERT_TRUE(ret == NO_ERROR);

      sleep(5);
      {
        std::lock_guard<std::mutex> lock(error_lock_);
        if (!camera_error_) {
          TEST_ERROR("%s: Capture Image Done", __func__);
          break;
        }
      }

    } while(repeat-- > 0);
  }

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());
}

/*
* BurstSnapshot: This test will test 10MP Burst jpg snapshot.
* Api test sequence:
*  - StartCamera
*  - CaptureImage - Burst Jpg
*  - StopCamera
*/
TEST_F(RecorderImageGTest, BurstSnapshot) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  camera_start_params_.frame_rate = 30;
  ret = recorder_.StartCamera(camera_id_, camera_start_params_);
  ASSERT_TRUE(ret == NO_ERROR);

  ImageParam image_param{};
  image_param.width         = 3872;
  image_param.height        = 2592;
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
            res_supported = true; // 1920x1080 YUV res supported.
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

  uint32_t num_images = burst_image_count_;
  for (uint32_t i = 0; i < num_images; i++) {
    meta_array.push_back(meta);
  }

  for (uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr, "test iteration = %d/%d\n", i, iteration_count_);
    int32_t repeat = num_images;
    do {
      {
        std::lock_guard<std::mutex> lock(error_lock_);
        camera_error_ = false;
      }
      ret = recorder_.CaptureImage(camera_id_, image_param, num_images,
                                   meta_array, cb);
      ASSERT_TRUE(ret == NO_ERROR);

      sleep(5);
      {
        std::lock_guard<std::mutex> lock(error_lock_);
        if (!camera_error_) {
          TEST_ERROR("%s Capture Image Done", __func__);
          break;
        }
      }
    }while (repeat-- > 0);
  }

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());
}

/*
* BurstSnapshotWithYuvCAC: This test will test burst capture with
*                     post processing. Post processing pipe is YUV CAC and JPEG.
* Api test sequence:
*  - StartCamera
*  - CreateSession
*  - CreateVideoTrack
*  - StartSesion
*  - GetSupportedPlugins
*  - CreatePlugin - YUV CAC
*  - ConfigImageCapture
*   loop Start {
*   ------------------
*   - CaptureImage - Burst With YUV CAC
*   ------------------
*   } loop End
*  - CancelCaptureImage
*  - DeletePlugin
*  - StopSession
*  - DeleteVideoTrack
*  - DeleteSession
*  - StopCamera
*/
TEST_F(RecorderImageGTest, BurstSnapshotWithYuvCAC) {
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
  video_track_param.low_power_mode = false;

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
  sleep(1);

  ImageParam image_param{};
  image_param.width         = 3872;
  image_param.height        = 2592;
  image_param.image_format  = ImageFormat::kJPEG;
  image_param.image_quality = default_jpeg_quality_;

  std::vector<CameraMetadata> meta_array;
  camera_metadata_entry_t entry;
  CameraMetadata meta;

  ret = recorder_.GetDefaultCaptureParam(camera_id_, meta);
  ASSERT_TRUE(ret == NO_ERROR);

  bool res_supported = false;
  // Check Supported JPEG snapshot resolutions.
  if (meta.exists(ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS)) {
    entry = meta.find(ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS);
    for (uint32_t i = 0 ; i < entry.count; i += 4) {
      if (HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED == entry.data.i32[i]) {
        if (ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT ==
            entry.data.i32[i+3]) {
          if (image_param.width == static_cast<uint32_t>(entry.data.i32[i+1])
              && image_param.height ==
                  static_cast<uint32_t>(entry.data.i32[i+2])) {
            res_supported = true; // 3840x2160 JPEG supported.
          }
        }
      }
    }
  }
  ASSERT_TRUE (res_supported != false);

  ImageCaptureCb cb = [this] (uint32_t camera_id, uint32_t image_count,
                              BufferDescriptor buffer,
                              MetaData meta_data) -> void
      { SnapshotCb(camera_id, image_count, buffer, meta_data); };

  ImageConfigParam image_config;
  PostprocPlugin yuv_cac_plugin;

  SupportedPlugins supported_plugins;
  ret = recorder_.GetSupportedPlugins(&supported_plugins);
  ASSERT_TRUE(ret == NO_ERROR);

  bool found = false;
  for (auto const& plugin_info : supported_plugins) {
    if (plugin_info.name == "YuvCac") {
      ret = recorder_.CreatePlugin(&yuv_cac_plugin.uid, plugin_info);
      ASSERT_TRUE(ret == NO_ERROR);

      image_config.Update(QMMF_POSTPROCESS_PLUGIN, yuv_cac_plugin, 0);
      found = true;
    }
  }
  ASSERT_TRUE(found == true);

  ImageThumbnail thumbnail;

  // Secondary thumbnail(Screennail) parameters.
  thumbnail.width = 320;
  thumbnail.height = 240;
  thumbnail.quality = 85;
  image_config.Update(QMMF_IMAGE_THUMBNAIL, thumbnail, 0);

  // Primary thumbnail parameters.
  thumbnail.width = 960;
  thumbnail.height = 480;
  thumbnail.quality = 90;
  image_config.Update(QMMF_IMAGE_THUMBNAIL, thumbnail, 1);

  ret = recorder_.ConfigImageCapture(camera_id_, image_config);
  ASSERT_TRUE(ret == NO_ERROR);

  uint32_t num_images = burst_image_count_;
  for (uint32_t num = 0; num < num_images; num++) {
    meta_array.push_back(meta);
  }

  for (uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
        test_info_->name(), i);

    ret = recorder_.CaptureImage(camera_id_, image_param, num_images,
                                 meta_array, cb);
    ASSERT_TRUE(ret == NO_ERROR);

    sleep(5);
  }

  ret = recorder_.CancelCaptureImage(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeletePlugin(yuv_cac_plugin.uid);
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

  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());
}

/*
* BurstSnapshotWithBayerLCAC: This test will test burst capture with
*                     post processing. Post processing pipe is Bayer LCAC,
*                     Bayer to YUV reprocessing and JPEG.
* Api test sequence:
*  - StartCamera
*  - CreateSession
*  - CreateVideoTrack
*  - StartSesion
*  - GetSupportedPlugins
*  - CreatePlugin - Bayer LCAC
*  - ConfigImageCapture
*   loop Start {
*   ------------------
*   - CaptureImage - Burst With Bayer LCAC
*   ------------------
*   } loop End
*  - CancelCaptureImage
*  - DeletePlugin
*  - StopSession
*  - DeleteVideoTrack
*  - DeleteSession
*  - StopCamera
*/
TEST_F(RecorderImageGTest, BurstSnapshotWithBayerLCAC) {
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
  video_track_param.low_power_mode = false;

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
  sleep(1);

  ImageParam image_param{};
  image_param.width         = 3872;
  image_param.height        = 2592;
  image_param.image_format  = ImageFormat::kJPEG;
  image_param.image_quality = default_jpeg_quality_;

  std::vector<CameraMetadata> meta_array;
  camera_metadata_entry_t entry;
  CameraMetadata meta;

  ret = recorder_.GetDefaultCaptureParam(camera_id_, meta);
  ASSERT_TRUE(ret == NO_ERROR);

  bool res_supported = false;
  // Check Supported JPEG snapshot resolutions.
  if (meta.exists(ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS)) {
    entry = meta.find(ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS);
    for (uint32_t i = 0 ; i < entry.count; i += 4) {
      if (HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED == entry.data.i32[i]) {
        if (ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT ==
            entry.data.i32[i+3]) {
          if (image_param.width == static_cast<uint32_t>(entry.data.i32[i+1])
              && image_param.height ==
                  static_cast<uint32_t>(entry.data.i32[i+2])) {
            res_supported = true; // 3840x2160 JPEG supported.
          }
        }
      }
    }
  }
  ASSERT_TRUE (res_supported != false);

  ImageCaptureCb cb = [this] (uint32_t camera_id, uint32_t image_count,
                              BufferDescriptor buffer,
                              MetaData meta_data) -> void
      { SnapshotCb(camera_id, image_count, buffer, meta_data); };

  ImageConfigParam image_config;
  PostprocPlugin bayer_lcac_plugin;

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

  ImageThumbnail thumbnail;

  // Secondary thumbnail(Screennail) parameters.
  thumbnail.width = 320;
  thumbnail.height = 240;
  thumbnail.quality = 85;
  image_config.Update(QMMF_IMAGE_THUMBNAIL, thumbnail, 0);

  // Primary thumbnail parameters.
  thumbnail.width = 960;
  thumbnail.height = 480;
  thumbnail.quality = 90;
  image_config.Update(QMMF_IMAGE_THUMBNAIL, thumbnail, 1);

  ret = recorder_.ConfigImageCapture(camera_id_, image_config);
  ASSERT_TRUE(ret == NO_ERROR);

  uint32_t num_images = burst_image_count_;
  for (uint32_t num = 0; num < num_images; num++) {
    meta_array.push_back(meta);
  }

  for (uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
        test_info_->name(), i);

    ret = recorder_.CaptureImage(camera_id_, image_param, num_images,
                                 meta_array, cb);
    ASSERT_TRUE(ret == NO_ERROR);

    sleep(10);
  }

  ret = recorder_.CancelCaptureImage(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

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

  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());
}

/*
* BurstSnapshotWithBayerLCAC15fps:  This test will test burst snapshot with
*                     post processing. Post processing pipe is Bayer LCAC,
*                     Bayer to YUV reprocessing and JPEG with two thumbnails.
* Api test sequence:
*  - StartCamera
*  - CreateSession
*  - CreateVideoTrack
*  - StartSesion
*  - GetSupportedPlugins
*  - CreatePlugin - Bayer LCAC
*  - ConfigImageCapture - Add LCAC and two thumbnails
*   loop Start {
*   ------------------
*   - Lock AE
*   - CaptureImage - Burst With Bayer LCAC
*   - Unlock AE
*   ------------------
*   } loop End
*  - CancelCaptureImage
*  - DeletePlugin
*  - StopSession
*  - DeleteVideoTrack
*  - DeleteSession
*  - StopCamera
*/
TEST_F(RecorderImageGTest, BurstSnapshotWithBayerLCAC15fps) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  std::condition_variable  ae_converge_signal;
  std::mutex ae_converge_mutex;
  bool ae_converged = false;

  CameraResultCb result_cb = [&] (uint32_t camera_id,
      const CameraMetadata &result) {
        if (result.exists(ANDROID_CONTROL_AE_STATE)) {
          uint8_t aec = result.find(ANDROID_CONTROL_AE_STATE).data.u8[0];
          if (((aec == ANDROID_CONTROL_AE_STATE_CONVERGED) ||
            (aec == ANDROID_CONTROL_AE_STATE_LOCKED))) {
            TEST_INFO("%s: AE is converged!!!", __func__);
            std::unique_lock<std::mutex> ae_converge_lock(ae_converge_mutex);
            ae_converged = true;
            ae_converge_signal.notify_one();
          }
        }
      };

  const uint32_t frame_rate = 15;
  camera_start_params_.frame_rate = frame_rate;
  ret = recorder_.StartCamera(camera_id_, camera_start_params_, result_cb);
  ASSERT_TRUE(ret == NO_ERROR);

  SessionCb session_status_cb = CreateSessionStatusCb();
  uint32_t session_id;
  ret = recorder_.CreateSession(session_status_cb, &session_id);
  ASSERT_TRUE(session_id > 0);
  ASSERT_TRUE(ret == NO_ERROR);

  TrackCb video_track_cb;
  video_track_cb.event_cb =
      [this] (uint32_t track_id, EventType event_type,
              void *event_data, size_t event_data_size) -> void {
      VideoTrackEventCb(track_id, event_type, event_data, event_data_size); };

  uint32_t video_track_id = 1;
  VideoTrackCreateParam video_track_param{camera_id_, VideoFormat::kAVC,
                                          640,
                                          480,
                                          frame_rate};
  video_track_param.low_power_mode = false;

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
  sleep(2);

  ImageParam image_param{};
  image_param.width         = 3872;
  image_param.height        = 2592;
  image_param.image_format  = ImageFormat::kJPEG;
  image_param.image_quality = default_jpeg_quality_;

  std::vector<CameraMetadata> meta_array;
  camera_metadata_entry_t entry;
  CameraMetadata meta;

  ret = recorder_.GetDefaultCaptureParam(camera_id_, meta);
  ASSERT_TRUE(ret == NO_ERROR);

  bool res_supported = false;
  // Check Supported JPEG snapshot resolutions.
  if (meta.exists(ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS)) {
    entry = meta.find(ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS);
    for (uint32_t i = 0 ; i < entry.count; i += 4) {
      if (HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED == entry.data.i32[i]) {
        if (ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT ==
            entry.data.i32[i+3]) {
          if (image_param.width == static_cast<uint32_t>(entry.data.i32[i+1])
              && image_param.height ==
                  static_cast<uint32_t>(entry.data.i32[i+2])) {
            res_supported = true; // 3840x2160 JPEG supported.
          }
        }
      }
    }
  }
  ASSERT_TRUE (res_supported != false);

  ImageCaptureCb cb = [this] (uint32_t camera_id, uint32_t image_count,
                              BufferDescriptor buffer,
                              MetaData meta_data) -> void
      { SnapshotCb(camera_id, image_count, buffer, meta_data); };

  ImageConfigParam image_config;
  PostprocPlugin bayer_lcac_plugin;

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

  ImageThumbnail thumbnail;

  // Secondary thumbnail(Screennail) parameters.
  thumbnail.width = 320;
  thumbnail.height = 240;
  thumbnail.quality = 85;
  image_config.Update(QMMF_IMAGE_THUMBNAIL, thumbnail, 0);

  // Primary thumbnail parameters.
  thumbnail.width = 960;
  thumbnail.height = 480;
  thumbnail.quality = 90;
  image_config.Update(QMMF_IMAGE_THUMBNAIL, thumbnail, 1);

  ret = recorder_.ConfigImageCapture(camera_id_, image_config);
  ASSERT_TRUE(ret == NO_ERROR);

  // Set frame rate otherwise default value is used
  int32_t fps_range[2];
  fps_range[0] = frame_rate;
  fps_range[1] = frame_rate;
  ret = meta.update(ANDROID_CONTROL_AE_TARGET_FPS_RANGE, fps_range, 2);
  ASSERT_TRUE(ret == NO_ERROR);

  uint32_t num_images = burst_image_count_;
  for (uint32_t num = 0; num < num_images; num++) {
    meta_array.push_back(meta);
  }

  for (uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
        test_info_->name(), i);

    {
      // Wait for AE convergence
      std::unique_lock<std::mutex> ae_converge_lock(ae_converge_mutex);
      if (!ae_converged) {
        TEST_INFO("%s: Wait for AE to Converged!", __func__);
        auto status = ae_converge_signal.wait_for(ae_converge_lock,
        std::chrono::seconds(30 / frame_rate + 1));
        ASSERT_TRUE(status == std::cv_status::no_timeout);
        TEST_INFO("%s: AE Converged succesfuly", __func__);
      } else {
        TEST_INFO("%s: AE is already converged!", __func__);
      }
    }
    // Lock AE
    CameraMetadata video_meta;
    ret = recorder_.GetCameraParam(camera_id_, video_meta);
    ASSERT_TRUE(ret == NO_ERROR);

    uint8_t ae_lock = ANDROID_CONTROL_AE_LOCK_ON;
    ret = video_meta.update(ANDROID_CONTROL_AE_LOCK, &ae_lock, 1);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.SetCameraParam(camera_id_, video_meta);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.CaptureImage(camera_id_, image_param, num_images,
                                 meta_array, cb);
    ASSERT_TRUE(ret == NO_ERROR);

    sleep(10);

    {
      std::unique_lock<std::mutex> ae_converge_lock(ae_converge_mutex);
      ae_converged = false;
    }
    // Unlock AE
    ae_lock = ANDROID_CONTROL_AE_LOCK_OFF;
    ret = video_meta.update(ANDROID_CONTROL_AE_LOCK, &ae_lock, 1);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.SetCameraParam(camera_id_, video_meta);
    ASSERT_TRUE(ret == NO_ERROR);
  }

  ret = recorder_.CancelCaptureImage(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

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

  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());
}

/*
* BurstSnapshotWithBayerLCAC15fpsWithCdsOff:  This test will test burst snapshot
*                     with post processing. Post processing pipe is Bayer LCAC,
*                     Bayer to YUV reprocessing and JPEG with two thumbnails.
* Api test sequence:
*  - StartCamera
*  - CreateSession
*  - CreateVideoTrack
*  - StartSesion
*  - GetSupportedPlugins
*  - CreatePlugin - Bayer LCAC
*  - ConfigImageCapture - Add LCAC and two thumbnails
*  - Disable CDS
*   loop Start {
*   ------------------
*   - Lock AE
*   - CaptureImage - Burst With Bayer LCAC
*   - Unlock AE
*   ------------------
*   } loop End
*  - CancelCaptureImage
*  - DeletePlugin
*  - StopSession
*  - DeleteVideoTrack
*  - DeleteSession
*  - StopCamera
*/
TEST_F(RecorderImageGTest, BurstSnapshotWithBayerLCAC15fpsWithCdsOff) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  std::condition_variable  ae_converge_signal;
  std::mutex ae_converge_mutex;
  bool ae_converged = false;

  CameraResultCb result_cb = [&] (uint32_t camera_id,
      const CameraMetadata &result) {
        if (result.exists(ANDROID_CONTROL_AE_STATE)) {
          uint8_t aec = result.find(ANDROID_CONTROL_AE_STATE).data.u8[0];
          if (((aec == ANDROID_CONTROL_AE_STATE_CONVERGED) ||
            (aec == ANDROID_CONTROL_AE_STATE_LOCKED))) {
            TEST_INFO("%s: AE is converged!!!", __func__);
            std::unique_lock<std::mutex> ae_converge_lock(ae_converge_mutex);
            ae_converged = true;
            ae_converge_signal.notify_one();
          }
        }
      };

  const uint32_t frame_rate = 15;
  camera_start_params_.frame_rate = frame_rate;
  ret = recorder_.StartCamera(camera_id_, camera_start_params_, result_cb);
  ASSERT_TRUE(ret == NO_ERROR);

  SessionCb session_status_cb = CreateSessionStatusCb();
  uint32_t session_id;
  ret = recorder_.CreateSession(session_status_cb, &session_id);
  ASSERT_TRUE(session_id > 0);
  ASSERT_TRUE(ret == NO_ERROR);

  TrackCb video_track_cb;
  video_track_cb.event_cb =
      [this] (uint32_t track_id, EventType event_type,
              void *event_data, size_t event_data_size) -> void {
      VideoTrackEventCb(track_id, event_type, event_data, event_data_size); };

  uint32_t video_track_id = 1;
  VideoTrackCreateParam video_track_param{camera_id_, VideoFormat::kAVC,
                                          640,
                                          480,
                                          frame_rate};
  video_track_param.low_power_mode = false;

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
  sleep(2);

  ImageParam image_param{};
  image_param.width         = 3872;
  image_param.height        = 2592;
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

  ImageConfigParam image_config;
  PostprocPlugin bayer_lcac_plugin;

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

  ImageThumbnail thumbnail;

  // Secondary thumbnail(Screennail) parameters.
  thumbnail.width = 320;
  thumbnail.height = 240;
  thumbnail.quality = 85;
  image_config.Update(QMMF_IMAGE_THUMBNAIL, thumbnail, 0);

  // Primary thumbnail parameters.
  thumbnail.width = 960;
  thumbnail.height = 480;
  thumbnail.quality = 90;
  image_config.Update(QMMF_IMAGE_THUMBNAIL, thumbnail, 1);

  ret = recorder_.ConfigImageCapture(camera_id_, image_config);
  ASSERT_TRUE(ret == NO_ERROR);

  int32_t cds_mode = 0; // 0-Off, 1-On, 2-Auto
  TEST_INFO("%s: Disable CDS", __func__);
  meta.update(  QCAMERA3_CDS_MODE, &cds_mode, 1);

  // Set frame rate otherwise default value is used
  int32_t fps_range[2];
  fps_range[0] = frame_rate;
  fps_range[1] = frame_rate;
  ret = meta.update(ANDROID_CONTROL_AE_TARGET_FPS_RANGE, fps_range, 2);
  ASSERT_TRUE(ret == NO_ERROR);

  uint32_t num_images = burst_image_count_;
  for (uint32_t num = 0; num < num_images; num++) {
    meta_array.push_back(meta);
  }

  for (uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
        test_info_->name(), i);

    {
      // Wait for AE convergence
      std::unique_lock<std::mutex> ae_converge_lock(ae_converge_mutex);
      if (!ae_converged) {
        TEST_INFO("%s: Wait for AE to Converged!", __func__);
        auto status = ae_converge_signal.wait_for(ae_converge_lock,
        std::chrono::seconds(30 / frame_rate + 1));
        ASSERT_TRUE(status == std::cv_status::no_timeout);
        TEST_INFO("%s: AE Converged succesfuly", __func__);
      } else {
        TEST_INFO("%s: AE is already converged!", __func__);
      }
    }
    // Lock AE
    CameraMetadata video_meta;
    ret = recorder_.GetCameraParam(camera_id_, video_meta);
    ASSERT_TRUE(ret == NO_ERROR);

    uint8_t ae_lock = ANDROID_CONTROL_AE_LOCK_ON;
    ret = video_meta.update(ANDROID_CONTROL_AE_LOCK, &ae_lock, 1);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.SetCameraParam(camera_id_, video_meta);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.CaptureImage(camera_id_, image_param, num_images,
                                 meta_array, cb);
    ASSERT_TRUE(ret == NO_ERROR);

    sleep(10);

    {
      std::unique_lock<std::mutex> ae_converge_lock(ae_converge_mutex);
      ae_converged = false;
    }
    // Unlock AE
    ae_lock = ANDROID_CONTROL_AE_LOCK_OFF;
    ret = video_meta.update(ANDROID_CONTROL_AE_LOCK, &ae_lock, 1);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.SetCameraParam(camera_id_, video_meta);
    ASSERT_TRUE(ret == NO_ERROR);
  }

  ret = recorder_.CancelCaptureImage(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

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

  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());
}

/*
* AutoBurstCaptureWithBayerLCAC:  This test will test burst snapshot with post
*                    processing and differnet frame rate. Post processing
*                    pipe is Bayer LCAC, Bayer to YUV reprocessing and
*                    JPEG with two thumbnails.
* Api test sequence:
* loop auto modes {
*    - StartCamera
*    - CreateSession
*    - CreateVideoTrack
*    - StartSesion
*    - GetSupportedPlugins
*    - CreatePlugin - Bayer LCAC
*    - ConfigImageCapture - Add LCAC and two thumbnails
*     loop Start {
*     ------------------
*     - Lock AE
*     - CaptureImage - Burst With Bayer LCAC
*     - Unlock AE
*     ------------------
*     } loop End
*    - CancelCaptureImage
*    - DeletePlugin
*    - StopSession
*    - DeleteVideoTrack
*    - DeleteSession
*    - StopCamera
* } loop End
*/
TEST_F(RecorderImageGTest, AutoBurstCaptureWithBayerLCAC) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  // Auto burst modes: 3 in 1 or 5 in 1 or 10 in 1 or 15 in 1 frames per second
  std::vector<uint32_t> auto_burst_modes = {3, 5, 10, 15};

  std::condition_variable  ae_converge_signal;
  std::mutex ae_converge_mutex;
  bool ae_converged = false;

  for (auto rate : auto_burst_modes) {

    CameraResultCb result_cb = [&] (uint32_t camera_id,
        const CameraMetadata &result) {
          if (result.exists(ANDROID_CONTROL_AE_STATE)) {
            uint8_t aec = result.find(ANDROID_CONTROL_AE_STATE).data.u8[0];
            if (((aec == ANDROID_CONTROL_AE_STATE_CONVERGED) ||
              (aec == ANDROID_CONTROL_AE_STATE_LOCKED))) {
              TEST_INFO("%s: AE is converged!!!", __func__);
              std::unique_lock<std::mutex> ae_converge_lock(ae_converge_mutex);
              ae_converged = true;
              ae_converge_signal.notify_one();
            }
          }
        };

    camera_start_params_.frame_rate = rate;
    ret = recorder_.StartCamera(camera_id_, camera_start_params_, result_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    SessionCb session_status_cb = CreateSessionStatusCb();
    uint32_t session_id;
    ret = recorder_.CreateSession(session_status_cb, &session_id);
    ASSERT_TRUE(session_id > 0);
    ASSERT_TRUE(ret == NO_ERROR);

    TrackCb video_track_cb;
    video_track_cb.event_cb =
        [this] (uint32_t track_id, EventType event_type,
                void *event_data, size_t event_data_size) -> void {
        VideoTrackEventCb(track_id, event_type, event_data, event_data_size); };

    uint32_t video_track_id = 1;
    VideoTrackCreateParam video_track_param{camera_id_, VideoFormat::kAVC,
                                            640,
                                            480,
                                            (float)rate};
    video_track_param.low_power_mode = false;

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
    sleep(2);

    ImageParam image_param{};
    image_param.width         = 3872;
    image_param.height        = 2592;
    image_param.image_format  = ImageFormat::kJPEG;
    image_param.image_quality = default_jpeg_quality_;

    std::vector<CameraMetadata> meta_array;
    camera_metadata_entry_t entry;
    CameraMetadata meta;

    ret = recorder_.GetDefaultCaptureParam(camera_id_, meta);
    ASSERT_TRUE(ret == NO_ERROR);

    bool res_supported = false;
    // Check Supported JPEG snapshot resolutions.
    if (meta.exists(ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS)) {
      entry = meta.find(ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS);
      for (uint32_t i = 0 ; i < entry.count; i += 4) {
        if (HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED == entry.data.i32[i]) {
          if (ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT ==
              entry.data.i32[i+3]) {
            if (image_param.width == static_cast<uint32_t>(entry.data.i32[i+1])
                && image_param.height ==
                    static_cast<uint32_t>(entry.data.i32[i+2])) {
              res_supported = true; // 3840x2160 JPEG supported.
            }
          }
        }
      }
    }
    ASSERT_TRUE (res_supported != false);

    ImageCaptureCb cb = [this] (uint32_t camera_id, uint32_t image_count,
                                BufferDescriptor buffer,
                                MetaData meta_data) -> void
        { SnapshotCb(camera_id, image_count, buffer, meta_data); };

    ImageConfigParam image_config;
    PostprocPlugin bayer_lcac_plugin;

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

    ImageThumbnail thumbnail;
    // Secondary thumbnail(Screennail) parameters.
    thumbnail.width = 320;
    thumbnail.height = 240;
    thumbnail.quality = 85;
    image_config.Update(QMMF_IMAGE_THUMBNAIL, thumbnail, 0);

    // Primary thumbnail parameters.
    thumbnail.width = 960;
    thumbnail.height = 480;
    thumbnail.quality = 90;
    image_config.Update(QMMF_IMAGE_THUMBNAIL, thumbnail, 1);

    ret = recorder_.ConfigImageCapture(camera_id_, image_config);
    ASSERT_TRUE(ret == NO_ERROR);

    // Set frame rate otherwise default value is used
    int32_t fps_range[2];
    fps_range[0] = rate;
    fps_range[1] = rate;
    ret = meta.update(ANDROID_CONTROL_AE_TARGET_FPS_RANGE, fps_range, 2);
    ASSERT_TRUE(ret == NO_ERROR);

    // Auto burst is for 2 seconds. Rate is specified for 1 second.
    uint32_t num_images = 2 * rate;

    for (uint32_t num = 0; num < num_images; num++) {
      meta_array.push_back(meta);
    }

    for (uint32_t i = 1; i <= iteration_count_; i++) {
      fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
      TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
          test_info_->name(), i);

      {
        // Wait for AE to converge.
        std::unique_lock<std::mutex> ae_converge_lock(ae_converge_mutex);
        if (!ae_converged) {
          TEST_INFO("%s: Wait for AE to Converged!", __func__);
          auto status = ae_converge_signal.wait_for(ae_converge_lock,
          std::chrono::seconds(30 / rate + 1));
          ASSERT_TRUE(status == std::cv_status::no_timeout);
          TEST_INFO("%s: AE Converged succesfuly", __func__);
        } else {
          TEST_INFO("%s: AE is already converged!", __func__);
        }
      }

      // Lock AE

      CameraMetadata video_meta;
      ret = recorder_.GetCameraParam(camera_id_, video_meta);
      ASSERT_TRUE(ret == NO_ERROR);

      uint8_t ae_lock = ANDROID_CONTROL_AE_LOCK_ON;
      ret = video_meta.update(ANDROID_CONTROL_AE_LOCK, &ae_lock, 1);
      ASSERT_TRUE(ret == NO_ERROR);

      ret = recorder_.SetCameraParam(camera_id_, video_meta);
      ASSERT_TRUE(ret == NO_ERROR);

      ret = recorder_.CaptureImage(camera_id_, image_param, num_images,
                                   meta_array, cb);
      ASSERT_TRUE(ret == NO_ERROR);

      sleep(10);

      {
        // AE and wait for to converge for next round of run.
        std::unique_lock<std::mutex> ae_converge_lock(ae_converge_mutex);
        ae_converged = false;
      }

      // Unlock AE
      ae_lock = ANDROID_CONTROL_AE_LOCK_OFF;
      ret = video_meta.update(ANDROID_CONTROL_AE_LOCK, &ae_lock, 1);
      ASSERT_TRUE(ret == NO_ERROR);

      ret = recorder_.SetCameraParam(camera_id_, video_meta);
      ASSERT_TRUE(ret == NO_ERROR);
    }

    ret = recorder_.CancelCaptureImage(camera_id_);
    ASSERT_TRUE(ret == NO_ERROR);

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
  }

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());
}

/*
* ContinuousSnapshotWithBayerLCAC:  This test will test continuous capture with
*                     post processing. Post processing pipe is Bayer LCAC,
*                     Bayer to YUV reprocessing and JPEG with two thumbnails.
* Api test sequence:
*  - StartCamera
*  - CreateSession
*  - CreateVideoTrack
*  - StartSesion
*  - GetSupportedPlugins
*  - CreatePlugin - Bayer LCAC
*  - ConfigImageCapture - Add LCAC and two thumbnails
*   loop Start {
*   ------------------
*   - Lock AE
*   - CaptureImage - Continuous Capture With Bayer LCAC
*   - CancelCaptureImage
*   - Unlock AE
*   ------------------
*   } loop End
*  - DeletePlugin
*  - StopSession
*  - DeleteVideoTrack
*  - DeleteSession
*  - StopCamera
*/
TEST_F(RecorderImageGTest, ContinuousSnapshotWithBayerLCAC) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  const uint32_t preview_frame_rate = 30;
  const uint32_t capture_frame_rate = 5;

  std::condition_variable  ae_converge_signal;
  std::mutex ae_converge_mutex;
  bool ae_converged = false;

  CameraResultCb result_cb = [&] (uint32_t camera_id,
      const CameraMetadata &result) {
        if (result.exists(ANDROID_CONTROL_AE_STATE)) {
          uint8_t aec = result.find(ANDROID_CONTROL_AE_STATE).data.u8[0];
          if (((aec == ANDROID_CONTROL_AE_STATE_CONVERGED) ||
            (aec == ANDROID_CONTROL_AE_STATE_LOCKED))) {
            TEST_INFO("%s: AE is converged!!", __func__);
            ae_converged = true;
            ae_converge_signal.notify_one();
          }
        }
      };

  camera_start_params_.frame_rate = preview_frame_rate;
  ret = recorder_.StartCamera(camera_id_, camera_start_params_, result_cb);
  ASSERT_TRUE(ret == NO_ERROR);

  SessionCb session_status_cb = CreateSessionStatusCb();
  uint32_t session_id;
  ret = recorder_.CreateSession(session_status_cb, &session_id);
  ASSERT_TRUE(session_id > 0);
  ASSERT_TRUE(ret == NO_ERROR);

  TrackCb video_track_cb;
  video_track_cb.event_cb =
      [this] (uint32_t track_id, EventType event_type,
              void *event_data, size_t event_data_size) -> void {
      VideoTrackEventCb(track_id, event_type, event_data, event_data_size); };

  uint32_t video_track_id = 1;
  VideoTrackCreateParam video_track_param{camera_id_, VideoFormat::kAVC,
                                          640,
                                          480,
                                          preview_frame_rate};
  video_track_param.low_power_mode = false;

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
  sleep(2);

  ImageParam image_param{};
  image_param.width         = 3872;
  image_param.height        = 2592;
  image_param.image_format  = ImageFormat::kJPEG;
  image_param.image_quality = default_jpeg_quality_;

  std::vector<CameraMetadata> meta_array;
  camera_metadata_entry_t entry;
  CameraMetadata meta;

  ret = recorder_.GetDefaultCaptureParam(camera_id_, meta);
  ASSERT_TRUE(ret == NO_ERROR);

  bool res_supported = false;
  // Check Supported JPEG snapshot resolutions.
  if (meta.exists(ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS)) {
    entry = meta.find(ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS);
    for (uint32_t i = 0 ; i < entry.count; i += 4) {
      if (HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED == entry.data.i32[i]) {
        if (ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT ==
            entry.data.i32[i+3]) {
          if (image_param.width == static_cast<uint32_t>(entry.data.i32[i+1])
              && image_param.height ==
                  static_cast<uint32_t>(entry.data.i32[i+2])) {
            res_supported = true; // 3840x2160 JPEG supported.
          }
        }
      }
    }
  }
  ASSERT_TRUE (res_supported != false);

  ImageCaptureCb cb = [this] (uint32_t camera_id, uint32_t image_count,
                              BufferDescriptor buffer,
                              MetaData meta_data) -> void
      { SnapshotCb(camera_id, image_count, buffer, meta_data); };

  ImageConfigParam image_config;
  PostprocPlugin bayer_lcac_plugin;

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

  SnapshotType snapshot_type;
  snapshot_type.type = SnapshotMode::kContinuous;
  image_config.Update(QMMF_SNAPSHOT_TYPE, snapshot_type, 0);

  // Ensure that preview and capture frame rate are multiple
  // otherwise we cannot ensure persistent capture rate.
  ASSERT_TRUE(preview_frame_rate % capture_frame_rate == 0);

  PostprocFrameSkip frame_skip;
  frame_skip.frame_skip = (preview_frame_rate / capture_frame_rate) - 1;
  image_config.Update(QMMF_POSTPROCESS_FRAME_SKIP, frame_skip, 0);

  ImageThumbnail thumbnail;

  // Secondary thumbnail(Screennail) parameters.
  thumbnail.width = 320;
  thumbnail.height = 240;
  thumbnail.quality = 85;
  image_config.Update(QMMF_IMAGE_THUMBNAIL, thumbnail, 0);

  // Primary thumbnail parameters.
  thumbnail.width = 960;
  thumbnail.height = 480;
  thumbnail.quality = 90;
  image_config.Update(QMMF_IMAGE_THUMBNAIL, thumbnail, 1);

  ret = recorder_.ConfigImageCapture(camera_id_, image_config);
  ASSERT_TRUE(ret == NO_ERROR);

  // Set frame rate otherwise default value is used
  int32_t fps_range[2];
  fps_range[0] = preview_frame_rate;
  fps_range[1] = preview_frame_rate;
  ret = meta.update(ANDROID_CONTROL_AE_TARGET_FPS_RANGE, fps_range, 2);
  ASSERT_TRUE(ret == NO_ERROR);

  meta_array.push_back(meta);

  for (uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
        test_info_->name(), i);

    {
      // Wait for AE convergence
      std::unique_lock<std::mutex> ae_converge_lock(ae_converge_mutex);
      if (!ae_converged) {
        TEST_INFO("%s: Wait for AE to Converged!", __func__);
        auto status = ae_converge_signal.wait_for(ae_converge_lock,
        std::chrono::seconds(30 / preview_frame_rate + 1));
        ASSERT_TRUE(status == std::cv_status::no_timeout);
        TEST_INFO("%s: AE Converged succesfuly", __func__);
      } else {
        TEST_INFO("%s: AE is already converged!", __func__);
      }
    }

    CameraMetadata video_meta;
    ret = recorder_.GetCameraParam(camera_id_, video_meta);
    ASSERT_TRUE(ret == NO_ERROR);

    // Lock AE for snapshot
    uint8_t ae_lock = ANDROID_CONTROL_AE_LOCK_ON;
    ret = video_meta.update(ANDROID_CONTROL_AE_LOCK, &ae_lock, 1);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.SetCameraParam(camera_id_, video_meta);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.CaptureImage(camera_id_, image_param, 1, meta_array, cb);
    ASSERT_TRUE(ret == NO_ERROR);

    sleep(10);

    ret = recorder_.CancelCaptureImage(camera_id_);
    ASSERT_TRUE(ret == NO_ERROR);

    {
      // AE and wait for to converge for next round of run.
      std::unique_lock<std::mutex> ae_converge_lock(ae_converge_mutex);
      ae_converged = false;
    }

    // Unlock AE
    ae_lock = ANDROID_CONTROL_AE_LOCK_OFF;
    ret = video_meta.update(ANDROID_CONTROL_AE_LOCK, &ae_lock, 1);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.SetCameraParam(camera_id_, video_meta);
    ASSERT_TRUE(ret == NO_ERROR);
  }

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

  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());
}

/*
* MaxSnapshotThumb: This test will test Max resolution JPEG snapshot
*                   with a max size thumbnail.
* Api test sequence:
*  - StartCamera
*   loop Start {
*   ------------------
*   - CaptureImage - JPEG with thumbnail
*   ------------------
*   } loop End
*  - StopCamera
*/
TEST_F(RecorderImageGTest, MaxSnapshotThumb) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.StartCamera(camera_id_, camera_start_params_);
  ASSERT_TRUE(ret == NO_ERROR);

  int32_t thumb_size[2] = {0,0};
  ImageParam image_param{};
  image_param.image_format  = ImageFormat::kJPEG;
  image_param.image_quality = default_jpeg_quality_;

  std::vector<CameraMetadata> meta_array;
  camera_metadata_entry_t entry;
  CameraMetadata meta;

  ret = recorder_.GetDefaultCaptureParam(camera_id_, meta);
  ASSERT_TRUE(ret == NO_ERROR);

  // Check Supported JPEG snapshot resolutions.
  if (meta.exists(ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS)) {
    entry = meta.find(ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS);
    for (uint32_t i = 0 ; i < entry.count; i += 4) {
      if (HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED == entry.data.i32[i]) {
        if (ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT ==
            entry.data.i32[i+3]) {
          if (image_param.width < static_cast<uint32_t>(entry.data.i32[i+1])) {
            image_param.width = entry.data.i32[i+1];
            image_param.height = entry.data.i32[i+2];
          }

          fprintf(stderr,"Supported Size %dx%d\n",
              entry.data.i32[i+1], entry.data.i32[i+2]);
        }
      }
    }
  }
  ASSERT_TRUE(image_param.width > 0 && image_param.height > 0);

  if (meta.exists(ANDROID_JPEG_AVAILABLE_THUMBNAIL_SIZES)) {
    entry = meta.find(ANDROID_JPEG_AVAILABLE_THUMBNAIL_SIZES);
    for (uint32_t i = 0 ; i < entry.count; i += 2) {
      if (thumb_size[0] < entry.data.i32[i]) {
        thumb_size[0] = entry.data.i32[i];
        thumb_size[1] = entry.data.i32[i+1];
      }
    }
  }
  ASSERT_TRUE(thumb_size[0] > 0 && thumb_size[1] > 0);
  ret = meta.update(ANDROID_JPEG_THUMBNAIL_SIZE, thumb_size, 2);
  ASSERT_TRUE(ret == NO_ERROR);


  /* we have only capture stream which will by default disable WB and lead to
     broken picture */
  uint8_t intent = ANDROID_CONTROL_CAPTURE_INTENT_PREVIEW;
  ret = meta.update(ANDROID_CONTROL_CAPTURE_INTENT, &intent, 1);
  ASSERT_TRUE(ret == NO_ERROR);
  meta_array.push_back(meta);

  fprintf(stderr,"Capturing %dx%d JPEG with %dx%d thumbnail\n",
      image_param.width, image_param.height, thumb_size[0], thumb_size[1]);
  for(uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
        test_info_->name(), i);

    ImageCaptureCb cb = [this] (uint32_t camera_id, uint32_t image_count,
                                BufferDescriptor buffer,
                                MetaData meta_data) -> void
        { SnapshotCb(camera_id, image_count, buffer, meta_data); };

    ret = recorder_.CaptureImage(camera_id_, image_param, 1, meta_array,
                                 cb);
    ASSERT_TRUE(ret == NO_ERROR);
    // Take snapshot after every 5 sec.
    sleep(5);
  }

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());

}

/*
* 1080pRawYUVSnapshot: This test will test 1080p YUV snapshot.
* Api test sequence:
*  - StartCamera
*   loop Start {
*   ------------------
*   - CaptureImage - Raw YUV
*   ------------------
*   } loop End
*  - StopCamera
*/
TEST_F(RecorderImageGTest, 1080pRawYUVSnapshot) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.StartCamera(camera_id_, camera_start_params_);
  ASSERT_TRUE(ret == NO_ERROR);

  ImageParam image_param{};
  image_param.width         = 1920;
  image_param.height        = 1080;
  image_param.image_format  = ImageFormat::kNV12;

  std::vector<CameraMetadata> meta_array;
  camera_metadata_entry_t entry;
  CameraMetadata meta;

  ret = recorder_.GetDefaultCaptureParam(camera_id_, meta);
  ASSERT_TRUE(ret == NO_ERROR);
  meta_array.push_back(meta);

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
            res_supported = true; // 1920x1080 YUV res supported.
          }
        }
      }
    }
  }
  ASSERT_TRUE(res_supported != false);

  for(uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
        test_info_->name(), i);

    ImageCaptureCb cb = [this] (uint32_t camera_id, uint32_t image_count,
                                BufferDescriptor buffer,
                                MetaData meta_data) -> void
        { SnapshotCb(camera_id, image_count, buffer, meta_data); };

    ret = recorder_.CaptureImage(camera_id_, image_param, 1, meta_array,
                                 cb);
    ASSERT_TRUE(ret == NO_ERROR);
    // Take snapshot after every 5 sec.
    sleep(5);
  }

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());

}

/*
* RawBayerRDI10Snapshot: This test will test BayerRDI (10 bits packed) snapshot.
* Api test sequence:
*  - StartCamera
*   loop Start {
*   ------------------
*   - CaptureImage - BayerRDI 10 bits
*   ------------------
*   } loop End
*  - StopCamera
*/
TEST_F(RecorderImageGTest, RawBayerRDI10Snapshot) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.StartCamera(camera_id_, camera_start_params_);
  ASSERT_TRUE(ret == NO_ERROR);

  camera_metadata_entry_t entry;
  CameraMetadata meta;
  int32_t w = 0, h = 0;
  ret = recorder_.GetDefaultCaptureParam(camera_id_, meta);
  // Check Supported bayer snapshot resolutions.
  if (meta.exists(ANDROID_SCALER_AVAILABLE_RAW_SIZES)) {
    entry = meta.find(ANDROID_SCALER_AVAILABLE_RAW_SIZES);
    for (uint32_t i = 0 ; i < entry.count; i += 2) {
      w = entry.data.i32[i+0];
      h = entry.data.i32[i+1];
      TEST_INFO("%s: (%d) Supported RAW RDI W(%d):H(%d)",
          __func__, i, w, h);
    }
  }
  ASSERT_TRUE(w > 0 && h > 0);
  ImageParam image_param{};
  image_param.width        = w; // 5344
  image_param.height       = h; // 4016
  image_param.image_format = ImageFormat::kBayerRDI10BIT;

  std::vector<CameraMetadata> meta_array;
  meta_array.push_back(meta);
  for(uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
        test_info_->name(), i);

    ImageCaptureCb cb = [this] (uint32_t camera_id, uint32_t image_count,
                                BufferDescriptor buffer,
                                MetaData meta_data) -> void
      { SnapshotCb(camera_id, image_count, buffer, meta_data); };

    ret = recorder_.CaptureImage(camera_id_, image_param, 1, meta_array,
                                 cb);
    ASSERT_TRUE(ret == NO_ERROR);
    // Take snapshot after every 5 sec.
    sleep(5);
  }

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());

}

/*
* RawBayerRDI12Snapshot: This test will test BayerRDI (12 bits packed) snapshot.
* Api test sequence:
*  - StartCamera
*   loop Start {
*   ------------------
*   - CaptureImage - BayerRDI 12 bits
*   ------------------
*   } loop End
*  - StopCamera
*/
TEST_F(RecorderImageGTest, RawBayerRDI12Snapshot) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.StartCamera(camera_id_, camera_start_params_);
  ASSERT_TRUE(ret == NO_ERROR);

  camera_metadata_entry_t entry;
  CameraMetadata meta;
  int32_t w = 0, h = 0;
  ret = recorder_.GetDefaultCaptureParam(camera_id_, meta);
  // Check Supported bayer snapshot resolutions.
  if (meta.exists(ANDROID_SCALER_AVAILABLE_RAW_SIZES)) {
    entry = meta.find(ANDROID_SCALER_AVAILABLE_RAW_SIZES);
    for (uint32_t i = 0 ; i < entry.count; i += 2) {
      w = entry.data.i32[i+0];
      h = entry.data.i32[i+1];
      TEST_INFO("%s: (%d) Supported RAW RDI W(%d):H(%d)",
          __func__, i, w, h);
    }
  }
  ASSERT_TRUE(w > 0 && h > 0);
  ImageParam image_param{};
  image_param.width        = w;
  image_param.height       = h;
  image_param.image_format = ImageFormat::kBayerRDI12BIT;

  std::vector<CameraMetadata> meta_array;
  meta_array.push_back(meta);
  for(uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
        test_info_->name(), i);

    ImageCaptureCb cb = [this] (uint32_t camera_id, uint32_t image_count,
                                BufferDescriptor buffer,
                                MetaData meta_data) -> void
      { SnapshotCb(camera_id, image_count, buffer, meta_data); };

    ret = recorder_.CaptureImage(camera_id_, image_param, 1, meta_array,
                                 cb);
    ASSERT_TRUE(ret == NO_ERROR);
    // Take snapshot after every 5 sec.
    sleep(5);
  }

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());
}

/*
* RawBayerRDI8Snapshot: This test will test BayerRDI (8 bits packed) snapshot.
* Api test sequence:
*  - StartCamera
*   loop Start {
*   ------------------
*   - CaptureImage - BayerRDI 8 bits
*   ------------------
*   } loop End
*  - StopCamera
*/
TEST_F(RecorderImageGTest, RawBayerRDI8Snapshot) {
  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.StartCamera(camera_id_, camera_start_params_);
  ASSERT_TRUE(ret == NO_ERROR);

  camera_metadata_entry_t entry;
  CameraMetadata meta;
  int32_t w = 0, h = 0;
  ret = recorder_.GetDefaultCaptureParam(camera_id_, meta);
  // Check Supported bayer snapshot resolutions.
  if (meta.exists(ANDROID_SCALER_AVAILABLE_RAW_SIZES)) {
    entry = meta.find(ANDROID_SCALER_AVAILABLE_RAW_SIZES);
    for (uint32_t i = 0 ; i < entry.count; i += 2) {
      w = entry.data.i32[i+0];
      h = entry.data.i32[i+1];
      TEST_INFO("%s: (%d) Supported RAW RDI W(%d):H(%d)",
          __func__, i, w, h);
    }
  }
  ASSERT_TRUE(w > 0 && h > 0);
  ImageParam image_param{};
  image_param.width        = w;
  image_param.height       = h;
  image_param.image_format = ImageFormat::kBayerRDI8BIT;

  std::vector<CameraMetadata> meta_array;
  meta_array.push_back(meta);
  for(uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
        test_info_->name(), i);

    ImageCaptureCb cb = [this] (uint32_t camera_id, uint32_t image_count,
                                BufferDescriptor buffer,
                                MetaData meta_data) -> void
      { SnapshotCb(camera_id, image_count, buffer, meta_data); };

    ret = recorder_.CaptureImage(camera_id_, image_param, 1, meta_array,
                                 cb);
    ASSERT_TRUE(ret == NO_ERROR);
    // Take snapshot after every 5 sec.
    sleep(5);
  }

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  fprintf(stderr,"---------- Test Completed %s.%s ----------\n",
      test_info_->test_case_name(), test_info_->name());
}

/*
* SingleSnapshotFocalLength: This test will test 4K jpg snapshot
* focal length change.
* Api test sequence:
*  - StartCamera
*  - update meta FL
*  - CaptureImage - Jpeg
*  - StopCamera
*/
TEST_F(RecorderImageGTest, SingleSnapshotFocalLength) {
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
  image_param.image_quality = 95;

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
            res_supported = true; // 1920x1080 YUV res supported.
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


  float focal_length = 4.83;
  meta.update(ANDROID_LENS_FOCAL_LENGTH, &focal_length, 1);

  int32_t fpsRange[2];
  fpsRange[0] = 4;
  fpsRange[1] = 4;
  meta.update(ANDROID_CONTROL_AE_TARGET_FPS_RANGE, fpsRange, 2);

  meta_array.push_back(meta);

  {
    std::lock_guard<std::mutex> lock(error_lock_);
    camera_error_ = false;
  }
  ret = recorder_.CaptureImage(camera_id_, image_param, 1,
                               meta_array, cb);
  ASSERT_TRUE(ret == NO_ERROR);

  sleep(5);
  {
    std::lock_guard<std::mutex> lock(error_lock_);
    if (camera_error_) {
      TEST_ERROR("%s Capture Image Failed", __func__);
    }
    ASSERT_TRUE(camera_error_ == false);
  }

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

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
TEST_F(RecorderImageGTest, LowResVideo10MPContinuousSnapshotWithLCACAndCdsOff) {
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

  bool res_supported = GtestCommon::ValidateResFromJpegSizes(meta,
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
TEST_F(RecorderImageGTest,
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

  bool res_supported = GtestCommon::ValidateResFromJpegSizes(meta,
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

#ifdef CAM_ARCH_V2
/*
* PreviewAndRaw10BitBayerSnapshot: This test will test BayerRDI (10 bits packed)
* snapshot for single camera, with max Camera resolution (active sensor
* array size).
* Api test sequence:
*  - StartCamera
*  - CreateSession (for yuv track)
*  - CreateVideoTrack
*  - StartSession
*  - GetDefaultCaptureParam
*   loop Start {
*   ------------------
*   - CaptureImage - Raw Bayer 10 Bits
*   ------------------
*   } loop End
*  - StopSession
*  - DeleteVideoTrack
*  - DeleteSession
*  - StopCamera
*/
TEST_F(RecorderImageGTest, PreviewAndRaw10BitBayerSnapshot) {
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

  TrackCb yuv_track_cb;
  yuv_track_cb.event_cb =
      [this] (uint32_t track_id, EventType event_type,
              void *event_data, size_t event_data_size) -> void {
      VideoTrackEventCb(track_id, event_type, event_data, event_data_size); };

  uint32_t yuv_track_id = 1;
  VideoTrackCreateParam yuv_track_param{camera_id_, VideoFormat::kYUV,
                                        640, 480, 30};

  yuv_track_cb.data_cb = [&, session_id] (uint32_t track_id,
    std::vector<BufferDescriptor> buffers,
    std::vector<MetaData> meta_buffers) {
        VideoTrackYUVDataCb(session_id, track_id, buffers, meta_buffers); };

  ret = recorder_.CreateVideoTrack(session_id, yuv_track_id,
                                   yuv_track_param, yuv_track_cb);
  ASSERT_TRUE(ret == NO_ERROR);

  std::vector<uint32_t> track_ids = {yuv_track_id};
  sessions_.insert(std::make_pair(session_id, track_ids));

  ret = recorder_.StartSession(session_id);
  ASSERT_TRUE(ret == NO_ERROR);

  // Record for sometime
  sleep(5);

  CameraMetadata meta;
  ret = recorder_.GetDefaultCaptureParam(camera_id_, meta);

  ImageParam image_param{};
  image_param.image_format = ImageFormat::kBayerRDI10BIT;

  GtestCommon::GetMaxSupportedCameraRes(meta, image_param.width,
    image_param.height);

  TEST_INFO("%s: Supported RAW RDI W(%d):H(%d)", __func__, image_param.width,
            image_param.height);
  ASSERT_TRUE(image_param.width > 0 && image_param.height > 0);

  std::vector<CameraMetadata> meta_array;
  meta_array.push_back(meta);
  for(uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
        test_info_->name(), i);

    ImageCaptureCb cb = [this] (uint32_t camera_id, uint32_t image_count,
                                BufferDescriptor buffer,
                                MetaData meta_data) -> void
      { SnapshotCb(camera_id, image_count, buffer, meta_data); };

    ret = recorder_.CaptureImage(camera_id_, image_param, 1, meta_array,
                                 cb);
    ASSERT_TRUE(ret == NO_ERROR);
    // Take snapshot after every 5 sec.
    sleep(5);
  }
  meta_array.clear();

  ret = recorder_.StopSession(session_id, false);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteVideoTrack(session_id, yuv_track_id);
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
* 4kSnapshotWithGPSInfo: This test will test 4k resolution JPEG snapshot
*                   with GPS info.
* Api test sequence:
*  - StartCamera
*   loop Start {
*   ------------------
*   - CreateSession
*   - CreateVideoTrack
*   - StartSession
*   - CaptureImage - JPEG with GPS info
*   - StopSession
*   - DeleteVideoTrack
*   - DeleteSession
*   ------------------
*   } loop End
*  - StopCamera
*/
TEST_F(RecorderImageGTest, 4kSnapshotWithGPSInfo) {
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

  TrackCb yuv_track_cb;
  yuv_track_cb.event_cb =
      [this] (uint32_t track_id, EventType event_type,
              void *event_data, size_t event_data_size) -> void {
      VideoTrackEventCb(track_id, event_type, event_data, event_data_size); };

  uint32_t yuv_track_id = 1;

  VideoTrackCreateParam yuv_track_param{camera_id_, VideoFormat::kYUV,
                                        640, 480, 30};

  if (camera_id_ == 2) {
    yuv_track_param.width = 960;
    yuv_track_param.height = 480;
  }
  yuv_track_param.low_power_mode = 1;
  yuv_track_cb.data_cb = [&, session_id] (uint32_t track_id,
      std::vector<BufferDescriptor> buffers,
      std::vector<MetaData> meta_buffers) {
    VideoTrackYUVDataCb(session_id, track_id, buffers, meta_buffers);
  };

  ret = recorder_.CreateVideoTrack(session_id, yuv_track_id,
                                   yuv_track_param, yuv_track_cb);
  ASSERT_TRUE(ret == NO_ERROR);

  std::vector<uint32_t> track_ids = {yuv_track_id};
  sessions_.insert(std::make_pair(session_id, track_ids));

  ret = recorder_.StartSession(session_id);
  ASSERT_TRUE(ret == NO_ERROR);

  ImageParam image_param{};
  image_param.image_format  = ImageFormat::kJPEG;
  image_param.image_quality = default_jpeg_quality_;

  std::vector<CameraMetadata> meta_array;
  CameraMetadata meta;

  ret = recorder_.GetDefaultCaptureParam(camera_id_, meta);
  ASSERT_TRUE(ret == NO_ERROR);

  bool res_supported = GtestCommon::GetMaxSupportedCameraRes(meta,
      image_param.width, image_param.height, HAL_PIXEL_FORMAT_BLOB);
  ASSERT_TRUE(res_supported != false);

  // Using random coordinates value for testing
  const double gps_coordinates[2] = {
    64.23, 56.23
  };

  TEST_INFO("%s: Setting ANDROID_JPEG_GPS_COORDINATES ", __func__);
  ret = meta.update(ANDROID_JPEG_GPS_COORDINATES, gps_coordinates, 2);
  ASSERT_TRUE(ret == NO_ERROR);

  struct timeval tv;
  gettimeofday(&tv, NULL);
  const int64_t gps_timestamp = static_cast<int64_t>(tv.tv_sec);
  TEST_INFO("%s: Setting ANDROID_JPEG_GPS_TIMESTAMP ", __func__);
  ret = meta.update(ANDROID_JPEG_GPS_TIMESTAMP, &gps_timestamp, 1);
  ASSERT_TRUE(ret == NO_ERROR);

  const uint8_t gps_processing_method[32] = "None";
  TEST_INFO("%s: Setting ANDROID_JPEG_GPS_PROCESSING_METHOD ", __func__);
  ret = meta.update(ANDROID_JPEG_GPS_PROCESSING_METHOD, gps_processing_method, 32);
  ASSERT_TRUE(ret == NO_ERROR);

  meta_array.push_back(meta);

  for(uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
        test_info_->name(), i);

    ImageCaptureCb cb = [this] (uint32_t camera_id, uint32_t image_count,
                                BufferDescriptor buffer,
                                MetaData meta_data) -> void
        { SnapshotCb(camera_id, image_count, buffer, meta_data); };

    ret = recorder_.CaptureImage(camera_id_, image_param, 1, meta_array,
                                 cb);
    ASSERT_TRUE(ret == NO_ERROR);
    // Take snapshot after every 5 sec.
    sleep(5);
  }
  meta_array.clear();

  ret = recorder_.CancelCaptureImage(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.StopSession(session_id, false);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteVideoTrack(session_id, yuv_track_id);
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
* DualCam4KSnapshotWithRaw: This test will request 4K JPEG and RAW snapshot
*                           at the same time for Dual Camera. The RAW resolution
*                           is decided by the Framework, and is the max RAW
*                           resolution provided by the sensor.
* Note: camera_id_ for dual cam to be set using adb property.
* Api test sequence:
*  - StartCamera
*  - CreateSession (for yuv track)
*  - CreateVideoTrack
*  - StartSession
*  - GetDefaultCaptureParam
*  - ConfigImageCapture - RAW+JPEG
*   loop Start {
*   ------------------
*   - CaptureImage - RAW+JPEG
*   ------------------
*   } loop End
*  - StopSession
*  - DeleteVideoTrack
*  - DeleteSession
*  - StopCamera
*/
TEST_F(RecorderImageGTest, DualCam4KSnapshotWithRaw) {
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

  TrackCb yuv_track_cb;
  yuv_track_cb.event_cb =
      [this] (uint32_t track_id, EventType event_type,
              void *event_data, size_t event_data_size) -> void {
      VideoTrackEventCb(track_id, event_type, event_data, event_data_size); };

  uint32_t yuv_track_id = 1;
  VideoTrackCreateParam yuv_track_param{camera_id_, VideoFormat::kYUV,
                                        640, 480, 30};

  yuv_track_cb.data_cb = [&, session_id] (uint32_t track_id,
    std::vector<BufferDescriptor> buffers,
    std::vector<MetaData> meta_buffers) {
        VideoTrackYUVDataCb(session_id, track_id, buffers, meta_buffers); };

  ret = recorder_.CreateVideoTrack(session_id, yuv_track_id,
                                   yuv_track_param, yuv_track_cb);
  ASSERT_TRUE(ret == NO_ERROR);

  std::vector<uint32_t> track_ids = {yuv_track_id};
  sessions_.insert(std::make_pair(session_id, track_ids));

  ret = recorder_.StartSession(session_id);
  ASSERT_TRUE(ret == NO_ERROR);

  // Record for sometime
  sleep(5);

  ImageParam image_param{};
  image_param.width         = 4096;
  image_param.height        = 2048;
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
      { SnapshotCb(camera_id, image_count, buffer, meta_data); };

  ImageConfigParam image_config;

  SnapshotType snapshot_type;
  snapshot_type.type = SnapshotMode::kStillPlusRaw;
  image_config.Update(QMMF_SNAPSHOT_TYPE, snapshot_type, 0);

  ret = recorder_.ConfigImageCapture(camera_id_, image_config);
  ASSERT_TRUE(ret == NO_ERROR);

  meta_array.push_back(meta);
  for (uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr,"test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
        test_info_->name(), i);
    ret = recorder_.CaptureImage(camera_id_, image_param, 1, meta_array, cb);
    ASSERT_TRUE(ret == NO_ERROR);
    // Take snapshot after every 5 sec.
    sleep(5);
  }
  meta_array.clear();

  ret = recorder_.CancelCaptureImage(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.StopSession(session_id, false);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = recorder_.DeleteVideoTrack(session_id, yuv_track_id);
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
#endif
