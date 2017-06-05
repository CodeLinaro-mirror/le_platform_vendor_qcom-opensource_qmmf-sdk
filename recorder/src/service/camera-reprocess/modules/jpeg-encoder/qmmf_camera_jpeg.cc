/*
 * Copyright (c) 2017, The Linux Foundation. All rights reserved.
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

#define TAG "CameraJpeg"

#include <sys/mman.h>

#include "qmmf_camera_jpeg.h"

namespace qmmf {

namespace recorder {

namespace reproc {

CameraJpeg::CameraJpeg(int32_t Id)
    : id_(Id),
      reprocess_flag_(false),
      ready_to_start_(false),
      jpeg_encoder_(nullptr) {
  QMMF_VERBOSE("%s:%s: Enter", TAG, __func__);
  jpeg_encoder_ = reprocjpegencoder::JpegEncoder::getInstance();
  QMMF_VERBOSE("%s:%s: Exit (0x%p)", TAG, __func__, this);
}

CameraJpeg::~CameraJpeg() {
  QMMF_VERBOSE("%s:%s: Enter ", TAG, __func__);
  reprocjpegencoder::JpegEncoder::releaseInstance();
  jpeg_encoder_ = nullptr;
  QMMF_VERBOSE("%s:%s: Exit (0x%p)", TAG, __func__, this);
}

int32_t CameraJpeg::Create(const int32_t stream_id,
                           const ReprocParam& input,
                           const ReprocParam& output,
                           const uint32_t frame_rate,
                           const uint32_t num_images,
                           const void* static_meta,
                           const void* context) {
  QMMF_VERBOSE("%s:%s: Enter", TAG, __func__);

  if (ready_to_start_) {
    QMMF_ERROR("%s:%s: Failed: Already configured.", TAG, __func__);
    return BAD_VALUE;
  }

  if (reprocess_flag_) {
    QMMF_ERROR("%s:%s: Failed: Wrong state.", TAG, __func__);
    return BAD_VALUE;
  }

  ready_to_start_ = true;

  QMMF_VERBOSE("%s:%s: Exit reproc_ID: %d", TAG, __func__, id_);
  return id_;
}

status_t CameraJpeg::GetCapabilities(ReprocCaps *caps) {
  caps->internal_buff = 1;
  caps->out_format = HAL_PIXEL_FORMAT_BLOB;
  caps->in_format = HAL_PIXEL_FORMAT_YCbCr_420_888;
  caps->scale_en = 0;
  caps->usage = 0;
  // TODO
  return NO_ERROR;
}

status_t CameraJpeg::Start() {
  QMMF_VERBOSE("%s:%s: Enter", TAG, __func__);
  if (!ready_to_start_) {
    return BAD_VALUE;
  }

  reprocess_flag_ = true;

  QMMF_VERBOSE("%s:%s: Exit", TAG, __func__);
  return NO_ERROR;
}

status_t CameraJpeg::Stop() {
  QMMF_INFO("%s:%s: The Jpeg thread is stopped Id_: %d", TAG, __func__, id_);
  ready_to_start_ = false;

  reprocess_flag_ = false;

  QMMF_INFO("%s:%s: Exit stop Id_: %d", TAG, __func__, id_);
  return NO_ERROR;
}

status_t CameraJpeg::Delete() {
  QMMF_VERBOSE("%s:%s: Enter ", TAG, __func__);

  reprocess_flag_ = false;
  ready_to_start_ = false;

  QMMF_VERBOSE("%s:%s: Exit", TAG, __func__);
  return NO_ERROR;
}

bool CameraJpeg::Process(StreamBuffer& in_buffer,
                         StreamBuffer& out_buffer) {
  QMMF_VERBOSE("%s:%s: %d: Enter in FD: %d out FD: %d ", TAG,
      __func__, __LINE__, in_buffer.fd, out_buffer.fd);

  void *buf_vaaddr = nullptr;
  if (in_buffer.data == nullptr) {
    buf_vaaddr = mmap(nullptr, in_buffer.size, PROT_READ  | PROT_WRITE,
        MAP_SHARED, in_buffer.fd, 0);
  } else {
    buf_vaaddr = in_buffer.data;
  }

  if (buf_vaaddr == MAP_FAILED) {
      QMMF_ERROR("%s:%s  ION mmap failed: %s (%d)", TAG, __func__,
          strerror(errno), errno);
  }

  void *out_vaaddr = nullptr;
  if (out_buffer.data == nullptr) {
    out_vaaddr = mmap(nullptr, out_buffer.size, PROT_READ  | PROT_WRITE,
        MAP_SHARED, out_buffer.fd, 0);
  } else {
    out_vaaddr = out_buffer.data;
  }

  if (out_vaaddr == MAP_FAILED) {
      QMMF_ERROR("%s:%s  ION mmap failed: %s (%d)", TAG, __func__,
          strerror(errno), errno);
  }

  if (buf_vaaddr != MAP_FAILED && out_vaaddr != MAP_FAILED) {
    size_t jpeg_size = 0;
    jpeg_encoder_->in_buffer_.img_data[0] = (uint8_t*)buf_vaaddr;
    jpeg_encoder_->in_buffer_.out_data[0] = (uint8_t*)out_vaaddr;
    jpeg_encoder_->in_buffer_.source_info = in_buffer.info;
    auto buf_vaddr = jpeg_encoder_->Encode(&jpeg_size);
    if (!buf_vaddr) {
      QMMF_VERBOSE("%s:%s: Jpeg out buffer is NULL", TAG, __func__);
    }

    if (in_buffer.data == nullptr) {
      munmap(buf_vaaddr, in_buffer.size);
    }
    if (out_buffer.data == nullptr) {
      munmap(out_vaaddr, out_buffer.size);
    }

    out_buffer.info.format = BufferFormat::kBLOB;
    out_buffer.info.plane_info[0].width = jpeg_size;
    out_buffer.data = nullptr;
    out_buffer.filled_length = jpeg_size;
    out_buffer.timestamp = in_buffer.timestamp;

  } else {
    QMMF_VERBOSE("%s:%s: SKIPP JPEG", TAG, __func__);
  }

  ReprocessLibCallback(in_buffer, out_buffer);
  QMMF_VERBOSE("%s:%s: Exit", TAG, __func__);
  return true;
}

void CameraJpeg::AddResult(const void* result) {
}

status_t CameraJpeg::ReturnBuff(StreamBuffer buffer) {
  QMMF_VERBOSE("%s:%s: StreamBuffer(0x%p) ts: %lld, streamId: %d", TAG,
       __func__, buffer.handle, buffer.timestamp, buffer.stream_id);
  return NO_ERROR;
}

}; // namespace reproc

}; // namespace recoder

}; // namespace qmmf
