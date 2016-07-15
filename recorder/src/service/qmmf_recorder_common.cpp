/*
* Copyright (c) 2016, The Linux Foundation. All rights reserved.
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

#define TAG "RecorderCommon"

#include "qmmf_recorder_common.h"

namespace qmmf {

namespace recorder {

extern "C" void DebugCameraStartParams (const char* _func_,
                                        CameraStartParam* params)
{
  QMMF_INFO("%s: zsl_mode = %d", _func_, params->zsl_mode);
  QMMF_INFO("%s: zsl_queue_depth = %d", _func_, params->zsl_queue_depth);
  QMMF_INFO("%s: zsl_width = %d", _func_, params->zsl_width);
  QMMF_INFO("%s: zsl_height = %d", _func_, params->zsl_height);
  QMMF_INFO("%s: frame_rate = %d", _func_, params->frame_rate);
  QMMF_INFO("%s: flags = %d", _func_, params->flags);
}

extern "C" void DebugVideoTrackCreateParam (const char* _func_,
                                            VideoTrackCreateParam* params)
{
  QMMF_INFO("%s: num_cameras = %d", _func_, params->num_cameras);
  for(uint8_t i = 0; i < params->num_cameras; i++) {
    QMMF_INFO("%s: camera_id[%d]", _func_, params->camera_ids[i]);
  }
  QMMF_INFO("%s: width = %d", _func_, params->width);
  QMMF_INFO("%s: height = %d", _func_, params->height);
  QMMF_INFO("%s: frame_rate = %d", _func_, params->frame_rate);
  QMMF_INFO("%s: codec_type = %d", _func_, params->codec_type);
  QMMF_INFO("%s: out_device = %d", _func_, params->out_device);
  #if 0
  QMMF_INFO("%s:%s: param.num_cameras = %d", TAG, __func__, param.num_cameras);
    for(uint32_t i = 0; i < param.num_cameras; i++) {
        QMMF_INFO("%s:%s: param.camera_ids[%d]=%d", TAG, __func__, i, param.camera_ids[i]);
    }
    QMMF_INFO("%s:%s: param.width = %d", TAG, __func__, param.width);
    QMMF_INFO("%s:%s: param.height = %d", TAG, __func__, param.height);
    QMMF_INFO("%s:%s: param.codec_type = %d", TAG, __func__, param.codec_type);
    QMMF_INFO("%s:%s: param.out_device = %d", TAG, __func__, param.out_device);
    QMMF_INFO("%s:%s: param.frame_rate = %d", TAG, __func__, param.frame_rate);
    QMMF_INFO("%s:%s: param.codec_param.avc.idr_interval = %d", TAG, __func__, param.codec_param.avc.idr_interval);
    QMMF_INFO("%s:%s: param.codec_param.avc.bitrate = %d", TAG, __func__, param.codec_param.avc.bitrate);
    QMMF_INFO("%s:%s: param.codec_param.avc.profile = %d", TAG, __func__, param.codec_param.avc.profile);
    QMMF_INFO("%s:%s: param.codec_param.avc.level = %d", TAG, __func__, param.codec_param.avc.level);
    QMMF_INFO("%s:%s: param.codec_param.avc.ratecontrol_type = %d", TAG, __func__, param.codec_param.avc.ratecontrol_type);
    QMMF_INFO("%s:%s: param.avc.qp_params.enable_init_qp = %d", TAG, __func__, param.codec_param.avc.qp_params.enable_init_qp);
    QMMF_INFO("%s:%s: param.codec_param.avc.qp_params.init_qp.init_IQP = %d", TAG, __func__, param.codec_param.avc.qp_params.init_qp.init_IQP);
    QMMF_INFO("%s:%s: param.codec_param.avc.qp_params.init_qp.init_PQP = %d", TAG, __func__, param.codec_param.avc.qp_params.init_qp.init_PQP);
    QMMF_INFO("%s:%s: param.codec_param.avc.qp_params.init_qp.init_BQP = %d", TAG, __func__, param.codec_param.avc.qp_params.init_qp.init_BQP);
    QMMF_INFO("%s:%s: param.codec_param.avc.qp_params.init_qp.init_QP_mode = %d", TAG, __func__, param.codec_param.avc.qp_params.init_qp.init_QP_mode);
    QMMF_INFO("%s:%s: param.codec_param.avc.qp_params.enable_qp_range = %d", TAG, __func__, param.codec_param.avc.qp_params.enable_qp_range);
    QMMF_INFO("%s:%s: param.codec_param.avc.qp_params.qp_range.min_QP = %d", TAG, __func__, param.codec_param.avc.qp_params.qp_range.min_QP);
    QMMF_INFO("%s:%s: param.codec_param.avc.qp_params.qp_range.max_QP = %d", TAG, __func__, param.codec_param.avc.qp_params.qp_range.max_QP);
    QMMF_INFO("%s:%s: param.codec_param.avc.qp_params.enable_qp_IBP_range = %d", TAG, __func__, param.codec_param.avc.qp_params.enable_qp_IBP_range);
    QMMF_INFO("%s:%s: param.codec_param.avc.qp_params.qp_IBP_range.min_IQP = %d", TAG, __func__, param.codec_param.avc.qp_params.qp_IBP_range.min_IQP);
    QMMF_INFO("%s:%s: param.codec_param.avc.qp_params.qp_IBP_range.max_IQP = %d", TAG, __func__, param.codec_param.avc.qp_params.qp_IBP_range.max_IQP);
    QMMF_INFO("%s:%s: param.codec_param.avc.qp_params.qp_IBP_range.min_PQP = %d", TAG, __func__, param.codec_param.avc.qp_params.qp_IBP_range.min_PQP);
    QMMF_INFO("%s:%s: param.codec_param.avc.qp_params.qp_IBP_range.max_PQP = %d", TAG, __func__, param.codec_param.avc.qp_params.qp_IBP_range.max_PQP);
    QMMF_INFO("%s:%s: param.codec_param.avc.qp_params.qp_IBP_range.min_BQP = %d", TAG, __func__, param.codec_param.avc.qp_params.qp_IBP_range.min_BQP);
    QMMF_INFO("%s:%s: param.codec_param.avc.qp_params.qp_IBP_range.max_BQP = %d", TAG, __func__, param.codec_param.avc.qp_params.qp_IBP_range.max_BQP);
  #endif
}

extern "C" void DebugVideoTrackParams (const char* _func_,
                                       VideoTrackParams* params)
{
  QMMF_INFO("%s: track_id = %d", _func_, params->track_id);
  QMMF_INFO("%s: num_cameras = %d", _func_, params->camera_ids.size());
  for(uint32_t i; i < params->camera_ids.size(); i++) {
    QMMF_INFO("%s: camera_id[%d]", _func_, params->camera_ids[i]);
  }
  QMMF_INFO("%s: width = %d", _func_, params->width);
  QMMF_INFO("%s: height = %d", _func_, params->height);
  QMMF_INFO("%s: frame_rate = %d", _func_, params->frame_rate);
  QMMF_INFO("%s: codec_type = %d", _func_, params->codec_type);
  QMMF_INFO("%s: camera_stream_type = %d", _func_, params->camera_stream_type);
}

}; //namespace recorder.

}; //namespace qmmf.