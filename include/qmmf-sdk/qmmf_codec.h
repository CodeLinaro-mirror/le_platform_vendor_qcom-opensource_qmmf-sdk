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

#pragma once

#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace qmmf {

using ::std::string;
using ::std::stringstream;
using ::std::vector;

typedef int32_t  CodecID;

enum class CodecType {
  kVideoEncoder,
  kVideoDecoder,
  kAudioEncoder,
  kAudioDecoder,
  kImageEncoder,
  kImageDecoder,
};

enum class VideoFormat {
  kHEVC,
  kAVC,
  kJPEG,
  kYUV,
  kBayerRDI,
  kBayerIdeal,
};

enum class CodecParamType {
  kBitRateType,
  kFrameRateType,
  kInsertIDRType,
  kIDRIntervalType,
  kCamFrameCropType,
  kMarkLtrType,
  kUseLtrType,
};

enum class AVCProfileType {
  kBaseline,
  kMain,
  kHigh,
};

enum class AVCLevelType {
  kLevel3,
  kLevel4,
  kLevel5,
  kLevel5_1,
  kLevel5_2,
};

enum class HEVCProfileType {
  kMain,
};

enum class HEVCLevelType {
  kLevel3,
  kLevel4,
  kLevel5,
  kLevel5_1,
  kLevel5_2,
};

/// @brief Data structure for specifying the initial quantization values to
/// video encoder
typedef struct VideoEncodeInitQP {
  uint32_t    init_IQP;       ///< First Iframe QP
  uint32_t    init_PQP;       ///< First Pframe QP
  uint32_t    init_BQP;       ///< First Bframe QP
  uint32_t    init_QP_mode;   ///< Bit field indicating which frame type(s) shall
                              ///< use the specified initial QP.
                              ///< Bit 0: Enable initial QP for I/IDR
                              ///<       and use value specified in init_IQP
                              ///< Bit 1: Enable initial QP for P
                              ///<       and use value specified in init_PQP
                              ///< Bit 2: Enable initial QP for B
                              ///<       and use value specified in init_BQP
} VideoEncodeInitQP;

typedef struct VideoEncodeQPRange {
  uint32_t    min_QP;
  uint32_t    max_QP;
} VideoEncodeQPRange;

typedef struct VideoEncodeIPBQPRange {
  uint32_t    min_IQP;
  uint32_t    max_IQP;
  uint32_t    min_PQP;
  uint32_t    max_PQP;
  uint32_t    min_BQP;
  uint32_t    max_BQP;
} VideoEncodeIPBQPRange;

typedef struct VideoQPParams {
  bool                  enable_init_qp;
  VideoEncodeInitQP     init_qp;
  bool                  enable_qp_range;
  VideoEncodeQPRange    qp_range;
  bool                  enable_qp_IBP_range;
  VideoEncodeIPBQPRange qp_IBP_range;
} VideoQPParams;

enum class VideoRateControlType {
  kDisable,
  kVariableSkipFrames,
  kVariable,
  kConstantSkipFrames,
  kConstant,
};

typedef struct AVCParams {
  uint32_t             idr_interval;
  uint32_t             bitrate;
  AVCProfileType       profile;
  AVCLevelType         level;
  VideoRateControlType ratecontrol_type;
  VideoQPParams        qp_params;
  uint32_t             ltr_count;
  uint32_t             hier_layer;
} AVCParams;

typedef struct HEVCParams {
  int32_t              idr_interval;
  uint32_t             bitrate;
  HEVCProfileType      profile;
  HEVCLevelType        level;
  VideoRateControlType ratecontrol_type;
  VideoQPParams        qp_params;
  uint32_t             ltr_count;
  uint32_t             hier_layer;
} HEVCParams;

typedef struct JPEGParams {
  int32_t quality;
} JPEGParams;

typedef union VideoCodecParams {
  HEVCParams hevc;
  AVCParams  avc;
  JPEGParams jpeg;
} VideoCodecParam;

enum class VideoTrackParamType {
  kBitRateType,
  kFrameRateType,
  kInsertIDRType,
  kIDRIntervalType,
  kIntraPeriodType,
  kCamFrameCropType,
};

typedef struct VideoEncodeIDRInterval {
  int32_t    idr_period;
  int32_t    num_P_frames;
  int32_t    num_B_frames;
} VideoEncodeIDRInterval;

typedef struct VideoEncLtrUse {
  int32_t id;
  int32_t frame;
} VideoLtrUse;

typedef struct VideoEncIdrInterval {
  int32_t idr_period;
  int32_t num_pframes;
  int32_t num_bframes;
} VideoIdrInterval;

//Dynamic Video Encode Parameters
typedef struct VideoEncSetParam {
  uint32_t            bitrate;
  uint32_t            fps;
  uint32_t            idr_request;
  uint32_t            ltr_mark;
  uint32_t            ltr_period;
  uint32_t            max_hip_layer;
  uint32_t            ltr_count;
  VideoEncLtrUse      ltr_use;
  VideoEncIdrInterval idr_interval;
} VideoEncSetParam;

enum class ImageFormat {
  kJPEG,
  kNV12,
  kBayerIdeal,
  kBayerRDI,
};

enum class ImageMetaDataType {
  kMainImage,
  kThumbnailImage,
  kRawImage,
  kCameraMeta,
};

enum class AudioFormat {
  kPCM,
  kAAC,
  kAMR,
  kG711,
};

union CodecFormat {
  VideoFormat video;
  AudioFormat audio;
  ImageFormat image;
};

struct CodecInfo {
  CodecType   type;
  CodecFormat format;
};

enum class AudioTrackParamType {
  kAudioEffectsParamType,
  kAudioVolumeParamType,
};

enum class AACFormat {
  kADTS,
  kADIF,
  kRAW,
};

enum class AACMode {
  kAALC,
  kHEVC_v1,
  kHEVC_v2,
};

struct AACParams {
  AACFormat format;
  AACMode   mode;
  int32_t   frame_length;
  int32_t   bit_rate;
};

struct AMRParams {
  bool    isWAMR;
  int32_t bit_rate;
};

enum class G711Mode {
  kALaw,
  kMuLaw,
};

struct G711Params {
  G711Mode mode;
  int32_t  bit_rate;
};

union AudioCodecParams {
  AACParams  aac;
  AMRParams  amr;
  G711Params g711;
};

}; /* namespace qmmf */
