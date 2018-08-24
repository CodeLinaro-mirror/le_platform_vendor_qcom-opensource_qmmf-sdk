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
* ZSLStartStopCamera: This test case will test Start & StopCamera Api in ZSL
*  mode.
* Api test sequence:
*   loop Start {
*   ------------------
*  - StartCamera
*  - StopCamera
*   ------------------
*   } loop End
*/
TEST_F(RecorderBaseGTest, StartStopCameraZSLMode) {

  fprintf(stderr,"\n---------- Run Test %s.%s ------------\n",
      test_info_->test_case_name(),test_info_->name());

  auto ret = Init();

  camera_start_params_.zsl_mode = true;
  camera_start_params_.zsl_width = 1920;
  camera_start_params_.zsl_height = 1080;
  camera_start_params_.zsl_queue_depth = 4;
  camera_start_params_.frame_rate = 30;

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
