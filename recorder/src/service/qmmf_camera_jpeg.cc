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

#define LOG_TAG "RecorderCameraJpeg"

#include <chrono>
#include <algorithm>
#include <fcntl.h>
#include <sys/mman.h>
#include <cutils/properties.h>

#include "recorder/src/service/qmmf_recorder_utils.h"

#include "qmmf_camera_jpeg.h"

namespace qmmf {

namespace recorder {

static const char *kVendorNameProp = "persist.qmmf.jpeg.vendor.name";
static const char *kProductNameProp = "persist.qmmf.jpeg.product.name";

CameraJpeg::CameraJpeg()
    : ExifGenerator(),
      ExifConverter(),
      reprocess_flag_(false),
      ready_to_start_(false),
      jpeg_encoder_(nullptr) {

  QMMF_INFO("%s: Enter", __func__);
  char prop[PROPERTY_VALUE_MAX];

  property_get(kVendorNameProp, prop, "QTI");
  vendor_name_ = prop;

  property_get(kProductNameProp, prop, "QMMF-CAMERA");
  product_name_ = prop;

  QMMF_INFO("%s: Exit (0x%p)", __func__, this);
}

CameraJpeg::~CameraJpeg() {
  QMMF_INFO("%s: Enter ", __func__);
  Delete();
  QMMF_INFO("%s: Exit (0x%p)", __func__, this);
}

int32_t CameraJpeg::Create(const int32_t stream_id,
                           const PostProcParam& input,
                           const PostProcParam& output,
                           const uint32_t frame_rate,
                           const uint32_t num_images,
                           const uint32_t jpeg_quality,
                           const void* static_meta,
                           const PostProcCb& cb,
                           const void* context) {
  QMMF_INFO("%s: Enter", __func__);

  if (ready_to_start_) {
    QMMF_ERROR("%s: Failed: Already configured.", __func__);
    return BAD_VALUE;
  }

  if (reprocess_flag_) {
    QMMF_ERROR("%s: Failed: Wrong state.", __func__);
    return BAD_VALUE;
  }
  CameraBufferMetaData meta_info{};
  if (FillMetaInfo(input, &meta_info) != NO_ERROR) {
    return BAD_VALUE;
  }

  jpeg_encoder_ = JpegEncoder::getInstance();
  results_.clear();

  capture_client_cb_ = cb;
  input_stream_id_   = stream_id;
  num_images_        = num_images;
  jpeg_params_.image_quality = jpeg_quality;
  ready_to_start_    = true;

  Run("Camera Jpeg");

  QMMF_INFO("%s: Exit", __func__);
  return 55; //TODO use reprocess ID
}

status_t CameraJpeg::GetCapabilities(PostProcCaps *caps) {
  caps->output_buff_ = 1;
  return NO_ERROR;
}

status_t CameraJpeg::Start() {
  QMMF_INFO("%s: Enter", __func__);
  if (!ready_to_start_) {
    return BAD_VALUE;
  }
  reprocess_flag_ = true;

  QMMF_INFO("%s: Exit", __func__);
  return NO_ERROR;
}

status_t CameraJpeg::Stop() {
  return NO_ERROR;
}

status_t CameraJpeg::Delete() {
  QMMF_INFO("%s: Enter ", __func__);

  RequestExitAndWait();

  JpegEncoder::releaseInstance();
  jpeg_encoder_ = nullptr;

  reprocess_flag_ = false;
  ready_to_start_ = false;

  QMMF_INFO("%s: Exit", __func__);
  return NO_ERROR;
}

status_t CameraJpeg::Configure(const std::vector<ImageThumbnail> &thumbs) {

  jpeg_params_.thumbnail_data.clear();
  for (auto const& thumb : thumbs) {
    JpegEncoder::jpeg_thumbnail jpeg_thumbnail(thumb.width, thumb.height, thumb.quality);
    jpeg_params_.thumbnail_data.push_back(jpeg_thumbnail);
  }
  return NO_ERROR;
}

void CameraJpeg::Process(StreamBuffer& in_buffer, StreamBuffer& out_buffer) {
  QMMF_INFO("%s: %d: Enter ", __func__, __LINE__);

  void *buf_vaaddr = mmap(nullptr, in_buffer.size, PROT_READ  | PROT_WRITE,
      MAP_SHARED, in_buffer.fd, 0);
  void *out_vaaddr = mmap(nullptr, out_buffer.size, PROT_READ  | PROT_WRITE,
      MAP_SHARED, out_buffer.fd, 0);

  CameraMetadata meta;
  {
    std::unique_lock<std::mutex> lock(result_lock_);
    while (results_.count(in_buffer.timestamp) == 0) {
      std::chrono::nanoseconds timeout(kWaitJPEGTimeout);
      auto ret = wait_for_result_.WaitFor(lock, timeout);
      if (ret != 0) {
        QMMF_ERROR("%s: Wait for jpeg result timed out", __func__);
        break;
      }
    }
    meta = results_.at(in_buffer.timestamp);
    results_.erase(in_buffer.timestamp);
  }

  if (buf_vaaddr != MAP_FAILED || out_vaaddr != MAP_FAILED) {
    if (!meta.isEmpty()) {
      unsigned char *exif_buffer = new unsigned char[getExifTempBuffSize()];
      int exif_size = Generate(meta, vendor_name_, product_name_,
                               out_buffer.info.plane_info[0].width,
                               out_buffer.info.plane_info[0].height,
                               exif_buffer);
      convertExifBinaryToExifInfoStruct(exif_buffer);

      delete[] exif_buffer;

      if (exif_size != 0) {
        jpeg_params_.exif_size = getExifEntitiesSize();
        jpeg_params_.exif_data = getExifEntitiesData();
      } else {
        QMMF_ERROR("%s Empty exif section!", __func__);
        jpeg_params_.exif_size = 0;
        jpeg_params_.exif_data = (void*)0;
      }
    }

  auto ret = jpeg_encoder_->Init(in_buffer.info.plane_info[0].width,
                                 in_buffer.info.plane_info[0].height);
  if (ret != 0) {
    QMMF_ERROR("%s: failed to inint Jpeg Encoder", __func__);
    munmap(buf_vaaddr, in_buffer.size);
    munmap(out_vaaddr, out_buffer.size);
    out_buffer.data = nullptr;
    return;
  }
    size_t jpeg_size = 0;
    jpeg_params_.img_data[0] = static_cast<uint8_t*>(buf_vaaddr);
    jpeg_params_.out_data[0] = static_cast<uint8_t*>(out_vaaddr);
    jpeg_params_.source_info = in_buffer.info;

    ret = jpeg_encoder_->Encode(jpeg_params_, jpeg_size);
    if (ret != 0) {
      QMMF_ERROR("%s: Jpeg Encode fails", __func__);
    }

    if (0 == jpeg_size) {
      QMMF_ERROR("%s: JPEG size is 0!", __func__);
    } else {
      out_buffer.info.plane_info[0].width = jpeg_size;
      out_buffer.data = out_vaaddr;
      out_buffer.filled_length = jpeg_size;
      out_buffer.second_thumb = false;
      AddJpegHeader(out_buffer);
    }

    memcpy(buf_vaaddr, out_vaaddr, jpeg_size);
    munmap(buf_vaaddr, in_buffer.size);
    munmap(out_vaaddr, out_buffer.size);
    out_buffer.data = nullptr;

    jpeg_encoder_->DeInit();
  } else {
    QMMF_INFO("%s: SKIPP JPEG", __func__);
  }

  QMMF_INFO("%s: Exit", __func__);
}

status_t CameraJpeg::AddJpegHeader(StreamBuffer &buffer) {
  // JPEG header.
  camera3_jpeg_blob_t header;
  header.jpeg_blob_id = CAMERA3_JPEG_BLOB_ID;
  header.jpeg_size    = buffer.filled_length;

  // The header must be appended at the end of the allocated buffer.
  uint32_t offset = buffer.filled_length - sizeof(header);
  uintptr_t data  = reinterpret_cast<uintptr_t>(buffer.data);
  void *vaddr     = reinterpret_cast<void *>(data + offset);
  memcpy(vaddr, &header, sizeof(header));

  return NO_ERROR;
}

void CameraJpeg::AddBuff(StreamBuffer in_buff, StreamBuffer out_buff) {
  std::unique_lock<std::mutex> lock(buffer_lock_);
  Buff buff;
  buff.in = in_buff;
  buff.out = out_buff;
  input_buffer_.push_back(buff);
  wait_for_buffer_.SignalAll();
}

void CameraJpeg::AddResult(const void* result) {

  CameraMetadata meta = *(reinterpret_cast<const CameraMetadata *>(result));

  if (meta.exists(ANDROID_CONTROL_CAPTURE_INTENT)) {
    auto cature_intent = meta.find(ANDROID_CONTROL_CAPTURE_INTENT).data.u8[0];
    if (cature_intent != ANDROID_CONTROL_CAPTURE_INTENT_STILL_CAPTURE) {
      QMMF_DEBUG("%s Metadata is not related to a still capture!",
          __func__);
      return;
    }
  }

  if (!meta.exists(ANDROID_SENSOR_TIMESTAMP)) {
    QMMF_ERROR("%s Sensor timestamp tag missing in result!", __func__);
    return;
  }
  auto timestamp = meta.find(ANDROID_SENSOR_TIMESTAMP).data.i64[0];

  std::lock_guard<std::mutex> lock(result_lock_);
  results_.emplace(timestamp, meta);
  wait_for_result_.SignalAll();
}

status_t CameraJpeg::ReturnBuff(StreamBuffer buffer) {
  QMMF_INFO("%s: StreamBuffer(0x%p) ts: %lld",
       __func__, buffer.handle, buffer.timestamp);
  return NO_ERROR;
}


bool CameraJpeg::ThreadLoop() {
  Buff buffer;
  {
    std::unique_lock<std::mutex> lock(buffer_lock_);
    while (input_buffer_.empty()) {
      std::chrono::nanoseconds timeout(kFrameTimeout);
      auto ret = wait_for_buffer_.WaitFor(lock, timeout);
      if (ret != 0) {
         QMMF_ERROR("%s: Wait for pending buffers timed out", __func__);
        return true;
      }
    }
    auto iter = input_buffer_.begin();
    buffer = *iter;
    input_buffer_.erase(iter);
  }
  Process(buffer.in, buffer.out);
  capture_client_cb_(buffer.in, buffer.out);
  return true;
}

status_t CameraJpeg::FillMetaInfo(const PostProcParam& input,
                                  CameraBufferMetaData* info) {
  int aligned_width = input.width;
  int aligned_height = input.height;

  switch (input.format) {
    case HAL_PIXEL_FORMAT_YCbCr_420_SP_VENUS:
    case HAL_PIXEL_FORMAT_NV12_ENCODEABLE:
    case HAL_PIXEL_FORMAT_YCbCr_420_888:
      info->format = BufferFormat::kNV12;
      info->num_planes = 2;
      info->plane_info[0].width = input.width;
      info->plane_info[0].height = input.height;
      info->plane_info[0].stride = aligned_width;
      info->plane_info[0].scanline = aligned_height;
      info->plane_info[0].size = aligned_width * aligned_height;
      info->plane_info[0].offset = 0;
      info->plane_info[1].width = input.width;
      info->plane_info[1].height = input.height/2;
      info->plane_info[1].stride = aligned_width;
      info->plane_info[1].scanline = aligned_height/2;
      info->plane_info[1].size = aligned_width * (aligned_height / 2);
      info->plane_info[1].offset = aligned_width * aligned_height;
      break;
    case HAL_PIXEL_FORMAT_YCbCr_420_SP_VENUS_UBWC:
      info->format = BufferFormat::kNV12UBWC;
      info->num_planes = 2;
      info->plane_info[0].width = input.width;
      info->plane_info[0].height = input.height;
      info->plane_info[0].stride = aligned_width;
      info->plane_info[0].scanline = aligned_height;
      info->plane_info[0].size = aligned_width * aligned_height;
      info->plane_info[0].offset = 0;
      info->plane_info[1].width = input.width;
      info->plane_info[1].height = input.height/2;
      info->plane_info[1].stride = aligned_width;
      info->plane_info[1].scanline = aligned_height/2;
      info->plane_info[1].size = aligned_width * (aligned_height / 2);
      info->plane_info[1].offset = aligned_width * aligned_height;
      break;
    case HAL_PIXEL_FORMAT_NV21_ZSL:
      info->format = BufferFormat::kNV21;
      info->num_planes = 2;
      info->plane_info[0].width = input.width;
      info->plane_info[0].height = input.height;
      info->plane_info[0].stride = aligned_width;
      info->plane_info[0].scanline = aligned_height;
      info->plane_info[0].size = aligned_width * aligned_height;
      info->plane_info[0].offset = 0;
      info->plane_info[1].width = input.width;
      info->plane_info[1].height = input.height/2;
      info->plane_info[1].stride = aligned_width;
      info->plane_info[1].scanline = aligned_height/2;
      info->plane_info[1].size = aligned_width * (aligned_height / 2);
      info->plane_info[1].offset = aligned_width * aligned_height;
      break;
    case HAL_PIXEL_FORMAT_YCbCr_422_888:
      info->format = BufferFormat::kNV16;
      info->num_planes = 2;
      info->plane_info[0].width = input.width;
      info->plane_info[0].height = input.height;
      info->plane_info[0].stride = aligned_width;
      info->plane_info[0].scanline = aligned_height;
      info->plane_info[0].size = aligned_width * aligned_height;
      info->plane_info[0].offset = 0;
      info->plane_info[1].width = input.width;
      info->plane_info[1].height = input.height;
      info->plane_info[1].stride = aligned_width;
      info->plane_info[1].scanline = aligned_height;
      info->plane_info[1].size = aligned_width * aligned_height;
      info->plane_info[1].offset = aligned_width * aligned_height;
      break;
    default:
      QMMF_ERROR("%s: Unsupported format: 0x%x", __func__,
                 input.format);
      return NAME_NOT_FOUND;
  }
  return NO_ERROR;
}

}; // namespace recoder

}; // namespace qmmf

