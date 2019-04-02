/*
 * Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
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

#pragma once

#include "interface/qmmf_postproc_module.h"

#include "common/jpeg-encoder/qmmf_jpeg_encoder.h"

#include <unordered_map>

namespace qmmf {

using namespace reprocjpegencoder;

namespace recorder {

class PostProcJpeg : public IPostProcModule {

 public:

  PostProcJpeg();

  ~PostProcJpeg();

  status_t Initialize(const PostProcIOParam &in_param,
                      const PostProcIOParam &out_param) override;

  status_t Delete() override;

  void SetCallbacks(IPostProcEventListener *cb) override {listener_ = cb;};

  status_t Configure(const std::string config_json_data) override;

  status_t GetConfig(std::string &config_json_data) override;

  status_t Process(const std::vector<StreamBuffer> &in_buffers,
                   const std::vector<StreamBuffer> &out_buffers) override;

  void AddResult(const void* result) override;

  status_t ReturnBuff(StreamBuffer &buffer) override { return NO_ERROR; };

  status_t Start() override;

  status_t Stop() override;

  status_t Abort(std::shared_ptr<void> &abort) override;

  PostProcIOParam GetInput(const PostProcIOParam &out) override;

  status_t ValidateOutput(const PostProcIOParam &output) override;

  status_t GetCapabilities(PostProcCaps &caps) override;

 private:

  enum class State {
    CREATED,
    INITIALIZED,
    ACTIVE,
    RUNING,
    ABORTED
  };

  static const int32_t kBufCount = 3; // count for buffer rotation

  std::map<uint32_t, CameraMetadata> results_;
  std::list<uint32_t> f_id_;

  reprocjpegencoder::JpegEncoder *jpeg_encoder_;
  IPostProcEventListener         *listener_;

  reprocjpegencoder::JpegEncoder::encode_params jpeg_params_;
  uint32_t                       image_width_;
  uint32_t                       image_height_;

  std::mutex                     state_lock_;
  State                          state_;
  std::shared_ptr<void>          abort_;

  QCondition                     wait_for_result_;
  std::mutex                     result_lock_;

  static const uint32_t          kMinWidth;
  static const uint32_t          kMinHeight;
  static const uint32_t          kMaxWidth;
  static const uint32_t          kMaxHeight;

  static const int32_t           kSupportedInputFormat;
  static const int32_t           kSupportedOutputFormat;
  static const int32_t           kWaitJPEGTimeout;
  static const int32_t           kMetaHistory;
};

}; //namespace recorder

}; //namespace qmmf
