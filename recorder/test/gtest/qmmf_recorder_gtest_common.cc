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

#define LOG_TAG "RecorderGTest"

#include <sys/types.h>
#include <sys/stat.h>
#include <camera/CameraMetadata.h>
#include <random>

#include "recorder/test/gtest/qmmf_recorder_gtest_common.h"

using namespace qcamera;

const std::string GtestCommon::kQmmfFolderPath = "/data/misc/qmmf/";

status_t DumpBitStream::SetUp(const StreamDumpInfo& dumpinfo) {
  TEST_DBG("%s: Enter", __func__);
  EXPECT_TRUE(dumpinfo.width > 0);
  EXPECT_TRUE(dumpinfo.height > 0);

  const char* type_string;
  switch (dumpinfo.format) {
    case VideoFormat::kAVC:
      type_string = "h264";
      break;
    case VideoFormat::kHEVC:
      type_string = "h265";
      break;
    case VideoFormat::kJPEG:
      type_string = "mjpg";
      break;
    default:
      type_string = "bin";
      break;
  }
  std::string extn(type_string);
  std::string bitstream_filepath("/data/misc/qmmf/gtest_track_");
  bitstream_filepath += std::to_string(dumpinfo.track_id) + "_";
  bitstream_filepath += std::to_string(dumpinfo.width) + "x";
  bitstream_filepath += std::to_string(dumpinfo.height) + ".";
  bitstream_filepath += extn;
  int32_t file_fd = open(bitstream_filepath.c_str(),
                          O_CREAT | O_WRONLY | O_TRUNC, 0655);
  if (file_fd <= 0) {
    TEST_ERROR("%s File open failed!", __func__);
    return BAD_VALUE;
  }
  uint8_t key_by_session_track_id = dumpinfo.session_id << 4
    | dumpinfo.track_id;
  file_fds_.insert(std::make_pair(key_by_session_track_id, file_fd));

  TEST_DBG("%s: Exit", __func__);
  return NO_ERROR;
}

status_t DumpBitStream::Dump(const std::vector<BufferDescriptor>& buffers,
   const uint32_t &session_id, const uint32_t &track_id) {

  TEST_DBG("%s: Enter", __func__);
  int32_t file_fd = GetFileFd(session_id, track_id);
  EXPECT_TRUE(file_fd > 0);

  for (auto& iter : buffers) {
    uint32_t exp_size = iter.size;
    TEST_DBG("%s:%s BitStream buffer data(0x%x):size(%d):ts(%lld):flag(0x%x)"
      ":buf_id(%d):capacity(%d)",  __func__, iter.data, iter.size,
       iter.timestamp, iter.flag, iter.buf_id, iter.capacity);

    uint32_t written_length = write(file_fd, iter.data, iter.size);
    TEST_DBG("%s: written_length(%d)", __func__, written_length);
    if (written_length != exp_size) {
      TEST_ERROR("%s: Bad Write error (%d) %s", __func__, errno,
      strerror(errno));
      return BAD_VALUE;
    }

    if(iter.flag & static_cast<uint32_t>(BufferFlags::kFlagEOS)) {
      TEST_INFO("%s EOS Last buffer!", __func__);
      break;
    }
  }

  TEST_DBG("%s: Exit", __func__);
  return NO_ERROR;
}

void DumpBitStream::Close(int32_t file_fd) {
  TEST_DBG("%s: Enter", __func__);
  if (file_fd > 0) {
    auto iter = file_fds_.find(file_fd);
    if(iter != file_fds_.end()) {
      close(file_fd);
      file_fds_.erase(iter);
    } else {
      TEST_WARN("%s: file_fd does not exist!", __func__);
    }
  }
  TEST_DBG("%s: Exit", __func__);
}

void DumpBitStream::CloseAll() {
  TEST_DBG("%s: Enter", __func__);
  for (auto& iter : file_fds_) {
    if (iter.second > 0) {
      close(iter.second);
    }
  }
  file_fds_.clear();
  TEST_DBG("%s: Exit", __func__);
}

#ifdef USE_SURFACEFLINGER
float GetFormatBpp(int32_t format) {
  //formats taken from graphics.h
  switch (format) {
    case HAL_PIXEL_FORMAT_RGBA_8888:
    case HAL_PIXEL_FORMAT_RGBX_8888:
    case HAL_PIXEL_FORMAT_BGRA_8888:
      return 4;
    case HAL_PIXEL_FORMAT_RGB_565:
    case HAL_PIXEL_FORMAT_RGBA_5551:
    case HAL_PIXEL_FORMAT_RGBA_4444:
    case HAL_PIXEL_FORMAT_YCbCr_422_SP:
    case HAL_PIXEL_FORMAT_YCbCr_422_I:
      return 2;
    case HAL_PIXEL_FORMAT_YV12:
    case HAL_PIXEL_FORMAT_YCrCb_420_SP:
      return 1.5;
    default:
      return -1;
  }
}

SFDisplaySink::SFDisplaySink(uint32_t width, uint32_t height) {
  TEST_INFO("%s: Enter 0x%p",__func__, this);

  auto ret = CreatePreviewSurface(width, height);
  if (ret != 0) {
    TEST_ERROR("%s: CreatePreviewSurface failed!",__func__);
  }

  TEST_INFO("%s: Exit",__func__);
}

SFDisplaySink::~SFDisplaySink() {
  TEST_INFO("%s: Enter 0x%p",__func__, this);

  DestroyPreviewSurface();

  TEST_INFO("%s: Exit",__func__);
}

int32_t SFDisplaySink::CreatePreviewSurface(uint32_t width, uint32_t height) {
  TEST_INFO("%s: Enter ",__func__);

  DisplayInfo dinfo;
  auto ret = NO_ERROR;
  sp<IBinder> display(SurfaceComposerClient::getBuiltInDisplay(
      ISurfaceComposer::eDisplayIdMain));
  SurfaceComposerClient::getDisplayInfo(display, &dinfo);

  surface_client_ = new SurfaceComposerClient();

  if(surface_client_.get() == nullptr) {
    TEST_ERROR("%s:Connection to Surface Composer failed!", __func__);
    return -1;
  }
  surface_control_ = surface_client_->createSurface(
      String8("QMMFRecorderService"),
      width, height, HAL_PIXEL_FORMAT_YCrCb_420_SP, 0);

  if (surface_control_.get() == nullptr) {
    TEST_ERROR("%s: Preview surface creation failed!",__func__);
    return -1;
  }

  preview_surface_ = surface_control_->getSurface();
  if (preview_surface_.get() == nullptr) {
    TEST_ERROR("%s: Preview surface creation failed!",__func__);
  }

  surface_client_->openGlobalTransaction();

  surface_control_->setLayer(0x7fffffff);
  surface_control_->setPosition(0, 0);
  surface_control_->setSize(width, height);
  surface_control_->show();

  surface_client_->closeGlobalTransaction();

  TEST_INFO("%s: Exit ",__func__);
  return ret;
}

void SFDisplaySink::DestroyPreviewSurface() {
  TEST_INFO("%s: Enter ",__func__);
  if(preview_surface_.get() != nullptr) {
    preview_surface_.clear();
  }
  if(surface_control_.get () != nullptr) {
    surface_control_->clear();
    surface_control_.clear();
  }
  if(surface_client_.get() != nullptr) {
    surface_client_->dispose();
    surface_client_.clear();
  }
  TEST_INFO("%s: Exit ",__func__);
}

void SFDisplaySink::HandlePreviewBuffer(BufferDescriptor &buffer,
    CameraBufferMetaData &meta_data) {
  TEST_INFO("%s: Enter ",__func__);

  if (buffer.data == nullptr) {
    TEST_ERROR("%s: No buffer!!", __func__);
    return;
  }

  ANativeWindow_Buffer info;
  preview_surface_->lock(&info, nullptr);

  char* img = reinterpret_cast<char *>(info.bits);
  if (img == nullptr) {
    TEST_ERROR("%s: No Surface flinger buffer!!", __func__);
    return;
  }
  uint32_t dst_offset = 0;
  uint32_t src_offset = 0;

  for ( int32_t i = 0; i < info.height; i++ ) {
    memcpy(img + dst_offset,
        reinterpret_cast<unsigned char *>(buffer.data) + src_offset,
        info.width);
    src_offset += info.width;
    dst_offset += info.stride;
  }

  src_offset += info.width * (info.height % 32);

  for ( int32_t i = 0; i < info.height/2; i++ ) {
    memcpy(img + dst_offset,
        reinterpret_cast<unsigned char *>(buffer.data) + src_offset,
        info.width);
    src_offset += info.width;
    dst_offset += info.stride;
  }

  preview_surface_->unlockAndPost();

  TEST_INFO("%s: Exit ",__func__);
}
#endif

void GtestCommon::SetUp() {

  TEST_INFO("%s Enter ", __func__);

  test_info_ = ::testing::UnitTest::GetInstance()->current_test_info();

  recorder_status_cb_.event_cb = [this] (EventType event_type, void *event_data,
                                         size_t event_data_size) -> void
      { RecorderCallbackHandler(event_type, event_data, event_data_size); };

  char prop_val[PROPERTY_VALUE_MAX];
  property_get(PROP_DUMP_BITSTREAM, prop_val, "0");
  if (atoi(prop_val) == 0) {
    dump_bitstream_.Enable(false);
  } else {
    dump_bitstream_.Enable(true);
  }
  property_get(PROP_DUMP_JPEG, prop_val, "0");
  is_dump_jpeg_enabled_ = (atoi(prop_val) == 0) ? false : true;
  property_get(PROP_DUMP_RAW, prop_val, "0");
  is_dump_raw_enabled_ = (atoi(prop_val) == 0) ? false : true;
  property_get(PROP_DUMP_YUV_FRAMES, prop_val, "0");
  is_dump_yuv_enabled_ = (atoi(prop_val) == 0) ? false : true;
  property_get(PROP_DUMP_YUV_FREQ, prop_val, DEFAULT_YUV_DUMP_FREQ);
  dump_yuv_freq_ = atoi(prop_val);
  property_get(PROP_N_ITERATIONS, prop_val, DEFAULT_ITERATIONS);
  iteration_count_ = atoi(prop_val);
  property_get(PROP_CAMERA_ID, prop_val, "0");
  camera_id_ = atoi(prop_val);
  property_get(PROP_RECORD_DURATION, prop_val, DEFAULT_RECORD_DURATION);
  record_duration_ = atoi(prop_val);
  property_get(PROP_DUMP_THUMBNAIL, prop_val, "0");
  is_dump_thumb_enabled_ = (atoi(prop_val) == 0) ? false : true;
  property_get(PROP_BURST_N_IMAGES, prop_val, DEFAULT_BURST_COUNT);
  burst_image_count_ = atoi(prop_val);
  property_get(PROP_JPEG_QUALITY, prop_val, IMAGE_QUALITY);
  default_jpeg_quality_ = atoi(prop_val);
  property_get(PROP_CDS_THRESHOLD, prop_val, "600");
  default_cds_threshold_ = atoi(prop_val);
  property_get(PROP_DEFAULT_EIS_MARGINS, prop_val, "1");
  default_eis_margins_ = atoi(prop_val);
#ifndef DISABLE_DISPLAY
  property_get(PROP_TOGGLE_DISPLAY_USAGE, prop_val, "1");
  use_display_ = (atoi(prop_val) == 0) ? false : true;
#endif
  property_get(PROP_TOGGLE_OVERLAY_USAGE, prop_val, "0");
  is_apply_overlay_ = (atoi(prop_val) == 0) ? false : true;
  property_get(PROP_UBWC_STREAM_ENABLE, prop_val, "1");
  ubwc_stream_enable_ = (atoi(prop_val) == 0) ? false : true;

  camera_start_params_ = {};
  camera_start_params_.zsl_mode         = false;
  camera_start_params_.zsl_queue_depth  = kZslQDepth;
  camera_start_params_.zsl_width        = kZslWidth;
  camera_start_params_.zsl_height       = kZslHeight;
  camera_start_params_.frame_rate       = 30;
  camera_start_params_.flags            = 0x0;

#ifndef DISABLE_DISPLAY
  display_started_ = false;
  enable_gfx_ = false;
#endif

#ifdef QCAMERA3_TAG_LOCAL_COPY
  vendor_tag_desc_ = nullptr;
#endif

  TEST_INFO("%s Exit ", __func__);
}

void GtestCommon::TearDown() {

  TEST_INFO("%s Enter ", __func__);
  TEST_INFO("%s Exit ", __func__);
}

int32_t GtestCommon::Init() {
  auto ret = recorder_.Connect(recorder_status_cb_);
  EXPECT_TRUE(ret == NO_ERROR);
  return ret;
}

int32_t GtestCommon::DeInit() {

  auto ret = recorder_.Disconnect();
  track_frame_count_map_.clear();
  EXPECT_TRUE(ret == NO_ERROR);
  return ret;
}

void GtestCommon::InitSupportedVHDRModes() {
  camera_metadata_entry_t entry;
  if (static_info_.exists(QCAMERA3_AVAILABLE_VIDEO_HDR_MODES)) {
    entry = static_info_.find(QCAMERA3_AVAILABLE_VIDEO_HDR_MODES);
    for (uint32_t i = 0 ; i < entry.count; i++) {
      supported_hdr_modes_.push_back(entry.data.i32[i]);
    }
  }
}

bool GtestCommon::IsVHDRSupported() {
  bool is_supported = false;
  for (const auto& mode : supported_hdr_modes_) {
    if (QCAMERA3_VIDEO_HDR_MODE_ON == mode) {
      is_supported = true;
      break;
    }
  }
  return is_supported;
}

void GtestCommon::InitSupportedNRModes() {
  camera_metadata_entry_t entry;
  if (static_info_.exists(
      ANDROID_NOISE_REDUCTION_AVAILABLE_NOISE_REDUCTION_MODES)) {
    entry = static_info_.find(
        ANDROID_NOISE_REDUCTION_AVAILABLE_NOISE_REDUCTION_MODES);
    for (uint32_t i = 0 ; i < entry.count; i++) {
      supported_nr_modes_.push_back(entry.data.u8[i]);
    }
  }
}

bool GtestCommon::IsNRSupported() {
  bool is_supported = false;
  for (const auto& mode : supported_nr_modes_) {
    if (ANDROID_NOISE_REDUCTION_MODE_HIGH_QUALITY == mode) {
      is_supported = true;
      break;
    }
  }
  return is_supported;
}

void GtestCommon::RecorderCallbackHandler(EventType event_type,
                                          void *event_data,
                                          size_t event_data_size) {
  TEST_INFO("%s Enter event: %d ", __func__, event_type);
  if (event_type == EventType::kCameraError &&
      event_data_size && event_data != nullptr) {
    RecorderErrorData *error_data = static_cast<RecorderErrorData *>(event_data);
    TEST_INFO("%s error_code %d camera_id %d",
        __func__, error_data->error_code, error_data->camera_id);
    test_wait_.Done();
    std::lock_guard<std::mutex> lock(error_lock_);
    camera_error_ = true;
  }
  TEST_INFO("%s Exit ", __func__);
}

void GtestCommon::SessionCallbackHandler(EventType event_type,
                                        void *event_data,
                                        size_t event_data_size) {
  TEST_INFO("%s: Enter", __func__);
  TEST_INFO("%s: Exit", __func__);
}

void GtestCommon::CameraResultCallbackHandler(uint32_t camera_id,
                                   const CameraMetadata &result) {
  TEST_DBG(stderr,"%s: camera_id: %d\n", __func__, camera_id);
  camera_metadata_ro_entry entry;
  entry = result.find(ANDROID_CONTROL_AWB_MODE);
  if (0 < entry.count) {
    TEST_DBG(stderr,"%s: AWB mode: %d\n", __func__, *entry.data.u8);
  } else {
    TEST_DBG(stderr,"%s: No AWB mode tag\n", __func__);
  }
  if (!result.exists(ANDROID_REQUEST_FRAME_COUNT)) {
    return;
  }
}

void GtestCommon::VideoTrackYUVDataCb(uint32_t session_id, uint32_t track_id,
                                      std::vector<BufferDescriptor> buffers,
                                      std::vector<MetaData> meta_buffers) {
  TEST_DBG("%s: Enter track_id: %d", __func__, track_id);
  if (is_dump_yuv_enabled_) {
    track_frame_count_map_[track_id]++;
    if (!(track_frame_count_map_[track_id] % dump_yuv_freq_)) {
      std::string file_path("/data/misc/qmmf/gtest_track_");
      size_t written_len;
      file_path += std::to_string(track_id) + "_";
      file_path += std::to_string(buffers[0].timestamp);
      file_path += ".yuv";
      FILE *file = fopen(file_path.c_str(), "w+");
      if (!file) {
        ALOGE("%s: Unable to open file(%s)", __func__,
            file_path.c_str());
        goto FAIL;
      }
      written_len = fwrite(buffers[0].data, sizeof(uint8_t),
                           buffers[0].size, file);
      TEST_DBG("%s: written_len =%d", __func__, written_len);
      if (buffers[0].size != written_len) {
        TEST_ERROR("%s: Bad Write error (%d):(%s)\n", __func__, errno,
            strerror(errno));
        goto FAIL;
      }
      TEST_INFO("%s: Buffer(0x%p) Size(%u) Stored@(%s)\n", __func__,
        buffers[0].data, written_len, file_path.c_str());

  FAIL:
      if (file != NULL) {
        fclose(file);
      }
    }
  }

#ifndef DISABLE_DISPLAY
  if (display_ && use_display_) {
    if (enable_gfx_) {
      DequeueGfxSurfaceBuffer();
      QueueGfxSurfaceBuffer();
    }
    PushFrameToDisplay(buffers[0], meta_buffers[0].cam_buffer_meta_data);
  }
#endif

  auto ret = recorder_.ReturnTrackBuffer(session_id, track_id, buffers);
  ASSERT_TRUE(ret == NO_ERROR);

  TEST_DBG("%s: Exit", __func__);
}

void GtestCommon::VideoTrackEncDataCb(uint32_t session_id,
                                    uint32_t track_id,
                                    std::vector<BufferDescriptor> &buffers,
                                    std::vector<MetaData> &meta_buffers) {

  TEST_DBG("%s: Enter", __func__);
  if (dump_bitstream_.IsUsed()) {
    dump_bitstream_.Dump(buffers, session_id, track_id);
  }
  // Return buffers back to service.
  auto ret = recorder_.ReturnTrackBuffer(session_id, track_id, buffers);
  ASSERT_TRUE(ret == NO_ERROR);

  TEST_DBG("%s: Exit", __func__);
}

void GtestCommon::VideoTrackEventCb(uint32_t track_id,
                                    EventType event_type,
                                    void *event_data,
                                    size_t event_data_size) {
    TEST_DBG("%s: Enter", __func__);
    TEST_DBG("%s: Exit", __func__);
}

void GtestCommon::SnapshotCb(uint32_t camera_id,
                               uint32_t image_sequence_count,
                               BufferDescriptor buffer, MetaData meta_data) {

  TEST_INFO("%s Enter", __func__);

  size_t written_len;

  if (meta_data.meta_flag  &
      static_cast<uint32_t>(MetaParamType::kCamBufMetaData)) {
    CameraBufferMetaData& cam_buf_meta = meta_data.cam_buffer_meta_data;
    TEST_DBG("%s: format(0x%x)", __func__, cam_buf_meta.format);
    TEST_DBG("%s: num_planes=%d", __func__, cam_buf_meta.num_planes);
    for (uint8_t i = 0; i < cam_buf_meta.num_planes; ++i) {
      TEST_DBG("plane[%d]:stride(%d)", __func__, i,
          cam_buf_meta.plane_info[i].stride);
      TEST_DBG("plane[%d]:scanline(%d)", __func__, i,
          cam_buf_meta.plane_info[i].scanline);
      TEST_DBG("plane[%d]:width(%d)", __func__, i,
          cam_buf_meta.plane_info[i].width);
      TEST_DBG("plane[%d]:height(%d)", __func__, i,
          cam_buf_meta.plane_info[i].height);
    }

    bool dump_file;
    bool dump_thumbnail = false;
    if (cam_buf_meta.format == BufferFormat::kBLOB) {
      dump_file = (is_dump_jpeg_enabled_) ? true : false;
      dump_thumbnail = (dump_file && is_dump_thumb_enabled_) ? true : false;
    } else {
      dump_file = (is_dump_raw_enabled_) ? true : false;
      fprintf(stderr, "\nRaw snapshot dumping enabled; "
              "keep track of free storage space.\n");
    }

    if (dump_file) {
      const char* ext_str;
      switch (cam_buf_meta.format) {
        case BufferFormat::kNV12:
        ext_str = "nv12";
        break;
        case BufferFormat::kNV21:
        ext_str = "nv21";
        break;
        case BufferFormat::kNV16:
        ext_str = "nv16";
        break;
        case BufferFormat::kBLOB:
        ext_str = "jpg";
        break;
        case BufferFormat::kRAW8:
        ext_str = "raw8";
        break;
        case BufferFormat::kRAW10:
        ext_str = "raw10";
        break;
        case BufferFormat::kRAW12:
        ext_str = "raw12";
        break;
        case BufferFormat::kRAW16:
        ext_str = "raw16";
        break;
        default:
        ext_str = "bin";
        break;
      }

      struct timeval tv;
      gettimeofday(&tv, NULL);
      uint64_t tv_ms = (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
      std::string file_path("/data/misc/qmmf/snapshot_");
      file_path += std::to_string(image_sequence_count) + "_";
      file_path += std::to_string(tv_ms) + ".";
      file_path += ext_str;
      FILE *file = fopen(file_path.c_str(), "w+");
      if (!file) {
        ALOGE("%s: Unable to open file(%s)", __func__,
            file_path.c_str());
        goto FAIL;
      }

      written_len = fwrite(buffer.data, sizeof(uint8_t), buffer.size, file);
      TEST_INFO("%s: written_len =%d", __func__, written_len);
      if (buffer.size != written_len) {
        ALOGE("%s: Bad Write error (%d):(%s)\n", __func__, errno,
              strerror(errno));
        goto FAIL;
      }
      TEST_INFO("%s: Buffer(0x%p) Size(%u) Stored@(%s)\n", __func__,
                buffer.data, written_len, file_path.c_str());

      if (dump_thumbnail) {
        auto ret = DumpThumbnail(buffer, cam_buf_meta,
                                 image_sequence_count, tv_ms);
        if (ret != NO_ERROR) {
          TEST_INFO("%s: Dump thumbnail faile failed!\n", __func__);
        }
      }

    FAIL:
      if (file != NULL) {
        fclose(file);
      }
    }
  }
  // Return buffer back to recorder service.
  recorder_.ReturnImageCaptureBuffer(camera_id, buffer);
  TEST_INFO("%s Exit", __func__);
}

status_t GtestCommon::QueueVideoFrame(VideoFormat format_type,
                                        const uint8_t *buffer, size_t size,
                                        int64_t timestamp, AVQueue *que) {
  int buffer_size = 0;
  uint8_t *tmp_buffer = NULL;
  AVPacket *packet = NULL;

  if ((size <= 5) || (NULL == que)) {
    return BAD_VALUE;
  }

  switch (format_type) {
    case VideoFormat::kAVC:
      if (buffer[0] == 0x00 && buffer[1] == 0x00 && buffer[2] == 0x00 &&
          buffer[3] == 0x01 && buffer[4] == 0x67) { /* SPS,PPS*/
        if (que->pps != NULL) {
          free(que->pps);
          que->pps = NULL;
        }
        que->pps = (char *)malloc(sizeof(char) * (size));
        memcpy(que->pps, buffer, size);
        que->pps_size = size;
        que->is_pps = true;
        return NO_ERROR;
      }
      if (buffer[0] == 0x00 && buffer[1] == 0x00 && buffer[2] == 0x00 &&
          buffer[3] == 0x01 && buffer[4] == 0x65) {
        if (que->is_pps != true) {
          buffer_size = que->pps_size;
        }
        que->is_pps = false;
      }
      break;
    case VideoFormat::kHEVC:
      if (buffer[0] == 0x00 && buffer[1] == 0x00 && buffer[2] == 0x00 &&
          buffer[3] == 0x01 && buffer[4] == 0x40) {/* VPS,SPS,PPS*/
        if (que->pps != NULL) {
          free(que->pps);
          que->pps = NULL;
        }
        que->pps = (char *)malloc(sizeof(char) * (size));
        memcpy(que->pps, buffer, size);
        que->pps_size = size;
        que->is_pps = true;
        return NO_ERROR;
      }

      if (buffer[0] == 0x00 && buffer[1] == 0x00 && buffer[2] == 0x00 &&
          buffer[3] == 0x01 && buffer[4] == 0x26) {
        if (que->is_pps != true) {
          buffer_size = que->pps_size;
        }
        que->is_pps = false;
      }
      break;
    default:
      TEST_ERROR("%s: Unsupported format type: %d", __func__, format_type);
      return BAD_VALUE;
  }

  /* Set pointer to start address */
  packet = (AVPacket *)malloc(sizeof(AVPacket));
  if ((NULL == packet)) {
    return NO_MEMORY;
  }

  /* Allocate a new frame object. */
  packet->data = tmp_buffer = (uint8_t *)malloc((size + buffer_size));
  if ((NULL == packet->data)) {
    free(packet);
    return NO_MEMORY;
  }

  if ((0 != buffer_size)) {
    memcpy(tmp_buffer, que->pps, que->pps_size);
    tmp_buffer += que->pps_size;
  }
  memcpy(tmp_buffer, buffer, size);
  packet->size = size + buffer_size;
  packet->timestamp = timestamp;
  AVQueuePushHead(que, packet);

  return NO_ERROR;
}

void GtestCommon::VideoCachedDataCb(uint32_t session_id, uint32_t track_id,
                                      std::vector<BufferDescriptor> buffers,
                                      std::vector<MetaData> meta_buffers,
                                      VideoFormat format_type,
                                      AVQueue *que) {

  for ( auto &iter : buffers) {
    if(iter.flag & static_cast<uint32_t>(BufferFlags::kFlagEOS)) {
      break;
    }

    auto ret = QueueVideoFrame(format_type, (uint8_t *) iter.data, iter.size,
                               iter.timestamp, que);
    if (NO_ERROR != ret) {
      TEST_ERROR("%s: Failed to cache video frame: %d\n", __func__, ret);
    }
  }
  auto ret = recorder_.ReturnTrackBuffer(session_id, track_id, buffers);
  ASSERT_TRUE(ret == NO_ERROR);
}

status_t GtestCommon::DumpQueue(AVQueue *queue, int32_t file_fd) {
  if ((NULL == queue) || (0 >= file_fd)) {
    return BAD_VALUE;
  }

  ssize_t q_size = AVQueueSize(queue);
  if (0 >= q_size) {
    TEST_ERROR("%s: Queue invalid or empty!", __func__);
    return BAD_VALUE;
  }

  AVPacket *pkt;
  for (ssize_t i = 0; i < q_size; i++) {
    pkt = (AVPacket *)AVQueuePopTail(queue);
    if (NULL != pkt) {
      if ((NULL != pkt->data)) {
        uint32_t written_length = write(file_fd, pkt->data, pkt->size);
        if (written_length != pkt->size) {
          TEST_ERROR("%s: Bad Write error (%d) %s", __func__, errno,
                     strerror(errno));
          free(pkt->data);
          free(pkt);

          return -errno;
        }
        free(pkt->data);
      } else {
        TEST_ERROR("%s: AV packet empty!", __func__);
      }
      free(pkt);
    } else {
      TEST_ERROR("%s: Invalid AV packet popped!", __func__);
    }
  }

  return NO_ERROR;
}

void GtestCommon::ClearSessions() {

  TEST_INFO("%s Enter ", __func__);
  std::map <uint32_t , std::vector<uint32_t> >::iterator it = sessions_.begin();
  for (; it != sessions_.end(); ++it) {
    it->second.clear();
  }
  sessions_.clear();
  TEST_INFO("%s Exit ", __func__);
}

void GtestCommon::ResultCallbackHandlerMatchCameraMeta(uint32_t camera_id,
                                                const CameraMetadata &result) {
  uint32_t meta_frame_number =
      result.find(ANDROID_REQUEST_FRAME_COUNT).data.i32[0];
  TEST_INFO("%s meta frame number =%d", __func__, meta_frame_number);
  std::lock_guard<std::mutex> lock(buffer_metadata_lock_);
  bool append = false;
  auto iter = buffer_metadata_map_.find(meta_frame_number);
  if (iter == buffer_metadata_map_.end()) {
    append = true;
  }
  if (append) {
    // New entry, camera meta arrived first.
    auto buffer_meta_tuple = std::make_tuple(BufferDescriptor(),
     CameraMetadata(result), 0, 0);
    buffer_metadata_map_.insert( { meta_frame_number, buffer_meta_tuple} );
  } else {
    // Buffer already arrived for this meta.
    auto& tuple  = buffer_metadata_map_[meta_frame_number];
    std::get<1>(tuple).append(result);
    // Buffer is exactly matched with it's camera meta data buffer. This test
    // code is demonstarting how buffer descriptor can be matched exactly with
    // its corresponding camera meta data. once buffer & meta data matched
    // application can take appropriate actions. this test app is doing
    // nothing it is just returning buffer back to service on match.
    std::vector<BufferDescriptor> buffers;
    buffers.push_back(std::get<0>(tuple));
    auto ret = recorder_.ReturnTrackBuffer(std::get<2>(tuple),
        std::get<3>(tuple), buffers);
    ASSERT_TRUE(ret == NO_ERROR);
    std::get<1>(tuple).clear();
    buffer_metadata_map_.erase(meta_frame_number);
    TEST_INFO("%s size of the map=%d", __func__,
        buffer_metadata_map_.size());
  }
}

void GtestCommon::VideoTrackDataCbMatchCameraMeta(uint32_t session_id,
    uint32_t track_id, std::vector<BufferDescriptor> buffers,
    std::vector<MetaData> meta_buffers) {

  uint32_t meta_frame_number = 0;
  for (uint32_t i = 0; i < meta_buffers.size(); ++i) {
    MetaData meta_data = meta_buffers[i];
    if (meta_data.meta_flag &
        static_cast<uint32_t>(MetaParamType::kCamMetaFrameNumber)) {
      meta_frame_number = meta_buffers[i].cam_meta_frame_number;
    }
  }
  TEST_INFO("%s meta frame number =%d", __func__, meta_frame_number);

  bool append = false;
  std::lock_guard<std::mutex> lock(buffer_metadata_lock_);
  auto iter = buffer_metadata_map_.find(meta_frame_number);
  if (iter == buffer_metadata_map_.end()) {
    append = true;
  }
  if (append) {
    // New entry, buffer arrived first.
    auto buffer_meta_tuple = std::make_tuple(BufferDescriptor(buffers[0]),
        CameraMetadata(), session_id, track_id);
    buffer_metadata_map_.insert( {meta_frame_number, buffer_meta_tuple} );
  } else {
    // MetaData already arrived for this buffer.
    auto& tuple  = buffer_metadata_map_[meta_frame_number];
    std::get<0>(tuple) = buffers[0];
    // Double check the meta frame number.
    uint32_t frame_number =
        std::get<1>(tuple).find(ANDROID_REQUEST_FRAME_COUNT).data.i32[0];
    ASSERT_TRUE(frame_number == meta_frame_number);
    // Buffer is exactly matched with it's camera meta data buffer. This test
    // code is demonstarting how buffer descriptor can be matched exactly with
    // its corresponding camera meta data. once buffer & meta data matched
    // application can take appropriate actions. this test app is doing
    // nothing it is just returning buffer back to service on match.
    auto ret = recorder_.ReturnTrackBuffer(session_id, track_id, buffers);
    ASSERT_TRUE(ret == NO_ERROR);
    std::get<1>(tuple).clear();
    buffer_metadata_map_.erase(meta_frame_number);

    TEST_INFO("%s size of the map=%d", __func__,
        buffer_metadata_map_.size());
  }
  TEST_DBG("%s: Exit", __func__);
}

void GtestCommon::ParseFaceInfo(const android::CameraMetadata &res,
                                  struct FaceInfo &info) {
  camera_metadata_ro_entry rect_entry, crop_entry;
  Rect<uint32_t> rect;
  uint32_t active_w = 0, active_h = 0;

  if (res.exists(ANDROID_STATISTICS_FACE_RECTANGLES)) {
    // Check Face Rectangles exit or not.
    rect_entry = res.find(ANDROID_STATISTICS_FACE_RECTANGLES);
    if (rect_entry.count > 0) {
      crop_entry = res.find(ANDROID_SCALER_CROP_REGION);
      if (crop_entry.count < 4) {
        TEST_ERROR("Unable to read crop region (count = %d)", crop_entry.count);
        ASSERT_TRUE(0);
      } else {
        active_w = crop_entry.data.i32[2];
        active_h = crop_entry.data.i32[3];
      }

      if ((active_w == 0) || (active_h == 0)) {
        TEST_ERROR("Invaild crop region(%d, %d)", active_w, active_h);
        ASSERT_TRUE(0);
      }

      TEST_INFO("%d face detected", rect_entry.count / 4);
      for (uint32_t i = 0 ; i < rect_entry.count; i += 4) {
        rect.left = rect_entry.data.i32[i + 0] *
                       info.fd_stream_width / active_w;
        rect.top = rect_entry.data.i32[i + 1] *
                       info.fd_stream_height / active_h;
        rect.width = rect_entry.data.i32[i + 2] *
                       info.fd_stream_width / active_w - rect.left;
        rect.height = rect_entry.data.i32[i + 3] *
                       info.fd_stream_height / active_h - rect.top;
        info.face_rect.push_back(rect);
      }
    }else {
      TEST_INFO("No face detected");
    }
  }
}

void GtestCommon::ApplyFaceOveralyOnStream(struct FaceInfo &info) {
  face_overlay_lock_.lock();
  if (face_bbox_active_) {
    uint32_t i;
    int ret;
    uint32_t last_num = face_bbox_id_.size();
    uint32_t cur_num = info.face_rect.size();
    OverlayParam object_params;

    for(i = 0; i < std::min(last_num, cur_num); i++) {
      ret = GtestCommon::recorder_.GetOverlayObjectParams(face_track_id_,
          face_bbox_id_[i], object_params);
      ASSERT_TRUE(ret == 0);
      object_params.dst_rect.start_x = info.face_rect[i].left;
      object_params.dst_rect.width = info.face_rect[i].width;
      if (object_params.dst_rect.width <= 0) {
        TEST_INFO("invalid width(%d)", object_params.dst_rect.width);
        object_params.dst_rect.width = 1;
      }
      object_params.dst_rect.start_y = info.face_rect[i].top;
      object_params.dst_rect.height = info.face_rect[i].height;
      if (object_params.dst_rect.height <= 0) {
        TEST_INFO("invalid width(%d)", object_params.dst_rect.height);
        object_params.dst_rect.height = 1;
      }
      ret = GtestCommon::recorder_.UpdateOverlayObjectParams(face_track_id_,
          face_bbox_id_[i], object_params);
      ASSERT_TRUE(ret == 0);
      ret = GtestCommon::recorder_.SetOverlay(face_track_id_, face_bbox_id_[i]);
      ASSERT_TRUE(ret == 0);
    }

    if (last_num > cur_num) {
      for(i = cur_num; i < last_num; i++) {
        ret = GtestCommon::recorder_.RemoveOverlay(face_track_id_,
                                                     face_bbox_id_[i]);
        ASSERT_TRUE(ret == 0);
      }
    } else if (last_num < cur_num) {
      for (i = last_num; i < cur_num; i++) {
        // Create BoundingBox type overlay.
        std::string bb_text("Face");
        uint32_t bbox_id;
        object_params = {};
        object_params.type  = OverlayType::kBoundingBox;
        object_params.color = kColorLightGreen;
        object_params.dst_rect.start_x = info.face_rect[i].left;
        object_params.dst_rect.start_y = info.face_rect[i].top;
        object_params.dst_rect.width   = info.face_rect[i].width;
        object_params.dst_rect.height  = info.face_rect[i].height;
        bb_text.copy(object_params.bounding_box.box_name, bb_text.length());
        ret = recorder_.CreateOverlayObject(face_track_id_,
                 object_params, &bbox_id);
        ASSERT_TRUE(ret == 0);
        face_bbox_id_.push_back(bbox_id);
        ret = GtestCommon::recorder_.SetOverlay(face_track_id_, bbox_id);
        ASSERT_TRUE(ret == 0);
      }
    }
  }
  face_overlay_lock_.unlock();
  info.face_rect.clear();
}

/** ValidateResFromStreamConfigs
*
* Validates whether input resolution is available in
* stream configurations.
*
* return: true if available
**/
bool GtestCommon::ValidateResFromStreamConfigs(const CameraMetadata& meta,
                                                const uint32_t width,
                                                const uint32_t height) {
  bool is_supported = false;
  if (meta.exists(ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS)) {
    auto entry = meta.find(ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS);
    for (uint32_t i = 0 ; i < entry.count; i += 4) {
      if (HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED == entry.data.i32[i]) {
        if (ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT ==
            entry.data.i32[i+3]) {
          if (width == static_cast<uint32_t>(entry.data.i32[i+1])
              && height == static_cast<uint32_t>(entry.data.i32[i+2])) {
            is_supported = true;
            break;
          }
        }
      }
    }
  } else {
    QMMF_ERROR("%s: Metadata ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS"
              " not available", __func__);
    return false;
  }
  return is_supported;
}

/** GetMinResFromStreamConfigs
*
* Searches for minimum supported resolution in stream configurations.
*
* return: true if available
**/
bool GtestCommon::GetMinResFromStreamConfigs(const CameraMetadata& meta,
                                              uint32_t &width,
                                              uint32_t &height) {
  bool found = false;
  width = 0xFFFF;
  height = 0xFFFF;

  if (meta.exists(ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS)) {
    auto entry = meta.find(ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS);
    for (uint32_t i = 0; i < entry.count; i += 4) {
      if (HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED == entry.data.i32[i] &&
          ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT ==
            entry.data.i32[i+3]) {
        if (width > static_cast<uint32_t>(entry.data.i32[i + 1]) &&
            height > static_cast<uint32_t>(entry.data.i32[i + 2])) {
          width = static_cast<uint32_t>(entry.data.i32[i + 1]);
          height = static_cast<uint32_t>(entry.data.i32[i + 2]);
          found = true;
        }
      }
    }
  } else {
    QMMF_ERROR("%s: Metadata ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS"
                " not available", __func__);
    return false;
  }

  return found;
}

/** ValidateResFromProcessedSizes
*
* Validates whether input resolution is available in
* processed sizes.
*
* return: true if available
**/
bool GtestCommon::ValidateResFromProcessedSizes(const CameraMetadata& meta,
                                          const uint32_t width,
                                          const uint32_t height) {
  bool is_supported = false;
#ifdef CAM_ARCH_V2
  is_supported = ValidateResFromStreamConfigs(meta, width, height);
#else
  if (meta.exists(ANDROID_SCALER_AVAILABLE_PROCESSED_SIZES)) {
    auto entry = meta.find(ANDROID_SCALER_AVAILABLE_PROCESSED_SIZES);
    for (uint32_t i = 0 ; i < entry.count; i += 2) {
      if(width == static_cast<uint32_t>(entry.data.i32[i+0]) &&
        height == static_cast<uint32_t>(entry.data.i32[i+1])) {
        is_supported = true;
        break;
      }
    }
  } else {
    QMMF_ERROR("%s: Metadata ANDROID_SCALER_AVAILABLE_PROCESSED_SIZES"
                " not available", __func__);
    return false;
  }
#endif
  return is_supported;
}

/** ValidateResFromJpegSizes
*
* Validates whether input resolution is available in jpeg sizes.
* Since ANDROID_SCALER_AVAILABLE_JPEG_SIZES tag is not available
* in static meta, jpeg size needs to be validated from available
* stream configuration, by filtering the resolutions with
* HAL_PIXEL_FORMAT_BLOB.
*
* return: true if available
**/
bool GtestCommon::ValidateResFromJpegSizes(const CameraMetadata& meta,
                                                  const uint32_t width,
                                                  const uint32_t height) {
  bool is_supported = false;
  if (meta.exists(ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS)) {
    auto entry = meta.find(ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS);
    for (uint32_t i = 0 ; i < entry.count; i += 4) {
      if (HAL_PIXEL_FORMAT_BLOB == entry.data.i32[i]) {
        if (ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT ==
            entry.data.i32[i+3]) {
          if (width == static_cast<uint32_t>(entry.data.i32[i+1])
              && height == static_cast<uint32_t>(entry.data.i32[i+2])) {
            is_supported = true;
            break;
          }
        }
      }
    }
  } else {
    QMMF_ERROR("%s: Metadata ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS"
                " not available", __func__);
    return false;
  }
  return is_supported;
}

/** ValidateResFromRawSizes
*
* Validates whether input resolution is available in
* raw sizes.
*
* return: true if available
**/
bool GtestCommon::ValidateResFromRawSizes(const CameraMetadata& meta,
                                                    const uint32_t width,
                                                    const uint32_t height) {
  bool is_supported = false;
#ifdef CAM_ARCH_V2
  if (meta.exists(ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS)) {
    auto entry = meta.find(ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS);
    for (uint32_t i = 0 ; i < entry.count; i += 4) {
      if (HAL_PIXEL_FORMAT_RAW10 == entry.data.i32[i]) {
        if (ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT ==
            entry.data.i32[i+3]) {
          if (width == static_cast<uint32_t>(entry.data.i32[i+1])
              && height == static_cast<uint32_t>(entry.data.i32[i+2])) {
            is_supported = true;
            break;
          }
        }
      }
    }
  } else {
    QMMF_ERROR("%s: Metadata ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS"
                " not available", __func__);
    return false;
  }
#else
  if (meta.exists(ANDROID_SCALER_AVAILABLE_RAW_SIZES)) {
    auto entry = meta.find(ANDROID_SCALER_AVAILABLE_RAW_SIZES);
    for (uint32_t i = 0 ; i < entry.count; i += 2) {
      if(width == static_cast<uint32_t>(entry.data.i32[i+0]) &&
        height == static_cast<uint32_t>(entry.data.i32[i+1])) {
        is_supported = true;
        break;
      }
    }
  } else {
    QMMF_ERROR("%s: Metadata ANDROID_SCALER_AVAILABLE_RAW_SIZES"
                " not available", __func__);
    return false;
  }
#endif
  return is_supported;
}

/** GetMaxSupportedCameraRes
*
* Searches for maximum supported camera resolution.
*
* return: true if available
**/
bool GtestCommon::GetMaxSupportedCameraRes(const CameraMetadata& meta,
                                      uint32_t &width, uint32_t &height,
                                      const int32_t format) {
  bool found = false;
  width = 0;
  height = 0;
  camera_metadata_ro_entry entry;
#ifdef CAM_ARCH_V2
  if (meta.exists(ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS)) {
    entry = meta.find(ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS);
    for (uint32_t i = 0; i < entry.count; i += 4) {
      if (HAL_PIXEL_FORMAT_RAW10 == entry.data.i32[i] &&
          ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT ==
            entry.data.i32[i+3]) {
        if (width < static_cast<uint32_t>(entry.data.i32[i + 1]) &&
            height < static_cast<uint32_t>(entry.data.i32[i + 2])) {
          width = static_cast<uint32_t>(entry.data.i32[i + 1]);
          height = static_cast<uint32_t>(entry.data.i32[i + 2]);
          found = true;
        }
      }
    }
    QMMF_INFO("%s: width=%d, height=%d", __func__, width, height);
  } else {
    QMMF_ERROR("%s: Metadata ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS"
                " not available", __func__);
    return false;
  }
#else
  if (HAL_PIXEL_FORMAT_RAW8  == format || HAL_PIXEL_FORMAT_RAW10 == format ||
      HAL_PIXEL_FORMAT_RAW12 == format || HAL_PIXEL_FORMAT_RAW16 == format) {
    if (!meta.exists(ANDROID_SCALER_AVAILABLE_RAW_SIZES)) {
      QMMF_ERROR("%s: Metadata ANDROID_SCALER_AVAILABLE_RAW_SIZES"
                  " not available", __func__);
      return false;
    }
    entry = meta.find(ANDROID_SCALER_AVAILABLE_RAW_SIZES);
  } else {
    if (!meta.exists(ANDROID_SCALER_AVAILABLE_PROCESSED_SIZES)) {
      QMMF_ERROR("%s: Metadata ANDROID_SCALER_AVAILABLE_PROCESSED_SIZES"
                  " not available", __func__);
      return false;
    }
    entry = meta.find(ANDROID_SCALER_AVAILABLE_PROCESSED_SIZES);
  }

  for (uint32_t i = 0 ; i < entry.count; i += 2) {
    if (width < static_cast<uint32_t>(entry.data.i32[i + 0]) &&
        height < static_cast<uint32_t>(entry.data.i32[i + 1])) {
      width = static_cast<uint32_t>(entry.data.i32[i + 0]);
      height = static_cast<uint32_t>(entry.data.i32[i + 1]);
      found = true;
    }
  }
#endif
  return found;
}

/** GetMinSupportedCameraRes
*
* Searches for minumum supported camera resolution.
*
* return: true if available
**/
bool GtestCommon::GetMinSupportedCameraRes(const CameraMetadata& meta,
                                                  uint32_t &width,
                                                  uint32_t &height) {
  bool found = false;
  width = 0xFFFF;
  height = 0xFFFF;
#ifdef CAM_ARCH_V2
  found = GetMinResFromStreamConfigs(meta, width, height);
#else
  camera_metadata_ro_entry entry;
  if (!meta.exists(ANDROID_SCALER_AVAILABLE_PROCESSED_SIZES)) {
    QMMF_ERROR("%s: Metadata ANDROID_SCALER_AVAILABLE_PROCESSED_SIZES"
                " not available", __func__);
    return false;
  }

  entry = meta.find(ANDROID_SCALER_AVAILABLE_PROCESSED_SIZES);
  for (uint32_t i = 0 ; i < entry.count; i += 2) {
    if (width > static_cast<uint32_t>(entry.data.i32[i + 0]) &&
        height > static_cast<uint32_t>(entry.data.i32[i + 1])) {
      width = static_cast<uint32_t>(entry.data.i32[i + 0]);
      height = static_cast<uint32_t>(entry.data.i32[i + 1]);
      found = true;
    }
  }
#endif
  return found;
}

#ifndef DISABLE_DISPLAY
void GtestCommon::DisplayCallbackHandler(DisplayEventType event_type,
                                           void *event_data,
                                           size_t event_data_size) {
  TEST_DBG("%s Enter ", __func__);
  TEST_DBG("%s Exit ", __func__);
}

void GtestCommon::DisplayVSyncHandler(int64_t time_stamp) {
  TEST_DBG("%s: Enter", __func__);
  TEST_DBG("%s: Exit", __func__);
}

status_t GtestCommon::StartDisplay(DisplayType display_type,
                                     uint32_t src_width, uint32_t src_height,
                                     uint32_t dst_width, uint32_t dst_height) {
  TEST_INFO("%s: Enter", __func__);
  int32_t res = 0;
  DisplayCb display_status_cb;
  display_ = new Display();
  EXPECT_TRUE(display_ != nullptr);

  res = display_->Connect();
  EXPECT_TRUE(res == 0);

  display_status_cb.EventCb = [&](DisplayEventType event_type, void *event_data,
                                  size_t event_data_size) {
    DisplayCallbackHandler(event_type, event_data, event_data_size);
  };

  display_status_cb.VSyncCb = [&](int64_t time_stamp) {
    DisplayVSyncHandler(time_stamp);
  };

  res = display_->CreateDisplay(display_type, display_status_cb);
  EXPECT_TRUE(res == 0);

  memset(&surface_config_, 0x0, sizeof surface_config_);

  surface_config_.width = src_width;
  surface_config_.height = src_height;
  surface_config_.format = SurfaceFormat::kFormatYCbCr420SemiPlanarVenus;
  surface_config_.buffer_count = 1;
  surface_config_.cache = 0;
  surface_config_.use_buffer = 1;
  surface_config_.z_order = 1;
  res = display_->CreateSurface(surface_config_, &surface_id_);
  EXPECT_TRUE(res == 0);

  display_started_ = 1;

  surface_param_.src_rect = {0.0, 0.0, (float)src_width, (float)src_height};
  surface_param_.dst_rect = {0.0, 0.0, (float)dst_width, (float)dst_height};
  surface_param_.surface_blending = SurfaceBlending::kBlendingCoverage;
  surface_param_.surface_flags.cursor = 0;
  surface_param_.frame_rate = 30;
  surface_param_.solid_fill_color = 0;
  surface_param_.surface_transform.rotation = 0.0f;
  surface_param_.surface_transform.flip_horizontal = 0;
  surface_param_.surface_transform.flip_vertical = 0;

  if (enable_gfx_) {
    memset(&gfx_surface_config_, 0x0, sizeof gfx_surface_config_);
    gfx_surface_config_.width = 352;
    gfx_surface_config_.height = 288;
    gfx_surface_config_.format = SurfaceFormat::kFormatBGRA8888;
    gfx_surface_config_.buffer_count = 4;
    gfx_surface_config_.cache = 0;
    gfx_surface_config_.use_buffer = 0;
    gfx_surface_config_.z_order = 2;
    auto ret = display_->CreateSurface(gfx_surface_config_, &gfx_surface_id_);
    if (ret != 0) {
      TEST_ERROR("%s: CreateSurface Failed!!", __func__);
    }

    gfx_surface_param_.src_rect = {0.0, 0.0, static_cast<float>(352),
                                   static_cast<float>(288)};
    gfx_surface_param_.dst_rect = {0.0, 0.0, static_cast<float>(352),
                                   static_cast<float>(288)};
    gfx_surface_param_.surface_blending = SurfaceBlending::kBlendingCoverage;
    gfx_surface_param_.surface_flags.cursor = 0;
    gfx_surface_param_.frame_rate = 30;
    gfx_surface_param_.solid_fill_color = 0;
    gfx_surface_param_.surface_transform.rotation = 0.0f;
    gfx_surface_param_.surface_transform.flip_horizontal = 0;
    gfx_surface_param_.surface_transform.flip_vertical = 0;
  }

  TEST_INFO("%s: Exit", __func__);
  return res;
}

status_t GtestCommon::StopDisplay(DisplayType display_type) {
  TEST_INFO("%s: Enter", __func__);
  int32_t res = 0;

  if (display_started_ == 1) {
    display_started_ = 0;
    res = display_->DestroySurface(surface_id_);
    if (res != 0) {
      TEST_ERROR("%s DestroySurface Failed!!", __func__);
    }

    if (enable_gfx_) {
      res = display_->DestroySurface(gfx_surface_id_);
      if (res != 0) {
        TEST_ERROR("%s  DestroyGfxSurface Failed!!", __func__);
      }
    }

    res = display_->DestroyDisplay(display_type);
    if (res != 0) {
      TEST_ERROR("%s DestroyDisplay Failed!!", __func__);
    }
    res = display_->Disconnect();

    if (display_ != nullptr) {
      TEST_INFO("%s: DELETE display_:%p", __func__, display_);
      delete display_;
      display_ = nullptr;
    }
  }
  TEST_INFO("%s: Exit", __func__);
  return res;
}

#endif

status_t GtestCommon::SetCameraFocalLength(const float focal_length) {
  CameraMetadata meta;
  auto ret = recorder_.GetDefaultCaptureParam(camera_id_, meta);
  EXPECT_TRUE(ret == NO_ERROR);

  if (meta.exists(ANDROID_LENS_INFO_AVAILABLE_FOCAL_LENGTHS)) {
    camera_metadata_entry_t entry;
    entry = meta.find(ANDROID_LENS_INFO_AVAILABLE_FOCAL_LENGTHS);
    for (uint32_t i = 0 ; i < entry.count; i++) {
      if (entry.data.f[i] == focal_length) {
        ret = recorder_.GetCameraParam(camera_id_, meta);
        EXPECT_TRUE(ret == NO_ERROR);
        meta.update(ANDROID_LENS_FOCAL_LENGTH, &focal_length, 1);
        ret = recorder_.SetCameraParam(camera_id_, meta);
        EXPECT_TRUE(ret == NO_ERROR);
        break;
      }
    }
  }
  return NO_ERROR;
}

/*
* RemoveSpaces: Utility method to remove the spaces from a string
*/
void GtestCommon::RemoveSpaces(std::string &str) {
  str.erase(std::remove(str.begin(), str.end(), ' '), str.end());
}

/*
* TokenizeString: Method to tokenize a string based on delimiter
*/
void GtestCommon::TokenizeString(std::string const &str,
                                 const char delim,
                                 std::vector<std::string> &out) {
  size_t start;
  size_t end = 0;
  while ((start = str.find_first_not_of(delim, end)) != std::string::npos) {
    end = str.find(delim, start);
    out.push_back(str.substr(start, end - start));
  }
}

/*
* ParseExposureTable: This method parses the Exposure Table from a text file.
*/
status_t GtestCommon::ParseExposureTable(std::string dir_path,
                                         std::string fileName,
                                         std::vector<ExposureTable> &exp_tables) {
  FILE *fp;
  if (dir_path.empty() || fileName.empty()) {
    TEST_ERROR("%s: Empty path for directory or file", __func__);
    return -EINVAL;
  }
  std::string path = dir_path.append(fileName);
  if (!(fp = fopen(path.c_str(), "r"))) {
    TEST_ERROR("%s: failed to open exposure table file: %s", __func__,
               path.c_str());
    return -EINVAL;
  } else {
    TEST_INFO("%s: Opening exposure table file: %s", __func__, path.c_str());
  }
  ExposureTable exp_table{};
  std::string input_str;
  const char delim_colon = ':', delim_space = ' ';
  std::string key, value;
  uint8_t knee_index = 0;
  std::ifstream input_file(path.c_str());
  std::vector<std::string> out, out_values;
  while (getline(input_file, input_str)) {
    out.clear();
    TokenizeString(input_str, delim_colon, out);
    key = out[0];
    value = out[1];
    RemoveSpaces(key);
    if (key.compare("is_valid") == 0) {
      exp_table.is_valid = std::atoi(value.c_str());
    } else if (key.compare("sensitivity_correction_factor") == 0) {
      exp_table.sensitivity_correction_factor = std::atof(value.c_str());
    } else if (key.compare("knee_count") == 0) {
      exp_table.knee_count = std::atof(value.c_str());
    } else if (key.compare("gain_knee_entries") == 0) {
      out_values.clear();
      TokenizeString(value, delim_space, out_values);
      for (knee_index = 0; knee_index < out_values.size(); knee_index++) {
        exp_table.gain_knee_entries[knee_index] =
          std::atof(out_values[knee_index].c_str());
      }
    } else if (key.compare("exp_time_knee_entries") == 0) {
      out_values.clear();
      TokenizeString(value, delim_space, out_values);
      for (knee_index = 0; knee_index < out_values.size(); knee_index++) {
        exp_table.exp_time_knee_entries[knee_index] =
          std::atof(out_values[knee_index].c_str());
      }
    } else if (key.compare("increment_priority_knee_entries") == 0) {
      out_values.clear();
      TokenizeString(value, delim_space, out_values);
      for (knee_index = 0; knee_index < out_values.size(); knee_index++) {
        exp_table.increment_priority_knee_entries[knee_index] =
          std::atof(out_values[knee_index].c_str());
      }
    } else if (key.compare("exp_index_knee_entries") == 0) {
      out_values.clear();
      TokenizeString(value, delim_space, out_values);
      for (knee_index = 0; knee_index < out_values.size(); knee_index++) {
        exp_table.exp_index_knee_entries[knee_index] =
          std::atof(out_values[knee_index].c_str());
      }
    } else if (key.compare("thres_anti_banding_min_exp_time_pct") == 0) {
      exp_table.thres_anti_banding_min_exp_time_pct = std::atof(value.c_str());
    } else {
      TEST_ERROR("%s: Invalid field %s\n", __func__, key.c_str());
      return -EINVAL;
    }
  }
  exp_tables.push_back(exp_table);
  return NO_ERROR;
}

/*
* PopulateExpTable: This method populates the Exposure Tables from files.
*/
status_t GtestCommon::PopulateExpTable(std::vector<ExposureTable> &exp_tables) {
  std::string dir_path(kQmmfFolderPath);
  DIR *dp;
  struct dirent *dirent;
  dp = opendir(dir_path.c_str());
  if (nullptr == dp) {
    QMMF_ERROR("%s: Failed to open %s folder", __func__, dir_path.c_str());
    return -EPERM;
  }
  size_t file_init_len = strlen("exposure_table");
  size_t ext_len = strlen(".txt");
  while ((dirent = readdir(dp)) != nullptr) {
    std::string fileName = std::string(dirent->d_name);
    if ((fileName.length() > file_init_len) &&
        (!strncmp("exposure_table", fileName.substr(0, file_init_len).c_str(),
                  strlen("exposure_table")) &&
         (!strncmp(".txt", fileName.substr(fileName.size() - ext_len).c_str(),
                   ext_len)))) {
      TEST_INFO("%s: File found %s", __func__, fileName.c_str());
      auto ret = ParseExposureTable(dir_path, fileName,
                                    exp_tables);
      if (ret != NO_ERROR) {
        TEST_ERROR("%s: Exposure Table Parsing failed", __func__);
        return -EFAULT;
      }
    }
  }
  return NO_ERROR;
}

#ifdef CAM_ARCH_V2
/**
 * This function can be called only after StartCamera. It tries to fetch
 * tag_id, on success, returns true and fills vendor tag_id. On failure,
 * returns false.
 */
bool GtestCommon::VendorTagSupported(const String8& name,
                                      const String8& section,
                                      uint32_t* tag_id) {
  TEST_DBG("%s: Enter", __func__);
  bool is_available = false;
  status_t result = 0;

  if (nullptr == tag_id) {
    TEST_ERROR("%s: tag_id is not allocated, returning", __func__);
    return false;
  }

  if (nullptr == vendor_tag_desc_.get()) {
    vendor_tag_desc_ = VendorTagDescriptor::getGlobalVendorTagDescriptor();
    if (nullptr == vendor_tag_desc_.get()) {
      TEST_ERROR("%s: Failed in fetching vendor tag descriptor", __func__);
      return false;
    }
  }

  result = vendor_tag_desc_->lookupTag(name, section, tag_id);
  if (0 != result) {
    TEST_ERROR("%s: TagId lookup failed with error: %d", __func__, result);
    return false;
  } else {
    TEST_INFO("%s: name = %s, section = %s, tag_id = 0x%x",
              __func__, name.string(), section.string(), *tag_id);
    is_available = true;
  }

  TEST_DBG("%s: Exit", __func__);
  return is_available;
}

/**
 * This function can be called only after StartCamera. It checks whether
 * tag_id is present in given meta, on success, returns true and fills
 * vendor tag_id. On failure, returns false.
 */
bool GtestCommon::VendorTagExistsInMeta(const CameraMetadata& meta,
                                         const String8& name,
                                         const String8& section,
                                         uint32_t* tag_id) {
  TEST_DBG("%s: Enter", __func__);
  bool is_available = false;

  if (VendorTagSupported(name, section, tag_id)) {
    if (meta.exists(*tag_id)) {
      is_available = true;
    } else {
      TEST_ERROR("%s: TagId does not exist in given meta", __func__);
      return false;
    }
  }

  TEST_DBG("%s: Exit", __func__);
  return is_available;
}

void GtestCommon::CreatePrivacyMaskOverlay (const uint32_t& video_track_id,
                                              const int32_t& width,
                                              const int32_t& height,
                                              uint32_t* mask_id) {
  // Create BoundingBox type overlay.
  OverlayParam object_params{};
  object_params.type  = OverlayType::kPrivacyMask;
  object_params.color = 0xFF9933FF; //Fill mask with color.
  // Dummy coordinates for test purpose.
  object_params.dst_rect.start_x = 20;
  object_params.dst_rect.start_y = 40;
  object_params.dst_rect.width   = width/8;
  object_params.dst_rect.height  = height/8;

  auto ret = recorder_.CreateOverlayObject(video_track_id, object_params,
                                           mask_id);
  ASSERT_TRUE(ret == 0);
  ret = recorder_.SetOverlay(video_track_id, *mask_id);
  ASSERT_TRUE(ret == 0);

  ret = recorder_.GetOverlayObjectParams(video_track_id, *mask_id,
                                             object_params);
  ASSERT_TRUE(ret == 0);

  object_params.dst_rect.start_x = (object_params.dst_rect.start_x +
    object_params.dst_rect.width < width) ? object_params.dst_rect.start_x + 20
                                          : 20;

  object_params.dst_rect.width = (object_params.dst_rect.start_x +
    object_params.dst_rect.width < width) ? object_params.dst_rect.width + 50
                                          : width/8;

  object_params.dst_rect.start_y = (object_params.dst_rect.start_y +
    object_params.dst_rect.height < height) ? object_params.dst_rect.start_y +
                                              10 : 40;

  object_params.dst_rect.height = (object_params.dst_rect.start_y +
    object_params.dst_rect.height < height) ? object_params.dst_rect.height +
                                          50 : height/8;

  ret = recorder_.UpdateOverlayObjectParams(video_track_id, *mask_id,
                                                object_params);
  ASSERT_TRUE(ret == 0);

}

void GtestCommon::DestroyPrivacyMaskOverlay (const uint32_t& video_track_id,
                                               const uint32_t& mask_id) {

  // Remove overlay object from video track.
  auto ret = recorder_.RemoveOverlay(video_track_id, mask_id);
  ASSERT_TRUE(ret == 0);

  // Delete overlay object.
  ret = recorder_.DeleteOverlayObject(video_track_id, mask_id);
  ASSERT_TRUE(ret == 0);
}
#endif
status_t GtestCommon::DumpThumbnail(BufferDescriptor buffer,
                                    const CameraBufferMetaData& meta_data,
                                    uint32_t image_sequence_count,
                                    uint64_t tv_ms) {
  uint8_t thumb_num = 0;
  uint8_t *in_img = static_cast<uint8_t*>(buffer.data);
  const CameraBufferMetaData &info = meta_data;

  if (meta_data.format != BufferFormat::kBLOB) {
    TEST_INFO("%s: Skip Thumbnail bump. In_fmt: %d \n",
        __func__, meta_data.format);
    return NO_INIT;
  }

  if (info.num_planes > 2) {

    //Main image
    std::string main_image_path = "/data/misc/qmmf/snapshot_" +
                                  std::to_string(image_sequence_count) + "_" +
                                  std::to_string(tv_ms) + "_main.jpg";

    FILE *thumb_file = fopen(main_image_path.c_str(), "w+");
    if (!thumb_file) {
      TEST_ERROR("%s: Unable to open thumb_file(%s)", __func__,
          main_image_path.c_str());
      return BAD_VALUE;
    }
    //Add SOI marker
    auto len = fwrite(&in_img[0], sizeof(uint8_t), 2, thumb_file);
    if (len != 2) {
      TEST_ERROR("%s: Fail to main image (%s)", __func__,
          main_image_path.c_str());
      fclose(thumb_file);
      return BAD_VALUE;
    }
    len = fwrite(&in_img[info.plane_info[0].offset], sizeof(uint8_t),
                      info.plane_info[0].size, thumb_file);
    if (len != info.plane_info[0].size) {
      TEST_ERROR("%s: Fail to store main image (%s)", __func__,
          main_image_path.c_str());
      fclose(thumb_file);
      return BAD_VALUE;
    }
    TEST_INFO("%s: Main image Size(%u) Stored@(%s)\n",
        __func__, info.plane_info[0].size, main_image_path.c_str());
    fclose(thumb_file);
    thumb_file = nullptr;

    // First thumbnail
    std::string thumb_path = "/data/misc/qmmf/snapshot_" +
                             std::to_string(image_sequence_count) + "_" +
                             std::to_string(tv_ms) + "_thumb_" +
                             std::to_string(thumb_num) + ".jpg";


    thumb_file = fopen(thumb_path.c_str(), "w+");
    if (!thumb_file) {
      TEST_ERROR("%s: Unable to open thumb_file(%s)", __func__,
          thumb_path.c_str());
      return BAD_VALUE;
    }

    len = fwrite(&in_img[info.plane_info[1].offset], sizeof(uint8_t),
                 info.plane_info[1].size, thumb_file);
    if (len != info.plane_info[1].size) {
      TEST_ERROR("%s: Fail to store thumbnail (%s)", __func__,
          thumb_path.c_str());
      fclose(thumb_file);
      return BAD_VALUE;
    }
    TEST_INFO("%s: Thumb (%d) Size(%u) Stored@(%s)\n",
        __func__, thumb_num, info.plane_info[1].size, thumb_path.c_str());
    fclose(thumb_file);
    thumb_file = nullptr;
    thumb_num++;

    // Second thumbnail
    thumb_path = "/data/misc/qmmf/snapshot_" +
                             std::to_string(image_sequence_count) + "_" +
                             std::to_string(tv_ms) + "_thumb_" +
                             std::to_string(thumb_num) + ".jpg";

    thumb_file = fopen(thumb_path.c_str(), "w+");
    if (!thumb_file) {
      TEST_ERROR("%s: Unable to open thumb_file(%s)", __func__,
          thumb_path.c_str());
      return BAD_VALUE;
    }

    uint32_t thumbnail_size = 0;
    for (uint32_t i = 2; i < info.num_planes; i++) {
      auto len = fwrite(&in_img[info.plane_info[i].offset], sizeof(uint8_t),
                         info.plane_info[i].size, thumb_file);
      if (len != info.plane_info[i].size) {
        TEST_ERROR("%s: Fail to store thumbnail (%s)", __func__,
            thumb_path.c_str());
        fclose(thumb_file);
        return BAD_VALUE;
      }
      thumbnail_size += len;
    }
    TEST_INFO("%s: Thumb (%d) Size(%u) Stored@(%s)\n",
        __func__, thumb_num, thumbnail_size, thumb_path.c_str());
    fclose(thumb_file);
    thumb_file = nullptr;
  }

  return NO_ERROR;
}


#ifndef DISABLE_DISPLAY
status_t GtestCommon::PushFrameToDisplay(BufferDescriptor &buffer,
                                           CameraBufferMetaData &meta_data) {
  TEST_DBG("%s: Enter", __func__);
  if (display_started_) {
    int32_t ret;
    surface_buffer_.plane_info[0].ion_fd = buffer.fd;
    surface_buffer_.buf_id = buffer.fd;
    surface_buffer_.format = SurfaceFormat::kFormatYCbCr420SemiPlanarVenus;
    surface_buffer_.plane_info[0].stride = meta_data.plane_info[0].stride;
    surface_buffer_.plane_info[0].size = buffer.size;
    surface_buffer_.plane_info[0].width = meta_data.plane_info[0].width;
    surface_buffer_.plane_info[0].height = meta_data.plane_info[0].height;
    surface_buffer_.plane_info[0].offset = 0;
    surface_buffer_.plane_info[0].buf = buffer.data;

    ret = display_->QueueSurfaceBuffer(surface_id_, surface_buffer_,
                                       surface_param_);
    if (ret != 0) {
      TEST_ERROR("%s QueueSurfaceBuffer Failed!!", __func__);
      return ret;
    }

    ret = display_->DequeueSurfaceBuffer(surface_id_, surface_buffer_);
    if (ret != 0) {
      TEST_ERROR("%s DequeueSurfaceBuffer Failed!!", __func__);
    }
  }
  TEST_DBG("%s: Exit", __func__);
  return NO_ERROR;
}

int32_t GtestCommon::DequeueGfxSurfaceBuffer() {
  TEST_DBG("%s: Enter", __func__);
  auto ret = 0;

  memset(&gfx_surface_buffer_, 0x0, sizeof gfx_surface_buffer_);

  gfx_surface_buffer_.format = SurfaceFormat::kFormatBGRA8888;
  gfx_surface_buffer_.acquire_fence = 0;
  gfx_surface_buffer_.release_fence = 0;

  ret = display_->DequeueSurfaceBuffer(gfx_surface_id_, gfx_surface_buffer_);
  if (ret != 0) {
    TEST_ERROR("%s: DequeueSurfaceBuffer Failed!!", __func__);
  }
  gfx_file = fopen("/data/misc/qmmf/Images/fasimo_352x288_bgra_8888.rgb", "r");
  if (!gfx_file) {
    TEST_ERROR("%s: Unable to open file", __func__);
    return -1;
  }
  int32_t offset = 0;
  for (uint32_t i = 0; i < gfx_surface_buffer_.plane_info[0].height; i++) {
    fread((uint8_t *)gfx_surface_buffer_.plane_info[0].buf +
              gfx_surface_buffer_.plane_info[0].offset + offset,
          sizeof(uint8_t), gfx_surface_buffer_.plane_info[0].width * 4,
          gfx_file);
    offset += ((gfx_surface_buffer_.plane_info[0].width +
                ((gfx_surface_buffer_.plane_info[0].width % 64) ?
                (64 - (gfx_surface_buffer_.plane_info[0].width % 64)): 0)) *4);
  }
  fclose(gfx_file);

  TEST_DBG("%s: Exit", __func__);
  return 0;
}

int32_t GtestCommon::QueueGfxSurfaceBuffer() {
  TEST_DBG("%s: Enter", __func__);

  memset(&gfx_surface_param_, 0x0, sizeof gfx_surface_param_);

  gfx_surface_param_.src_rect = {0.0, 0.0, 352.0, 288.0};
  gfx_surface_param_.dst_rect = {0.0, 0.0, 352.0, 288.0};
  gfx_surface_param_.surface_blending = SurfaceBlending::kBlendingCoverage;
  gfx_surface_param_.surface_flags.cursor = 0;
  gfx_surface_param_.frame_rate = 30;
  gfx_surface_param_.solid_fill_color = 0;

  auto ret = display_->QueueSurfaceBuffer(gfx_surface_id_, gfx_surface_buffer_,
                                          gfx_surface_param_);
  if (ret != 0) {
    TEST_ERROR("%s: QueueSurfaceBuffer Failed!!", __func__);
  }

  TEST_DBG("%s: Exit", __func__);
  return 0;
}
#endif

status_t GtestCommon::DrawOverlay(void *data, int32_t width, int32_t height) {

  TEST_DBG("%s: Enter", __func__);
  status_t ret = 0;

#if USE_SKIA
  //Create Skia canvas outof ION memory.
  SkImageInfo imageInfo = SkImageInfo::Make(width, height,
      kRGBA_8888_SkColorType, kPremul_SkAlphaType);

#ifdef QCAMERA3_TAG_LOCAL_COPY
  canvas_ = (SkCanvas::MakeRasterDirect(imageInfo,
      static_cast<unsigned char*>(data), width *4)).release();
#else
  canvas_ = SkCanvas::NewRasterDirect(imageInfo,
      static_cast<unsigned char*>(data), width *4);
#endif

#elif USE_CAIRO
  cr_surface_ = cairo_image_surface_create_for_data(static_cast<unsigned char*>
                                                    (data),
                                                    CAIRO_FORMAT_ARGB32, width,
                                                    height, width * 4);
  EXPECT_TRUE(cr_surface_ != nullptr);

  cr_context_ = cairo_create (cr_surface_);
  EXPECT_TRUE(cr_context_ != nullptr);
#endif

  struct timeval tv;
  time_t now_time;
  struct tm *time;
  char date_buf[40];
  char time_buf[40];

  gettimeofday(&tv, NULL);
  now_time = tv.tv_sec;
  time = localtime(&now_time);

  strftime(date_buf, sizeof date_buf, "%Y/%m/%d", time);
  strftime(time_buf, sizeof time_buf, "%H:%M:%S", time);

  TEST_INFO("%s: date:time (%s:%s)", __func__, date_buf, time_buf);

  double x_date, y_date;
  x_date = y_date = 0.0;

#if USE_SKIA
  canvas_->clear(SK_AlphaOPAQUE);

  int32_t date_len = strlen(date_buf);
  int32_t time_len = strlen(time_buf);

  SkPaint paint;
  paint.setColor(kColorRed);
  paint.setTextSize(SkIntToScalar(DATETIME_PIXEL_SIZE));
  paint.setAntiAlias(true);
  paint.setTextScaleX(1);

  SkString date_text(date_buf, date_len);
  canvas_->drawText(date_text.c_str(), date_text.size(), x_date, y_date, paint);

  SkString time_text(time_buf, time_len);
  int32_t per_char_size = DATETIME_TEXT_BUF_WIDTH/date_text.size();
  float x_time = (DATETIME_TEXT_BUF_WIDTH - (time_text.size() * per_char_size));
  x_time = x_time > 0 ? (x_time) : 0;
  float y_time = DATETIME_TEXT_BUF_HEIGHT - DATETIME_PIXEL_SIZE/2;
  canvas_->drawText(time_text.c_str(), time_text.size(), x_time, y_time, paint);
  canvas_->flush();
  usleep(1000);

#elif USE_CAIRO
  ClearSurface();
  cairo_select_font_face(cr_context_, "@cairo:Serif", CAIRO_FONT_SLANT_ITALIC,
                          CAIRO_FONT_WEIGHT_BOLD);
  cairo_set_font_size (cr_context_, DATETIME_PIXEL_SIZE);
  cairo_set_antialias (cr_context_, CAIRO_ANTIALIAS_BEST);
  EXPECT_TRUE(CAIRO_STATUS_SUCCESS == cairo_status(cr_context_));

  cairo_font_extents_t font_extent;
  cairo_font_extents (cr_context_, &font_extent);
  TEST_DBG("%s: ascent=%f, descent=%f, height=%f, max_x_advance=%f,"
      " max_y_advance = %f", __func__, font_extent.ascent, font_extent.descent,
       font_extent.height, font_extent.max_x_advance,
       font_extent.max_y_advance);

  cairo_text_extents_t date_text_extents;
  cairo_text_extents (cr_context_, date_buf, &date_text_extents);

  TEST_DBG("%s: Date: te.x_bearing=%f, te.y_bearing=%f, te.width=%f,"
      " te.height=%f, te.x_advance=%f, te.y_advance=%f", __func__,
      date_text_extents.x_bearing, date_text_extents.y_bearing,
      date_text_extents.width, date_text_extents.height,
      date_text_extents.x_advance, date_text_extents.y_advance);

  cairo_font_options_t *options;
  options = cairo_font_options_create ();
  cairo_font_options_set_antialias (options, CAIRO_ANTIALIAS_DEFAULT);
  cairo_set_font_options (cr_context_, options);
  cairo_font_options_destroy (options);

  //(0,0) is at topleft corner of draw buffer.
  y_date = height/2.0; // height is buffer height.
  y_date = std::max(y_date, date_text_extents.height - (font_extent.descent/2.0));
  cairo_move_to (cr_context_, x_date, y_date);

  // Draw date.
  RGBAValues text_color{};
  ExtractColorValues(kColorRed, &text_color);
  cairo_set_source_rgba (cr_context_, text_color.red, text_color.green,
                         text_color.blue, text_color.alpha);

  cairo_show_text (cr_context_, date_buf);
  EXPECT_TRUE(CAIRO_STATUS_SUCCESS == cairo_status(cr_context_));

  cairo_text_extents_t time_text_extents;
  cairo_text_extents (cr_context_, time_buf, &time_text_extents);
  TEST_DBG("%s: Time: te.x_bearing=%f, te.y_bearing=%f, te.width=%f,"
    " te.height=%f, te.x_advance=%f, te.y_advance=%f", __func__,
    time_text_extents.x_bearing, time_text_extents.y_bearing,
    time_text_extents.width, time_text_extents.height,
    time_text_extents.x_advance, time_text_extents.y_advance);
  // Calculate the x_time to draw the time text extact middle of buffer.
  // Use x_width which usally few pixel less than the width of the actual
  // drawn text.
  double x_time = (width - time_text_extents.width)/2.0; // width_ is buffer width.
  double y_time = y_date + (date_text_extents.height - (font_extent.descent/2));
  cairo_move_to (cr_context_, x_time, y_time);
  cairo_show_text (cr_context_, time_buf);
  EXPECT_TRUE(CAIRO_STATUS_SUCCESS == cairo_status(cr_context_));

  cairo_surface_flush(cr_surface_);

  if (cr_surface_) {
    cairo_surface_destroy(cr_surface_);
  }
  if (cr_context_) {
    cairo_destroy(cr_context_);
  }
#endif

  TEST_DBG("%s: Exit", __func__);
  return ret;
}

void GtestCommon::ExtractColorValues(uint32_t hex_color, RGBAValues* color) {

  color->red   = ((hex_color >> 24) & 0xff) / 255.0;
  color->green = ((hex_color >> 16) & 0xff) / 255.0;
  color->blue  = ((hex_color >> 8) & 0xff) / 255.0;
  color->alpha = ((hex_color) & 0xff) / 255.0;
}

void GtestCommon::ClearSurface() {
#if USE_SKIA

#elif USE_CAIRO
  cairo_set_operator(cr_context_, CAIRO_OPERATOR_CLEAR);
  cairo_paint(cr_context_);
  cairo_surface_flush(cr_surface_);
  cairo_set_operator(cr_context_, CAIRO_OPERATOR_OVER);
  ASSERT_TRUE(CAIRO_STATUS_SUCCESS == cairo_status(cr_context_));
#endif
}

status_t GtestCommon::FillCropMetadata(CameraMetadata& meta,
                                            int32_t sensor_mode_w,
                                            int32_t sensor_mode_h,
                                            int32_t crop_x, int32_t crop_y,
                                            int32_t crop_w, int32_t crop_h) {

  auto active_array_size = meta.find(ANDROID_SENSOR_INFO_ACTIVE_ARRAY_SIZE);
  if (!active_array_size.count) {
    TEST_ERROR("%s: Active sensor array size is missing!", __func__);
    return NAME_NOT_FOUND;
  }
  // Take the active pixel array width and height as base on which to
  // recalculate the actual crop region dimensions.
  float x = active_array_size.data.i32[2];
  float y = active_array_size.data.i32[3];
  float width = active_array_size.data.i32[2];
  float height = active_array_size.data.i32[3];

  // Get the crop region scale ratios and recalculate them against the base.
  x *= (static_cast<float>(crop_x) / sensor_mode_w);
  y *= (static_cast<float>(crop_y) / sensor_mode_h);
  width *= (static_cast<float>(crop_w) / sensor_mode_w);
  height *= (static_cast<float>(crop_h) / sensor_mode_h);

  int32_t crop_region[] = {
      static_cast<int32_t>(round(x)),
      static_cast<int32_t>(round(y)),
      static_cast<int32_t>(round(width)),
      static_cast<int32_t>(round(height)),
  };
  auto ret = meta.update(ANDROID_SCALER_CROP_REGION, crop_region, 4);
  if (NO_ERROR != ret) {
    TEST_ERROR("%s: Failed to set crop region metadata!", __func__);
    return ret;
  }

  return NO_ERROR;
}
