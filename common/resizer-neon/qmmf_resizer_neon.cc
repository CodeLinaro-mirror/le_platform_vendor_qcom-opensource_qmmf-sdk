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

#define LOG_TAG "CommonNEONResizer"

#include <cstdint>
#include <adreno/c2d2.h>
#include <linux/msm_kgsl.h>

#include "common/utils/qmmf_log.h"

#include "qmmf_resizer_neon.h"

namespace qmmf {

NEONResizer::NEONResizer()
  : handle_(nullptr) {
  QMMF_VERBOSE("%s: Enter", __func__);
  QMMF_VERBOSE("%s: Exit (0x%p)", __func__, this);
}

NEONResizer::~NEONResizer() {
  QMMF_VERBOSE("%s: Enter", __func__);
  DeInit();
  QMMF_VERBOSE("%s: Exit (0x%p)", __func__, this);
}

RESIZER_STATUS NEONResizer::Init() {
  std::lock_guard<std::mutex> lock(lock_);
  if (handle_) {
    QMMF_INFO("%s: The neon resizer is already init", __func__);
    return RESIZER_STATUS_OK;
  }
  auto res = neonresizer::resn_init(&handle_);
  if (neonresizer::RESN_SUCCESS != res) {
    QMMF_ERROR("%s: Failed! %d", __func__, res);
    return RESIZER_STATUS_ERROR;
  }

  QMMF_INFO("%s: version: %s", __func__, neonresizer::resn_get_version());

  return RESIZER_STATUS_OK;
}

void NEONResizer::DeInit() {
  std::lock_guard<std::mutex> lock(lock_);
  if (handle_) {
    neonresizer::resn_deinit(handle_);
    handle_ = nullptr;
  }
}

RESIZER_STATUS NEONResizer::Draw(StreamBuffer& src_buffer,
                                 StreamBuffer& dst_buffer) {
  RESIZER_STATUS status = RESIZER_STATUS_OK;


  if (ValidateInParams(src_buffer, dst_buffer) != RESIZER_STATUS_OK) {
    QMMF_ERROR("%s Input validation error!!!", __func__);
    return RESIZER_STATUS_ERROR;
  }

  std::lock_guard<std::mutex> lock(lock_);
  if (handle_ == nullptr) {
    QMMF_ERROR("%s The neon resizer handle is null!!!", __func__);
    return RESIZER_STATUS_ERROR;
  }

  neonresizer::resn_t params;
  FillProcessParams(src_buffer, dst_buffer, params);
  auto ret = neonresizer::resn_process(handle_, &params);
  if (neonresizer::RESN_SUCCESS != ret) {
    QMMF_ERROR("%s: Neon process error: %d", __func__, ret);
    return RESIZER_STATUS_ERROR;
  }

  return status;
}

RESIZER_STATUS NEONResizer::FillProcessParams(const StreamBuffer& src_buffer,
                                              const StreamBuffer& dst_buffer,
                                              neonresizer::resn_t &params) {
  //default tuning should be generate internaly
  params.resn_tuning = nullptr;

  params.src_luma = (unsigned char *)src_buffer.data;
  auto luma_len = src_buffer.info.plane_info[0].stride *
                  src_buffer.info.plane_info[0].scanline;
  params.src_chroma = (unsigned char *)((intptr_t)params.src_luma + luma_len);

  // Output data pointers
  auto chroma_len = dst_buffer.info.plane_info[0].stride *
                    dst_buffer.info.plane_info[0].scanline;
  params.dst_luma = (unsigned char *)dst_buffer.data;
  params.dst_chroma = (unsigned char *)((intptr_t)params.dst_luma + chroma_len);

  // Input buffer dimensions
  params.src_width = src_buffer.info.plane_info[0].width;
  params.src_height = src_buffer.info.plane_info[0].height;
  params.src_stride = src_buffer.info.plane_info[0].stride;

  // Output buffer dimensions
  params.dst_width = dst_buffer.info.plane_info[0].width;
  params.dst_height = dst_buffer.info.plane_info[0].height;
  params.dst_stride = dst_buffer.info.plane_info[0].stride;

  QMMF_DEBUG("%s: SRC: %s", __func__, src_buffer.info.ToString().c_str());
  QMMF_DEBUG("%s: DST: %s", __func__, dst_buffer.info.ToString().c_str());

  return RESIZER_STATUS_OK;
}

RESIZER_STATUS NEONResizer::ValidateInParams(const StreamBuffer& src_buffer,
                                             const StreamBuffer& dst_buffer) {
  if (src_buffer.data == nullptr || dst_buffer.data == nullptr) {
    QMMF_ERROR("%s Bad buffer address!!!", __func__);
    return RESIZER_STATUS_ERROR;
  }

  auto &src_info = src_buffer.info;
  auto &dst_info = dst_buffer.info;

  if (src_info.num_planes == 0 || dst_info.num_planes == 0) {
    QMMF_ERROR("%s Bad planes number!!!", __func__);
    return RESIZER_STATUS_ERROR;
  }

  auto &src_plane_info = src_info.plane_info[0];
  auto &dst_plane_info = dst_info.plane_info[0];

  if (src_plane_info.width == 0 || src_plane_info.height == 0 ||
      dst_plane_info.width == 0 || dst_plane_info.height == 0) {
    QMMF_ERROR("%s Bad img width or height size number!!!", __func__);
    return RESIZER_STATUS_ERROR;
  }

  if (dst_plane_info.width % 8) {
    QMMF_ERROR("%s Output width needs to be multiple of 8 (w: %d)!!!",
        __func__, dst_plane_info.width);
    return RESIZER_STATUS_ERROR;
  }

  return RESIZER_STATUS_OK;
}


} //namespace qmmf ends here
