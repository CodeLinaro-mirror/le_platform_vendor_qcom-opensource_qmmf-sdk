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

#include <utils/KeyedVector.h>
#include <algorithm>
#include <fcntl.h>
#include <sys/mman.h>

#include "recorder/src/service/qmmf_recorder_utils.h"

#include "qmmf_camera_jpeg.h"

namespace qmmf {

namespace recorder {

CameraJpeg::CameraJpeg(int32_t Id)
    : id_(Id),
      reprocess_flag_(false),
      ready_to_start_(false),
      jpeg_encoder_(nullptr) {
  QMMF_VERBOSE("%s:%s: Enter", TAG, __func__);
  jpeg_encoder_ = JpegEncoder::getInstance();
  QMMF_VERBOSE("%s:%s: Exit (0x%p)", TAG, __func__, this);
}

CameraJpeg::~CameraJpeg() {
  QMMF_VERBOSE("%s:%s: Enter ", TAG, __func__);
  JpegEncoder::releaseInstance();
  jpeg_encoder_ = nullptr;
  QMMF_VERBOSE("%s:%s: Exit (0x%p)", TAG, __func__, this);
}

int32_t CameraJpeg::Create(const int32_t stream_id,
                           const ReprocParam& input,
                           const ReprocParam& output,
                           const uint32_t frame_rate,
                           const uint32_t num_images,
                           const void* static_meta,
                           const ReprocessNodeCb& cb,
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

  capture_client_cb_ = cb.resultCb;
  get_empty_buff_cb_ = cb.bufferCb;

  ready_to_start_ = true;

  QMMF_VERBOSE("%s:%s: Exit reproc_ID: %d", TAG, __func__, id_);
  return id_;
}

status_t CameraJpeg::GetCapabilities(ReprocCaps *caps) {
  caps->internal_buff = 1;
  caps->format = HAL_PIXEL_FORMAT_BLOB;
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
  Run("Camera Jpeg");

  QMMF_VERBOSE("%s:%s: Exit", TAG, __func__);
  return NO_ERROR;
}

status_t CameraJpeg::Stop() {

  RequestExitAndWait();
  QMMF_INFO("%s:%s: The Jpeg thread is stopped Id_: %d", TAG, __func__, id_);
  ready_to_start_ = false;

  StreamBuffer buffer, b;
  memset(&b, 0x0, sizeof(b));
  b.fd = -1;

  Mutex::Autolock lock(wait_lock_);
  auto iter = input_buffer_.begin();
  while (iter != input_buffer_.end()) {
    buffer = *iter;
    iter = input_buffer_.erase(iter);
    QMMF_INFO("%s:%s: back to client node: FD: %d", TAG, __func__, buffer.fd);
    capture_client_cb_(b, buffer);
  }

  reprocess_flag_ = false;

  QMMF_INFO("%s:%s: Exit stop Id_: %d", TAG, __func__, id_);
  return NO_ERROR;
}

status_t CameraJpeg::Delete() {
  QMMF_VERBOSE("%s:%s: Enter ", TAG, __func__);

  //unmap
  for (uint32_t i = 0; i < mapped_buffs_.size(); i++) {
    if (mapped_buffs_.valueAt(i).addr) {
      auto map = mapped_buffs_.valueAt(i);
      QMMF_VERBOSE("%s:%s: Unmap %p", TAG, __func__, map.addr);
      munmap(map.addr, map.size);
    }
  }

  mapped_buffs_.clear();

  reprocess_flag_ = false;
  ready_to_start_ = false;

  QMMF_VERBOSE("%s:%s: Exit", TAG, __func__);
  return NO_ERROR;
}

void* CameraJpeg::MapBuff(StreamBuffer& buffer) {
  void *vaaddr;
  if (mapped_buffs_.indexOfKey(buffer.fd) < 0 || mapped_buffs_.isEmpty()) {
    vaaddr = mmap(nullptr, buffer.size, PROT_READ  | PROT_WRITE,
        MAP_SHARED, buffer.fd, 0);
    map_data_t map;
    map.addr = vaaddr;
    map.size = buffer.size;
    mapped_buffs_.add(buffer.fd, map);
  } else {
    vaaddr = mapped_buffs_.valueFor(buffer.fd).addr;
  }
  return vaaddr;
}

void CameraJpeg::Process(StreamBuffer& in_buffer,
                         StreamBuffer& out_buffer) {
  QMMF_VERBOSE("%s:%s: %d: Enter in FD: %d out FD: %d ", TAG,
      __func__, __LINE__, in_buffer.fd, out_buffer.fd);

  void *buf_vaaddr = mmap(nullptr, in_buffer.size, PROT_READ,
      MAP_SHARED, in_buffer.fd, 0);
  void *out_vaaddr = MapBuff(out_buffer);
  if (buf_vaaddr != MAP_FAILED && out_vaaddr != MAP_FAILED) {
    size_t jpeg_size = 0;
    jpeg_encoder_->in_buffer_.img_data[0] = (uint8_t*)buf_vaaddr;
    jpeg_encoder_->in_buffer_.out_data[0] = (uint8_t*)out_vaaddr;
    jpeg_encoder_->in_buffer_.source_info = in_buffer.info;
    auto buf_vaddr = jpeg_encoder_->Encode(&jpeg_size);
    if (!buf_vaddr) {
      QMMF_VERBOSE("%s:%s: Jpeg out buffer is NULL", TAG, __func__);
    }
    munmap(buf_vaaddr, in_buffer.size);
    out_buffer.info.plane_info[0].width = jpeg_size;
    out_buffer.data = nullptr;
    out_buffer.filled_length = jpeg_size;
    out_buffer.timestamp = in_buffer.timestamp;

  } else {
    QMMF_VERBOSE("%s:%s: SKIPP JPEG", TAG, __func__);
  }

  QMMF_VERBOSE("%s:%s: Exit", TAG, __func__);
}

void CameraJpeg::AddBuff(StreamBuffer in_buff) {
  Mutex::Autolock lock(wait_lock_);
  input_buffer_.push_back(in_buff);
  wait_for_buffer_.signal();
}

void CameraJpeg::AddResult(const void* result) {
}

status_t CameraJpeg::ReturnBuff(StreamBuffer buffer) {
  QMMF_VERBOSE("%s:%s: StreamBuffer(0x%p) ts: %lld, streamId: %d", TAG,
       __func__, buffer.handle, buffer.timestamp, buffer.stream_id);
  return NO_ERROR;
}

bool CameraJpeg::ThreadLoop() {
  status_t ret = NO_ERROR;
  StreamBuffer buffer;
  {
    Mutex::Autolock lock(wait_lock_);
    if (input_buffer_.empty()) {
      ret = wait_for_buffer_.waitRelative(wait_lock_, kFrameTimeout);
      if (ret == TIMED_OUT) {
        QMMF_DEBUG("%s:%s: Wait for frame available timed out", TAG, __func__);
        return true;
      }
    }
    auto iter = input_buffer_.begin();
    buffer = *iter;
    input_buffer_.erase(iter);
  }

  StreamBuffer b;
  memset(&b, 0x0, sizeof(b));
  get_empty_buff_cb_(&b);
  b.stream_id = id_;
  Process(buffer, b);
  capture_client_cb_(buffer, b);
  return true;
}

}; // namespace recoder

}; // namespace qmmf
