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

#include <sys/types.h>

#include <cstdint>
#include <type_traits>
#include <vector>

#include <binder/Parcel.h>

#include "common/qmmf_codec_internal.h"
#include "include/qmmf-sdk/qmmf_recorder_params.h"

namespace qmmf {
namespace recorder {

using ::android::Parcel;
using ::std::vector;
using ::std::underlying_type;

struct AudioTrackCreateParamInternal : public AudioTrackCreateParam {
  AudioTrackCreateParamInternal() {}
  AudioTrackCreateParamInternal(AudioTrackCreateParam& base)
      : AudioTrackCreateParam(base) {}
  AudioTrackCreateParamInternal(const AudioTrackCreateParam& base)
      : AudioTrackCreateParam(const_cast<AudioTrackCreateParam&>(base)) {}

  void ToParcel(Parcel* parcel) const {
    parcel->writeUint32(static_cast<uint32_t>(in_devices.size()));
    for (const DeviceId device : in_devices)
      parcel->writeInt32(static_cast<int32_t>(device));
    parcel->writeUint32(sample_rate);
    parcel->writeUint32(channels);
    parcel->writeUint32(bit_depth);
    parcel->writeInt32(static_cast<underlying_type<AudioFormat>::type>(format));
    switch (format) {
      case AudioFormat::kPCM:
        // nothing to write
        break;
      case AudioFormat::kAAC:
        AACParamsInternal(codec_params.aac).ToParcel(parcel);
        break;
      case AudioFormat::kAMR:
        AMRParamsInternal(codec_params.amr).ToParcel(parcel);
        break;
      case AudioFormat::kG711:
        G711ParamsInternal(codec_params.g711).ToParcel(parcel);
        break;
    }
    parcel->writeInt32(static_cast<int32_t>(out_device));
    parcel->writeUint32(flags);
  }

  AudioTrackCreateParamInternal& FromParcel(const Parcel& parcel) {
    size_t number_of_elements = static_cast<size_t>(parcel.readUint32());
    for (size_t index = 0; index < number_of_elements; ++index)
      in_devices.push_back(static_cast<DeviceId>(parcel.readInt32()));
    sample_rate = parcel.readUint32();
    channels = parcel.readUint32();
    bit_depth = parcel.readUint32();
    format = static_cast<AudioFormat>(parcel.readInt32());
    switch (format) {
      case AudioFormat::kPCM:
        // nothing to read
        break;
      case AudioFormat::kAAC:
        codec_params.aac = AACParamsInternal().FromParcel(parcel);
        break;
      case AudioFormat::kAMR:
        codec_params.amr = AMRParamsInternal().FromParcel(parcel);
        break;
      case AudioFormat::kG711:
        codec_params.g711 = G711ParamsInternal().FromParcel(parcel);
        break;
    }
    out_device = static_cast<DeviceId>(parcel.readInt32());
    flags = parcel.readUint32();
    return *this;
  }
};

struct VideoTrackCreateParamInternal : public VideoTrackCreateParam {
  VideoTrackCreateParamInternal() {}
  VideoTrackCreateParamInternal(VideoTrackCreateParam& base)
      : VideoTrackCreateParam(base) {}
  VideoTrackCreateParamInternal(const VideoTrackCreateParam& base)
      : VideoTrackCreateParam(const_cast<VideoTrackCreateParam&>(base)) {}

  void ToParcel(Parcel* parcel) const {
    parcel->writeUint32(camera_id);
    parcel->writeUint32(width);
    parcel->writeUint32(height);
    parcel->writeUint32(frame_rate);
    parcel->writeInt32(
        static_cast<underlying_type<VideoFormat>::type>(format_type));
    switch (format_type) {
      case VideoFormat::kHEVC:
        HEVCParamsInternal(codec_param.hevc).ToParcel(parcel);
        break;
      case VideoFormat::kAVC:
        AVCParamsInternal(codec_param.avc).ToParcel(parcel);
        break;
      case VideoFormat::kYUV:
      case VideoFormat::kBayerRDI:
      case VideoFormat::kBayerIdeal:
        // nothing to write
        break;
    }
    parcel->writeUint32(out_device);
  }

  VideoTrackCreateParamInternal& FromParcel(const Parcel& parcel) {
    camera_id = parcel.readUint32();
    width = parcel.readUint32();
    height = parcel.readUint32();
    frame_rate = parcel.readUint32();
    format_type = static_cast<VideoFormat>(parcel.readInt32());
    switch (format_type) {
      case VideoFormat::kHEVC:
        codec_param.hevc = HEVCParamsInternal().FromParcel(parcel);
        break;
      case VideoFormat::kAVC:
        codec_param.avc = AVCParamsInternal().FromParcel(parcel);
        break;
      case VideoFormat::kYUV:
      case VideoFormat::kBayerRDI:
      case VideoFormat::kBayerIdeal:
        // nothing to read
        break;
    }
    out_device = parcel.readUint32();
    return *this;
  }
};

struct CameraStartParamInternal : public CameraStartParam {
  CameraStartParamInternal() {}
  CameraStartParamInternal(CameraStartParam& base) : CameraStartParam(base) {}
  CameraStartParamInternal(const CameraStartParam& base)
      : CameraStartParam(const_cast<CameraStartParam&>(base)) {}

  void ToParcel(Parcel* parcel) const {
    parcel->writeInt32(static_cast<int32_t>(zsl_mode));
    parcel->writeUint32(zsl_queue_depth);
    parcel->writeUint32(zsl_width);
    parcel->writeUint32(zsl_height);
    parcel->writeUint32(frame_rate);
    parcel->writeUint32(flags);
  }

  CameraStartParamInternal& FromParcel(const Parcel& parcel) {
    zsl_mode = static_cast<bool>(parcel.readInt32());
    zsl_queue_depth = parcel.readUint32();
    zsl_width = parcel.readUint32();
    zsl_height = parcel.readUint32();
    frame_rate = parcel.readUint32();
    flags = parcel.readUint32();
    return *this;
  }
};

struct ImageParamInternal : public ImageParam {
  ImageParamInternal() {}
  ImageParamInternal(ImageParam& base) : ImageParam(base) {}
  ImageParamInternal(const ImageParam& base)
      : ImageParam(const_cast<ImageParam&>(base)) {}

  void ToParcel(Parcel* parcel) const {
    parcel->writeUint32(width);
    parcel->writeUint32(height);
    parcel->writeUint32(image_quality);
    parcel->writeInt32(
        static_cast<underlying_type<ImageFormat>::type>(image_format));
  }

  ImageParamInternal& FromParcel(const Parcel& parcel) {
    width = parcel.readUint32();
    height = parcel.readUint32();
    image_quality = parcel.readUint32();
    image_format = static_cast<ImageFormat>(parcel.readInt32());
    return *this;
  }
};

struct ImageCaptureConfigInternal : public ImageCaptureConfig {
  ImageCaptureConfigInternal() {}
  ImageCaptureConfigInternal(ImageCaptureConfig& base)
      : ImageCaptureConfig(base) {}
  ImageCaptureConfigInternal(const ImageCaptureConfig& base)
      : ImageCaptureConfig(const_cast<ImageCaptureConfig&>(base)) {}

  void ToParcel(Parcel* parcel) const {
    parcel->writeUint32(sensor_frame_skip_interval);
    parcel->writeInt32(static_cast<int32_t>(with_exif));
    parcel->writeInt32(static_cast<int32_t>(with_camera_meta));
    parcel->writeInt32(static_cast<int32_t>(with_raw));
    parcel->writeInt32(
        static_cast<underlying_type<ImageFormat>::type>(raw_image_format));
    parcel->writeUint32(thumbnail_image_param.size());
    for (const ImageParam& image_param : thumbnail_image_param)
      ImageParamInternal(image_param).ToParcel(parcel);
  }

  ImageCaptureConfigInternal& FromParcel(const Parcel& parcel) {
    sensor_frame_skip_interval = parcel.readUint32();
    with_exif = static_cast<bool>(parcel.readInt32());
    with_camera_meta = static_cast<bool>(parcel.readInt32());
    with_raw = static_cast<bool>(parcel.readInt32());
    raw_image_format = static_cast<ImageFormat>(parcel.readInt32());
    size_t number_of_elements = parcel.readUint32();
    for (size_t index = 0; index < number_of_elements; ++index)
      thumbnail_image_param.push_back(ImageParamInternal().FromParcel(parcel));
    return *this;
  }
};

}; // recorder
}; // qmmf
