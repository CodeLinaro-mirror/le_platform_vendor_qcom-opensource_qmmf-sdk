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

namespace qmmf {

// Codec format for video tracks.
enum class VideoFormat {
    kHEVC,
    kAVC,
    kJPEG,
    kYUV,
    kBayerRDI,
    kBayerIdeal
};

// Codec format for audio tracks.
enum class AudioFormat {
    kAAC,
    kAMR,
    kG711,
    kPCM
};

enum class ImageFormat {
    kJPEG,
    kNV12,
    kBayerIdeal,
    kBayerRDI
};

enum class CodecType {
    kVideoEncoder,
    kVideoDecoder,
    kAudioEncoder,
    kAudioDecoder,
    kImageEncoder,
    kImageDecoder,
};

typedef std::string CodecID;

typedef struct CodecInfo {
    CodecType type;
    union {
        AudioFormat audio;
        VideoFormat video;
    } format;
    CodecID id;
} CodecInfo;

enum class AACMode {
    kAALC,
    kHEVC_v1,
    kHEVC_v2
};

enum class AACFormat {
    kADTS,
    kADIF,
    kRAW
};

typedef struct AACParams {
    AACFormat format;
    AACMode mode;
    uint32_t frame_length;
    uint32_t bit_rate;
} AACParams;

typedef struct AMRParams {
    bool isWAMR;
    uint32_t bit_rate;
} NBAMRParams;

enum class G711Mode {
    kG711ALaw,
    kG711MuLaw
};

typedef struct G711Params {
    G711Mode G711_mode;
    uint32_t bit_rate;
} G711Params;

// @brief Detail audio encoder paramters
//
// The values of the union structure is interpreted based on the
// audio format type
typedef union AudioCodecParams {
    AACParams aac;
    AMRParams amr;
    G711Params g711;
} AudioCodecParams;

enum class AVCProfileType {
    kBaseline,
    kMain,
    kHigh
};

enum class AVCLevelType {
    kLevel3,
    kLevel4,
    kLevel5,
    kLevel5_1,
    kLevel5_2,
};

enum class HEVCProfileType {
    kMain
};

enum class HEVCLevelType {
    kLevel3,
    kLevel4,
    kLevel5,
    kLevel5_1,
    kLevel5_2,
};

// @brief param data for kInitQPType
// init_IQP       : First Iframe QP
// init_PQP       : First Pframe QP
// init_BQP       : First Bframe QP
// init_QP_mode : Bit field indicating which frame type(s) shall
//                            use the specified initial QP.
//                         Bit 0: Enable initial QP for I/IDR
//                                and use value specified in init_IQP
//                         Bit 1: Enable initial QP for P
//                                and use value specified in init_PQP
//                         Bit 2: Enable initial QP for B
//                                and use value specified in init_BQP
typedef struct VideoEncodeInitQP {
    int32_t    init_IQP;
    int32_t    init_PQP;
    int32_t    init_BQP;
    int32_t    init_QP_mode;
} VideoEncodeInitQP;

// @brief param data for kQPRangeType
typedef struct VideoEncodeQPRange {
    int32_t    min_QP;
    int32_t    max_QP;
} VideoEncodeQPRange;

// @brief param data for kIPBQPRangeType
typedef struct VideoEncodeIPBQPRange {
    int32_t    min_IQP;
    int32_t    max_IQP;
    int32_t    min_PQP;
    int32_t    max_PQP;
    int32_t    min_BQP;
    int32_t    max_BQP;
} VideoEncodeIPBQPRange;

typedef struct VideoQPParams {
    bool enable_init_qp;
    VideoEncodeInitQP init_qp;
    bool enable_qp_range;
    VideoEncodeQPRange qp_range;
    bool enable_qp_IBP_range;
    VideoEncodeIPBQPRange qp_IBP_range;
} VideoQPParams;

typedef struct HEVCParams {
    int32_t idr_interval;
    int32_t bitrate;
    AVCProfileType profile;
    AVCProfileLevelType level;
    VideoRateControlType ratecontrol_type;
    VideoQPParams qp_params;
} HEVCParams;

typedef struct AVCParams {
    int32_t idr_interval;
    uint32_t bitrate;
    HEVCProfileType profile;
    HEVCLevelType level;
    VideoRateControlType ratecontrol_type;
    VideoQPParams qp_params;
} AVCParams;

typedef struct JPEGParams {
    int32_t quality;
} JPEGParams;

// Detail video encoder paramters
//
// The values of the union structure is interpreted based on the
// codec type
typedef union VideoCodecParams {
    HEVCParams hevc;
    AVCParams avc;
    JPEGParams jpeg;
} VideoCodecParams;

typedef struct ThumbnailParams {
    uint32_t width;
    uint32_t height;
    uint32_t quality;
} ThumbnailParams;

// tuple to represent image information. First parameter is the width
// second height, third image_quality and fourth image type.
// For thumbnail images only kJPEG is supported
// For YUV and Bayer formats, quality is ignored
typedef std::tuple <uint32_t, uint32_t, uint32_t, ImageType> ImageInfo;

// @brief Detail configuration for ImageCapture

// num_images: Total number of images to capture. If this value is set to -1,
//      take image continues until CancelImageCapture is called
// main_image_param: width, height, quality, format info for main image
// thumbnail_image_param : This is a vector to represent thumbnail info in a image
//       Do note that number of thumbnails supported would be limited based on platform
// sensor_frame_skip_interval: When multiple images needs to be captured clients can set the
//      sample_rate of capture through this parameter. If this value is set to 1, every
//      alternate image is captured, if 2, every 3rd image is captured and so on
// with_exif: Applies only when image codec is set to JPEG. If set to true, EXIF along with
//      with thumbnails are embedded with the image. Else thumbnails and camera meta information
//      is send separately through metadata
// with_camera_meta: Applies only with with_exif is set to false. In this case, camera metadata is
//      send separately
// with_raw: Enables clients to take a RAW image along with JPEG. This can
//       be set to true only when ImageType is NOT RAW
// raw_image_type: Could be either of RDI RAW or IDEAL Raw
struct ImageParam {
    uint32_t num_images;
    ImageInfo main_image_param;
    std::vector<ImageInfo> thumbnail_image_param;
    uint32_t sensor_frame_skip_interval;
    bool with_exif;
    bool with_camera_meta;
    bool with_raw;
    ImageFormat raw_image_format;
};

}; // namespace qmmf
