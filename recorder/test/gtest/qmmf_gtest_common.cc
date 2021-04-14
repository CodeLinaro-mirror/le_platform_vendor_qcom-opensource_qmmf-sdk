/*
* Copyright (c) 2018-2021, The Linux Foundation. All rights reserved.
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
#include <ios>
#include <random>

#include "recorder/test/gtest/qmmf_gtest_common.h"

#ifndef CAMERA_HAL1_SUPPORT
using namespace qcamera;
#endif

using ::std::ios;
using ::std::ofstream;
using ::std::streampos;

const std::string GtestCommon::kQmmfFolderPath = "/data/misc/qmmf/";

status_t DumpBitStream::SetUp(const StreamDumpInfo& dumpinfo) {
  TEST_DBG("%s: Enter", __func__);
  EXPECT_TRUE(dumpinfo.width > 0);
  EXPECT_TRUE(dumpinfo.height > 0);
  struct timeval tv;
  gettimeofday(&tv, NULL);
  SplitFileInfo file_info = {dumpinfo, tv.tv_sec, 0, nullptr, 0};

  std::string bitstream_filepath = GetFileName(file_info);
  int32_t file_fd = open(bitstream_filepath.c_str(),
                         O_CREAT | O_WRONLY | O_TRUNC, 0655);
  if (file_fd < 0) {
    TEST_ERROR("%s File open failed!", __func__);
    return BAD_VALUE;
  }
  file_info.file_fd = file_fd;
  uint8_t key_by_session_track_id = GenerateKey(dumpinfo.session_id,
    dumpinfo.track_id);
  split_file_info_.insert(std::make_pair(key_by_session_track_id, file_info));

  TEST_DBG("%s: Exit", __func__);
  return NO_ERROR;
}

status_t DumpBitStream::SplitFile(const uint8_t file_index) {
  TEST_DBG("%s: Enter", __func__);

  SplitFileInfo& file_info = split_file_info_[file_index];
  file_info.part_number += 1;
  EXPECT_TRUE(file_info.streaminfo.width > 0);
  EXPECT_TRUE(file_info.streaminfo.height > 0);

  std::string bitstream_filepath = GetFileName(file_info);

  int32_t file_fd = file_info.file_fd;
  close(file_fd);
  // Get New FileFd
  file_fd = open(bitstream_filepath.c_str(),
                 O_CREAT | O_WRONLY | O_TRUNC, 0655);
  if (file_fd < 0) {
    TEST_ERROR("%s File open failed for part number: %d", __func__,
               file_info.part_number);
    return BAD_VALUE;
  }
  file_info.file_fd = file_fd;

  if (file_info.streaminfo.format == VideoFormat::kAVC ||
      file_info.streaminfo.format == VideoFormat::kHEVC) {
    // header or first buffer dump required at start of each file dump in case
    // of AVC and HEVC format
    BufferDescriptor *buf = file_info.header;
    uint32_t exp_size = buf->size;
    uint32_t written_length = write(file_fd, buf->data, buf->size);
    if (written_length != exp_size) {
      TEST_ERROR("%s: Bad Write error (%d) %s", __func__, errno,
                 strerror(errno));
      return BAD_VALUE;
    }
  }

  TEST_DBG("%s: Exit", __func__);
  return NO_ERROR;
}

std::string DumpBitStream::GetFileName(const SplitFileInfo& file_info) {
  const char* type_string;
  switch (file_info.streaminfo.format) {
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
  char prop_val[PROPERTY_VALUE_MAX];
  property_get(PROP_DUMP_TO_EXT, prop_val, "0");
  if (atoi(prop_val) == 1) {
    bitstream_filepath = "/mnt/sdcard/data/misc/qmmf/gtest_track_";
  }
  bitstream_filepath += std::to_string(file_info.streaminfo.track_id) + "_";
  bitstream_filepath += std::to_string(file_info.streaminfo.width) + "x";
  bitstream_filepath += std::to_string(file_info.streaminfo.height) + "_";
  bitstream_filepath += std::to_string(file_info.timestamp) + "_";
  bitstream_filepath += std::to_string(file_info.part_number) + ".";
  bitstream_filepath += extn;
  return bitstream_filepath;
}

status_t DumpBitStream::Dump(const std::vector<BufferDescriptor>& buffers,
   const uint32_t &session_id, const uint32_t &track_id) {

  TEST_DBG("%s: Enter", __func__);
  uint8_t key_by_session_track_id = GenerateKey(session_id, track_id);
  int32_t file_fd = GetFileFd(session_id, track_id);
  EXPECT_TRUE(file_fd >= 0);

  if (!split_file_info_[key_by_session_track_id].header && buffers.size()) {
    TEST_DBG("%s: First video frame", __func__);
    BufferDescriptor *buf = new BufferDescriptor();
    BufferDescriptor *head = const_cast<BufferDescriptor*>(&buffers[0]);
    buf->size = head->size;
    buf->data = malloc(head->size);
    memcpy(buf->data, head->data, head->size);
    split_file_info_[key_by_session_track_id].header = buf;
  }

  uint64_t file_size = GetFileSize(file_fd);
  for (auto& iter : buffers) {
    uint32_t exp_size = iter.size;
    TEST_DBG("%s:%s BitStream buffer data(0x%x):size(%d):ts(%lld):flag(0x%x)"
      ":buf_id(%d):capacity(%d)",  __func__, iter.data, iter.size,
       iter.timestamp, iter.flag, iter.buf_id, iter.capacity);

    if (file_size + iter.size > MAX_DUMP_SIZE) {
      auto ret = SplitFile(key_by_session_track_id);
      EXPECT_TRUE(ret == NO_ERROR);
      file_size += iter.size;
    }
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

void DumpBitStream::Close(const uint32_t &session_id,
                          const uint32_t &track_id) {
  TEST_DBG("%s: Enter", __func__);
  uint8_t key_by_session_track_id = GenerateKey(session_id, track_id);
  int32_t file_fd = split_file_info_[key_by_session_track_id].file_fd;
  if (file_fd >= 0) {
    close(file_fd);
    BufferDescriptor *buf = split_file_info_[key_by_session_track_id].header;
    free(buf->data);
    delete(buf);
    split_file_info_.erase(key_by_session_track_id);
  } else {
    TEST_WARN("%s: file_fd does not exist!", __func__);
  }
  TEST_DBG("%s: Exit", __func__);
}

void DumpBitStream::CloseAll() {
  TEST_DBG("%s: Enter", __func__);
  for(auto& iter : split_file_info_) {
    if (iter.second.file_fd >= 0) {
      close(iter.second.file_fd);
    }
    if(iter.second.header) {
      BufferDescriptor *buf = iter.second.header;
      free(buf->data);
      delete(buf);
    }
  }
  split_file_info_.clear();
  TEST_DBG("%s: Exit", __func__);
}

void FrameTrace::SetUp(uint32_t session_id, uint32_t track_id, float fps) {
  std::lock_guard<std::mutex> lk(lock_);
  session_id_ = session_id;
  track_id_   = track_id;
  track_fps_  = fps;
}

void FrameTrace::Reset() {
  std::lock_guard<std::mutex> lk(lock_);
  previous_timestamp_   = 0;
  total_frames_         = 0;
  total_dropped_frames_ = 0;
}

void FrameTrace::BufferAvailableCb(BufferDescriptor buffer) {

  if (!enabled_) {
    // Not enabled.
    return;
  }

  std::lock_guard<std::mutex> lk(lock_);
  total_frames_++;

  // Timestamp Δ in us = current frame timestamp - previous frame timestamp.
  uint64_t current_delta = (buffer.timestamp - previous_timestamp_);

  // Calculate the expected timestamp Δ in us.
  uint64_t expected_delta = 1000000L / track_fps_;

  // Adjust timestamp Δ with variance.
  uint64_t delta = current_delta + (expected_delta * kTimestampVariance);

  // Calculate if there are any frames dropped and how many.
  int32_t dropped_frames = (delta / expected_delta) - 1;

  if ((dropped_frames > 0) && (previous_timestamp_ != 0)) {
    total_frames_ += dropped_frames;
    total_dropped_frames_ += dropped_frames;

    TEST_WARN("%s: Session %u | Track %u | Expected timestamp Δ = %llu us | "
        "Current timestamp Δ = %llu us | DROPPED FRAMES = %d | TOTAL DROPPED "
        "FRAMES = %u / %u", __func__, session_id_, track_id_, expected_delta,
        current_delta, dropped_frames, total_dropped_frames_, total_frames_);
  }

  // Save current timestamp for use in next call.
  previous_timestamp_ = buffer.timestamp;
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
  property_get(PROP_EIS_H_MARGIN, prop_val, "-1.0");
  eis_h_margin_ = atof(prop_val);
  property_get(PROP_EIS_V_MARGIN, prop_val, "-1.0");
  eis_v_margin_ = atof(prop_val);
  property_get(PROP_TIMELAPSE_INTERVAL, prop_val, "2.0");
  timelapse_interval_ = atof(prop_val);
  property_get(PROP_FRAME_DEBUG, prop_val, "0");
  is_frame_debug_enabled_ = (atoi(prop_val) == 0) ? false : true;
  property_get(PROP_SENSOR_CONFIG_FILE, prop_val, "");
  sensor_mode_file_name_ = std::string(prop_val);
  property_get(PROP_MEASURE_SOF_LATENCY, prop_val, "0");
  enable_sof_latency_ = (atoi(prop_val) == 0) ? false : true;
  property_get(PROP_AF_MODE, prop_val, "0");
  af_mode_ = atoi(prop_val);
  property_get(PROP_CAMERA_FPS, prop_val, DEFAULT_CAMERA_FPS);
  camera_fps_ = atof(prop_val);
  property_get(PROP_EIS, prop_val, "0");
  is_eis_on_ = (atoi(prop_val) == 0) ? false : true;
  property_get(PROP_SHDR, prop_val, "0");
  is_shdr_on_ = (atoi(prop_val) == 0) ? false : true;
  property_get(PROP_SNAPSHOT_STREAM_ON, prop_val, "0");
  is_snap_stream_on_ = (atoi(prop_val) == 0) ? false : true;
  property_get(PROP_LDC, prop_val, "0");
  is_ldc_on_ = (atoi(prop_val) == 0) ? false : true;

  // Read First Video Stream Params
  VideoStreamInfo stream { };
  property_get(PROP_FIRST_STREAM_WIDTH, prop_val, DEFAULT_FIRST_STREAM_WIDTH);
  stream.width = atoi(prop_val);

  property_get(PROP_FIRST_STREAM_HEIGHT, prop_val,
               DEFAULT_FIRST_STREAM_HEIGHT);
  stream.height = atoi(prop_val);

  property_get(PROP_FIRST_STREAM_FPS, prop_val, DEFAULT_FIRST_STREAM_FPS);
  stream.fps = atof(prop_val);

  stream.source_stream_id = 0; // First Stream, Linked ID should be 0.

  property_get(PROP_FIRST_STREAM_FORMAT, prop_val,
               DEFAULT_FIRST_STREAM_FORMAT);
  SetVideoStreamFormat(prop_val, stream.format);

  // Insert first stream into Map. Taking stream ID as 1.
  stream_info_map_.emplace(kFirstStreamID, stream);

  // Read Second Video Stream Params
  property_get(PROP_SECOND_STREAM_WIDTH, prop_val,
               DEFAULT_SECOND_STREAM_WIDTH);
  stream.width = atoi(prop_val);

  property_get(PROP_SECOND_STREAM_HEIGHT, prop_val,
               DEFAULT_SECOND_STREAM_HEIGHT);
  stream.height = atoi(prop_val);

  property_get(PROP_SECOND_STREAM_FPS, prop_val, DEFAULT_SECOND_STREAM_FPS);
  stream.fps = atof(prop_val);

  property_get(PROP_SECOND_STREAM_SOURCE_ID, prop_val, "0");
  stream.source_stream_id = atoi(prop_val);

  property_get(PROP_SECOND_STREAM_FORMAT, prop_val,
               DEFAULT_SECOND_STREAM_FORMAT);
  SetVideoStreamFormat(prop_val, stream.format);

  // Insert Second stream into Map. Taking stream ID as 2.
  stream_info_map_.emplace(kSecondStreamID, stream);

  // Read Third Video Stream Params
  property_get(PROP_THIRD_STREAM_WIDTH, prop_val, DEFAULT_THIRD_STREAM_WIDTH);
  stream.width = atoi(prop_val);

  property_get(PROP_THIRD_STREAM_HEIGHT, prop_val,
               DEFAULT_THIRD_STREAM_HEIGHT);
  stream.height = atoi(prop_val);

  property_get(PROP_THIRD_STREAM_FPS, prop_val, DEFAULT_THIRD_STREAM_FPS);
  stream.fps = atof(prop_val);

  property_get(PROP_THIRD_STREAM_SOURCE_ID, prop_val, "0");
  stream.source_stream_id = atoi(prop_val);

  property_get(PROP_THIRD_STREAM_FORMAT, prop_val,
               DEFAULT_THIRD_STREAM_FORMAT);
  SetVideoStreamFormat(prop_val, stream.format);

  // Insert Third stream into Map. Taking stream ID as 3.
  stream_info_map_.emplace(kThirdStreamID, stream);

  // Read Fourth Video Stream Params
  property_get(PROP_FOURTH_STREAM_WIDTH, prop_val, DEFAULT_FOURTH_STREAM_WIDTH);
  stream.width = atoi(prop_val);

  property_get(PROP_FOURTH_STREAM_HEIGHT, prop_val,
               DEFAULT_FOURTH_STREAM_HEIGHT);
  stream.height = atoi(prop_val);

  property_get(PROP_FOURTH_STREAM_FPS, prop_val, DEFAULT_FOURTH_STREAM_FPS);
  stream.fps = atof(prop_val);

  property_get(PROP_FOURTH_STREAM_SOURCE_ID, prop_val, "0");
  stream.source_stream_id = atoi(prop_val);

  property_get(PROP_FOURTH_STREAM_FORMAT, prop_val,
               DEFAULT_FOURTH_STREAM_FORMAT);
  SetVideoStreamFormat(prop_val, stream.format);

  // Insert Fourth stream into Map. Taking stream ID as 4.
  stream_info_map_.emplace(kFourthStreamID, stream);

  // Read Fifth Video Stream Params
  property_get(PROP_FIFTH_STREAM_WIDTH, prop_val, DEFAULT_FIFTH_STREAM_WIDTH);
  stream.width = atoi(prop_val);

  property_get(PROP_FIFTH_STREAM_HEIGHT, prop_val,
               DEFAULT_FIFTH_STREAM_HEIGHT);
  stream.height = atoi(prop_val);

  property_get(PROP_FIFTH_STREAM_FPS, prop_val, DEFAULT_FIFTH_STREAM_FPS);
  stream.fps = atof(prop_val);

  property_get(PROP_FIFTH_STREAM_SOURCE_ID, prop_val, "0");
  stream.source_stream_id = atoi(prop_val);

  property_get(PROP_FIFTH_STREAM_FORMAT, prop_val,
               DEFAULT_FIFTH_STREAM_FORMAT);
  SetVideoStreamFormat(prop_val, stream.format);

  // Insert Fifth stream into Map. Taking stream ID as 5.
  stream_info_map_.emplace(kFifthStreamID, stream);

  // Read JPEG Snapshot Stream
  property_get(PROP_SNAPSHOT_STREAM_WIDTH, prop_val,
               DEFAULT_SNAPSHOT_STREAM_WIDTH);
  snap_width_ = atoi(prop_val);

  property_get(PROP_SNAPSHOT_STREAM_HEIGHT, prop_val,
               DEFAULT_SNAPSHOT_STREAM_HEIGHT);
  snap_height_ = atoi(prop_val);

  property_get(PROP_SNAPSHOT_STREAM_FORMAT, prop_val,
               DEFAULT_SNAPSHOT_STREAM_FORMAT);
  SetSnapShotStreamFormat(prop_val);

  property_get(PROP_NUM_SNAPSHOT, prop_val,
               DEFAULT_SNAPSHOT_COUNT);
  snap_count_ = atoi(prop_val);

  property_get(PROP_SNAPSHOT_MODE, prop_val,
               DEFAULT_PROP_SNAPSHOT_MODE);
  SetSnapshotMode(prop_val);

#ifdef QCAMERA3_TAG_LOCAL_COPY
  vendor_tag_desc_ = nullptr;
#endif

  TEST_INFO("%s Exit ", __func__);
}

void GtestCommon::SetSnapshotMode(char prop[]) {
  std::string value = prop;
  if (value == "Video") {
    snap_mode_ = SnapshotMode::kVideo;
  } else if (value == "Still") {
    snap_mode_ = SnapshotMode::kStill;
  } else if (value == "StillPlusRaw") {
    snap_mode_ = SnapshotMode::kStillPlusRaw;
  } else if (value == "Continuous") {
    snap_mode_ = SnapshotMode::kContinuous;
  } else if (value == "Zsl") {
    snap_mode_ = SnapshotMode::kZsl;
  } else if (value == "VideoPlusRaw") {
    snap_mode_ = SnapshotMode::kVideoPlusRaw;
  }
}

std::string GtestCommon::GetSnapshotMode() {
  if (snap_mode_ == SnapshotMode::kVideo) {
    return "Video";
  } else if (snap_mode_ == SnapshotMode::kStill) {
    return "Still";
  } else if (snap_mode_ == SnapshotMode::kStillPlusRaw) {
    return "StillPlusRaw";
  } else if (snap_mode_ == SnapshotMode::kContinuous) {
    return "Continuous";
  } else if (snap_mode_ == SnapshotMode::kZsl) {
    return "ZSL";
  } else if (snap_mode_ == SnapshotMode::kVideoPlusRaw) {
    return "VideoPlusRaw";
  } else {
    return "Invalid Mode";
  }
}

void GtestCommon::SetSnapShotStreamFormat(char prop[]) {
  std::string value = prop;
  if (value == "JPEG") {
    snap_format_ = ImageFormat::kJPEG;
  } else if (value == "NV12") {
    snap_format_ = ImageFormat::kNV12;
  } else if (value == "NV21") {
    snap_format_ = ImageFormat::kNV21;
  } else if (value == "RAW8") {
    snap_format_ = ImageFormat::kBayerRDI8BIT;
  } else if (value == "RAW10") {
    snap_format_ = ImageFormat::kBayerRDI10BIT;
  } else if (value == "RAW12") {
    snap_format_ = ImageFormat::kBayerRDI12BIT;
  } else if (value == "RAW16") {
    snap_format_ = ImageFormat::kBayerRDI16BIT;
  }
}

void GtestCommon::SetVideoStreamFormat(char prop[], VideoFormat &format) {
  std::string value = prop;
  if (value == "AVC") {
    format = VideoFormat::kAVC;
  } else if (value == "HEVC") {
    format = VideoFormat::kHEVC;
  } else if (value == "YUV") {
    format = VideoFormat::kNV12;
  } else if (value == "RGB") {
    format = VideoFormat::kRGB;
  } else if (value == "RAW8") {
    format = VideoFormat::kBayerRDI8BIT;
  } else if (value == "RAW10") {
    format = VideoFormat::kBayerRDI10BIT;
  } else if (value == "RAW`12") {
    format = VideoFormat::kBayerRDI12BIT;
  }
}

void GtestCommon::PrintStreamInfo(uint32_t num) {

  std::cout << "\n############################################################"
      << std::endl;
  for (uint32_t i = kFirstStreamID; i <= num; i++) {
    auto stream = stream_info_map_[i];
    std::cout << "Video Stream Info:" << i << " Width:"
        << stream.width << " Height:" << stream.height << " FPS:"
        << stream.fps << " Source Stream ID:" << stream.source_stream_id
        << " Format: " << GetVideoStreamFormat(stream.format) << std::endl;
  }
  if (is_snap_stream_on_) {
    std::cout << "Snapshot Stream Info:" << " Width:" << snap_width_
        << " Height:" << snap_height_ << " Format:"
        << GetSnapshotStreamFormat() << " Mode:" <<
        GetSnapshotMode() << std::endl;
  }
}

std::string GtestCommon::GetVideoStreamFormat(VideoFormat &fmt) {
  if (fmt == VideoFormat::kAVC) {
    return "AVC";
  } else if (fmt == VideoFormat::kHEVC) {
    return "HEVC";
  } else if (fmt == VideoFormat::kNV12) {
    return "YUV";
  } else if (fmt == VideoFormat::kRGB) {
    return "RGB";
  } else if (fmt == VideoFormat::kRGB) {
    return "RGB";
  } else if (fmt == VideoFormat::kBayerRDI8BIT) {
    return "RAW8";
  } else if (fmt == VideoFormat::kBayerRDI10BIT) {
    return "RAW10";
  } else if (fmt == VideoFormat::kBayerRDI12BIT) {
    return "RAW12";
  } else if (fmt == VideoFormat::kBayerIdeal) {
    return "RAWIDEAL";
  } else {
    return "Invalid Video Format";
  }
}

std::string GtestCommon::GetSnapshotStreamFormat() {
  if (snap_format_ == ImageFormat::kJPEG) {
    return "JPEG";
  } else if (snap_format_ == ImageFormat::kNV12) {
    return "NV12";
  } else if (snap_format_ == ImageFormat::kNV21) {
    return "NV21";
  } else if (snap_format_ == ImageFormat::kBayerRDI8BIT) {
    return "RAW8";
  } else if (snap_format_ == ImageFormat::kBayerRDI10BIT) {
    return "RAW10";
  } else if (snap_format_ == ImageFormat::kBayerRDI12BIT) {
    return "RAW12";
  } else if (snap_format_ == ImageFormat::kBayerRDI16BIT) {
    return "RAW16";
  } else {
    return "Invalid Snapshot Format";
  }
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
#ifndef CAMERA_HAL1_SUPPORT
  camera_metadata_entry_t entry;
  if (static_info_.exists(QCAMERA3_AVAILABLE_VIDEO_HDR_MODES)) {
    entry = static_info_.find(QCAMERA3_AVAILABLE_VIDEO_HDR_MODES);
    for (uint32_t i = 0 ; i < entry.count; i++) {
      supported_hdr_modes_.push_back(entry.data.i32[i]);
    }
  }
#endif
}

bool GtestCommon::IsVHDRSupported() {
  bool is_supported = false;
#ifndef CAMERA_HAL1_SUPPORT
  for (const auto& mode : supported_hdr_modes_) {
    if (QCAMERA3_VIDEO_HDR_MODE_ON == mode) {
      is_supported = true;
      break;
    }
  }
#endif
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

void GtestCommon::RecorderCallbackHandler(EventType type, void *payload,
                                          size_t size) {
  TEST_INFO("%s Enter event: %d ", __func__, (int32_t) type);
  if (type == EventType::kCameraError && size && payload != nullptr) {
    ASSERT_TRUE(size == sizeof(uint32_t));
    auto camera_id = *(static_cast<uint32_t*>(payload));
    test_wait_.Done();
    std::lock_guard<std::mutex> lock(error_lock_);
    camera_error_ = true;
  } else if (type == EventType::kCameraOpened && size && payload != nullptr) {
    ASSERT_TRUE(size == sizeof(uint32_t));
    auto camera_id = *(static_cast<uint32_t*>(payload));
    std::lock_guard<std::mutex> lk(camera_state_lock_);
    camera_state_[camera_id] = GtestCameraState::kOpened;
    camera_state_updated_.notify_all();
  } else if (type == EventType::kCameraClosing && size && payload != nullptr) {
    ASSERT_TRUE(size == sizeof(uint32_t));
    auto camera_id = *(static_cast<uint32_t*>(payload));
    std::lock_guard<std::mutex> lk(camera_state_lock_);
    camera_state_[camera_id] = GtestCameraState::kClosing;
    camera_state_updated_.notify_all();
  } else if (type == EventType::kCameraClosed && size && payload != nullptr) {
    ASSERT_TRUE(size == sizeof(uint32_t));
    auto camera_id = *(static_cast<uint32_t*>(payload));
    std::lock_guard<std::mutex> lk(camera_state_lock_);
    camera_state_[camera_id] = GtestCameraState::kClosed;
    camera_state_updated_.notify_all();
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

  if (enable_sof_latency_) {
    struct timespec time;
    clock_gettime(CLOCK_BOOTTIME, &time);
    auto current_time_ms = time.tv_sec * 1000 + (time.tv_nsec / 1000000);
    auto buf_time_ms = buffers[0].timestamp / 1000000;
    auto latency = current_time_ms - buf_time_ms;
    TEST_INFO("%s: SOF Latency: %llu ms\n", __func__,
        latency);
  }

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

void GtestCommon::VideoTrackRawDataCb(uint32_t session_id, uint32_t track_id,
                                      std::vector<BufferDescriptor> buffers,
                                      std::vector<MetaData> meta_buffers) {
  TEST_DBG("%s: Enter track_id: %d", __func__, track_id);
  if (is_dump_raw_enabled_) {
    static uint32_t fcounter = 0;
    ++fcounter;

    if (fcounter == dump_yuv_freq_) {
      std::string file_path("/data/misc/qmmf/gtest_track_");
      std::string ext_str;
      file_path += std::to_string(track_id) + "_";
      file_path += std::to_string(buffers[0].timestamp);
      file_path += ".";
      switch (meta_buffers[0].cam_buffer_meta_data.format) {
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
          ext_str = "raw";
          break;
      }
      file_path += ext_str;
      streampos before, after;
      ofstream out_file(file_path.c_str(), ios::out | ios::binary |
                        ios::trunc);
      if (!out_file.is_open()) {
        TEST_DBG("%s: error opening file[%s]", __func__, filename.c_str());
        goto FAIL;
      }

      before = out_file.tellp();
      out_file.write(reinterpret_cast<const char *>(buffers[0].data),
                     buffers[0].size);
      after = out_file.tellp();
      if (buffers[0].size != (after - before)) {
        TEST_ERROR("%s: Bad Write error (%d):(%s)\n", __func__, errno,
                   strerror(errno));
        goto FAIL;
      }
      TEST_INFO("%s: Buffer(0x%p) Size(%u) Stored@(%s)\n", __func__,
                buffers[0].data, (after - before), file_path.c_str());

    FAIL:
      if (out_file.is_open()) {
        out_file.close();
      }

      fcounter = 0;
    }
  }
  auto ret = recorder_.ReturnTrackBuffer(session_id, track_id, buffers);
  ASSERT_TRUE(ret == NO_ERROR);

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

void GtestCommon::VideoTrackRGBDataCb(uint32_t session_id, uint32_t track_id,
                                      std::vector<BufferDescriptor> buffers,
                                      std::vector<MetaData> meta_buffers) {
  TEST_DBG("%s: Enter track_id: %d", __func__, track_id);
  if (is_dump_raw_enabled_) {
    static uint32_t fcounter = 0;
    ++fcounter;

    if (fcounter == dump_yuv_freq_) {
      std::string file_path("/data/misc/qmmf/gtest_track_");
      size_t written_len;
      file_path += std::to_string(track_id) + "_";
      file_path += std::to_string(buffers[0].timestamp);
      file_path += ".rgb";
      FILE *file = fopen(file_path.c_str(), "w+");
      if (!file) {
        ALOGE("%s: Unable to open file(%s)", __func__,
            file_path.c_str());
        goto FAIL;
      }

      written_len = fwrite(buffers[0].data, sizeof(uint8_t),
                           buffers[0].size, file);
      TEST_INFO("%s: written_len =%d", __func__, written_len);
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
      fcounter = 0;
    }
  }

  auto ret = recorder_.ReturnTrackBuffer(session_id, track_id, buffers);
  ASSERT_TRUE(ret == NO_ERROR);

  TEST_DBG("%s: Exit", __func__);
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
      TEST_ERROR("%s: Unsupported format type: %d", __func__,
        (int32_t) format_type);
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
#ifdef __LIBGBM__
      if (GBM_FORMAT_IMPLEMENTATION_DEFINED == entry.data.i32[i]) {
#else
      if (HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED == entry.data.i32[i]) {
#endif
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
#ifdef __LIBGBM__
      if (GBM_FORMAT_IMPLEMENTATION_DEFINED == entry.data.i32[i] &&
          ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT ==
            entry.data.i32[i+3]) {
#else
      if (HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED == entry.data.i32[i] &&
          ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT ==
            entry.data.i32[i+3]) {
#endif
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
#ifdef __LIBGBM__
      if (GBM_FORMAT_BLOB == entry.data.i32[i]) {
#else
      if (HAL_PIXEL_FORMAT_BLOB == entry.data.i32[i]) {
#endif
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
#ifdef __LIBGBM__
      if (GBM_FORMAT_RAW10 == entry.data.i32[i]) {
#else
      if (HAL_PIXEL_FORMAT_RAW10 == entry.data.i32[i]) {
#endif
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
#ifdef __LIBGBM__
      if (GBM_FORMAT_RAW10 == entry.data.i32[i] &&
          ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT ==
            entry.data.i32[i+3]) {
#else
      if (HAL_PIXEL_FORMAT_RAW10 == entry.data.i32[i] &&
          ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT ==
            entry.data.i32[i+3]) {
#endif
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
#ifdef __LIBGBM__
  if (GBM_FORMAT_RAW10 == format || GBM_FORMAT_RAW16 == format) {
#else
  if (HAL_PIXEL_FORMAT_RAW8  == format || HAL_PIXEL_FORMAT_RAW10 == format ||
      HAL_PIXEL_FORMAT_RAW12 == format || HAL_PIXEL_FORMAT_RAW16 == format) {
#endif
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

status_t GtestCommon::SetCameraZoom(const float zoom) {
  CameraMetadata meta;
  auto ret = recorder_.GetCameraParam(camera_id_, meta);
  EXPECT_TRUE(ret == NO_ERROR);

  if (meta.exists(ANDROID_SENSOR_INFO_ACTIVE_ARRAY_SIZE)) {
    auto active_array_size = meta.find(ANDROID_SENSOR_INFO_ACTIVE_ARRAY_SIZE);
    EXPECT_TRUE(active_array_size.count > 0);

    int32_t width = active_array_size.data.i32[2];
    int32_t height = active_array_size.data.i32[3];

    int32_t crop[4];
    crop[2] = static_cast<int32_t>(width / zoom);
    crop[3] = (crop[2] * height / width);
    crop[0] = (width - crop[2]) / 2;
    crop[1] = (height - crop[3]) / 2;

    TEST_INFO("%s: zoom: %.2f crop[0]=%d, crop[1]=%d, crop[2]=%d, crop[3]=%d",
        __func__, zoom, crop[0], crop[1], crop[2], crop[3]);

    auto ret = meta.update(ANDROID_SCALER_CROP_REGION, crop, 4);
    EXPECT_TRUE(ret == NO_ERROR);

    ret = recorder_.SetCameraParam(camera_id_, meta);
    EXPECT_TRUE(ret == NO_ERROR);
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
* ListFilesFromDir: Method to list all the file names present in dir_path
* starting with name_starts_with and ending with extension.
*/
status_t GtestCommon::ListFilesFromDir(std::string dir_path,
                                       std::string name_starts_with,
                                       std::string extension,
                                       std::vector<std::string> &files_list) {
  DIR *dp;
  struct dirent *dirent;
  if (dir_path.empty()) {
    TEST_ERROR("%s: Empty path for directory", __func__);
    return -EINVAL;
  }
  dp = opendir(dir_path.c_str());
  if (nullptr == dp) {
    QMMF_ERROR("%s: Failed to open %s folder", __func__, dir_path.c_str());
    return -EPERM;
  }
  size_t file_init_len = strlen(name_starts_with.c_str());
  size_t ext_len = strlen(extension.c_str());
  while ((dirent = readdir(dp)) != nullptr) {
    std::string fileName = std::string(dirent->d_name);
    if ((fileName.length() > file_init_len) &&
        (name_starts_with.compare(0, file_init_len,
                                  fileName.substr(0, file_init_len)) == 0) &&
        (extension.compare(0, ext_len,
                           fileName.substr(fileName.size() - ext_len)) == 0)) {
      TEST_INFO("%s: File found %s", __func__, fileName.c_str());
      files_list.push_back(fileName);
    }
  }
  return NO_ERROR;
}

/*
* PopulateDeFogTable: This method populates the DeFog Tables from files.
*/
status_t GtestCommon::PopulateDeFogTables(
    std::vector<DeFogTable> &defog_tables) {
  std::string dir_path(kQmmfFolderPath);
  std::vector<std::string> files_list;
  ListFilesFromDir(dir_path, "defog_table", ".txt", files_list);

  for (auto fileName:files_list) {
    std::string path = dir_path + fileName;
    FILE *fp;
    if (!(fp = fopen(path.c_str(), "r"))) {
      TEST_ERROR("%s: failed to open defog table file: %s", __func__,
                 path.c_str());
      return -EINVAL;
    } else {
      TEST_INFO("%s: Opening defog table file: %s", __func__, path.c_str());
    }

    DeFogTable defog_table{};
    std::string input_str;
    const char delim_colon = ':', delim_space = ' ';
    std::string key, value;
    uint8_t index = 0;
    std::ifstream input_file(path.c_str());
    std::vector<std::string> out, out_values;

    while (getline(input_file, input_str)) {
      out.clear();
      TokenizeString(input_str, delim_colon, out);
      key = out[0];
      value = out[1];
      RemoveSpaces(key);
      if (key.compare("enable") == 0) {
        defog_table.enable = std::atoi(value.c_str());
      } else if (key.compare("algo_type") == 0) {
        defog_table.algo_type = std::atoi(value.c_str());
      } else if (key.compare("algo_decision_mode") == 0) {
        defog_table.algo_decision_mode = std::atoi(value.c_str());
      } else if (key.compare("strength") == 0) {
        defog_table.strength = std::atoi(value.c_str());
      } else if (key.compare("convergence_speed") == 0) {
        defog_table.convergence_speed = std::atoi(value.c_str());
      } else if (key.compare("lp_color_comp_gain") == 0) {
        defog_table.lp_color_comp_gain = std::atof(value.c_str());
      } else if (key.compare("abc_en") == 0) {
        defog_table.abc_en = std::atoi(value.c_str());
      } else if (key.compare("acc_en") == 0) {
        defog_table.acc_en = std::atoi(value.c_str());
      } else if (key.compare("afsd_en") == 0) {
        defog_table.afsd_en = std::atoi(value.c_str());
      } else if (key.compare("afsd_2a_en") == 0) {
        defog_table.afsd_2a_en = std::atoi(value.c_str());
      } else if (key.compare("defog_dark_thres") == 0) {
        defog_table.defog_dark_thres = std::atoi(value.c_str());
      } else if (key.compare("defog_bright_thres") == 0) {
        defog_table.defog_bright_thres = std::atoi(value.c_str());
      } else if (key.compare("abc_gain") == 0) {
        defog_table.abc_gain = std::atof(value.c_str());
      } else if (key.compare("acc_max_dark_str") == 0) {
        defog_table.acc_max_dark_str = std::atof(value.c_str());
      } else if (key.compare("acc_max_bright_str") == 0) {
        defog_table.acc_max_bright_str = std::atof(value.c_str());
      } else if (key.compare("dark_limit") == 0) {
        defog_table.dark_limit = std::atoi(value.c_str());
      } else if (key.compare("bright_limit") == 0) {
        defog_table.bright_limit = std::atoi(value.c_str());
      } else if (key.compare("dark_preserve") == 0) {
        defog_table.dark_preserve = std::atoi(value.c_str());
      } else if (key.compare("bright_preserve") == 0) {
        defog_table.bright_preserve = std::atoi(value.c_str());
      } else if (key.compare("dnr_trigger") == 0) {
        out_values.clear();
        TokenizeString(value, delim_space, out_values);
        for (index = 0; out_values.size() <= 9 &&
             index + 2 < out_values.size(); index++) {
          defog_table.trig_params.dnr_trigger[index/3].start =
            std::atof(out_values[index].c_str());
          ++index;
          defog_table.trig_params.dnr_trigger[index/3].end =
            std::atof(out_values[index].c_str());
          ++index;
          defog_table.trig_params.dnr_trigger[index/3].fog_p =
            std::atoi(out_values[index].c_str());
        }
      } else if (key.compare("lux_trigger") == 0) {
        out_values.clear();
        TokenizeString(value, delim_space, out_values);
        for (index = 0; out_values.size() <= 9 &&
             index + 2 < out_values.size(); index++) {
          defog_table.trig_params.lux_trigger[index/3].start =
            std::atof(out_values[index].c_str());
          ++index;
          defog_table.trig_params.lux_trigger[index/3].end =
            std::atof(out_values[index].c_str());
          ++index;
          defog_table.trig_params.lux_trigger[index/3].fog_p =
            std::atoi(out_values[index].c_str());

        }
      } else if (key.compare("cct_trigger") == 0) {
        out_values.clear();
        TokenizeString(value, delim_space, out_values);
        for (index = 0; out_values.size() <= 12 &&
             index + 2 < out_values.size(); index++) {
            defog_table.trig_params.cct_trigger[index/3].start =
              std::atof(out_values[index].c_str());
            ++index;
            defog_table.trig_params.cct_trigger[index/3].end =
              std::atof(out_values[index].c_str());
            ++index;
            defog_table.trig_params.cct_trigger[index/3].fog_p =
              std::atoi(out_values[index].c_str());
        }
      } else {
        TEST_ERROR("%s: Invalid field %s\n", __func__, key.c_str());
        return -EINVAL;
      }
    }
    defog_tables.push_back(defog_table);
  }
  return NO_ERROR;
}

/*
* PopulateExpTable: This method populates the Exposure Tables from files.
*/
status_t GtestCommon::PopulateExpTables(
    std::vector<ExposureTable> &exp_tables) {
  std::string dir_path(kQmmfFolderPath);
  std::vector<std::string> files_list;
  ListFilesFromDir(dir_path, "exposure_table", ".txt", files_list);

  for (auto fileName : files_list) {
    std::string path = dir_path.append(fileName);
    FILE *fp;
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
        __func__, (int32_t) meta_data.format);
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

  CameraMetadata static_meta;
  auto ret = recorder_.GetCameraCharacteristics(camera_id_, static_meta);
  if (NO_ERROR != ret) {
    TEST_ERROR("%s: GetCameraCharacteristics failed!", __func__);
    return ret;
  }
  auto active_array_size = static_meta.find(ANDROID_SENSOR_INFO_ACTIVE_ARRAY_SIZE);
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
  ret = meta.update(ANDROID_SCALER_CROP_REGION, crop_region, 4);
  if (NO_ERROR != ret) {
    TEST_ERROR("%s: Failed to set crop region metadata!", __func__);
    return ret;
  }

  return NO_ERROR;
}

void GtestCommon::ConfigureImageParam() {

  bool res_supported = false;
  CameraMetadata static_meta;

  auto ret = recorder_.GetCameraCharacteristics(camera_id_, static_meta);
  ASSERT_TRUE(ret == NO_ERROR);

  // Configure Snapshot Mode And Image Param
  ImageConfigParam image_config;
  SnapshotType snapshot_type;
  snapshot_type.type = snap_mode_;
  snapshot_type.raw_format = snap_format_;
  image_config.Update(QMMF_SNAPSHOT_TYPE, snapshot_type);

  ImageParam image_param{};
  image_param.image_format = snap_format_;

  if (snap_format_ == ImageFormat::kJPEG) {
    image_param.width = snap_width_;
    image_param.height = snap_height_;
    image_param.image_quality = default_jpeg_quality_;

    res_supported = GtestCommon::ValidateResFromJpegSizes(
        static_meta, image_param.width, image_param.height);
    ASSERT_TRUE(res_supported != false);
  } else if (snap_format_ == ImageFormat::kBayerRDI8BIT ||
             snap_format_ == ImageFormat::kBayerRDI10BIT ||
             snap_format_ == ImageFormat::kBayerRDI12BIT ||
             snap_format_ == ImageFormat::kBayerRDI16BIT) {
    // Configure max resolution for Bayer Snapshot.
    GtestCommon::GetMaxSupportedCameraRes(static_meta, image_param.width,
                                          image_param.height);

    TEST_INFO("%s: Supported Max Capture W(%d):H(%d)", __func__,
              image_param.width, image_param.height);
    ASSERT_TRUE(image_param.width > 0 && image_param.height > 0);

    if (snap_mode_ == SnapshotMode::kStillPlusRaw || snap_mode_
        == SnapshotMode::kVideoPlusRaw) {
      image_param.image_format = ImageFormat::kJPEG;
      image_param.width = snap_width_;
      image_param.height = snap_height_;
      image_param.image_quality = default_jpeg_quality_;
    }
  } else if (snap_format_ == ImageFormat::kNV12 ||
             snap_format_ == ImageFormat::kNV21) {
    image_param.width = snap_width_;
    image_param.height = snap_height_;
    res_supported = GtestCommon::ValidateResFromStreamConfigs(
        static_meta, image_param.width, image_param.height);
    ASSERT_TRUE(res_supported != false);
  }

  ret = recorder_.ConfigImageCapture(camera_id_, image_param, image_config);
  ASSERT_TRUE(ret == NO_ERROR);
}

void GtestCommon::TakeSnapshot() {
  std::vector < CameraMetadata > meta_array;
  CameraMetadata meta;

  auto ret = recorder_.GetDefaultCaptureParam(camera_id_, meta);
  ASSERT_TRUE(ret == NO_ERROR);

  meta_array.push_back(meta);

  ImageCaptureCb cb = [&](uint32_t camera_id, uint32_t image_count,
                          BufferDescriptor buffer,
                          MetaData meta_data) -> void {
      SnapshotCb(camera_id, image_count, buffer, meta_data);
  };

  for (uint32_t i = 0; i < snap_count_; i++) {

    ret = recorder_.CaptureImage(camera_id_, 1, meta_array, cb);
    ASSERT_TRUE(ret == NO_ERROR);

    if (snap_mode_ == SnapshotMode::kContinuous) {
      // Continuous Snapshot requires only one call to CaptureImage()
      sleep(record_duration_ / 2);
      break;
    }
    // Take Snapshot with every 5 sec.
    sleep(5);
  }

  ret = recorder_.CancelCaptureImage(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  sleep(1);
}

void GtestCommon::SetCameraExtraParam(CameraExtraParam &param) {
  if (is_eis_on_) {
    // Enable EIS
    EISSetup eis_mode;
    eis_mode.enable = true;
    param.Update(QMMF_EIS, eis_mode);
  }
  if (is_shdr_on_) {
    // Enable HDR
    VideoHDRMode vid_hdr_mode;
    vid_hdr_mode.enable = true;
    param.Update(QMMF_VIDEO_HDR_MODE, vid_hdr_mode);
  }
  if (is_ldc_on_) {
    // Enable LDc
    LDCMode ldc_mode;
    ldc_mode.enable = true;
    param.Update(QMMF_LDC, ldc_mode);
  }

  std::cout << "EIS is :" << (is_eis_on_ ? "On" : "Off") << " SHDR is :"
      << (is_shdr_on_ ? "On" : "Off") << " LDC is :" <<
      (is_ldc_on_ ? "On" : "Off") << std::endl;
}
