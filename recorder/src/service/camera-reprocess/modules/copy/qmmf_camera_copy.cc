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

#define TAG "CameraCopy"

#include <sys/mman.h>

#include "qmmf_camera_copy.h"

namespace qmmf {

namespace recorder {

CameraCopy::CameraCopy(int32_t Id)
    : id_(Id),
      reprocess_flag_(false),
      ready_to_start_(false) {
  QMMF_INFO("%s:%s: Enter", TAG, __func__);
  QMMF_INFO("%s:%s: Exit (0x%p)", TAG, __func__, this);
}

CameraCopy::~CameraCopy() {
  QMMF_INFO("%s:%s: Enter ", TAG, __func__);
  QMMF_INFO("%s:%s: Exit (0x%p)", TAG, __func__, this);
}

int32_t CameraCopy::Create(const int32_t stream_id,
                           const ReprocParam& input,
                           const ReprocParam& output,
                           const uint32_t frame_rate,
                           const uint32_t num_images,
                           const void* static_meta,
                           const void* context) {
  QMMF_INFO("%s:%s: Enter", TAG, __func__);

  if (ready_to_start_) {
    QMMF_ERROR("%s:%s: Failed: Already configured.", TAG, __func__);
    return BAD_VALUE;
  }

  if (reprocess_flag_) {
    QMMF_ERROR("%s:%s: Failed: Wrong state.", TAG, __func__);
    return BAD_VALUE;
  }

  ready_to_start_ = true;

  QMMF_INFO("%s:%s: Exit reproc_ID: %d", TAG, __func__, id_);
  return id_;
}

status_t CameraCopy::GetCapabilities(ReprocCaps *caps) {
  caps->internal_buff = 10;
  caps->out_format = -1;
  caps->in_format = -1;
  caps->scale_en = 0;
  caps->usage = 0;
  // TODO
  return NO_ERROR;
}

status_t CameraCopy::Start() {
  QMMF_INFO("%s:%s: Enter", TAG, __func__);
  if (!ready_to_start_) {
    return BAD_VALUE;
  }

  reprocess_flag_ = true;

  QMMF_INFO("%s:%s: Exit", TAG, __func__);
  return NO_ERROR;
}

status_t CameraCopy::Stop() {
  QMMF_INFO("%s:%s: Enter stop Id_: %d", TAG, __func__, id_);

  reprocess_flag_ = false;

  QMMF_INFO("%s:%s: Exit stop Id_: %d", TAG, __func__, id_);
  return NO_ERROR;
}

status_t CameraCopy::Delete() {
  QMMF_INFO("%s:%s: Enter ", TAG, __func__);

  reprocess_flag_ = false;
  ready_to_start_ = false;

  QMMF_INFO("%s:%s: Exit", TAG, __func__);
  return NO_ERROR;
}

void CameraCopy::AddResult(const void* result) {
}

status_t CameraCopy::ReturnBuff(StreamBuffer buffer) {
  QMMF_VERBOSE("%s:%s: StreamBuffer(%p) ts: %lld, streamId: %d", TAG,
       __func__, buffer.handle, buffer.timestamp, buffer.stream_id);
  return NO_ERROR;
}

bool CameraCopy::Process(StreamBuffer& in_buff, StreamBuffer& out_buff) {
  bool ret = true;

  if (reprocess_flag_ == true) {

    void *buf_vaaddr = nullptr;
    if (in_buff.data == nullptr) {
      buf_vaaddr = mmap(nullptr, in_buff.size, PROT_READ  | PROT_WRITE,
          MAP_SHARED, in_buff.fd, 0);
    } else {
      buf_vaaddr = in_buff.data;
    }

    if (buf_vaaddr == MAP_FAILED) {
        QMMF_ERROR("%s:%s  ION mmap failed: %s (%d)", TAG, __func__,
            strerror(errno), errno);
    }


    void *vaaddr = nullptr;
    if (out_buff.data == nullptr) {
      vaaddr = mmap(nullptr, out_buff.size, PROT_READ  | PROT_WRITE,
          MAP_SHARED, out_buff.fd, 0);
    } else {
      vaaddr = out_buff.data;
    }

    if (vaaddr == MAP_FAILED) {
        QMMF_ERROR("%s:%s  ION mmap failed: %s (%d)", TAG, __func__,
            strerror(errno), errno);
    }

    /* check size before memcpy */
    if (out_buff.size >= in_buff.size) {
      memcpy(vaaddr, buf_vaaddr, in_buff.size);
    }

    QMMF_INFO("%s:%s: memcpy Done", TAG, __func__);

    if (in_buff.data == nullptr) {
      munmap(buf_vaaddr, in_buff.size);
    }
    if (out_buff.data == nullptr) {
      munmap(vaaddr, out_buff.size);
    }
  }

  ReprocessLibCallback(in_buff, out_buff);

  return ret;
}

}; // namespace recoder

}; // namespace qmmf
