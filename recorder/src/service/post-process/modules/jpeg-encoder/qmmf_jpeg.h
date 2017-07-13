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

#pragma once

#include <utils/Condition.h>
#include <utils/KeyedVector.h>

#include "../../interface/qmmf_postproc_module.h"

#include "qmmf_jpeg_core.h"

namespace qmmf {

namespace recorder {

class PostProcJpeg : public IPostProcModule {

 public:

  PostProcJpeg(int32_t Id);

  ~PostProcJpeg();

  status_t Create(const int32_t stream_id,
                  const PostProcCreateParam& input,
                  const PostProcCreateParam& output,
                  const uint32_t frame_rate,
                  const uint32_t num_images,
                  const void* static_meta,
                  const void* context,
                  int32_t &out_stream_id) override;

  status_t Delete() override;

  void SetCallbacks(IPostProcEventListener *cb) override {listener_ = cb;};

  status_t Configure(const std::string config_json_data) override;

  status_t Process(const std::vector<StreamBuffer> &in_buffers,
                   const std::vector<StreamBuffer> &out_buffers) override;

  void AddResult(const void* result) override {};

  status_t ReturnBuff(StreamBuffer &buffer) override { return NO_ERROR; };

  status_t Start() override;

  status_t Stop() override;

  PostProcCreateParam GetInput(const PostProcCreateParam &out) override;

  PostProcCreateParam GetOutput(const PostProcCreateParam &in) override;

  status_t GetCapabilities(PostProcCaps &caps) override;

 private:

  int32_t                        id_;

  bool                           reprocess_flag_;
  bool                           ready_to_start_;

  reprocjpegencoder::JpegEncoder *jpeg_encoder_;
  IPostProcEventListener         *listener_;

};

}; //namespace recorder

}; //namespace qmmf
