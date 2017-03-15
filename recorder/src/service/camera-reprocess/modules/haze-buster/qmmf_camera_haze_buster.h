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

#include <qmmf-alg/qmmf_alg_intf.h>

#include <utils/KeyedVector.h>
#include <utils/Log.h>

#include "common/qmmf_common_utils.h"

#include "../../interface/qmmf_camera_module.h"

namespace qmmf {

namespace recorder {

class CameraHazeBuster : public Callbacks,
                         public ICameraModule {

 public:

  CameraHazeBuster(int32_t Id);

  ~CameraHazeBuster();

  status_t Create(const int32_t stream_id,
                  const ReprocParam& input,
                  const ReprocParam& output,
                  const uint32_t frame_rate,
                  const uint32_t num_images,
                  const void* static_meta,
                  const void* context) override;

  status_t Delete() override;

  bool Process(StreamBuffer& in_buffer, StreamBuffer& out_buffer) override;

  void AddResult(const void* result) override;

  status_t ReturnBuff(StreamBuffer buffer) override;

  status_t GetCapabilities(ReprocCaps *caps) override;

  status_t Start() override;

  status_t Stop() override;

 private:

  struct HazeBusterLibInterface {
    void        *handle;
    void        *context;
    bool        configured;
    qmmf_alg_status_t (*init)(void **handle,
                              qmmf_alg_blob_t *calibration_data);
    void              (*deinit)(void *handle);
    qmmf_alg_status_t (*get_caps)(void *handle, qmmf_alg_caps_t *caps);
    qmmf_alg_status_t (*set_tuning)(void *handle, qmmf_alg_blob_t *blob);
    qmmf_alg_status_t (*config)(void *handle, qmmf_alg_config_t *config);
    qmmf_alg_status_t (*register_bufs)(void *handle, qmmf_alg_buf_list_t bufs);
    qmmf_alg_status_t (*unregister_bufs)(void *handle,
                                         qmmf_alg_buf_list_t bufs);
    qmmf_alg_status_t (*flush)(void *handle);
    qmmf_alg_status_t (*process)(void *handle,
                                 qmmf_alg_process_data_t *proc_data);
    qmmf_alg_status_t (*get_debug_info_log)(void *handle, char **log);
  };

  int32_t                id_;

  bool                   reprocess_flag_;
  bool                   ready_to_start_;

  HazeBusterLibInterface hazebuster_lib_;

};

}; //namespace recorder

}; //namespace qmmf
