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

#include <iomanip>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "include/qmmf-sdk/qmmf_codec.h"

namespace qmmf {

using ::std::get;
using ::std::pair;
using ::std::string;
using ::std::stringstream;
using ::std::underlying_type;
using ::std::vector;

/* basic types of audio and video devices */
enum class DeviceType {
  kVideoIn,
  kVideoOut,
  kAudioIn,
  kAudioOut,
};

/* specific subtypes of video input devices */
enum class VideoInSubtype {
  kNone = 0,
  kCamera,
  kDefault /* always last */
};

/* specific subtypes of video output devices */
enum class VideoOutSubtype {
  kNone = 0,
  kLCD,
  kHDMI,
  kDefault /* always last */
};

/* specific subtypes of audio input devices */
enum class AudioInSubtype {
  kNone = 0,
  kBuiltIn,
  kHeadSet,
  kBlueTooth,
  kHDMI,
  kSPDIF,
  kLine,
  kUSB,
  kDefault /* always last */
};

/* specific subtypes of audio output devices */
enum class AudioOutSubtype {
  kNone = 0,
  kBuiltIn,
  kHeadPhone,
  kBlueTooth,
  kHDMI,
  kSPDIF,
  kLine,
  kUSB,
  kDefault /* always last */
};

/* handle to a specific device */
typedef int32_t DeviceId;

union DeviceSubType {
  VideoInSubtype video_in;
  VideoOutSubtype video_out;
  AudioInSubtype audio_in;
  AudioOutSubtype audio_out;

  string ToString(const DeviceType key) const {
    stringstream stream;
    switch (key) {
      case DeviceType::kVideoIn:
        stream << "video_in["
               << static_cast<underlying_type<VideoInSubtype>::type>(video_in)
               << "]";
        break;
      case DeviceType::kVideoOut:
        stream << "video_out["
               << static_cast<underlying_type<VideoOutSubtype>::type>(video_out)
               << "]";
        break;
      case DeviceType::kAudioIn:
        stream << "audio_in["
               << static_cast<underlying_type<AudioInSubtype>::type>(audio_in)
               << "]";
        break;
      case DeviceType::kAudioOut:
        stream << "audio_out["
               << static_cast<underlying_type<AudioOutSubtype>::type>(audio_out)
               << "]";
        break;
      default:
        stream << "Invalid Key["
               << static_cast<underlying_type<DeviceType>::type>(key) << "]";
        break;
    }
    return stream.str();
  }
};

/* describes a particular device */
struct DeviceInfo {
  DeviceType type;
  DeviceSubType subtype;
  DeviceId id;

  string ToString() const {
    stringstream stream;
    stream << "type[" << static_cast<underlying_type<DeviceType>::type>(type)
           << "] ";
    stream << "subtype[" << subtype.ToString(type) << "] ";
    stream << "id[" << id << "]";
    return stream.str();
  }
};

struct Dimension {
  /* represented by width then height */
  pair<int32_t, int32_t> dimension;

  string ToString() const {
    stringstream stream;
    stream << get<0>(dimension) << ", ";
    stream << get<1>(dimension);
    return stream.str();
  }
};

struct VideoCaps {
  vector<Dimension> dimensions;
  vector<int32_t> frame_rates;
  vector<ImageFormat> formats;

  string ToString() const {
    stringstream stream;
    stream << "dimensions[";
    for (const Dimension& dimension : dimensions)
      stream << dimension.ToString() << ", ";
    stream << "SIZE[" << dimensions.size() << "]], ";
    stream << "frame_rates[";
    for (int32_t frame_rate : frame_rates)
      stream << frame_rate << ", ";
    stream << "SIZE[" << frame_rates.size() << "]], ";
    stream << "formats[";
    for (ImageFormat format : formats)
      stream << static_cast<underlying_type<ImageFormat>::type>(format) << ", ";
    stream << "SIZE[" << formats.size() << "]]";
    return stream.str();
  }
};

struct AudioCaps {
  vector<AudioFormat> formats;
  vector<int32_t> sample_rates;
  vector<int32_t> channels;
  vector<int32_t> bit_depths;

  string ToString() const {
    stringstream stream;
    stream << "formats[";
    for (AudioFormat format : formats)
      stream << static_cast<underlying_type<AudioFormat>::type>(format) << ", ";
    stream << "SIZE[" << formats.size() << "]]";
    stream << "sample_rates[";
    for (int32_t sample_rate : sample_rates)
      stream << sample_rate << ", ";
    stream << "SIZE[" << sample_rates.size() << "]], ";
    stream << "channels[";
    for (int32_t channel : channels)
      stream << channel << ", ";
    stream << "SIZE[" << channels.size() << "]], ";
    stream << "bit_depths[";
    for (int32_t bit_depth : bit_depths)
      stream << bit_depth << ", ";
    stream << "SIZE[" << bit_depths.size() << "]], ";
    return stream.str();
  }
};

union DeviceSpecificCaps{
  VideoCaps video;
  AudioCaps audio;

  string ToString(const DeviceType key) const {
    stringstream stream;
    switch (key) {
      case DeviceType::kVideoIn:
      case DeviceType::kVideoOut:
        stream << "video[" << video.ToString() << "]";
        break;
      case DeviceType::kAudioIn:
      case DeviceType::kAudioOut:
        stream << "audio[" << audio.ToString() << "]";
        break;
      default:
        stream << "Invalid Key["
               << static_cast<underlying_type<DeviceType>::type>(key) << "]";
        break;
    }
    return stream.str();
  }

  /* needed for unions with non-trivial members */
  DeviceSpecificCaps() : video(VideoCaps()) {}
  DeviceSpecificCaps(DeviceSpecificCaps& caps) : video(caps.video) {}
  DeviceSpecificCaps(const DeviceSpecificCaps& caps) : video(caps.video) {}
  DeviceSpecificCaps(VideoCaps _video) : video(_video) {}
  DeviceSpecificCaps(AudioCaps _audio) : audio(_audio) {}
  ~DeviceSpecificCaps() {}
};

struct DeviceCaps {
  DeviceType type;
  DeviceSpecificCaps caps;

  string ToString() const {
    stringstream stream;
    stream << "type[" << static_cast<underlying_type<DeviceType>::type>(type)
           << "] ";
    stream << "caps[" << caps.ToString(type) << "]";
    return stream.str();
  }
};

}; /* namespace qmmf */
