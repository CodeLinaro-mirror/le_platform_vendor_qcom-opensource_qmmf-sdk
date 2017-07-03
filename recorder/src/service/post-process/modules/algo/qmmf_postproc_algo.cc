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

#define TAG "QmmfPostprocAlgo"

#include <stdio.h>
#include <sys/mman.h>
#include <sys/types.h>

#include "qmmf_postproc_algo.h"

namespace qmmf {

namespace recorder {

using namespace qmmf_alg_plugin;

PostProcAlg::PostProcAlg(int32_t Id, std::string lib)
    : id_(Id),
      Lib_(lib),
      reprocess_flag_(false),
      ready_to_start_(false) {
  QMMF_INFO("%s:%s: Enter", TAG, __func__);

  try {
    Utils::LoadLib(Lib_, lib_handle_);

    QmmfAlgLoadPlugin LoadPluginFunc;
    Utils::LoadLibHandler(lib_handle_, QMMF_ALFO_LIB_LOAD_FUNC, LoadPluginFunc);
    std::vector<uint8_t> calibration_data;
    algo_ = LoadPluginFunc(calibration_data);
  } catch (const std::exception &e) {
    QMMF_ERROR("%s:%s: Error loading: %s exception: %s", TAG, __func__,
        Lib_.c_str(), e.what());
    throw e;
  }
  QMMF_INFO("%s:%s: Exit (0x%p)", TAG, __func__, this);
}

PostProcAlg::~PostProcAlg() {
  QMMF_INFO("%s:%s: Enter ", TAG, __func__);

  buffs_.clear();

  try {
    Utils::UnloadLib(lib_handle_);
  } catch (const std::exception &e) {
    QMMF_ERROR("%s:%s: Error releasing: %s exception: %s", TAG, __func__,
        Lib_.c_str(), e.what());
    throw e;
  }

  QMMF_INFO("%s:%s: Exit (0x%p)", TAG, __func__, this);
}

status_t PostProcAlg::Create(const int32_t stream_id,
                             const PostProcCreateParam& input,
                             const PostProcCreateParam& output,
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

  algo_->SetCallbacks(this);

  ready_to_start_ = true;

  QMMF_INFO("%s:%s: Exit reproc_ID: %d", TAG, __func__, id_);
  out_stream_id = id_;

  return NO_ERROR;
}

PostProcCreateParam PostProcAlg::GetInput(const PostProcCreateParam &out) {
  Requirements requirements;
  PostProcCreateParam in = out;

  requirements.width_    = out.width;
  requirements.height_   = out.height;
  requirements.stride_   = out.stride;
  requirements.scanline_ = out.scanline;
  // todo: get work with qmmf format (PixelFormat) instead of HAL format
  //algo_params.formats_.push_back(GetAlgFormat(out.format));

  std::vector<Requirements> alg_out = {requirements};
  requirements = algo_->GetInputRequirements(alg_out);

  in.width    = requirements.width_;
  in.height   = requirements.height_;
  in.stride   = requirements.stride_;
  in.scanline = requirements.scanline_;
  //in.format   = GetQmmfFormat(algo_params.formats_.front());

  return in;
}

PostProcCreateParam PostProcAlg::GetOutput(const PostProcCreateParam &in) {
  PostProcCreateParam out = in;
  // todo: remove GetOutput API. GetInput should be enough.
  return out;
}

status_t PostProcAlg::GetCapabilities(PostProcCaps &caps) {
  qmmf_alg_plugin::Capabilities algo_caps = algo_->GetCaps();

  caps.output_buff_        = algo_caps.out_buffer_requirements_.count_;
  caps.min_width_          = algo_caps.out_buffer_requirements_.min_width_;
  caps.min_height_         = algo_caps.out_buffer_requirements_.min_height_;
  caps.max_width_          = algo_caps.out_buffer_requirements_.min_width_;
  caps.max_height_         = algo_caps.out_buffer_requirements_.min_height_;
  caps.crop_support_       = algo_caps.crop_support_;
  caps.scale_support_      = algo_caps.scale_support_;
  caps.inplace_processing_ = algo_caps.inplace_processing_;
  caps.lib_version_        = algo_caps.lib_version_;

  for (auto fmt : algo_caps.in_buffer_requirements_.pixel_formats_) {
    caps.in_formats_.push_back(GetQmmfFormat(fmt));
  }

  for (auto fmt : algo_caps.out_buffer_requirements_.pixel_formats_) {
    caps.out_formats_.push_back(GetQmmfFormat(fmt));
  }
  return NO_ERROR;
}

status_t PostProcAlg::Start() {
  QMMF_INFO("%s:%s: Enter", TAG, __func__);
  if (!ready_to_start_) {
    return BAD_VALUE;
  }

  reprocess_flag_ = true;

  QMMF_INFO("%s:%s: Exit", TAG, __func__);
  return NO_ERROR;
}

status_t PostProcAlg::Stop() {
  QMMF_INFO("%s:%s: Enter stop Id_: %d", TAG, __func__, id_);

  ready_to_start_ = false;

  reprocess_flag_ = false;

  QMMF_INFO("%s:%s: Exit stop Id_: %d", TAG, __func__, id_);
  return NO_ERROR;
}

status_t PostProcAlg::Delete() {
  QMMF_INFO("%s:%s: Enter ", TAG, __func__);

  algo_->Abort();
  delete algo_;
  algo_ = nullptr;

  reprocess_flag_ = false;
  ready_to_start_ = false;

  QMMF_INFO("%s:%s: Exit", TAG, __func__);
  return NO_ERROR;
}

status_t PostProcAlg::Configure(const std::string config_json_data) {
  try {
    algo_->Configure(config_json_data);
  } catch (const std::exception &e) {
    QMMF_ERROR("%s:%s: Error while configuring exception: %s", TAG,
        __func__, e.what());
    return BAD_VALUE;
  }

  return NO_ERROR;
}

status_t PostProcAlg::Process(
    const std::vector<StreamBuffer> &in_buffers,
    const std::vector<StreamBuffer> &out_buffers) {

  if (reprocess_flag_ == true) {
    std::vector<AlgBuffer> in_alg_buffers;
    auto ret = PrepareAlgBuffer(in_alg_buffers, in_buffers);
    if (ret != NO_ERROR) {
      QMMF_ERROR("%s:%s: Fail to prepare in buffers", TAG, __func__);
      return BAD_VALUE;
    }

    std::vector<AlgBuffer> out_alg_buffers;
    ret = PrepareAlgBuffer(out_alg_buffers, out_buffers);
    if (ret != NO_ERROR) {
      QMMF_ERROR("%s:%s: Fail to prepare out buffers", TAG, __func__);
      return BAD_VALUE;
    }

    try {
      algo_->RegisterInputBuffers(in_alg_buffers);
      algo_->RegisterOutputBuffers(out_alg_buffers);
    } catch (const std::exception &e) {
      QMMF_ERROR("%s:%s: Error registering buffers exception: %s", TAG,
          __func__, e.what());
      throw e;
    }

    try {
      algo_->Process(in_alg_buffers, out_alg_buffers);
    } catch (const std::exception &e) {
      QMMF_ERROR("%s:%s: Error while processing exception: %s", TAG,
          __func__, e.what());
      algo_->UnregisterInputBuffers(in_alg_buffers);
      algo_->UnregisterOutputBuffers(out_alg_buffers);
      return BAD_VALUE;
    }
  } else {
    for (auto iter : in_buffers) {
      Listener_->OnFrameReady(iter);
    }
    for (auto iter : out_buffers ) {
      Listener_->OnFrameProcessed(iter);
    }
  }

  return NO_ERROR;
}

void PostProcAlg::OnFrameProcessed(const AlgBuffer &input_buffer) {

  const std::vector<AlgBuffer> buffers = {input_buffer};
  algo_->UnregisterInputBuffers(buffers);

  // return stream buffer to upper layer
  StreamBuffer buf = GetStreamBuffer(input_buffer);
  Listener_->OnFrameProcessed(buf);
}

void PostProcAlg::OnFrameReady(const AlgBuffer &output_buffer) {

  const std::vector<AlgBuffer> buffers = {output_buffer};
  algo_->UnregisterOutputBuffers(buffers);

  // return stream buffer to upper layer
  StreamBuffer buf = GetStreamBuffer(output_buffer);
  Listener_->OnFrameReady(buf);
}

void PostProcAlg::OnError(RuntimeError err) {
  QMMF_ERROR("%s:%s: Error %d", TAG, __func__, err);
  Listener_->OnError(err);
}

PixelFormat PostProcAlg::GetAlgFormat(BufferFormat format) {
  switch (format) {
  case BufferFormat::kNV12UBWC:
    return kNv12UBWC;
  case BufferFormat::kNV12:
    return kNv12;
  case BufferFormat::kNV21:
    return kNv21;
  case BufferFormat::kBLOB:
    return kJpeg;
  case BufferFormat::kRAW10:
    return kRawBggr10;
  case BufferFormat::kRAW12:
    return kRawBggr12;
  case BufferFormat::kRAW16:
    return kRawBggr16;
  default:
    return kNv12;
  }
}

BufferFormat PostProcAlg::GetQmmfFormat(PixelFormat format) {
  switch (format) {
  case kNv12UBWC:
    return BufferFormat::kNV12UBWC;
  case kNv12:
    return BufferFormat::kNV12;
  case kNv21:
    return BufferFormat::kNV21;
  case kJpeg:
    return BufferFormat::kBLOB;
  case kRawBggr10:
    return BufferFormat::kRAW10;
  case kRawBggr12:
    return BufferFormat::kRAW12;
  case kRawBggr16:
    return BufferFormat::kRAW16;
  default:
    return BufferFormat::kNV12;
  }
}

status_t PostProcAlg::PrepareAlgBuffer(
    std::vector<AlgBuffer> &algo_buffs,
    const std::vector<StreamBuffer> stream_buffs) {

  for (auto stream_buffer : stream_buffs) {
    if (stream_buffer.fd == -1 || stream_buffer.data == nullptr) {
      QMMF_ERROR("%s:%s buffer FD %d address %p", TAG, __func__,
          stream_buffer.fd, stream_buffer.data);
      return BAD_VALUE;
    }

    uint32_t offset = 0;
    std::vector<BufferPlane> planes;
    for (uint32_t i = 0; i < stream_buffer.info.num_planes; i++) {
      BufferPlane plane(stream_buffer.info.plane_info[i].width,
                        stream_buffer.info.plane_info[i].height,
                        stream_buffer.info.plane_info[i].stride,
                        offset,
                        stream_buffer.info.plane_info[i].scanline *
                            stream_buffer.info.plane_info[i].stride);
      planes.push_back(plane);
      offset += stream_buffer.info.plane_info[i].scanline *
          stream_buffer.info.plane_info[i].stride;
    }

    AlgBuffer buf(reinterpret_cast<uint8_t*>(stream_buffer.data),
                  stream_buffer.fd,
                  stream_buffer.size,
                  false,
                  GetAlgFormat(stream_buffer.info.format),
                  stream_buffer.timestamp,
                  stream_buffer.frame_number,
                  planes);

    algo_buffs.push_back(buf);

    std::lock_guard<std::mutex> lock(buffs_lock_);

    // store stream buffer because we need to return this buffer to upper layer
    if (buffs_.count(stream_buffer.fd) != 0) {
      QMMF_ERROR("%s:%s Failed to add FD %d", TAG, __func__, stream_buffer.fd);
      return BAD_VALUE;
    }
    buffs_[stream_buffer.fd] = stream_buffer;
  }

  return NO_ERROR;
}

StreamBuffer PostProcAlg::GetStreamBuffer(const AlgBuffer &algo_buf) {
  std::lock_guard<std::mutex> lock(buffs_lock_);

  int32_t fd = algo_buf.fd_;
  if (buffs_.count(fd) == 0) {
    QMMF_ERROR("%s:%s Failed to find entry for FD %d ", TAG, __func__, fd);
    assert(0);
  }

  StreamBuffer stream_buf = buffs_[fd];
  buffs_.erase(fd);

  return stream_buf;
}

}; // namespace recoder

}; // namespace qmmf
