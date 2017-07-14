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

#define TAG "CameraHazeBuster"

#include <stdio.h>
#include <sys/mman.h>
#include <sys/types.h>

#include "qmmf_haze_buster.h"

namespace qmmf {

namespace recorder {

static const char *kHazeBusterLib = "libqmmf_alg_hazebuster.so";


PostProcHazeBuster::PostProcHazeBuster(int32_t Id)
    : CameraAlg(kHazeBusterLib),
      id_(Id),
      reprocess_flag_(false),
      ready_to_start_(false) {
  QMMF_INFO("%s:%s: Enter", TAG, __func__);
  QMMF_INFO("%s:%s: Exit (0x%p)", TAG, __func__, this);
}

PostProcHazeBuster::~PostProcHazeBuster() {
  QMMF_INFO("%s:%s: Enter ", TAG, __func__);
  QMMF_INFO("%s:%s: Exit (0x%p)", TAG, __func__, this);
}

status_t PostProcHazeBuster::Create(const int32_t stream_id,
                                    const ReprocParam& input,
                                    const ReprocParam& output,
                                    const uint32_t frame_rate,
                                    const uint32_t num_images,
                                    const void* static_meta,
                                    const void* context,
                                    int32_t &out_stream_id) {
  QMMF_INFO("%s:%s: Enter", TAG, __func__);

  if (ready_to_start_) {
    QMMF_ERROR("%s:%s: Failed: Already configured.", TAG, __func__);
    return BAD_VALUE;
  }

  if (reprocess_flag_) {
    QMMF_ERROR("%s:%s: Failed: Wrong state.", TAG, __func__);
    return BAD_VALUE;
  }

  if (Init(&context_, nullptr) != NO_ERROR) {
    QMMF_ERROR("%s:%s: Alg Init error", TAG, __func__);
    return BAD_VALUE;
  }

  ready_to_start_ = true;

  QMMF_INFO("%s:%s: Exit reproc_ID: %d", TAG, __func__, id_);
  out_stream_id = id_;

  return NO_ERROR;
}

status_t PostProcHazeBuster::GetCapabilities(ReprocCaps *caps) {
  caps->internal_buff = 10;
  caps->out_format = -1; /* out == in */
  caps->in_format = HAL_PIXEL_FORMAT_YCbCr_420_888;
  caps->scale_en = 0;
  caps->usage = 0;
  // TODO
  return NO_ERROR;
}

status_t PostProcHazeBuster::Start() {
  QMMF_INFO("%s:%s: Enter", TAG, __func__);
  if (!ready_to_start_) {
    return BAD_VALUE;
  }

  reprocess_flag_ = true;

  QMMF_INFO("%s:%s: Exit", TAG, __func__);
  return NO_ERROR;
}

status_t PostProcHazeBuster::Stop() {
  QMMF_INFO("%s:%s: Enter stop Id_: %d", TAG, __func__, id_);

  ready_to_start_ = false;

  reprocess_flag_ = false;

  QMMF_INFO("%s:%s: Exit stop Id_: %d", TAG, __func__, id_);
  return NO_ERROR;
}

status_t PostProcHazeBuster::Delete() {
  QMMF_INFO("%s:%s: Enter ", TAG, __func__);

  Deinit(context_);

  reprocess_flag_ = false;
  ready_to_start_ = false;

  QMMF_INFO("%s:%s: Exit", TAG, __func__);
  return NO_ERROR;
}

status_t PostProcHazeBuster::Process(StreamBuffer& in_buff, StreamBuffer& out_buff) {
  status_t ret = NO_ERROR;

  if (reprocess_flag_ == true) {

    void *buf_vaaddr;
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

    void *vaaddr;
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

    qmmf_alg_process_data_t proc_data {};
    qmmf_alg_buffer_t in_bufs[1] {};
    qmmf_alg_buffer_t out_bufs[1] {};

    proc_data.input.cnt  = 1;
    proc_data.input.bufs = in_bufs;
    proc_data.output.cnt = 1;
    proc_data.output.bufs = out_bufs;
    proc_data.user_data  = this;
    proc_data.complete   = nullptr;

    proc_data.input.bufs[0].vaddr = reinterpret_cast<uint8_t*>(buf_vaaddr);
    proc_data.input.bufs[0].fmt.width = in_buff.info.plane_info[0].width;
    proc_data.input.bufs[0].fmt.height = in_buff.info.plane_info[0].height;
    proc_data.input.bufs[0].fmt.num_planes = 2;
    proc_data.input.bufs[0].fmt.plane[0].stride =
        in_buff.info.plane_info[0].stride;
    proc_data.input.bufs[0].fmt.plane[0].length =
        in_buff.info.plane_info[0].scanline * in_buff.info.plane_info[0].stride;
    proc_data.input.bufs[0].fmt.plane[1].stride =
        in_buff.info.plane_info[1].stride;
    proc_data.input.bufs[0].fmt.plane[1].length =
        in_buff.info.plane_info[1].scanline * in_buff.info.plane_info[1].stride;

    proc_data.output.bufs[0].vaddr = reinterpret_cast<uint8_t*>(vaaddr);
    proc_data.output.bufs[0].fmt.width = out_buff.info.plane_info[0].width;
    proc_data.output.bufs[0].fmt.height = out_buff.info.plane_info[0].height;
    proc_data.output.bufs[0].fmt.num_planes = 2;
    proc_data.output.bufs[0].fmt.plane[0].stride =
        out_buff.info.plane_info[0].stride;
    proc_data.output.bufs[0].fmt.plane[0].length =
        out_buff.info.plane_info[0].scanline * out_buff.info.plane_info[0].stride;
    proc_data.output.bufs[0].fmt.plane[1].stride =
        out_buff.info.plane_info[1].stride;
    proc_data.output.bufs[0].fmt.plane[1].length =
        out_buff.info.plane_info[1].scanline * out_buff.info.plane_info[1].stride;

    ret = AlgProcess(context_, &proc_data);
    if (NO_ERROR != ret) {
      QMMF_ERROR("%s:%s: Failed to process images, status(%d)", TAG, __func__,
          ret);
    }

    if (in_buff.data == nullptr) {
      munmap(buf_vaaddr, in_buff.size);
    }
    if (out_buff.data == nullptr) {
      munmap(vaaddr, in_buff.size);
    }
  }

  Listener_->OnFrameReady(out_buff);
  Listener_->OnFrameProcessed(in_buff);

  return ret;
}

}; // namespace recoder

}; // namespace qmmf
