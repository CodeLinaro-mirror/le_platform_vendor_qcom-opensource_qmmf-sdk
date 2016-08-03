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
#include <tuple>
#include <vector>

#include <binder/Parcel.h>

namespace qmmf {

using ::android::Parcel;
using ::std::boolalpha;
using ::std::noboolalpha;
using ::std::string;
using ::std::stringstream;
using ::std::tuple;
using ::std::vector;

enum class CodecType {
  kVideoEncoder,
  kVideoDecoder,
  kAudioEncoder,
  kAudioDecoder,
  kImageEncoder,
  kImageDecoder,
};

/*
 * Codec type for video tracks. If the codec type is set as YUV
 * no encoding is done on the track buffers returned to client.
 */
enum class VideoFormat {
  kHEVC,
  kAVC,
  kJPEG,
  kYUV,
  kBayerRDI,
  kBayerIdeal,
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

/* @brief param data for kInitQPType
 * init_IQP       : First Iframe QP
 * init_PQP       : First Pframe QP
 * init_BQP       : First Bframe QP
 * init_QP_mode : Bit field indicating which frame type(s) shall
 *                            use the specified initial QP.
 *                         Bit 0: Enable initial QP for I/IDR
 *                                and use value specified in init_IQP
 *                         Bit 1: Enable initial QP for P
 *                                and use value specified in init_PQP
 *                         Bit 2: Enable initial QP for B
 *                                and use value specified in init_BQP
 */
typedef struct VideoEncodeInitQP {
  uint32_t    init_IQP;
  uint32_t    init_PQP;
  uint32_t    init_BQP;
  uint32_t    init_QP_mode;
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
} AVCParams;

typedef struct HEVCParams {
  int32_t              idr_interval;
  uint32_t             bitrate;
  HEVCProfileType      profile;
  HEVCLevelType        level;
  VideoRateControlType ratecontrol_type;
  VideoQPParams        qp_params;
} HEVCParams;

typedef struct JPEGParams {
  int32_t quality;
} JPEGParams;

/*
 * Detail video encoder paramters
 *
 * The values of the union structure is interpreted based on the
 * codec type
 */
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

/* @brief param data for kIntraPeriodType */
typedef struct VideoEncodeIDRInterval {
  int32_t    idr_period;
  int32_t    num_P_frames;
  int32_t    num_B_frames;
} VideoEncodeIDRInterval;

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


/* list of formats for audio codecs */
enum class AudioFormat {
  kPCM,
  kAAC,
  kAMR,
  kG711,
};

/* Handle to a specific codec */
typedef int CodecID;

struct CodecIDList {
  vector<CodecID> ids;

  string ToString() const {
    stringstream stream;
    for (CodecID id : ids)
      stream << id << ", ";
    stream << "SIZE[" << ids.size() << "]";
    return stream.str();
  }

  void ToParcel(Parcel* parcel) const {
    parcel->writeUint32(static_cast<uint32_t>(ids.size()));
    for (CodecID id : ids)
      parcel->writeInt32(static_cast<int32_t>(id));
  }

  void FromParcel(const Parcel& parcel) {
    size_t number_of_elements = static_cast<size_t>(parcel.readUint32());
    for (size_t index = 0; index < number_of_elements; ++index)
      ids.push_back(static_cast<CodecID>(parcel.readInt32()));
  }
};

union CodecFormat {
  VideoFormat video;
  AudioFormat    audio;
  ImageFormat    image;

  string ToString(CodecType key) const {
    stringstream stream;
    switch (key) {
      case CodecType::kVideoEncoder:
      case CodecType::kVideoDecoder:
        stream << "video[" << static_cast<int>(video) << "]";
        break;
      case CodecType::kAudioEncoder:
      case CodecType::kAudioDecoder:
        stream << "audio[" << static_cast<int>(audio) << "]";
        break;
      case CodecType::kImageEncoder:
      case CodecType::kImageDecoder:
        stream << "image[" << static_cast<int>(image) << "]";
        break;
    }
    return stream.str();
  }

  void ToParcel(CodecType key, Parcel* parcel) const {
    parcel->writeInt32(static_cast<int32_t>(key));
    switch (key) {
      case CodecType::kVideoEncoder:
      case CodecType::kVideoDecoder:
        parcel->writeInt32(static_cast<int32_t>(video));
        break;
      case CodecType::kAudioEncoder:
      case CodecType::kAudioDecoder:
        parcel->writeInt32(static_cast<int32_t>(audio));
        break;
      case CodecType::kImageEncoder:
      case CodecType::kImageDecoder:
        parcel->writeInt32(static_cast<int32_t>(image));
        break;
    }
  }

  void FromParcel(const Parcel& parcel) {
    CodecType key = static_cast<CodecType>(parcel.readInt32());
    switch (key) {
      case CodecType::kVideoEncoder:
      case CodecType::kVideoDecoder:
        video = static_cast<VideoFormat>(parcel.readInt32());
        break;
      case CodecType::kAudioEncoder:
      case CodecType::kAudioDecoder:
        audio = static_cast<AudioFormat>(parcel.readInt32());
        break;
      case CodecType::kImageEncoder:
      case CodecType::kImageDecoder:
        image = static_cast<ImageFormat>(parcel.readInt32());
        break;
    }
  }
};

struct CodecInfo {
  CodecType type;
  CodecFormat format;
  CodecID id;

  string ToString() const {
    stringstream stream;
    stream << "type[" << static_cast<int>(type) << "] ";
    stream << "format[" << format.ToString(type) << "] ";
    stream << "id[" << id << "]";
    return stream.str();
  }

  void ToParcel(Parcel* parcel) const {
    parcel->writeInt32(static_cast<int32_t>(type));
    format.ToParcel(type, parcel);
    parcel->writeInt32(static_cast<int32_t>(id));
  }

  void FromParcel(const Parcel& parcel) {
    type = static_cast<CodecType>(parcel.readInt32());
    format.FromParcel(parcel);
    id = static_cast<CodecID>(parcel.readInt32());
  }
};

struct CodecInfoList {
  vector<CodecInfo> codec_infos;

  string ToString() const {
    stringstream stream;
    stream << "codecs[";
    for (const CodecInfo& codec_info : codec_infos)
      stream << codec_info.ToString() << ", ";
    stream << "SIZE[" << codec_infos.size() << "]]";
    return stream.str();
  }

  void ToParcel(Parcel* parcel) const {
    parcel->writeUint32(static_cast<uint32_t>(codec_infos.size()));
    for (const CodecInfo& codec_info : codec_infos)
      codec_info.ToParcel(parcel);
  }

  void FromParcel(const Parcel& parcel) {
    size_t number_of_elements = static_cast<size_t>(parcel.readUint32());
    for (size_t index = 0; index < number_of_elements; ++index) {
      CodecInfo codec_info;
      codec_info.FromParcel(parcel);
      codec_infos.push_back(codec_info);
    }
  }
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
  AACMode mode;
  int frame_length;
  int bit_rate;

  string ToString() const {
    stringstream stream;
    stream << "format[" << static_cast<int>(format) << "] ";
    stream << "mode[" << static_cast<int>(mode) << "] ";
    stream << "frame_length[" << frame_length << "] ";
    stream << "bit_rate[" << bit_rate << "]";
    return stream.str();
  }

  void ToParcel(Parcel* parcel) const {
    parcel->writeInt32(static_cast<int32_t>(format));
    parcel->writeInt32(static_cast<int32_t>(mode));
    parcel->writeInt32(static_cast<int32_t>(frame_length));
    parcel->writeInt32(static_cast<int32_t>(bit_rate));
  }

  void FromParcel(const Parcel& parcel) {
    format = static_cast<AACFormat>(parcel.readInt32());
    mode = static_cast<AACMode>(parcel.readInt32());
    frame_length = static_cast<int>(parcel.readInt32());
    bit_rate = static_cast<int>(parcel.readInt32());
  }
};

struct AMRParams {
  bool isWAMR;
  int bit_rate;

  string ToString() const {
    stringstream stream;
    stream << "frame_length[" << boolalpha << isWAMR << noboolalpha << "] ";
    stream << "bit_rate[" << bit_rate << "]";
    return stream.str();
  }

  void ToParcel(Parcel* parcel) const {
    parcel->writeInt32(static_cast<int32_t>(isWAMR));
    parcel->writeInt32(static_cast<int32_t>(bit_rate));
  }

  void FromParcel(const Parcel& parcel) {
    isWAMR = static_cast<bool>(parcel.readInt32());
    bit_rate = static_cast<int>(parcel.readInt32());
  }
};

enum class G711Mode {
  kALaw,
  kMuLaw,
};

struct G711Params {
  G711Mode mode;
  int bit_rate;

  string ToString() const {
    stringstream stream;
    stream << "mode[" << static_cast<int>(mode) << "] ";
    stream << "bit_rate[" << bit_rate << "]";
    return stream.str();
  }

  void ToParcel(Parcel* parcel) const {
    parcel->writeInt32(static_cast<int32_t>(mode));
    parcel->writeInt32(static_cast<int32_t>(bit_rate));
  }

  void FromParcel(const Parcel& parcel) {
    mode = static_cast<G711Mode>(parcel.readInt32());
    bit_rate = static_cast<int>(parcel.readInt32());
  }
};

union AudioCodecParams {
  AACParams aac;
  AMRParams amr;
  G711Params g711;

  string ToString(AudioFormat key) const {
    stringstream stream;
    switch (key) {
      case AudioFormat::kPCM:
        stream << "N/A (PCM)";
        break;
      case AudioFormat::kAAC:
        stream << "aac[" << aac.ToString() << "]";
        break;
      case AudioFormat::kAMR:
        stream << "amr[" << amr.ToString() << "]";
        break;
      case AudioFormat::kG711:
        stream << "g711[" << g711.ToString() << "]";
        break;
    }
    return stream.str();
  }

  void ToParcel(AudioFormat key, Parcel* parcel) const {
    parcel->writeInt32(static_cast<int32_t>(key));
    switch (key) {
      case AudioFormat::kPCM: /* nothing to write */ break;
      case AudioFormat::kAAC: aac.ToParcel(parcel); break;
      case AudioFormat::kAMR: amr.ToParcel(parcel); break;
      case AudioFormat::kG711: g711.ToParcel(parcel); break;
    }
  }

  void FromParcel(const Parcel& parcel) {
    AudioFormat key = static_cast<AudioFormat>(parcel.readInt32());
    switch (key) {
      case AudioFormat::kPCM: /* nothing to read */ break;
      case AudioFormat::kAAC: aac.FromParcel(parcel); break;
      case AudioFormat::kAMR: amr.FromParcel(parcel); break;
      case AudioFormat::kG711: g711.FromParcel(parcel); break;
    }
  }
};

}; /* namespace qmmf */
