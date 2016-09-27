/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *     Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *     Neither the name of The Linux Foundation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.

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

#define TAG "QMMF_AVCodec"

#include <iomanip>
#include <string>
#include <sstream>

#include "qmmf_avcodec.h"

namespace qmmf {

using ::std::setbase;
using ::std::string;
using ::std::stringstream;

struct __attribute__((packed)) AudioEncoderMetadata {
  uint32_t offset_to_frame;
  uint32_t frame_size;
  uint32_t encoded_pcm_samples;
  uint32_t lsw_ts;
  uint32_t msw_ts;
  uint32_t nflags;

  string ToString() const {
    stringstream stream;
    stream << "offset_to_frame[" << offset_to_frame << "] ";
    stream << "frame_size[" << frame_size << "] ";
    stream << "encoded_pcm_samples[" << encoded_pcm_samples << "] ";
    stream << "lsw_ts[" << lsw_ts << "] ";
    stream << "msw_ts[" << msw_ts << "] ";
    stream << "nflags[" << ::std::setbase(16) << nflags << ::std::setbase(10)
           << "]";
    return stream.str();
  }
};

// static
OMX_CALLBACKTYPE AVCodec::callbacks_ = {
    &OnEvent, &OnEmptyBufferDone, &OnFillBufferDone};

AVCodec::AVCodec()
    : state_(OMX_StateLoaded),
      state_pending_(OMX_StateLoaded),
      input_stop_(false),
      output_stop_(false),
      port_status_(true),
      cmd_buffer_index_(0),
      signal_queue_(CMD_BUF_MAX_COUNT) {

  QMMF_INFO("%s:%s Enter", TAG, __func__);

  omx_client_ = new OmxClient();
  if(nullptr == omx_client_.get())
      QMMF_ERROR("%s:%s OMX-IL createtion failed", TAG, __func__);
  else
      QMMF_INFO("%s:%s created OMX-IL instance(%p)", TAG, __func__,
          omx_client_.get());

  QMMF_INFO("%s:%s Exit", TAG, __func__);
}

AVCodec::~AVCodec() {

  QMMF_INFO("%s:%s Enter", TAG, __func__);

  if(omx_client_.get()) {
      DeleteHandle();
  }

  state_ = OMX_StateInvalid;
  state_pending_ = OMX_StateInvalid;
  input_source_ = nullptr;
  output_source_ = nullptr;
  if(in_buff_hdr_) {
    delete []in_buff_hdr_;
    in_buff_hdr_ = nullptr;
  }
  if(out_buff_hdr_) {
    delete []out_buff_hdr_;
    out_buff_hdr_ = nullptr;
  }
  cmd_buffer_index_ = 0;
  signal_queue_.Clear();

  QMMF_INFO("%s:%s Exit", TAG, __func__);
}

status_t AVCodec::CreateHandle(char* component_name) {

  QMMF_INFO("%s:%s Enter", TAG, __func__);
  status_t ret = 0;

  ret = omx_client_->CreateOmxHandle(component_name, this, callbacks_);
  if(ret != OK) {
    QMMF_ERROR("%s:%s failed to create OMX component(%s)", TAG, __func__,
        component_name);
    return ret;
  }

  QMMF_INFO("%s:%s Exit", TAG, __func__);
  return ret;
}

status_t AVCodec::DeleteHandle() {

  QMMF_INFO("%s:%s Enter", TAG, __func__);
  status_t ret = 0;

  if((state_ != OMX_StateLoaded)) {
      QMMF_INFO("%s:%s Move state to Loaded state", TAG, __func__);
      ret = SetState(OMX_StateLoaded, OMX_TRUE);
      assert(ret == OK);
  }

  ret = omx_client_->ReleaseOmxHandle();
  assert(ret == OK);

  QMMF_INFO("%s:%s Exit", TAG, __func__);
  return ret;
}

status_t AVCodec::GetComponentRole(char* role, uint32_t *num_comps,
                                   OMX_U8 **comp_names) {

  QMMF_INFO("%s:%s Enter", TAG, __func__);
  status_t ret;

  ret = omx_client_->GetComponentsOfRole(role, num_comps, comp_names);
  if(ret != OK) {
    QMMF_ERROR("%s:%s failed to get component role(%s)", TAG, __func__, role);
    return ret;
  }

  QMMF_INFO("%s:%s Exit", TAG, __func__);
  return ret;
}

status_t AVCodec::ConfigureCodec(CodecType format_type,
                                 CodecCreateParam& codec_param) {

  QMMF_INFO("%s:%s Enter", TAG, __func__);
  status_t ret = 0;

  String8 component_name;

  switch(format_type) {
    case CodecType::kVideoEncoder:
      switch(codec_param.video_param.format_type) {
        case VideoFormat::kAVC:
          component_name.appendFormat("OMX.qcom.video.encoder.avc");
          break;
        case VideoFormat::kHEVC:
          component_name.appendFormat("OMX.qcom.video.encoder.hevc");
          break;
        //TODO: JPEG/YUV/Bayer
        default:
          QMMF_ERROR("%s:%s Unknown Video Codec", TAG, __func__);
          return -1;
        }
      break;
    case CodecType::kVideoDecoder:
      break;
    case CodecType::kAudioEncoder:
      switch(codec_param.audio_param.format) {
        case AudioFormat::kAAC:
          component_name.appendFormat("OMX.qcom.audio.encoder.aac");
          break;
        case AudioFormat::kAMR:
          if (codec_param.audio_param.codec_params.amr.isWAMR)
            component_name.appendFormat("OMX.qcom.audio.encoder.amrwb");
          else
            component_name.appendFormat("OMX.qcom.audio.encoder.amrnb");
          break;
        case AudioFormat::kG711:
          switch(codec_param.audio_param.codec_params.g711.mode) {
            case G711Mode::kALaw:
              component_name.appendFormat("OMX.qcom.audio.encoder.g711alaw");
              break;
            case G711Mode::kMuLaw:
              component_name.appendFormat("OMX.qcom.audio.encoder.g711mlaw");
              break;
            default:
              QMMF_ERROR("%s:%s Unknown Audio Codec", TAG, __func__);
              return -1;
          }
          break;
        default:
          QMMF_ERROR("%s:%s Unknown Audio Codec", TAG, __func__);
          return -1;
        }
      break;
    case CodecType::kAudioDecoder:
      break;
    case CodecType::kImageEncoder:
      break;
    case CodecType::kImageDecoder:
      break;
    default:
      QMMF_ERROR("%s:%s Unimplemented requested Codec", TAG, __func__);
      return -1;
  }

  ret = CreateHandle(const_cast<char *>(component_name.string()));
  if(ret != OK) {
      QMMF_ERROR("%s:%s failed to create omx handle", TAG, __func__);
      return ret;
  }

  if(format_type == CodecType::kVideoEncoder) {
    ret = ConfigureVideoEncoder(codec_param);
  }
  else if(format_type == CodecType::kAudioEncoder) {
    ret = ConfigureAudioEncoder(codec_param);
  }
  else {
    QMMF_ERROR("%s:%s codec type not implemented", TAG, __func__);
    ret = -1;
  }

  format_type_ = format_type;
  event_cb_ = codec_param.event_cb;
  // Set Component to Idle state
  ret = SetState(OMX_StateIdle, OMX_FALSE);

  QMMF_INFO("%s:%s Exit", TAG, __func__);
  return ret;
}

status_t AVCodec::ConfigureVideoEncoder(CodecCreateParam& codec_param) {

  QMMF_INFO("%s:%s Enter", TAG, __func__);
  status_t ret = 0;

  bool enable_init_qp = false;
  bool enable_qp_range = false;
  bool enable_qp_IBP_range = false;
  uint32_t width = codec_param.video_param.width;
  uint32_t height = codec_param.video_param.height;
  uint32_t frame_rate = codec_param.video_param.frame_rate;
  uint32_t init_IQP, init_PQP, init_BQP;
  uint32_t min_QP, max_QP;
  uint32_t min_IQP, max_IQP,  min_PQP, max_PQP, min_BQP, max_BQP;
  uint32_t ltr_count, hier_num_layer;
  VideoRateControlType rate_control;

  //Prepend SPS/PPS to IDR frames
  PrependSPSPPSToIDRFramesParams param;
  memset(&param, 0, sizeof(PrependSPSPPSToIDRFramesParams));
  param.nSize = sizeof(PrependSPSPPSToIDRFramesParams);
  param.bEnable = OMX_FALSE;
  ret = omx_client_->SetParameter(
            (OMX_INDEXTYPE)OMX_QcomIndexParamSequenceHeaderWithIDR,
            (OMX_PTR)&param);
  if (ret != OK) {
      QMMF_ERROR("%s:%s Failed to configure in band sps/pps", TAG, __func__);
      return ret;
  }

  ret = SetPortParams(kPortIndexInput, width, height, frame_rate);
  if (ret != OK) {
    QMMF_ERROR("%s:%s Failed to set port definiton on %s", TAG, __func__,
        OMX_PORT_NAME(kPortIndexInput));
    return ret;
  }

  ret = SetPortParams(kPortIndexOutput, width, height, frame_rate);
  if (ret != OK) {
    QMMF_ERROR("%s:%s Failed to set port definiton on %s", TAG, __func__,
        OMX_PORT_NAME(kPortIndexOutput));
    return ret;
  }

  switch(codec_param.video_param.format_type) {
    case VideoFormat::kAVC:
      enable_init_qp =
        codec_param.video_param.codec_param.avc.qp_params.enable_init_qp;
      enable_qp_range =
        codec_param.video_param.codec_param.avc.qp_params.enable_qp_range;
      enable_qp_IBP_range =
        codec_param.video_param.codec_param.avc.qp_params.enable_qp_IBP_range;
      init_IQP =
        codec_param.video_param.codec_param.avc.qp_params.init_qp.init_IQP;
      init_PQP =
        codec_param.video_param.codec_param.avc.qp_params.init_qp.init_PQP;
      init_BQP =
        codec_param.video_param.codec_param.avc.qp_params.init_qp.init_BQP;
      min_QP =
        codec_param.video_param.codec_param.avc.qp_params.qp_range.min_QP;
      max_QP =
        codec_param.video_param.codec_param.avc.qp_params.qp_range.max_QP;
      min_IQP =
        codec_param.video_param.codec_param.avc.qp_params.qp_IBP_range.min_IQP;
      max_IQP =
        codec_param.video_param.codec_param.avc.qp_params.qp_IBP_range.max_IQP;
      min_PQP =
        codec_param.video_param.codec_param.avc.qp_params.qp_IBP_range.min_PQP;
      max_PQP=
        codec_param.video_param.codec_param.avc.qp_params.qp_IBP_range.max_PQP;
      min_BQP =
        codec_param.video_param.codec_param.avc.qp_params.qp_IBP_range.min_BQP;
      max_BQP =
        codec_param.video_param.codec_param.avc.qp_params.qp_IBP_range.max_BQP;
      ltr_count = codec_param.video_param.codec_param.avc.ltr_count;
      hier_num_layer = codec_param.video_param.codec_param.avc.hier_layer;
      rate_control = codec_param.video_param.codec_param.avc.ratecontrol_type;

      ret = SetupAVCEncoderParameters(codec_param);
      break;
    case VideoFormat::kHEVC:
      enable_init_qp =
        codec_param.video_param.codec_param.hevc.qp_params.enable_init_qp;
      enable_qp_range =
        codec_param.video_param.codec_param.hevc.qp_params.enable_qp_range;
      enable_qp_IBP_range =
        codec_param.video_param.codec_param.hevc.qp_params.enable_qp_IBP_range;
      init_IQP =
        codec_param.video_param.codec_param.hevc.qp_params.init_qp.init_IQP;
      init_PQP =
        codec_param.video_param.codec_param.hevc.qp_params.init_qp.init_PQP;
      init_BQP =
        codec_param.video_param.codec_param.hevc.qp_params.init_qp.init_BQP;
      min_QP =
        codec_param.video_param.codec_param.hevc.qp_params.qp_range.min_QP;
      max_QP =
        codec_param.video_param.codec_param.hevc.qp_params.qp_range.max_QP;
      min_IQP =
        codec_param.video_param.codec_param.hevc.qp_params.qp_IBP_range.min_IQP;
      max_IQP =
        codec_param.video_param.codec_param.hevc.qp_params.qp_IBP_range.max_IQP;
      min_PQP =
        codec_param.video_param.codec_param.hevc.qp_params.qp_IBP_range.min_PQP;
      max_PQP=
        codec_param.video_param.codec_param.hevc.qp_params.qp_IBP_range.max_PQP;
      min_BQP =
        codec_param.video_param.codec_param.hevc.qp_params.qp_IBP_range.min_BQP;
      max_BQP =
        codec_param.video_param.codec_param.hevc.qp_params.qp_IBP_range.max_BQP;
      ltr_count = codec_param.video_param.codec_param.hevc.ltr_count;
      hier_num_layer = codec_param.video_param.codec_param.hevc.hier_layer;
      rate_control = codec_param.video_param.codec_param.hevc.ratecontrol_type;

      ret = SetupHEVCEncoderParameters(codec_param);
      break;
    default:
      QMMF_ERROR("%s:%s Codec Type does not support", TAG, __func__);
      return -1;
  }

  if(ret != OK) {
    QMMF_ERROR("%s:%s Failed to set up codec parameter", TAG, __func__);
    return ret;
  }

  //SetUp QP parameter.
  if(enable_init_qp) {
    if(rate_control != VideoRateControlType::kDisable) {
      // RC ON
      QOMX_EXTNINDEX_VIDEO_INITIALQP initqp;
      InitOMXParams(&initqp);
      initqp.nPortIndex = kPortIndexOutput;
      initqp.nQpI = init_IQP;
      initqp.nQpP = init_PQP;
      initqp.nQpB = init_BQP;
      initqp.bEnableInitQp = 0x7; // Intial QP applied to all frame
      ret = omx_client_->SetParameter(
                (OMX_INDEXTYPE)QOMX_IndexParamVideoInitialQp,
                (OMX_PTR)&initqp);
      if (ret != OK) {
        QMMF_ERROR("%s:%s Failed to set Initial QP parameter", TAG, __func__);
        return ret;
      }
    } else {
      // RC OFF
      OMX_VIDEO_PARAM_QUANTIZATIONTYPE initqp;
      InitOMXParams(&initqp);
      initqp.nPortIndex = kPortIndexOutput;
      initqp.nQpI = init_IQP;
      initqp.nQpP = init_PQP;
      initqp.nQpB = init_BQP;
      ret = omx_client_->SetParameter(
                (OMX_INDEXTYPE)OMX_IndexParamVideoQuantization,
                (OMX_PTR)&initqp);
      if (ret != OK) {
        QMMF_ERROR("%s:%s Failed to set Initial QP parameter", TAG, __func__);
        return ret;
      }
    }
  }

  if(enable_qp_range) {
    OMX_QCOM_VIDEO_PARAM_QPRANGETYPE qp_range;
    InitOMXParams(&qp_range);
    qp_range.nPortIndex = kPortIndexOutput;
    ret = omx_client_->GetParameter(
              (OMX_INDEXTYPE)OMX_QcomIndexParamVideoQPRange,
              (OMX_PTR)&qp_range);
    if (ret != OK) {
      QMMF_ERROR("%s:%s Failed to get QP Min/Max Range", TAG, __func__);
      return ret;
    }
    qp_range.minQP = min_QP;
    qp_range.maxQP = max_QP;

    ret = omx_client_->SetParameter(
              (OMX_INDEXTYPE)OMX_QcomIndexParamVideoQPRange,
              (OMX_PTR)&qp_range);
    if (ret != OK) {
      QMMF_ERROR("%s:%s Failed to set QP Min/Max Range", TAG, __func__);
      return ret;
    }
  }

  if(enable_qp_IBP_range) {
    OMX_QCOM_VIDEO_PARAM_IPB_QPRANGETYPE qp_range;
    InitOMXParams(&qp_range);
    qp_range.nPortIndex = kPortIndexOutput;

    ret = omx_client_->GetParameter(
              (OMX_INDEXTYPE)OMX_QcomIndexParamVideoIPBQPRange,
              (OMX_PTR)&qp_range);
    if(ret != OK) {
      QMMF_ERROR("%s:%s Failed to get IPBQP Range parameter", TAG, __func__);
      return ret;
    }
    qp_range.minIQP = min_IQP;
    qp_range.maxIQP = max_IQP;
    qp_range.minPQP = min_PQP;
    qp_range.maxPQP = max_PQP;
    qp_range.minBQP = min_BQP;
    qp_range.maxBQP = max_BQP;

    ret = omx_client_->SetParameter(
              (OMX_INDEXTYPE)OMX_QcomIndexParamVideoIPBQPRange,
              (OMX_PTR)&qp_range);
    if(ret != OK) {
      QMMF_ERROR("%s:%s Failed to set IPBQP Range parameter", TAG, __func__);
      return ret;
    }
  }

  if(ltr_count > 0) {
    QOMX_VIDEO_PARAM_LTRCOUNT_TYPE ltr_frame;
    InitOMXParams(&ltr_frame);
    ltr_frame.nPortIndex = kPortIndexOutput;

    ret = omx_client_->GetParameter((OMX_INDEXTYPE)QOMX_IndexParamVideoLTRCount,
              (OMX_PTR)&ltr_frame);
    if (ret != OK) {
        QMMF_ERROR("%s:%s Failed to get ltr count parameter", TAG, __func__);
        return ret;
    }

    ltr_frame.nCount = ltr_count;
    ret = omx_client_->SetParameter((OMX_INDEXTYPE)QOMX_IndexParamVideoLTRCount,
              (OMX_PTR)&ltr_frame);
    if (ret != OK) {
        QMMF_ERROR("%s:%s Failed to set ltr count parameter", TAG, __func__);
        return ret;
    }
  }

  if(hier_num_layer > 0) {
    QOMX_VIDEO_HIERARCHICALLAYERS hier_layer;
    InitOMXParams(&hier_layer);
    hier_layer.nPortIndex = kPortIndexOutput;

    ret = omx_client_->GetParameter(
              (OMX_INDEXTYPE)OMX_QcomIndexHierarchicalStructure,
              (OMX_PTR)&hier_layer);
    if (ret != OK) {
        QMMF_ERROR("%s:%s Failed to get hierarchial parameter", TAG, __func__);
        return ret;
    }

    hier_layer.eHierarchicalCodingType = QOMX_HIERARCHICALCODING_P;
    hier_layer.nNumLayers = hier_num_layer;

    ret = omx_client_->SetParameter(
              (OMX_INDEXTYPE)OMX_QcomIndexHierarchicalStructure,
              (OMX_PTR)&hier_layer);
    if (ret != OK) {
        QMMF_ERROR("%s:%s Failed to set hierarchial parameter", TAG, __func__);
        return ret;
    }
  }

  ret = ConfigureBitrate(codec_param);
  if(ret != OK) {
    QMMF_ERROR("%s:%s Failed to configure bitrate", TAG, __func__);
    return ret;
  }

  QMMF_INFO("%s:%s setupVideoEncoder succeeded", TAG, __func__);
  QMMF_INFO("%s:%s Exit", TAG, __func__);
  return ret;
}

status_t AVCodec::ConfigureAudioEncoder(CodecCreateParam& codec_param) {
  QMMF_DEBUG("%s: %s() TRACE", TAG, __func__);
  OMX_ERRORTYPE result;

  // get the port information
  OMX_PORT_PARAM_TYPE audio_ports;
  InitOMXParams(&audio_ports);
  result = omx_client_->GetParameter(OMX_IndexParamAudioInit,
                                     static_cast<OMX_PTR>(&audio_ports));
  if (result != OMX_ErrorNone) {
    QMMF_ERROR("%s: %s() failed to get audio_port parameters: %d", TAG,
               __func__, result);
    return ::android::FAILED_TRANSACTION;
  }
  QMMF_VERBOSE("%s: %s() audio_ports.nPorts[%u]", TAG, __func__,
               audio_ports.nPorts);
  QMMF_VERBOSE("%s: %s() audio_ports.nStartPortNumber[%u]", TAG, __func__,
               audio_ports.nStartPortNumber);

  // query the encoder input buffer requirements
  OMX_PARAM_PORTDEFINITIONTYPE input_port;
  InitOMXParams(&input_port);
  input_port.nPortIndex = audio_ports.nStartPortNumber;
  result = omx_client_->GetParameter(OMX_IndexParamPortDefinition,
                                     static_cast<OMX_PTR>(&input_port));
  if (result != OMX_ErrorNone) {
    QMMF_ERROR("%s: %s() failed to get input_port parameters: %d", TAG,
               __func__, result);
    return ::android::FAILED_TRANSACTION;
  }
  if (input_port.eDir != OMX_DirInput) {
    QMMF_ERROR("%s: %s() input_port is not configured for input", TAG,
               __func__);
    return ::android::BAD_VALUE;
  }
  QMMF_VERBOSE("%s: %s() input_port.nBufferCountMin[%u]", TAG, __func__,
               input_port.nBufferCountMin);
  QMMF_VERBOSE("%s: %s() input_port.nBufferSize[%u]", TAG, __func__,
               input_port.nBufferSize);

  // hardcode the number of input buffers
  uint32_t buf_count = INPUT_MAX_COUNT;
  if (input_port.nBufferCountActual != buf_count) {
    input_port.nBufferCountActual = buf_count;
    result = omx_client_->SetParameter(OMX_IndexParamPortDefinition,
                                       static_cast<OMX_PTR>(&input_port));
    if (result != OMX_ErrorNone) {
      QMMF_ERROR("%s: %s() failed to set new buffer count[%d] on %s", TAG,
                 __func__, input_port.nBufferCountActual,
                 OMX_PORT_NAME(input_port.nPortIndex));
      return result;
    }
    result = omx_client_->GetParameter(OMX_IndexParamPortDefinition,
                                       &input_port);
    if (result != OMX_ErrorNone) {
      QMMF_ERROR("%s: %s() failed to getParameter on %s", TAG, __func__,
                 OMX_PORT_NAME(input_port.nPortIndex));
      return result;
    }
    QMMF_DEBUG("%s: %s() new buffer specs: count[%d] size[%d]", TAG, __func__,
               input_port.nBufferCountActual, input_port.nBufferSize);
    if (buf_count != input_port.nBufferCountActual) {
      QMMF_ERROR("%s: %s() failed to confirm count on %s", TAG, __func__,
                 OMX_PORT_NAME(input_port.nPortIndex));
      return ::android::BAD_VALUE;
    }
  }
  in_buff_hdr_size_ = input_port.nBufferCountActual;

  // set the PCM input parameters
  OMX_AUDIO_PARAM_PCMMODETYPE pcm_params;
  InitOMXParams(&pcm_params);
  pcm_params.nPortIndex = kPortIndexInput;
  pcm_params.nChannels = codec_param.audio_param.channels;
  pcm_params.nSamplingRate = codec_param.audio_param.sample_rate;
  pcm_params.bInterleaved = OMX_TRUE;
  result = omx_client_->SetParameter(OMX_IndexParamAudioPcm,
                                     static_cast<OMX_PTR>(&pcm_params));
  if (result != OMX_ErrorNone) {
    QMMF_ERROR("%s: %s() failed to set PCM parameters: %d", TAG, __func__,
               result);
    return ::android::FAILED_TRANSACTION;
  }

  // query the encoder output buffer requirements
  OMX_PARAM_PORTDEFINITIONTYPE output_port;
  InitOMXParams(&output_port);
  output_port.nPortIndex = audio_ports.nStartPortNumber + 1;
  result = omx_client_->GetParameter(OMX_IndexParamPortDefinition,
                                     static_cast<OMX_PTR>(&output_port));
  if (result != OMX_ErrorNone) {
    QMMF_ERROR("%s: %s() failed to get output_port parameters: %d", TAG,
               __func__, result);
    return ::android::FAILED_TRANSACTION;
  }
  if (output_port.eDir != OMX_DirOutput) {
    QMMF_ERROR("%s: %s() output_port is not configured for output", TAG,
               __func__);
    return ::android::BAD_VALUE;
  }
  QMMF_VERBOSE("%s: %s() output_port.nBufferCountMin[%u]", TAG, __func__,
               output_port.nBufferCountMin);
  QMMF_VERBOSE("%s: %s() output_port.nBufferSize[%u]", TAG, __func__,
               output_port.nBufferSize);
  out_buff_hdr_size_ = output_port.nBufferCountActual;

  switch (codec_param.audio_param.format) {
    case AudioFormat::kAAC: {
      // set the AAC output parameters
      OMX_AUDIO_PARAM_AACPROFILETYPE aac_params;
      InitOMXParams(&aac_params);
      aac_params.nPortIndex = kPortIndexOutput;
      aac_params.nChannels = codec_param.audio_param.channels;
      aac_params.nSampleRate = codec_param.audio_param.sample_rate;
      aac_params.nBitRate = codec_param.audio_param.codec_params.aac.bit_rate;
      switch (codec_param.audio_param.channels) {
        case 1:
          aac_params.eChannelMode = OMX_AUDIO_ChannelModeMono;
          break;
        case 2:
          aac_params.eChannelMode = OMX_AUDIO_ChannelModeStereo;
          break;
        default:
          QMMF_ERROR("%s: %s() unsupported number of channels: %d", TAG,
                     __func__, codec_param.audio_param.channels);
          return ::android::BAD_VALUE;
      }
      switch (codec_param.audio_param.codec_params.aac.format) {
        case AACFormat::kADTS:
          aac_params.eAACStreamFormat = OMX_AUDIO_AACStreamFormatMP4ADTS;
          break;
        case AACFormat::kRaw:
          aac_params.eAACStreamFormat = OMX_AUDIO_AACStreamFormatRAW;
          break;
        default:
          QMMF_ERROR("%s: %s() unsupported AAC format: %d", TAG, __func__,
                     codec_param.audio_param.codec_params.aac.format);
          return ::android::BAD_VALUE;
      }
      switch (codec_param.audio_param.codec_params.aac.mode) {
        case AACMode::kAALC:
          aac_params.eAACProfile = OMX_AUDIO_AACObjectLC;
          break;
        case AACMode::kHEVC_v1:
          aac_params.eAACProfile = OMX_AUDIO_AACObjectHE;
          break;
        case AACMode::kHEVC_v2:
          aac_params.eAACProfile = OMX_AUDIO_AACObjectHE_PS;
          break;
        default:
          QMMF_ERROR("%s: %s() unsupported AAC mode: %d", TAG, __func__,
                     codec_param.audio_param.codec_params.aac.mode);
          return ::android::BAD_VALUE;
      }
      result = omx_client_->SetParameter(OMX_IndexParamAudioAac,
                                         static_cast<OMX_PTR>(&aac_params));
      if (result != OMX_ErrorNone) {
        QMMF_ERROR("%s: %s() failed to set AAC parameters: %d", TAG, __func__,
                   result);
        return ::android::FAILED_TRANSACTION;
      }
      break;
    }
    case AudioFormat::kAMR:
      // set the AMR output parameters
      OMX_AUDIO_PARAM_AMRTYPE amr_params;
      InitOMXParams(&amr_params);
      amr_params.nPortIndex = kPortIndexOutput;
      amr_params.nChannels = codec_param.audio_param.channels;
      if (codec_param.audio_param.codec_params.amr.isWAMR)
        amr_params.eAMRBandMode = OMX_AUDIO_AMRBandModeWB8;
      else
        amr_params.eAMRBandMode = OMX_AUDIO_AMRBandModeNB7;
      result = omx_client_->SetParameter(OMX_IndexParamAudioAmr,
                                         static_cast<OMX_PTR>(&amr_params));
      if (result != OMX_ErrorNone) {
        QMMF_ERROR("%s: %s() failed to set AMR parameters: %d", TAG, __func__,
                   result);
        return ::android::FAILED_TRANSACTION;
      }
      break;
    case AudioFormat::kG711:
      // set the G711 output parameters
      OMX_AUDIO_PARAM_PCMMODETYPE pcm_params;
      InitOMXParams(&pcm_params);
      pcm_params.nPortIndex = kPortIndexInput;
      pcm_params.nChannels = codec_param.audio_param.channels;
      pcm_params.nSamplingRate = codec_param.audio_param.sample_rate;
      result = omx_client_->SetParameter(OMX_IndexParamAudioPcm,
                                         static_cast<OMX_PTR>(&pcm_params));
      if (result != OMX_ErrorNone) {
        QMMF_ERROR("%s: %s() failed to set G711 parameters: %d", TAG, __func__,
                   result);
        return ::android::FAILED_TRANSACTION;
      }
      break;
    default:
      QMMF_ERROR("%s: %s() unknown audio codec: %d", TAG, __func__,
                 static_cast<int>(codec_param.audio_param.format));
      return ::android::BAD_VALUE;
  }

  return ::android::NO_ERROR;
}

status_t AVCodec::SetPortParams(OMX_U32 port, OMX_U32 width, OMX_U32 height,
                                OMX_U32 frame_rate) {

  status_t ret = 0;
  OMX_PARAM_PORTDEFINITIONTYPE port_def;
  InitOMXParams(&port_def);

  port_def.nPortIndex = port;
  ret = omx_client_->GetParameter(OMX_IndexParamPortDefinition,
                                 (OMX_PTR)&port_def);
  if(ret != 0) {
    QMMF_ERROR("%s:%s Failed to get OMX_IndexParamPortDefinition",TAG,__func__);
    return ret;
  }

  FractionToQ16(port_def.format.video.xFramerate,(int)(frame_rate * 2), 2);
  port_def.format.video.nFrameWidth = width;
  port_def.format.video.nFrameHeight = height;

  ret = omx_client_->SetParameter(OMX_IndexParamPortDefinition,
                                 (OMX_PTR)&port_def);
  if(ret != 0) {
    QMMF_ERROR("%s:%s failed to set OMX_IndexParamPortDefinition",TAG,__func__);
    return ret;
  }

  return ret;
}

status_t AVCodec::GetBufferRequirements(OMX_U32 port_index, uint32_t *buf_count,
                                        uint32_t *buf_size) {

  status_t ret = 0;

  OMX_PARAM_PORTDEFINITIONTYPE port_def;
  InitOMXParams(&port_def);

  port_def.nPortIndex = port_index;
  ret = omx_client_->GetParameter(OMX_IndexParamPortDefinition,
                                 (OMX_PTR)&port_def);
  if(ret != 0) {
    QMMF_ERROR("%s:%s Failed to get OMX_IndexParamPortDefinition",TAG, __func__);
    return ret;
  }

  *buf_count = port_def.nBufferCountActual;
  *buf_size = port_def.nBufferSize;

  QMMF_INFO("%s:%s %s: buf count(%d), buf size(%d)", TAG, __func__,
      OMX_PORT_NAME( port_index), port_def.nBufferCountActual,
      port_def.nBufferSize);
  return ret;
}

status_t AVCodec::SetupAVCEncoderParameters(CodecCreateParam& param) {

  QMMF_INFO("%s:%s Enter", TAG, __func__);
  status_t ret = 0;

  uint32_t frame_rate = param.video_param.frame_rate;
  uint32_t iframe_interval = param.video_param.codec_param.avc.idr_interval;

  OMX_VIDEO_PARAM_AVCTYPE h264_type;
  InitOMXParams(&h264_type);
  h264_type.nPortIndex = kPortIndexOutput;

  ret = omx_client_->GetParameter(OMX_IndexParamVideoAvc, &h264_type);
  if (ret != OK) {
    QMMF_ERROR("%s:%s Failed to get AVC video param", TAG, __func__);
    return ret;
  }

  h264_type.nAllowedPictureTypes =
      OMX_VIDEO_PictureTypeI | OMX_VIDEO_PictureTypeP;
  h264_type.eProfile =
      static_cast<OMX_VIDEO_AVCPROFILETYPE>(GetVideoProfile(param));
  h264_type.eLevel = static_cast<OMX_VIDEO_AVCLEVELTYPE>(GetVideoLevel(param));

  if(h264_type.eProfile == OMX_VIDEO_AVCProfileBaseline) {
    h264_type.nSliceHeaderSpacing = 0;
    h264_type.bUseHadamard = OMX_TRUE;
    h264_type.nRefFrames = 1;
    h264_type.nBFrames = 0;
    h264_type.nPFrames = frame_rate*iframe_interval;
    if(h264_type.nPFrames == 0) {
      h264_type.nAllowedPictureTypes = OMX_VIDEO_PictureTypeI;
    }
    h264_type.nRefIdx10ActiveMinus1 = 0;
    h264_type.nRefIdx11ActiveMinus1 = 0;
    h264_type.bEntropyCodingCABAC = OMX_FALSE;
    h264_type.bWeightedPPrediction = OMX_FALSE;
    h264_type.bconstIpred = OMX_FALSE;
    h264_type.bDirect8x8Inference = OMX_FALSE;
    h264_type.bDirectSpatialTemporal = OMX_FALSE;
    h264_type.nCabacInitIdc = 0;
  } else {
    h264_type.nSliceHeaderSpacing = 0;
    h264_type.bUseHadamard = OMX_TRUE;
    h264_type.nRefFrames = 2;
    h264_type.nBFrames = 1;
    h264_type.nPFrames = frame_rate*iframe_interval;
    h264_type.nAllowedPictureTypes =
        OMX_VIDEO_PictureTypeI | OMX_VIDEO_PictureTypeP | OMX_VIDEO_PictureTypeB;
    h264_type.nRefIdx10ActiveMinus1 = 0;
    h264_type.nRefIdx11ActiveMinus1 = 0;
    h264_type.bEntropyCodingCABAC = OMX_TRUE;
    h264_type.bWeightedPPrediction = OMX_TRUE;
    h264_type.bconstIpred = OMX_TRUE;
    h264_type.bDirect8x8Inference = OMX_TRUE;
    h264_type.bDirectSpatialTemporal = OMX_TRUE;
    h264_type.nCabacInitIdc = 1;
  }

  if (h264_type.nBFrames != 0) {
      h264_type.nAllowedPictureTypes |= OMX_VIDEO_PictureTypeB;
  }

  h264_type.bEnableUEP = OMX_FALSE;
  h264_type.bEnableFMO = OMX_FALSE;
  h264_type.bEnableASO = OMX_FALSE;
  h264_type.bEnableRS = OMX_FALSE;
  h264_type.bFrameMBsOnly = OMX_TRUE;
  h264_type.bMBAFF = OMX_FALSE;
  h264_type.eLoopFilterMode = OMX_VIDEO_AVCLoopFilterEnable;

  ret = omx_client_->SetParameter(OMX_IndexParamVideoAvc, &h264_type);
  if (ret != OK) {
    QMMF_ERROR("%s:%s Failed to set AVC codec parameter", TAG, __func__);
    return ret;
  }

  QMMF_INFO("%s:%s Exit", TAG, __func__);
  return ret;
}

status_t AVCodec::SetupHEVCEncoderParameters(CodecCreateParam& param) {

  QMMF_INFO("%s:%s Enter", TAG, __func__);
  status_t ret = 0;

  uint32_t frame_rate = param.video_param.frame_rate;
  uint32_t iframe_interval = param.video_param.codec_param.hevc.idr_interval;

  OMX_VIDEO_PARAM_HEVCTYPE hevc_type;
  InitOMXParams(&hevc_type);
  hevc_type.nPortIndex = kPortIndexOutput;

  ret = omx_client_->GetParameter((OMX_INDEXTYPE)OMX_IndexParamVideoHevc,
                                  &hevc_type);
  if (ret != OK) {
    QMMF_ERROR("%s:%s Failed to get HEVC video param", TAG, __func__);
    return ret;
  }

  hevc_type.eProfile =
      static_cast<OMX_VIDEO_HEVCPROFILETYPE>(GetVideoProfile(param));
  hevc_type.eLevel =
      static_cast<OMX_VIDEO_HEVCLEVELTYPE>(GetVideoLevel(param));

  ret = omx_client_->SetParameter((OMX_INDEXTYPE)OMX_IndexParamVideoHevc,
                                   &hevc_type);
  if (ret != OK) {
    QMMF_ERROR("%s:%s Failed to get HEVC video param", TAG, __func__);
    return ret;
  }

  QOMX_VIDEO_INTRAPERIODTYPE intra;
  intra.nPortIndex = kPortIndexOutput;
  omx_client_->GetConfig(
          (OMX_INDEXTYPE)QOMX_IndexConfigVideoIntraperiod,
          (OMX_PTR)&intra);
  if (ret != OK) {
    QMMF_ERROR("%s:%s Failed to get video intra period", TAG, __func__);
    return ret;
  }
   intra.nPFrames = frame_rate*iframe_interval;
   //TODO: remove hard code B frame value
  intra.nBFrames = 0;
  ret = omx_client_->SetConfig(
          (OMX_INDEXTYPE)QOMX_IndexConfigVideoIntraperiod,
          (OMX_PTR)&intra);
  if (ret != OK) {
    QMMF_ERROR("%s:%s Failed to set video intra period", TAG, __func__);
    return ret;
  }

  QMMF_INFO("%s:%s Exit", TAG, __func__);
  return ret;
}

status_t AVCodec::GetVideoProfile(CodecCreateParam& param) {

  int32_t profile = -1;
  VideoFormat codec_format = param.video_param.format_type;
  switch(codec_format) {
    case VideoFormat::kAVC:
      switch(param.video_param.codec_param.avc.profile) {
        case AVCProfileType::kBaseline:
          profile = OMX_VIDEO_AVCProfileBaseline;
         break;
        case AVCProfileType::kMain:
          profile = OMX_VIDEO_AVCProfileMain;
          break;
        case AVCProfileType::kHigh:
          profile = OMX_VIDEO_AVCProfileHigh ;
          break;
      }
      break;
    case VideoFormat::kHEVC:
      switch(param.video_param.codec_param.hevc.profile) {
        case HEVCProfileType::kMain:
          profile = OMX_VIDEO_HEVCProfileMain;
          break;
      }
      break;
    default:
      QMMF_ERROR("%s:%s Unknown codec type(%d)", TAG, __func__, codec_format);
      break;
  }
  return profile;
}

status_t AVCodec::GetVideoLevel(CodecCreateParam& param) {

  int32_t level = -1;
  VideoFormat codec_format = param.video_param.format_type;
  switch(codec_format) {
    case VideoFormat::kAVC:
      switch(param.video_param.codec_param.avc.level) {
        case AVCLevelType::kLevel3:
          level = OMX_VIDEO_AVCLevel3;
          break;
        case AVCLevelType::kLevel4:
          level = OMX_VIDEO_AVCLevel4;
          break;
        case AVCLevelType::kLevel5:
          level = OMX_VIDEO_AVCLevel5;
          break;
        case AVCLevelType::kLevel5_1:
          level = OMX_VIDEO_AVCLevel51;
          break;
        case AVCLevelType::kLevel5_2:
          level = OMX_VIDEO_AVCLevel52;
          break;
      }
      break;
    case VideoFormat::kHEVC:
        switch(param.video_param.codec_param.hevc.level) {
          case HEVCLevelType::kLevel3:
            level = OMX_VIDEO_HEVCMainTierLevel3;
            break;
          case HEVCLevelType::kLevel4:
            level = OMX_VIDEO_HEVCMainTierLevel4;
            break;
          case HEVCLevelType::kLevel5:
            level = OMX_VIDEO_HEVCMainTierLevel5;
            break;
          case HEVCLevelType::kLevel5_1:
            level = OMX_VIDEO_HEVCMainTierLevel41;
            break;
          case HEVCLevelType::kLevel5_2:
            level = OMX_VIDEO_HEVCMainTierLevel52;
            break;
        }
        break;
    default:
      QMMF_ERROR("%s:%s Unknown codec type(%d)", TAG, __func__, codec_format);
      break;
  }
  return level;
}

status_t AVCodec::ConfigureBitrate(CodecCreateParam& param) {

  uint32_t bitrate = 0;
  VideoRateControlType mode;

  VideoFormat codec_format = param.video_param.format_type;
  switch(codec_format) {
    case VideoFormat::kAVC:
      bitrate = param.video_param.codec_param.avc.bitrate;
      mode = param.video_param.codec_param.avc.ratecontrol_type;
      break;
    case VideoFormat::kHEVC:
      bitrate = param.video_param.codec_param.hevc.bitrate;
      mode = param.video_param.codec_param.hevc.ratecontrol_type;
      break;
    default:
      QMMF_ERROR("%s:%s Unknown codec type(%d)", TAG, __func__, codec_format);
      return -1;
  }

  OMX_VIDEO_CONTROLRATETYPE control_rate;
  switch(mode) {
    case VideoRateControlType::kDisable:
      control_rate = OMX_Video_ControlRateDisable;
      break;
    case VideoRateControlType::kVariableSkipFrames:
      control_rate = OMX_Video_ControlRateVariableSkipFrames;
      break;
    case VideoRateControlType::kVariable:
      control_rate = OMX_Video_ControlRateVariable;
      break;
    case VideoRateControlType::kConstantSkipFrames:
      control_rate = OMX_Video_ControlRateConstantSkipFrames;
      break;
    case VideoRateControlType::kConstant:
      control_rate = OMX_Video_ControlRateConstant;
      break;
    default:
      control_rate = OMX_Video_ControlRateVariable;
  }

  OMX_VIDEO_PARAM_BITRATETYPE bitrate_type;
  InitOMXParams(&bitrate_type);
  bitrate_type.nPortIndex = kPortIndexOutput;

  status_t ret = omx_client_->GetParameter(OMX_IndexParamVideoBitrate,
                                           &bitrate_type);
  if (ret != OK) {
    QMMF_ERROR("%s:%s Failed to get OMX_IndexParamVideoBitrate", TAG, __func__);
    return ret;
  }

  bitrate_type.eControlRate = control_rate;
  bitrate_type.nTargetBitrate = bitrate;

  ret = omx_client_->SetParameter(OMX_IndexParamVideoBitrate, &bitrate_type);
  if (ret != OK) {
    QMMF_ERROR("%s:%s Failed to set OMX_IndexParamVideoBitrate", TAG, __func__);
    return ret;
  }

  return ret;
}

status_t AVCodec::UseBuffer(OMX_U32 port, void *imp) {

  status_t ret = 0;
  QMMF_INFO("%s:%s Enter", TAG, __func__);

  OMX_PARAM_PORTDEFINITIONTYPE port_def;
  InitOMXParams(&port_def);
  port_def.nPortIndex = port;
  ret = omx_client_->GetParameter(OMX_IndexParamPortDefinition, &port_def);
  if(ret != OK) {
      QMMF_ERROR("%s:%s Failed to getParameter on %s", TAG, __func__,
          OMX_PORT_NAME(port));
      return ret;
  }

  if (format_type_ == CodecType::kVideoEncoder) {
    uint32_t buf_count = (port == kPortIndexInput) ?
                             INPUT_MAX_COUNT : OUTPUT_MAX_COUNT;

    if(port_def.nBufferCountActual != buf_count) {

      port_def.nBufferCountActual = buf_count;
      ret = omx_client_->SetParameter(OMX_IndexParamPortDefinition,
                                     (OMX_PTR)&port_def);
      if(ret != OK) {
        QMMF_ERROR("%s:%s Failed to set new buffer count(%d) on %s", TAG,
            __func__, port_def.nBufferCountActual, OMX_PORT_NAME(port));
        return ret;
      }
      ret = omx_client_->GetParameter(OMX_IndexParamPortDefinition, &port_def);
      if(ret != OK) {
        QMMF_ERROR("%s:%s Failed to getParameter on %s", TAG, __func__,
            OMX_PORT_NAME(port));
        return ret;
      }
      QMMF_INFO("%s:%s New Buf count(%d), size(%d)", TAG, __func__,
          port_def.nBufferCountActual, port_def.nBufferSize);
      assert(buf_count == port_def.nBufferCountActual);
    }

    if (port == kPortIndexInput)
      in_buff_hdr_size_ = port_def.nBufferCountActual;
    else
      out_buff_hdr_size_ = port_def.nBufferCountActual;
  }

  if(port == kPortIndexInput) {
    IInputCodecSource *impl = static_cast<IInputCodecSource*>(imp);
    assert(impl != NULL);
    input_source_ = impl;

    //allocate memory for buffer header
    in_buff_hdr_ = new OMX_BUFFERHEADERTYPE*[port_def.nBufferCountActual];
    if(in_buff_hdr_ ==  NULL) {
      QMMF_ERROR("%s:%s Failed to allocate buffer header on %s", TAG, __func__,
          OMX_PORT_NAME(kPortIndexInput));
      return NO_MEMORY;
    }

    if (format_type_ == CodecType::kVideoEncoder) {
      StoreMetaDataInBuffersParams meta_mode;
      InitOMXParams(&meta_mode);
      meta_mode.nPortIndex = kPortIndexInput;
      meta_mode.bStoreMetaData = OMX_TRUE;
      ret = omx_client_->SetParameter(
                (OMX_INDEXTYPE)OMX_QcomIndexParamVideoMetaBufferMode,
                (OMX_PTR)&meta_mode);
      if(ret != OK) {
        QMMF_ERROR("%s:%s Failed to set VideoEncode MetaBufferMode",TAG,__func__);
        return ret;
      }
    }

  } else {
    IOutputCodecSource *impl = static_cast<IOutputCodecSource*>(imp);
    assert(impl != NULL);
    output_source_ = impl;

    out_buff_hdr_ = new OMX_BUFFERHEADERTYPE*[port_def.nBufferCountActual];
    if(out_buff_hdr_ ==  NULL) {
        QMMF_ERROR("%s:%s Failed to allocate buffer header on %s",TAG, __func__,
            OMX_PORT_NAME(kPortIndexOutput));
        return NO_MEMORY;
    }
  }

  QMMF_INFO("%s:%s Exit", TAG, __func__);
  return ret;
}

status_t AVCodec::ReleaseBuffer() {

  QMMF_INFO("%s:%s Enter", TAG, __func__);
  status_t ret = 0;

  DeleteHandle();

  delete []in_buff_hdr_;
  in_buff_hdr_ = NULL;

  delete []out_buff_hdr_;
  out_buff_hdr_ = NULL;

  assert(cmd_buffer_index_ == 0);
  signal_queue_.Clear();

  input_source_ = NULL;
  output_source_ = NULL;
  port_status_ = true;

  return ret;
}

status_t AVCodec::StartCodec() {

  QMMF_INFO("%s:%s Enter", TAG, __func__);
  status_t ret = 0;

  QMMF_INFO("%s:%s current state(%s), pending state(%s)", TAG, __func__,
      OMX_STATE_NAME(state_), OMX_STATE_NAME(state_pending_));

  if(port_status_ == false) {
    ret = omx_client_->SendCommand(OMX_CommandPortEnable, kPortIndexInput, NULL);
    if(ret != 0) {
        QMMF_ERROR("%s:%s Failed to enable port on %s", TAG, __func__,
            OMX_PORT_NAME(kPortIndexInput));
        return ret;
    }

    ret = omx_client_->SendCommand(OMX_CommandPortEnable, kPortIndexOutput, NULL);
    if(ret != 0) {
        QMMF_ERROR("%s:%s Failed to enable port on %s", TAG, __func__,
            OMX_PORT_NAME(kPortIndexOutput));
        return ret;
    }
  }

  OMX_PARAM_PORTDEFINITIONTYPE port_def;
  InitOMXParams(&port_def);
  port_def.nPortIndex = kPortIndexInput;
  ret = omx_client_->GetParameter(OMX_IndexParamPortDefinition,
            (OMX_PTR)&port_def);
  if(ret != OK) {
    QMMF_ERROR("%s:%s Failed to get port definiton on %s", TAG, __func__,
        OMX_PORT_NAME(kPortIndexInput));
    return ret;
  }
  uint32_t buf_size = port_def.nBufferSize;

  if (format_type_ == CodecType::kVideoEncoder) {
    for(uint32_t i = 0; i < port_def.nBufferCountActual; ++i) {
        buf_size = sizeof(encoder_media_buffer_type);
        ret = omx_client_->AllocateBuffer(&in_buff_hdr_[i], kPortIndexInput, NULL,
                  buf_size);
        if(ret != OK) {
            QMMF_ERROR("%s:%s Failed to allocate buffer on %s", TAG, __func__,
                OMX_PORT_NAME(kPortIndexInput));
            return ret;
        }

        encoder_media_buffer_type* mediaBuffer =
            (encoder_media_buffer_type*)in_buff_hdr_[i]->pBuffer;
        assert(mediaBuffer != NULL);
        mediaBuffer->buffer_type =
            MetadataBufferType::kMetadataBufferTypeGrallocSource;
        mediaBuffer->meta_handle = NULL;
    }
  } else {
    for(uint32_t i = 0; i < port_def.nBufferCountActual; ++i) {
      ret = omx_client_->UseBuffer(&in_buff_hdr_[i], kPortIndexInput, NULL,
                                   buf_size, NULL);
      if(ret != OK) {
          QMMF_ERROR("%s:%s Failed to allocate buffer on %s", TAG, __func__,
                     OMX_PORT_NAME(kPortIndexInput));
          return ret;
      }
    }
  }

  InitOMXParams(&port_def);
  port_def.nPortIndex = kPortIndexOutput;
  ret = omx_client_->GetParameter(OMX_IndexParamPortDefinition,
            (OMX_PTR)&port_def);
  if(ret != OK) {
    QMMF_ERROR("%s:%s Failed to get port definiton on %s", TAG, __func__,
        OMX_PORT_NAME(kPortIndexOutput));
    return ret;
  }
  buf_size = port_def.nBufferSize;

  if (format_type_ == CodecType::kVideoEncoder) {
    for(uint32_t i = 0; i < port_def.nBufferCountActual; ++i) {
      ret = omx_client_->UseBuffer(&out_buff_hdr_[i], kPortIndexOutput, NULL,
                                   buf_size, NULL);
      if(ret != OK) {
        QMMF_ERROR("%s:%s Failed to allocate buffer on %s", TAG, __func__,
                   OMX_PORT_NAME(kPortIndexOutput));
        return ret;
      }
    }
  } else {
    for(uint32_t i = 0; i < port_def.nBufferCountActual; ++i) {
      ret = omx_client_->AllocateBuffer(&out_buff_hdr_[i], kPortIndexOutput,
                                        NULL, buf_size);
      if(ret != OK) {
        QMMF_ERROR("%s:%s Failed to allocate buffer on %s", TAG, __func__,
                   OMX_PORT_NAME(kPortIndexInput));
        return ret;
      }

      CodecBuffer* buffer = new CodecBuffer;
      buffer->pointer = nullptr;
      buffer->fd = -1;
      out_buff_hdr_[i]->pAppPrivate = reinterpret_cast<OMX_PTR>(buffer);
      QMMF_VERBOSE("%s:%s allocated pBuffer[%p] and pAppPrivate[%p]", TAG,
                   __func__, out_buff_hdr_[i]->pBuffer,
                   out_buff_hdr_[i]->pAppPrivate);
    }
  }

  if(port_status_ == false) {
    CodecCmdType* cmd = (CodecCmdType *)signal_queue_.Pop();
    assert(cmd != nullptr);
    QMMF_INFO("%s:%s Popped buffer from cmd queue(%p) for index(%d)", TAG,
        __func__, cmd, cmd_buffer_index_);
    cmd_buffer_index_--;

    if((cmd->event_result != OMX_ErrorNone) ||
        (cmd->event_type != OMX_EventCmdComplete) ||
        (cmd->event_cmd != OMX_CommandPortEnable)) {
      QMMF_ERROR("%s:%s Expecting Cmd complete vs command found(%d)", TAG,
          __func__, cmd->event_cmd);
      return cmd->event_result;
    }

    cmd = (CodecCmdType *)signal_queue_.Pop();
    assert(cmd != nullptr);
    QMMF_INFO("%s:%s Popped buffer from cmd queue(%p) for index(%d)", TAG,
        __func__, cmd, cmd_buffer_index_);
    cmd_buffer_index_--;

    if((cmd->event_result != OMX_ErrorNone) ||
       (cmd->event_type != OMX_EventCmdComplete) ||
       (cmd->event_cmd != OMX_CommandPortEnable)) {
      QMMF_ERROR("%s:%s Expecting Cmd complete vs command found(%d)", TAG,
          __func__, cmd->event_cmd);
      return cmd->event_result;
    }

    port_status_ = true;
  }

  ret = WaitState(OMX_StateIdle);
  if(ret != OK) {
    QMMF_ERROR("%s:%s Wait for state %s failed", TAG, __func__,
        OMX_STATE_NAME(OMX_StateIdle));
    return ret;
  }

  QMMF_INFO("%s:%s Move to Component to Executing state", TAG, __func__);
  ret = SetState(OMX_StateExecuting, OMX_TRUE);
  assert(ret == OK);

  {
    Mutex::Autolock autoLock(input_stop_lock_);
    input_stop_ = false;
  }

  {
    Mutex::Autolock autoLock(output_stop_lock_);
    output_stop_ = false;
  }

  pthread_create(&read_thread_, NULL, DeliverInput, (void*)this);
  pthread_create(&read_thread_, NULL, DeliverOutput, (void*)this);


  QMMF_INFO("%s:%s current state(%s), pending state(%s)", TAG, __func__,
      OMX_STATE_NAME(state_), OMX_STATE_NAME(state_pending_));
  QMMF_INFO("%s:%s Exit", TAG, __func__);
  return ret;
}

status_t AVCodec::StopCodec() {

  QMMF_INFO("%s:%s Enter", TAG, __func__);
  status_t ret = 0;

  if ((state_ == OMX_StateIdle) || (state_pending_ == OMX_StateIdle)) {
    QMMF_WARN("%s:%s Encoder is already in Idle state", TAG, __func__);
    return ret;
  }

  {
    Mutex::Autolock autoLock(input_stop_lock_);
    input_stop_ = true;
  }

  CodecCmdType *cmd = (CodecCmdType *)signal_queue_.Pop();
  assert(cmd != nullptr);
  QMMF_INFO("%s:%s Popped buffer from cmd queue(%p) for index(%d)", TAG,
      __func__, cmd, cmd_buffer_index_);
  cmd_buffer_index_--;

  if((cmd->event_result != OMX_ErrorNone) ||
     (cmd->event_flags != OMX_BUFFERFLAG_EOS)) {
      QMMF_ERROR("%s:%s Expecting EOS and found(%d) flag", TAG, __func__,
          cmd->event_flags);
      return OMX_ErrorUndefined;
  }

  ret =  SetState(OMX_StateIdle, OMX_TRUE);
  if(ret != 0) {
   QMMF_ERROR("%s:%s Failed to move to OMX_StateIdle state!", TAG, __func__);
   return ret;
  }

  //Disable both port
  ret = omx_client_->SendCommand(OMX_CommandPortDisable, kPortIndexOutput, 0);
  if(ret != 0) {
   QMMF_ERROR("%s:%s Failed to disbale port on %s", TAG, __func__,
       OMX_PORT_NAME(kPortIndexOutput));
   return ret;
  }

  ret = omx_client_->SendCommand(OMX_CommandPortDisable, kPortIndexInput, 0);
  if(ret != 0) {
   QMMF_ERROR("%s:%s Failed to disbale port on %s", TAG, __func__,
       OMX_PORT_NAME(kPortIndexInput));
   return ret;
  }

  port_status_ = false;

  //DeRegister buffer on both port
  for(uint32_t i = 0; i < in_buff_hdr_size_; i++) {
    ret = omx_client_->FreeBuffer(in_buff_hdr_[i], kPortIndexInput);
    if(ret != 0) {
      QMMF_ERROR("%s:%s Failed to free buffer on %s", TAG, __func__,
          OMX_PORT_NAME(kPortIndexInput));
      return ret;
    }
  }

  for(uint32_t i = 0; i < out_buff_hdr_size_; i++) {
    if (format_type_ == CodecType::kAudioEncoder)
      delete reinterpret_cast<CodecBuffer*>(out_buff_hdr_[i]->pAppPrivate);
    ret = omx_client_->FreeBuffer(out_buff_hdr_[i], kPortIndexOutput);
    if(ret != 0) {
      QMMF_ERROR("%s:%s Failed to free buffer on %s", TAG, __func__,
          OMX_PORT_NAME(kPortIndexOutput));
      return ret;
    }
  }

  cmd = (CodecCmdType *)signal_queue_.Pop();
  assert(cmd != nullptr);
  QMMF_INFO("%s:%s Popped buffer from cmd queue(%p) for index(%d)", TAG,
      __func__, cmd, cmd_buffer_index_);
  cmd_buffer_index_--;

  if((cmd->event_result != OMX_ErrorNone) ||
      (cmd->event_type != OMX_EventCmdComplete) ||
      (cmd->event_cmd != OMX_CommandPortDisable)) {
    QMMF_ERROR("%s:%s Expecting Cmd complete vs command found(%d)", TAG,
        __func__, cmd->event_cmd);
    return cmd->event_result;
  }

  cmd = (CodecCmdType *)signal_queue_.Pop();
  assert(cmd != nullptr);
  QMMF_INFO("%s:%s Popped buffer from cmd queue(%p) for index(%d)", TAG,
      __func__, cmd, cmd_buffer_index_);
  cmd_buffer_index_--;

  if((cmd->event_result != OMX_ErrorNone) ||
     (cmd->event_type != OMX_EventCmdComplete) ||
     (cmd->event_cmd != OMX_CommandPortDisable)) {
    QMMF_ERROR("%s:%s Expecting Cmd complete vs command found(%d)", TAG,
        __func__, cmd->event_cmd);
    return cmd->event_result;
  }

  QMMF_INFO("%s:%s current state(%s), pending state(%s)", TAG, __func__,
      OMX_STATE_NAME(state_), OMX_STATE_NAME(state_pending_));
  QMMF_INFO("%s:%s Exit", TAG, __func__);
  return ret;
}

void AVCodec::StopOutput() {

  Mutex::Autolock autoLock(output_stop_lock_);
  output_stop_ = true;
}

status_t AVCodec::PauseCodec() {

  QMMF_INFO("%s:%s Enter", TAG, __func__);
  status_t ret = 0;

  QMMF_INFO("%s:%s Exit", TAG, __func__);
  return ret;
}

status_t AVCodec::ResumeCodec() {

  QMMF_INFO("%s:%s Enter", TAG, __func__);
  status_t ret = 0;

  QMMF_INFO("%s:%s Exit", TAG, __func__);
  return ret;
}

status_t AVCodec::SetParameters(CodecParamType param_type, void *params,
                                size_t param_size) {

  QMMF_INFO("%s:%s Enter", TAG, __func__);
  status_t ret = 0;
  uint32_t *value;
  VideoEncIdrInterval *idr_interval;
  VideoEncLtrUse *ltr_use;
  OMX_INDEXTYPE index;

  switch (param_type) {
    case CodecParamType::kBitRateType:
      value = static_cast<uint32_t*>(params);
      OMX_VIDEO_CONFIG_BITRATETYPE bitrate_params;
      InitOMXParams(&bitrate_params);
      bitrate_params.nSize = sizeof(bitrate_params);
      bitrate_params.nPortIndex = kPortIndexOutput;
      bitrate_params.nEncodeBitrate = *value;
      index = OMX_IndexConfigVideoBitrate;
      ret = omx_client_->SetConfig(index, &bitrate_params);
      break;
    case CodecParamType::kFrameRateType:
      value = static_cast<uint32_t*>(params);
      OMX_CONFIG_FRAMERATETYPE fps_params;
      InitOMXParams(&fps_params);
      fps_params.nPortIndex = kPortIndexOutput;
      FractionToQ16(fps_params.xEncodeFramerate,(int)((*value) * 2), 2);
      index = OMX_IndexConfigVideoFramerate;
      ret = omx_client_->SetConfig(index, &fps_params);
      break;
    case CodecParamType::kInsertIDRType:
      OMX_CONFIG_INTRAREFRESHVOPTYPE idr_params;
      InitOMXParams(&idr_params);
      idr_params.nPortIndex = kPortIndexOutput;
      idr_params.IntraRefreshVOP = OMX_TRUE;
      index = OMX_IndexConfigVideoIntraVOPRefresh;
      ret = omx_client_->SetConfig(index, &idr_params);
      break;
    case CodecParamType::kIDRIntervalType:
      idr_interval = static_cast<VideoEncIdrInterval*>(params);
      QOMX_VIDEO_INTRAPERIODTYPE intra_params;
      InitOMXParams(&intra_params);
      intra_params.nPortIndex = kPortIndexOutput;
      intra_params.nPFrames = idr_interval->num_pframes;
      intra_params.nBFrames = idr_interval->num_bframes;
      intra_params.nIDRPeriod = idr_interval->idr_period;
      index = (OMX_INDEXTYPE)QOMX_IndexConfigVideoIntraperiod;
      ret = omx_client_->SetConfig(index, &intra_params);
      break;
    case CodecParamType::kMarkLtrType:
      value = static_cast<uint32_t*>(params);
      QOMX_VIDEO_CONFIG_LTRMARK_TYPE  matkltr_params;
      InitOMXParams(&matkltr_params);
      matkltr_params.nPortIndex = kPortIndexInput;
      matkltr_params.nID = *value;
      index = (OMX_INDEXTYPE)QOMX_IndexConfigVideoLTRMark;
      ret = omx_client_->SetConfig(index, &matkltr_params);
      break;
    case CodecParamType::kUseLtrType:
      ltr_use = static_cast<VideoEncLtrUse*>(params);
      QOMX_VIDEO_CONFIG_LTRUSE_TYPE useltr_params;
      InitOMXParams(&useltr_params);
      useltr_params.nPortIndex = kPortIndexInput;
      useltr_params.nID = ltr_use->id;
      useltr_params.nFrames = ltr_use->frame;
      index = (OMX_INDEXTYPE)QOMX_IndexConfigVideoLTRUse;
      ret = omx_client_->SetConfig(index, &useltr_params);
      break;
    default:
      QMMF_ERROR("%s:%s Unknown param type", TAG, __func__);
      return -1;
  }

  if(ret != OK) {
    QMMF_ERROR("%s:%s Failed to set codec param", TAG, __func__);
    return ret;
  }

  QMMF_INFO("%s:%s Exit", TAG, __func__);
  return ret;
}

bool AVCodec::IsInputPortStop() {

  Mutex::Autolock autoLock(input_stop_lock_);
  return input_stop_;
}

bool inline AVCodec::IsOutputPortStop() {

  Mutex::Autolock autoLock(output_stop_lock_);
  return output_stop_;
}

status_t AVCodec::Flush(OMX_U32 index) {

  QMMF_INFO("%s:%s Enter", TAG, __func__);
  status_t ret = 0;

  ret = omx_client_->SendCommand(OMX_CommandFlush, index, 0);
  if(ret != 0) {
    QMMF_ERROR("%s:%s Failed to call flush command on %s", TAG, __func__,
        OMX_PORT_NAME(index));
    return ret;
  }

  CodecCmdType *cmd = (CodecCmdType *)signal_queue_.Pop();
  assert(cmd != nullptr);
  QMMF_INFO("%s:%s Popped buffer from cmd queue(%p) for index(%d)", TAG,
       __func__, cmd, cmd_buffer_index_);
  cmd_buffer_index_--;

  if((cmd->event_result != OMX_ErrorNone) ||
     (cmd->event_type != OMX_EventCmdComplete) ||
     (cmd->event_cmd != OMX_CommandFlush)) {
    QMMF_ERROR("%s:%s Expecting Cmd complete for flush vs command found(%d)",
        TAG, __func__, cmd->event_cmd);
    return cmd->event_result;
  }

  /* Wait for flush complete for both ports */
  if (index == OMX_ALL) {
    cmd = (CodecCmdType *)signal_queue_.Pop();
    assert(cmd != nullptr);
    QMMF_INFO("%s:%s Popped buffer from cmd queue(%p) for index(%d)", TAG,
        __func__, cmd, cmd_buffer_index_);
    cmd_buffer_index_--;

    if((cmd->event_result != OMX_ErrorNone) ||
       (cmd->event_type != OMX_EventCmdComplete) ||
       (cmd->event_cmd != OMX_CommandFlush)) {
      QMMF_ERROR("%s:%s Expecting Cmd complete for flush vs command found(%d)",
        TAG, __func__, cmd->event_cmd);
      return cmd->event_result;
    }
  }

  QMMF_INFO("%s:%s Exit", TAG, __func__);
  return ret;
}

void* AVCodec::DeliverInput(void *arg) {

  QMMF_INFO("%s:%s Enter", TAG, __func__);
  status_t ret = 0;

  AVCodec *avcodec = static_cast<AVCodec*>(arg);
  StreamBuffer stream_buffer;

  OMX_BUFFERHEADERTYPE *buf_header;
  bool thread_stop = false;

  while(1) {
    memset(&stream_buffer, 0x0, sizeof(stream_buffer));
    ret = avcodec->getInputBufferSource()->Read(stream_buffer);

    buffer_handle_t native_handle;
    memset(&native_handle, 0x0, sizeof native_handle);
    if (avcodec->format_type_ == CodecType::kVideoEncoder) {
      native_handle = stream_buffer.handle;
      assert(native_handle != NULL);
      assert(native_handle->data[0] != 0);
    }

    buf_header = avcodec->GetBufferHdr(stream_buffer);
    assert(buf_header != NULL);

    if(ret != 0)  {
      QMMF_ERROR("%s:%s InputSource Read failed. Send EOS", TAG, __func__);
      buf_header->nFlags = OMX_BUFFERFLAG_EOS;
      thread_stop = true;
    }

    if(avcodec->IsInputPortStop()) {
      QMMF_INFO("%s:%s Encoder is stopped. Send EOS", TAG, __func__);
      buf_header->nFlags = OMX_BUFFERFLAG_EOS;
      thread_stop = true;
    }

    if (avcodec->format_type_ == CodecType::kVideoEncoder) {
      buf_header->nFilledLen = native_handle->data[4];
      buf_header->nTimeStamp = stream_buffer.timestamp / 1000;
    } else {
      buf_header->nFilledLen = stream_buffer.size;
      buf_header->nTimeStamp  = stream_buffer.timestamp;
    }

    if (avcodec->format_type_ == CodecType::kVideoEncoder)
      QMMF_VERBOSE("%s:%s ETB buffer fd(%d), ts(%lld)", TAG, __func__,
          stream_buffer.handle->data[0], stream_buffer.timestamp);
    else
      QMMF_VERBOSE("%s:%s ETB buffer data(%p), fd(%d), ts(%lld)", TAG, __func__,
          stream_buffer.data, stream_buffer.fd, stream_buffer.timestamp);

    ret = avcodec->EmptyThisBuffer(buf_header);
    if(ret != 0) {
        QMMF_ERROR("%s:%s ETB failed for buffer(%p)", TAG, __func__,
            buf_header->pBuffer);
        break;
    }

    if(thread_stop == true) {
      avcodec->getInputBufferSource()->NotifyStatus(
          CodecInputPortStatus::kInputPortStop);
      break;
    }
  }

  QMMF_INFO("%s:%s Exit", TAG, __func__);
  return NULL;
}

void* AVCodec::DeliverOutput(void *arg) {

  QMMF_INFO("%s:%s Enter", TAG, __func__);
  status_t ret = 0;

  CodecBuffer codec_buffer;
  OMX_BUFFERHEADERTYPE *buf_header;
  AVCodec *avcodec = static_cast<AVCodec*>(arg);
  while(1) {
    memset(&codec_buffer, 0x0, sizeof(codec_buffer));
    ret = avcodec->getOutputBufferSource()->GetBuffer(codec_buffer);

    assert(codec_buffer.pointer != NULL);

    buf_header = avcodec->GetBufferHdr(codec_buffer);
    assert(buf_header != NULL);


    if(avcodec->IsOutputPortStop()) {
      QMMF_INFO("%s:%s Encoder is stop. exit from thread", TAG, __func__);
      avcodec->getOutputBufferSource()->ReturnBuffer(codec_buffer);
      break;
    }

    ret = avcodec->FillThisBuffer(buf_header);
    if(ret != 0) {
      QMMF_ERROR("%s:%s FTB failed for buffer(%p)", TAG, __func__,
          buf_header->pBuffer);
      break;
    }

    QMMF_VERBOSE("%s:%s FTB buffer(%p), fd(%d)", TAG, __func__,
        codec_buffer.pointer, codec_buffer.fd);
  }

  QMMF_INFO("%s:%s Exit", TAG, __func__);
  return NULL;
}

void AVCodec::DeliverEvent(OMX_EVENTTYPE event, OMX_U32 data1, OMX_U32 data2) {

  if(event_cb_)
    event_cb_(event, data1, data2);
}

OMX_BUFFERHEADERTYPE *AVCodec::GetBufferHdr(StreamBuffer& buffer) {

  bool found = false;
  if (format_type_ == CodecType::kVideoEncoder) {
    for(uint32_t i = 0; i < in_buff_hdr_size_; i++) {
      encoder_media_buffer_type* mediaBuffer =
          (encoder_media_buffer_type*)in_buff_hdr_[i]->pBuffer;
      if(mediaBuffer->meta_handle == NULL) {
        QMMF_INFO("%s:%s Register native handle(%p) in buffer list(%p)", TAG,
            __func__, buffer.handle, in_buff_hdr_[i]);
        mediaBuffer->meta_handle = buffer.handle;
        return in_buff_hdr_[i];
      }
      if(mediaBuffer->meta_handle == buffer.handle) {
        return in_buff_hdr_[i];
      }
    }
    QMMF_ERROR("%s:%s No Buffer header found for(%p)", TAG, __func__,
              buffer.handle);
  } else {
    for(uint32_t i = 0; i < in_buff_hdr_size_; i++) {
      void* buf = static_cast<void*>(in_buff_hdr_[i]->pBuffer);
      if(buf == nullptr) {
        QMMF_INFO("%s:%s Register buffer(%p), fd(%d) in buffer list(%p)", TAG,
            __func__, buffer.data, buffer.fd, in_buff_hdr_[i]);
        in_buff_hdr_[i]->pBuffer = static_cast<OMX_U8*>(buffer.data);
        in_buff_hdr_[i]->pAppPrivate = reinterpret_cast<OMX_PTR>(buffer.fd);
        return in_buff_hdr_[i];
      }
      if(buf == buffer.data) {
        return in_buff_hdr_[i];
      }
    }
    QMMF_ERROR("%s:%s No Buffer header found for(%p)", TAG, __func__,
              buffer.data);
  }
  assert(found == true);

  return nullptr;
}

OMX_BUFFERHEADERTYPE *AVCodec::GetBufferHdr(CodecBuffer& buffer) {

  bool found = false;
  if (format_type_ == CodecType::kVideoEncoder) {
    for(uint32_t i = 0; i < out_buff_hdr_size_; i++) {
      void* buf = static_cast<void*>(out_buff_hdr_[i]->pAppPrivate);
      if(buf == nullptr) {
        QMMF_INFO("%s:%s Register buffer(%p), fd(%d) in buffer list(%p)", TAG,
            __func__, buffer.pointer, buffer.fd, out_buff_hdr_[i]);
        out_buff_hdr_[i]->pBuffer = static_cast<OMX_U8 *>(buffer.pointer);
        out_buff_hdr_[i]->pAppPrivate = static_cast<OMX_PTR>(buffer.pointer);
        return out_buff_hdr_[i];
      }
      if(buf == buffer.pointer) {
        return out_buff_hdr_[i];
      }
    }
  } else {
    for(uint32_t i = 0; i < out_buff_hdr_size_; i++) {
      CodecBuffer* buf = reinterpret_cast<CodecBuffer*>
                                         (out_buff_hdr_[i]->pAppPrivate);
      if(buf->pointer == nullptr) {
        QMMF_INFO("%s:%s Register buffer(%p), fd(%d) in buffer list(%p)", TAG,
            __func__, buffer.pointer, buffer.fd, out_buff_hdr_[i]);
        buf->pointer = buffer.pointer;
        buf->fd = buffer.fd;
        return out_buff_hdr_[i];
      }
      if(buf->pointer == buffer.pointer) {
        return out_buff_hdr_[i];
      }
    }
  }
  QMMF_ERROR("%s:%s No Buffer header found for(%p)",TAG,__func__,buffer.pointer);
  assert(found == true);

  return NULL;
}

status_t AVCodec::PushEventCommand(OMX_EVENTTYPE event, OMX_COMMANDTYPE command,
                                   OMX_U32 data, OMX_U32 flag) {

  status_t ret = 0;

  cmd_buffer_[cmd_buffer_index_].event_type = event;
  cmd_buffer_[cmd_buffer_index_].event_cmd = command;
  cmd_buffer_[cmd_buffer_index_].event_data= data;
  cmd_buffer_[cmd_buffer_index_].event_flags = flag;
  cmd_buffer_[cmd_buffer_index_].event_result = OMX_ErrorNone;

  QMMF_INFO("%s:%s Pushing cmd buffer(%p)", TAG, __func__,
      &cmd_buffer_[cmd_buffer_index_]);
  ret = signal_queue_.Push(&cmd_buffer_[cmd_buffer_index_]);
  if(ret != OK) {
    QMMF_ERROR("%s:%s Failed to push cmd buffer(%p)", TAG, __func__,
        &cmd_buffer_[cmd_buffer_index_]);
    return ret;
  }
  cmd_buffer_index_++;

  return ret;
}

status_t AVCodec::EmptyThisBuffer(OMX_BUFFERHEADERTYPE *buffer) {

  return omx_client_->EmptyThisBuffer(buffer);
}

status_t AVCodec::FillThisBuffer(OMX_BUFFERHEADERTYPE *buffer) {

  return omx_client_->FillThisBuffer(buffer);
}

status_t AVCodec::SetState(OMX_STATETYPE state, OMX_BOOL synchronous) {

  status_t ret = OK;

  QMMF_INFO("%s:%s current state(%s), pending state(%s)", TAG, __func__,
      OMX_STATE_NAME(state_), OMX_STATE_NAME(state_pending_));

  if (state == state_) {
    QMMF_WARN("%s:%s Current state is already %s", TAG, __func__,
        OMX_STATE_NAME(state));
    return ret;
  }

  // check for pending state transition
  if(state_ != state_pending_) {
    ret = WaitState(state_pending_);
    if(ret != OK) {
      QMMF_ERROR("%s:%s Wait for %s failed", TAG, __func__,
          OMX_STATE_NAME(state_pending_));
      return ret;
    }
  }

  // check for invalid transition
  if(((state == OMX_StateLoaded) && (state_ != OMX_StateIdle)) ||
      ((state == OMX_StateExecuting) && (state_ != OMX_StateIdle))) {
    QMMF_ERROR("%s:%s Invalid state tranisition: state %s to %s", TAG, __func__,
        OMX_STATE_NAME(state), OMX_STATE_NAME(state_));
    return OMX_ErrorIncorrectStateTransition;
  }

  QMMF_INFO("%s:%s Moving to state(%s) from(%s)", TAG, __func__,
      OMX_STATE_NAME(state), OMX_STATE_NAME(state_));
  ret = omx_client_->SendCommand(OMX_CommandStateSet, state, 0);

  if (ret != OK) {
    QMMF_ERROR("%s:%s Failed to set state(%s)", TAG, __func__,
        OMX_STATE_NAME(state));
    return ret;
  }

  state_pending_ = state;

  if (synchronous == OMX_TRUE) {
    return WaitState(state);
  }

  QMMF_INFO("%s:%s current state(%s), pending state(%s)", TAG, __func__,
      OMX_STATE_NAME(state_), OMX_STATE_NAME(state_pending_));
  return ret;
}

status_t AVCodec::WaitState(OMX_STATETYPE state) {

  status_t ret = OK;

  if(state_ == state) {
    QMMF_INFO("%s:%s State is already in %s", TAG, __func__,
        OMX_STATE_NAME(state));
    return ret;
  }

  CodecCmdType *cmd = (CodecCmdType *)signal_queue_.Pop();
  assert(cmd != nullptr);
  QMMF_INFO("%s:%s Popped buffer from cmd queue(%p) for index(%d)", TAG,
      __func__, cmd, cmd_buffer_index_);
  cmd_buffer_index_--;

  ret = cmd->event_result;

  if((cmd->event_type != OMX_EventCmdComplete) ||
      (cmd->event_cmd != OMX_CommandStateSet)) {
     QMMF_ERROR("%s:%s Expecting state change", TAG, __func__);
    return ret;
  }

  if((OMX_STATETYPE)cmd->event_data != state) {
    QMMF_ERROR("%s:%s Wrong state found(%s)", TAG, __func__,
        OMX_STATE_NAME((OMX_STATETYPE)cmd->event_data));
    return OMX_ErrorUndefined;
  }

  state_ = (OMX_STATETYPE)cmd->event_data;
  QMMF_INFO("%s:%s Reached state(%s)", TAG, __func__, OMX_STATE_NAME(state));

  return ret;
}

OMX_ERRORTYPE AVCodec::OnEvent(
                    OMX_IN OMX_HANDLETYPE component __attribute__((__unused__)),
                    OMX_IN OMX_PTR app_data,
                    OMX_IN OMX_EVENTTYPE event,
                    OMX_IN OMX_U32 data1,
                    OMX_IN OMX_U32 data2,
                    OMX_IN OMX_PTR event_data __attribute__((__unused__))) {

  QMMF_INFO("%s:%s Enter", TAG, __func__);
  AVCodec *avcodec = (AVCodec *)app_data;
  assert(avcodec != nullptr);

  if (event == OMX_EventCmdComplete) {
    if ((OMX_COMMANDTYPE)data1 == OMX_CommandStateSet) {
      QMMF_INFO("%s:%s Event callback: state is %s", TAG, __func__,
          OMX_STATE_NAME((OMX_STATETYPE)data2));
      avcodec->PushEventCommand(event, OMX_CommandStateSet, data2, 0x0);

    } else if ((OMX_COMMANDTYPE)data1 == OMX_CommandFlush) {
      QMMF_INFO("%s:%s Event callback: flush complete on port : %s", TAG,
          __func__, OMX_PORT_NAME(data2));
      avcodec->PushEventCommand(event, OMX_CommandFlush, data2, 0x0);

    } else if ((OMX_COMMANDTYPE)data1 == OMX_CommandPortDisable) {
      QMMF_INFO("%s:%s Event callback: %s port disable", TAG, __func__,
              OMX_PORT_NAME(data2));
      avcodec->PushEventCommand(event, OMX_CommandPortDisable, data2, 0x0);

    } else if ((OMX_COMMANDTYPE)data1 == OMX_CommandPortEnable) {
      QMMF_INFO("%s:%s Event callback: %s port enable", TAG, __func__,
                OMX_PORT_NAME(data2));
      avcodec->PushEventCommand(event, OMX_CommandPortEnable, data2, 0x0);

    } else {
      QMMF_WARN("%s:%s Unimplemented command", TAG, __func__);
    }
  } else if (event == OMX_EventError) {
    avcodec->DeliverEvent(event, data1, data2);

  } else if (event == OMX_EventBufferFlag) {
    QMMF_INFO("%s:%s Event callback: Buffer flag received", TAG, __func__);

  } else {
    QMMF_WARN("%s:%s Unimplemented event", TAG, __func__);
  }

  QMMF_INFO("%s:%s Exit", TAG, __func__);
  return OMX_ErrorNone;
}

OMX_ERRORTYPE AVCodec::OnEmptyBufferDone(
                      OMX_IN OMX_HANDLETYPE handle __attribute__((__unused__)),
                      OMX_IN OMX_PTR app_data,
                      OMX_IN OMX_BUFFERHEADERTYPE* buf_header) {
  QMMF_DEBUG("%s:%s Enter", TAG, __func__);
  QMMF_VERBOSE("%s:%s buf_header[%p]", TAG, __func__, buf_header);
  QMMF_VERBOSE("%s:%s buf_header->pBuffer[%p]", TAG, __func__,
               buf_header->pBuffer);
  QMMF_VERBOSE("%s:%s buf_header->pAppPrivate[%p]", TAG, __func__,
               buf_header->pAppPrivate);

  //TODO: use pBuffer
  AVCodec *avcodec = (AVCodec *)app_data;
  StreamBuffer stream_buffer;
  memset(&stream_buffer, 0x0, sizeof stream_buffer);
  if (avcodec->format_type_ == CodecType::kVideoEncoder) {
    encoder_media_buffer_type* mediaBuffer =
        (encoder_media_buffer_type*)buf_header->pBuffer;
    assert(mediaBuffer->meta_handle != NULL);

    stream_buffer.handle = mediaBuffer->meta_handle;

    QMMF_INFO("%s:%s EBD fd(%d), ts(%lld)", TAG, __func__,
        stream_buffer.handle->data[0], buf_header->nTimeStamp);
  } else {
    assert(buf_header->pBuffer != nullptr);
    stream_buffer.data = buf_header->pBuffer;
    stream_buffer.fd = reinterpret_cast<int32_t>(buf_header->pAppPrivate);
    QMMF_INFO("%s:%s EBD buffer[%s]", TAG, __func__,
              stream_buffer.ToString().c_str());
  }

  avcodec->getInputBufferSource()->SignalBufferReturned(stream_buffer);
  if(buf_header->nFlags & OMX_BUFFERFLAG_EOS) {
    QMMF_INFO("%s:%s No more buffer to process on input port", TAG, __func__);
    avcodec->getInputBufferSource()->NotifyStatus(
        CodecInputPortStatus::kInputPortIdle);
  }

  QMMF_INFO("%s:%s Exit", TAG, __func__);
  return OMX_ErrorNone;
}

OMX_ERRORTYPE AVCodec::OnFillBufferDone(
                      OMX_IN OMX_HANDLETYPE handle __attribute__((__unused__)),
                      OMX_IN OMX_PTR app_data,
                      OMX_IN OMX_BUFFERHEADERTYPE* buf_header) {
  QMMF_DEBUG("%s:%s Enter", TAG, __func__);
  QMMF_VERBOSE("%s:%s buf_header[%p]", TAG, __func__, buf_header);
  QMMF_VERBOSE("%s:%s buf_header->pBuffer[%p]", TAG, __func__,
               buf_header->pBuffer);
  QMMF_VERBOSE("%s:%s buf_header->pAppPrivate[%p]", TAG, __func__,
               buf_header->pAppPrivate);

  AVCodec *avcodec = (AVCodec *)app_data;
  assert(buf_header->pBuffer);
  CodecBuffer codec_buffer;
  memset(&codec_buffer, 0x0, sizeof codec_buffer);

  codec_buffer.pointer = buf_header->pBuffer;
  codec_buffer.filled_length = buf_header->nFilledLen;
  codec_buffer.flag = buf_header->nFlags;
  codec_buffer.ts = buf_header->nTimeStamp;

  if (avcodec->format_type_ == CodecType::kAudioEncoder) {
    CodecBuffer* buf = reinterpret_cast<CodecBuffer*>(buf_header->pAppPrivate);
    codec_buffer.pointer = buf->pointer;
    codec_buffer.fd = buf->fd;
    codec_buffer.filled_length = 0;
    codec_buffer.frame_length = buf_header->nAllocLen;
    memset(codec_buffer.pointer, 0x0, sizeof codec_buffer.frame_length);

    uint8_t* src = reinterpret_cast<uint8_t*>(buf_header->pBuffer);
    unsigned int num_of_frames = src[0];
    QMMF_VERBOSE("%s:%s number of audio frames[%u]", TAG, __func__,
                 num_of_frames);
    ++src;

    if (!((codec_buffer.flag) & OMX_BUFFERFLAG_EOS) && (num_of_frames > 0)) {
      AudioEncoderMetadata* meta = reinterpret_cast<AudioEncoderMetadata*>(src);
      QMMF_VERBOSE("%s:%s audio metadata[%s]", TAG, __func__,
                   meta->ToString().c_str());
      size_t length = meta->frame_size;
      const uint8_t* source_ptr = reinterpret_cast<const uint8_t*>
                                                  (buf_header->pBuffer)
                                  + 1 + meta->offset_to_frame;
      uint8_t* dest_ptr = reinterpret_cast<uint8_t*>(codec_buffer.pointer)
                          + codec_buffer.filled_length;
      memcpy(dest_ptr, source_ptr, length);
      codec_buffer.filled_length += length;
      codec_buffer.ts = ((uint64_t)(meta->msw_ts) << 32) |
                        (uint64_t)(meta->lsw_ts);
      src += sizeof(meta);
      --num_of_frames;
    }
    if (num_of_frames > 0)
      QMMF_WARN("%s:%s multiple audio frames were found in one buffer",
                TAG, __func__);
  }

  if ((codec_buffer.flag) & OMX_BUFFERFLAG_EOS) {
    QMMF_INFO("%s:%s received output port EOS", TAG, __func__);
    avcodec->StopOutput();
    avcodec->PushEventCommand((OMX_EVENTTYPE)0, (OMX_COMMANDTYPE)0, 0,
        OMX_BUFFERFLAG_EOS);
  }

  QMMF_INFO("%s:%s FBD buffer[%s]", TAG, __func__,
            codec_buffer.ToString().c_str());

  avcodec->getOutputBufferSource()->ReturnBuffer(codec_buffer);
  QMMF_INFO("%s:%s Exit", TAG, __func__);
  return OMX_ErrorNone;
}

} // namespace qmmf
