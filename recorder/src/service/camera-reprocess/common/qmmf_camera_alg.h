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

#include <string>

#include <qmmf-alg/qmmf_alg_intf.h>

#include "recorder/src/service/camera-reprocess/interface/qmmf_camera_reprocess.h"

namespace qmmf {

namespace recorder {

class CameraAlg {

 public:

   CameraAlg(const char *alg_lib_path);

   virtual ~CameraAlg();

   status_t Init(void **handle, qmmf_alg_blob_t *calibration_data) {
     auto ret = NO_ERROR;
     KpiLog(__func__, "Start");
     auto status = alg_lib_.init(handle, calibration_data);
     KpiLog(__func__, "End");
     if (status != QMMF_ALG_SUCCESS) {
       QMMF_ERROR("%s: Error! (%d)\n", __func__, status);
       ret = BAD_VALUE;
     }
     return ret;
   };

   void Deinit(void *handle) {
     KpiLog(__func__, "Start");
     alg_lib_. deinit(handle);
     KpiLog(__func__, "End");
   };

   status_t GetCaps(void *handle, qmmf_alg_caps_t *caps) {
     auto ret = NO_ERROR;
     KpiLog(__func__, "Start");
     auto status = alg_lib_.get_caps(handle, caps);
     KpiLog(__func__, "End");
     if (status != QMMF_ALG_SUCCESS) {
       QMMF_ERROR("%s: Error! (%d)\n", __func__, status);
       ret = BAD_VALUE;
     }
     return ret;
   };

   status_t SetTuning(void *handle, qmmf_alg_blob_t *blob) {
     auto ret = NO_ERROR;
     KpiLog(__func__, "Start");
     auto status = alg_lib_.set_tuning(handle, blob);
     KpiLog(__func__, "End");
     if (status != QMMF_ALG_SUCCESS) {
       QMMF_ERROR("%s: Error! (%d)\n", __func__, status);
       ret = BAD_VALUE;
     }
     return ret;
   };

   status_t Config(void *handle, qmmf_alg_config_t *config) {
     auto ret = NO_ERROR;
     KpiLog(__func__, "Start");
     auto status = alg_lib_.config(handle, config);
     KpiLog(__func__, "End");
     if (status != QMMF_ALG_SUCCESS) {
       QMMF_ERROR("%s: Error! (%d)\n", __func__, status);
       ret = BAD_VALUE;
     }
     return ret;
   };

   status_t RegisterBufs(void *handle, qmmf_alg_buf_list_t bufs) {
     auto ret = NO_ERROR;
     KpiLog(__func__, "Start");
     auto status = alg_lib_.register_bufs(handle, bufs);
     KpiLog(__func__, "End");
     if (status != QMMF_ALG_SUCCESS) {
       QMMF_ERROR("%s: Error! (%d)\n", __func__, status);
       ret = BAD_VALUE;
     }
     return ret;
   };


   status_t UnregisterBufs(void *handle, qmmf_alg_buf_list_t bufs) {
     auto ret = NO_ERROR;
     KpiLog(__func__, "Start");
     auto status = alg_lib_.unregister_bufs(handle, bufs);
     KpiLog(__func__, "End");
     if (status != QMMF_ALG_SUCCESS) {
       QMMF_ERROR("%s: Error! (%d)\n", __func__, status);
       ret = BAD_VALUE;
     }
     return ret;
   };

   status_t Flush(void *handle) {
     auto ret = NO_ERROR;
     KpiLog(__func__, "Start");
     auto status = alg_lib_.flush(handle);
     KpiLog(__func__, "End");
     if (status != QMMF_ALG_SUCCESS) {
       QMMF_ERROR("%s: Error! (%d)\n", __func__, status);
       ret = BAD_VALUE;
     }
     return ret;
   };

   status_t AlgProcess(void *handle, qmmf_alg_process_data_t *proc_data) {
     auto ret = NO_ERROR;
     KpiLog(__func__, "Start");
     auto status = alg_lib_.process(handle, proc_data);
     KpiLog(__func__, "End");
     if (status != QMMF_ALG_SUCCESS) {
       QMMF_ERROR("%s: Error! (%d)\n", __func__, status);
       ret = BAD_VALUE;
     }
     return ret;
   };

   status_t GetDebugInfoLog(void *handle, char **log) {
     auto ret = NO_ERROR;
     KpiLog(__func__, "Start");
     auto status = alg_lib_.get_debug_info_log(handle, log);
     KpiLog(__func__, "End");
     if (status != QMMF_ALG_SUCCESS) {
       QMMF_ERROR("%s: Error! (%d)\n", __func__, status);
       ret = BAD_VALUE;
     }
     return ret;
   };

 private:

  inline void KpiLog(const char* func, const char* name) {
    if (kpi_debug_) {
      QMMF_ERROR("%s: [KPI] alg: %s (%s)\n", func, kAlgName_, name);
    }
  }

  struct AlgInterface {
    void        *handle;
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

  AlgInterface alg_lib_;

  const char *kAlgName_;
  uint32_t    kpi_debug_;
};

}  // namespace recorder ends here

}  // namespace qmmf ends here
