/*
 * Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
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

#define LOG_TAG "RecorderJpeg"

#include <sys/mman.h>
#include <sstream>
#include <json/json.h>

#include "qmmf_jpeg.h"

namespace qmmf {

namespace recorder {

const int32_t PostProcJpeg::kMetaHistory = 30; //frames

const int32_t PostProcJpeg::kWaitJPEGTimeout = 100000000; // 100 ms

const uint32_t PostProcJpeg::kMinWidth  = 160;
const uint32_t PostProcJpeg::kMinHeight = 120;
const uint32_t PostProcJpeg::kMaxWidth  = 5104;
const uint32_t PostProcJpeg::kMaxHeight = 4092;

const int32_t PostProcJpeg::kSupportedInputFormat = HAL_PIXEL_FORMAT_YCbCr_420_888;
const int32_t PostProcJpeg::kSupportedOutputFormat = HAL_PIXEL_FORMAT_BLOB;

PostProcJpeg::PostProcJpeg()
    : jpeg_encoder_(nullptr),
      state_(State::CREATED),
      abort_(nullptr) {
  QMMF_VERBOSE("%s: Enter", __func__);
  jpeg_encoder_ = reprocjpegencoder::JpegEncoder::getInstance();
  jpeg_params_.image_quality = 95;
  results_.clear();
  f_id_.clear();
  QMMF_VERBOSE("%s: Exit (0x%p)", __func__, this);
}

PostProcJpeg::~PostProcJpeg() {
  QMMF_VERBOSE("%s: Enter ", __func__);
  reprocjpegencoder::JpegEncoder::releaseInstance();
  jpeg_encoder_ = nullptr;
  QMMF_VERBOSE("%s: Exit (0x%p)", __func__, this);
}

status_t PostProcJpeg::Initialize(const PostProcIOParam &in_param,
                                  const PostProcIOParam &out_param) {
  QMMF_VERBOSE("%s: Enter", __func__);

  std::lock_guard<std::mutex> lock(state_lock_);
  state_ = State::INITIALIZED;
  image_width_ = out_param.width;
  image_height_ = out_param.height;
  jpeg_params_.thumbnail_data.clear();

  return NO_ERROR;
}

PostProcIOParam PostProcJpeg::GetInput(const PostProcIOParam &out) {
  PostProcIOParam input_param = out;

  // set number of needed buffers for rotation if client does not limit it
  if (out.buffer_max > 0 && out.buffer_max < kBufCount) {
    input_param.buffer_count = out.buffer_max;
  } else {
    input_param.buffer_count = kBufCount;
  }
  if (out.internal_format != BufferFormat::kUnsupported) {
    input_param.format = out.internal_format;
  } else {
    input_param.format = Common::FromHalToQmmfFormat(kSupportedInputFormat);
  }
  return input_param;
}

status_t PostProcJpeg::ValidateOutput(const PostProcIOParam &output) {
  if (output.format != Common::FromHalToQmmfFormat(kSupportedOutputFormat)) {
    QMMF_ERROR("%s: Output format(%d) not supported", __func__, output.format);
    return BAD_TYPE;
  }

  if ((output.width < kMinWidth || output.width > kMaxWidth) ||
      (output.height < kMinHeight || output.height > kMaxHeight)) {
    QMMF_ERROR("%s: Output dimensions(%dx%d) not supported", __func__,
        output.width, output.height);
    return BAD_VALUE;
  }

  return NO_ERROR;
}

status_t PostProcJpeg::GetCapabilities(PostProcCaps &caps) {
  caps.output_buff_        = 1;
  caps.min_width_          = kMinWidth;
  caps.min_height_         = kMinHeight;
  caps.max_width_          = kMaxWidth;
  caps.max_height_         = kMaxHeight;
  caps.crop_support_       = false;
  caps.scale_support_      = false;
  caps.inplace_processing_ = false;
  caps.usage_              = 0;

  caps.formats_.insert(BufferFormat::kBLOB);

  return NO_ERROR;
}

status_t PostProcJpeg::Start() {
  QMMF_VERBOSE("%s: Enter %p", __func__, this);

  auto ret = jpeg_encoder_->Init(image_width_, image_height_);
  if (ret != 0) {
    QMMF_ERROR("%s: failed to inint Jpeg Encoder", __func__);
    return BAD_VALUE;
  }

  std::lock_guard<std::mutex> lock(state_lock_);
  state_ = State::ACTIVE;
  return NO_ERROR;
}

status_t PostProcJpeg::Stop() {
  QMMF_INFO("%s: Enter %p", __func__, this);

  auto ret = jpeg_encoder_->DeInit();
  if (ret != 0) {
    QMMF_ERROR("%s: failed to inint Jpeg Encoder", __func__);
    return BAD_VALUE;
  }

  std::lock_guard<std::mutex> lock(state_lock_);
  state_ = State::INITIALIZED;
  return NO_ERROR;
}

status_t PostProcJpeg::Abort(std::shared_ptr<void> &abort) {
  QMMF_INFO("%s: Enter %p", __func__, this);
  std::lock_guard<std::mutex> lock(state_lock_);
  if (state_ == State::RUNING) {
    QMMF_VERBOSE("%s: Acquire abort done handler", __func__);
    abort_ = abort;
  }
  state_ = State::ABORTED;
  return NO_ERROR;
}

status_t PostProcJpeg::Delete() {
  QMMF_VERBOSE("%s: Enter %p", __func__, this);
  std::lock_guard<std::mutex> lock(state_lock_);
  state_ = State::CREATED;
  return NO_ERROR;
}

status_t PostProcJpeg::Configure(const std::string config_json_data) {
  QMMF_VERBOSE("%s: Enter %p", __func__, this);
  Json::Reader r;
  Json::Value root;

  auto ret = r.parse(config_json_data, root);
  if (ret == 0) {
    QMMF_INFO("%s: no json data", __func__);
    return NO_ERROR;
  }

  if (!root.isMember("jpeg quality") || root["jpeg quality"].empty()) {
    QMMF_INFO("%s:no jpeg quality configuration", __func__);
  } else {
    jpeg_params_.image_quality = root["jpeg quality"].asUInt();
  }

  if (!root.isMember("thumbnail") || root["thumbnail"].empty()) {
    QMMF_INFO("%s:no thumbnail configuration", __func__);
  } else {
    jpeg_params_.thumbnail_data.clear();
    for (Json::Value::ArrayIndex i = 0; i < root["thumbnail"].size(); i++) {
      QMMF_INFO("%s:add thumbnail[%d] dim %dx%d quality %d", __func__, i,
          root["thumbnail"][i]["width"].asUInt(),
          root["thumbnail"][i]["height"].asUInt(),
          root["thumbnail"][i]["quality"].asUInt());

      jpeg_params_.thumbnail_data.emplace_back(
          root["thumbnail"][i]["width"].asUInt(),
          root["thumbnail"][i]["height"].asUInt(),
          root["thumbnail"][i]["quality"].asUInt());
    }
  }

  if (!root.isMember("maker note") || root["maker note"].empty()) {
    QMMF_INFO("%s:no maker note configuration", __func__);
    jpeg_params_.disable_maker_note = false;
  } else {
    jpeg_params_.disable_maker_note = !root["maker note"].asUInt();
    QMMF_INFO("%s:maker note flag: %d", __func__,
        jpeg_params_.disable_maker_note);
  }

  QMMF_VERBOSE("%s: Exit %p", __func__, this);

  return NO_ERROR;
}

void PostProcJpeg::AddResult(const void* result) {

  CameraMetadata meta = *(reinterpret_cast<const CameraMetadata *>(result));

  if (meta.exists(ANDROID_CONTROL_CAPTURE_INTENT)) {
    auto capture_intent = meta.find(ANDROID_CONTROL_CAPTURE_INTENT).data.u8[0];
    if ( !((capture_intent == ANDROID_CONTROL_CAPTURE_INTENT_STILL_CAPTURE) ||
         (capture_intent == ANDROID_CONTROL_CAPTURE_INTENT_VIDEO_SNAPSHOT)) ) {
      QMMF_INFO("%s Metadata is not related to a still capture!",
          __func__);
      return;
    }
  }

  if (!meta.exists(ANDROID_REQUEST_FRAME_COUNT)) {
    QMMF_ERROR("%s Sensor frame count tag missing in result!", __func__);
    return;
  }

  uint32_t  meta_frame_number =
        meta.find(ANDROID_REQUEST_FRAME_COUNT).data.i32[0];

  std::unique_lock<std::mutex> lock(result_lock_);
  if (results_.size() > kMetaHistory) {
    results_.erase(results_.begin());
  }

  if (f_id_.size() > kMetaHistory) {
    f_id_.erase(f_id_.begin());
  }

  results_.emplace(meta_frame_number, meta);
  f_id_.emplace_back(meta_frame_number);

  wait_for_result_.SignalAll();
}

status_t PostProcJpeg::Process(const std::vector<StreamBuffer> &in_buffers,
                               const std::vector<StreamBuffer> &out_buffers) {
  StreamBuffer in_buffer = in_buffers.front();
  StreamBuffer out_buffer = out_buffers.front();

  QMMF_VERBOSE("%s: %d: Enter in FD: %d out FD: %d ",
      __func__, __LINE__, in_buffer.fd, out_buffer.fd);

  {
    std::lock_guard<std::mutex> lock(state_lock_);
    if (state_ != State::ACTIVE) {
      listener_->OnFrameReturn(out_buffer);
      listener_->OnFrameProcessed(in_buffer);
      return NO_ERROR;
    }
    state_ = State::RUNING;
  }

  CameraMetadata meta;
  {
    std::unique_lock<std::mutex> lock(result_lock_);
    while (results_.count(in_buffer.frame_number) == 0) {
      std::chrono::nanoseconds timeout(kWaitJPEGTimeout);
      auto ret = wait_for_result_.WaitFor(lock, timeout);
      if (ret != 0) {
        QMMF_ERROR("%s: Wait for jpeg result timed out", __func__);
        break;
      }
    }
    try {
      meta = results_.at(in_buffer.frame_number);
      if (!f_id_.empty()) {
        auto it = f_id_.begin();
        auto end = f_id_.end();
        while (it != end) {
          if (*it == in_buffer.frame_number) {
            break;
          }
          results_.erase(*it);
          f_id_.erase(it++);
        }
      }
    } catch (const std::out_of_range& oor) {
        QMMF_ERROR("%s: Result not exist: %s", __func__, oor.what());
    }
  }

  if (!meta.isEmpty()) {
    //todo create EXIF
    QMMF_ERROR("%s: todo create EXIF", __func__);
  }

  jpeg_params_.img_data[0] = static_cast<uint8_t*>(in_buffer.data);
  jpeg_params_.out_data[0] = static_cast<uint8_t*>(out_buffer.data);
  jpeg_params_.source_info = in_buffer.info;

  size_t jpeg_size;
  auto ret = jpeg_encoder_->Encode(jpeg_params_, jpeg_size);
  if (ret != 0) {
    QMMF_ERROR("%s: Jpeg Encode fails", __func__);
  }

  out_buffer.info.format = BufferFormat::kBLOB;
  out_buffer.info.num_planes = 1;
  out_buffer.info.plane_info[0].width = jpeg_size;
  out_buffer.info.plane_info[0].height = 1;
  out_buffer.second_thumb = (jpeg_params_.thumbnail_data.size() == 2);
  out_buffer.filled_length = jpeg_size;
  out_buffer.timestamp = in_buffer.timestamp;

  std::lock_guard<std::mutex> lock(state_lock_);
  if (state_ == State::ABORTED) {
    listener_->OnFrameReturn(out_buffer);
    listener_->OnFrameProcessed(in_buffer);

    QMMF_VERBOSE("%s: Release abort done handler", __func__);
    abort_ = nullptr;
  } else {
    listener_->OnFrameReady(out_buffer);
    listener_->OnFrameProcessed(in_buffer);
    state_ = State::ACTIVE;
  }

  QMMF_VERBOSE("%s: Exit", __func__);

  return NO_ERROR;
}

}; // namespace recoder

}; // namespace qmmf
