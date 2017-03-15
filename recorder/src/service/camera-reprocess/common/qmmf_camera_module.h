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
#include <utils/Log.h>

#include "common/qmmf_common_utils.h"

#include "../interface/qmmf_camera_module.h"
#include "../interface/qmmf_camera_reprocess.h"
#include "../common/qmmf_camera_thread.h"
#include "../factory/qmmf_camera_factory.h"
#include "../interface/qmmf_camera_module.h"
#include "../interface/qmmf_camera_reprocess.h"

namespace qmmf {

namespace recorder {

class CameraModule : public Callbacks,
                     public CameraThread,
                     public ICameraReprocess {

 public:

  CameraModule(String8 name, IPostProcCameraContext* context);

  ~CameraModule();

  status_t Create(const int32_t stream_id,
                  const ReprocParam& input,
                  const ReprocParam& output,
                  const uint32_t frame_rate,
                  const uint32_t num_images,
                  const void* static_meta,
                  const void* context) override;

  status_t Delete() override;

  void AddBuff(StreamBuffer in_buffer) override;

  void AddResult(const void* result) override;

  status_t ReturnBuff(StreamBuffer buffer) override;

  status_t Start() override;

  status_t Stop() override;

  void GetCapabilities(ReprocCaps *caps) {*caps = caps_;}

 private:

  struct map_data_t {
    void* addr;
    size_t size;
  };

  void* MapBuff(StreamBuffer& buffer);

  bool ThreadLoop() override;

  String8                name_;
  int32_t                id_;

  bool                   reprocess_flag_;
  bool                   ready_to_start_;

  Condition              wait_for_buffer_;
  Mutex                  wait_lock_;
  List<StreamBuffer>     input_buffer_;

  static const nsecs_t kFrameTimeout  = 50000000;  // 50 ms.

  KeyedVector<uint32_t, map_data_t> mapped_buffs_;

  sp<ICameraModule>             camera_reprocess_;
  sp<ReprocessFactory>          reprocess_factory_;

  ReprocCaps                    caps_;

};

}; //namespace recorder

}; //namespace qmmf
