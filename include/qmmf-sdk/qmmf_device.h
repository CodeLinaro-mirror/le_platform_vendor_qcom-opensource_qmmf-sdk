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
#include <utility>
#include <vector>

#include <binder/Parcel.h>

#include "include/qmmf-sdk/qmmf_codec.h"

namespace qmmf {

using ::android::Parcel;
using ::std::get;
using ::std::pair;
using ::std::string;
using ::std::stringstream;
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
typedef int DeviceID;

struct DeviceIdList {
  vector<DeviceID> ids;

  string ToString() const {
    stringstream stream;
    for (DeviceID id : ids)
      stream << id << ", ";
    stream << "SIZE[" << ids.size() << "]";
    return stream.str();
  }

  void ToParcel(Parcel* parcel) const {
    parcel->writeUint32(static_cast<uint32_t>(ids.size()));
    for (DeviceID id : ids)
      parcel->writeInt32(static_cast<int32_t>(id));
  }

  void FromParcel(const Parcel& parcel) {
    size_t number_of_elements = static_cast<size_t>(parcel.readUint32());
    for (size_t index = 0; index < number_of_elements; ++index)
      ids.push_back(static_cast<DeviceID>(parcel.readInt32()));
  }
};

union DeviceSubType {
  VideoInSubtype video_in;
  VideoOutSubtype video_out;
  AudioInSubtype audio_in;
  AudioOutSubtype audio_out;

  string ToString(DeviceType key) const {
    stringstream stream;
    switch (key) {
      case DeviceType::kVideoIn:
        stream << "video_in[" << static_cast<int>(video_in) << "]";
        break;
      case DeviceType::kVideoOut:
        stream << "video_out[" << static_cast<int>(video_out) << "]";
        break;
      case DeviceType::kAudioIn:
        stream << "audio_in[" << static_cast<int>(audio_in) << "]";
        break;
      case DeviceType::kAudioOut:
        stream << "audio_out[" << static_cast<int>(audio_out) << "]";
        break;
    }
    return stream.str();
  }

  void ToParcel(DeviceType key, Parcel* parcel) const {
    parcel->writeInt32(static_cast<int32_t>(key));
    switch (key) {
      case DeviceType::kVideoIn:
        parcel->writeInt32(static_cast<int32_t>(video_in));
        break;
      case DeviceType::kVideoOut:
        parcel->writeInt32(static_cast<int32_t>(video_out));
        break;
      case DeviceType::kAudioIn:
        parcel->writeInt32(static_cast<int32_t>(audio_in));
        break;
      case DeviceType::kAudioOut:
        parcel->writeInt32(static_cast<int32_t>(audio_out));
        break;
    }
  }

  void FromParcel(const Parcel& parcel) {
    DeviceType key = static_cast<DeviceType>(parcel.readInt32());
    switch (key) {
      case DeviceType::kVideoIn:
        video_in = static_cast<VideoInSubtype>(parcel.readInt32());
        break;
      case DeviceType::kVideoOut:
        video_out = static_cast<VideoOutSubtype>(parcel.readInt32());
        break;
      case DeviceType::kAudioIn:
        audio_in = static_cast<AudioInSubtype>(parcel.readInt32());
        break;
      case DeviceType::kAudioOut:
        audio_out = static_cast<AudioOutSubtype>(parcel.readInt32());
        break;
    }
  }
};

/* describes a particular device */
struct DeviceInfo {
  DeviceType type;
  DeviceSubType subtype;
  DeviceID id;

  string ToString() const {
    stringstream stream;
    stream << "type[" << static_cast<int>(type) << "] ";
    stream << "subtype[" << subtype.ToString(type) << "] ";
    stream << "id[" << id << "]";
    return stream.str();
  }

  void ToParcel(Parcel* parcel) const {
    parcel->writeInt32(static_cast<int32_t>(type));
    subtype.ToParcel(type, parcel);
    parcel->writeInt32(static_cast<int32_t>(id));
  }

  void FromParcel(const Parcel& parcel) {
    type = static_cast<DeviceType>(parcel.readInt32());
    subtype.FromParcel(parcel);
    id = static_cast<DeviceID>(parcel.readInt32());
  }
};

struct Dimension {
  /* represented by width then height */
  pair<int, int> dimension;

  string ToString() const {
    stringstream stream;
    stream << get<0>(dimension) << ", ";
    stream << get<1>(dimension);
    return stream.str();
  }

  void ToParcel(Parcel* parcel) const {
    parcel->writeInt32(static_cast<int32_t>(get<0>(dimension)));
    parcel->writeInt32(static_cast<int32_t>(get<1>(dimension)));
  }

  void FromParcel(const Parcel& parcel) {
    int width = static_cast<int>(parcel.readInt32());
    int height = static_cast<int>(parcel.readInt32());
    dimension = pair<int, int>(width, height);
  }
};

struct VideoCaps {
  vector<Dimension> dimensions;
  vector<int> frame_rates;
  vector<ImageFormat> formats;

  string ToString() const {
    stringstream stream;
    stream << "dimensions[";
    for (const Dimension& dimension : dimensions)
      stream << dimension.ToString() << ", ";
    stream << "SIZE[" << dimensions.size() << "]], ";
    stream << "frame_rates[";
    for (int frame_rate : frame_rates)
      stream << frame_rate << ", ";
    stream << "SIZE[" << frame_rates.size() << "]], ";
    stream << "formats[";
    for (ImageFormat format : formats)
      stream << static_cast<int>(format) << ", ";
    stream << "SIZE[" << formats.size() << "]]";
    return stream.str();
  }

  void ToParcel(Parcel* parcel) const {
    parcel->writeUint32(static_cast<uint32_t>(dimensions.size()));
    for (const Dimension& dimension : dimensions)
      dimension.ToParcel(parcel);
    parcel->writeUint32(static_cast<uint32_t>(frame_rates.size()));
    for (int frame_rate : frame_rates)
      parcel->writeInt32(static_cast<int32_t>(frame_rate));
    parcel->writeUint32(static_cast<uint32_t>(formats.size()));
    for (ImageFormat format : formats)
      parcel->writeInt32(static_cast<int32_t>(format));
  }

  void FromParcel(const Parcel& parcel) {
    size_t number_of_elements = static_cast<size_t>(parcel.readUint32());
    for (size_t index = 0; index < number_of_elements; ++index) {
      Dimension dimension;
      dimension.FromParcel(parcel);
      dimensions.push_back(dimension);
    }
    number_of_elements = static_cast<size_t>(parcel.readUint32());
    for (size_t index = 0; index < number_of_elements; ++index)
      frame_rates.push_back(static_cast<int>(parcel.readInt32()));
    number_of_elements = static_cast<size_t>(parcel.readUint32());
    for (size_t index = 0; index < number_of_elements; ++index) {
      ImageFormat format;
      formats.push_back(static_cast<ImageFormat>(parcel.readInt32()));
    }
  }
};

struct AudioCaps {
  vector<AudioFormat> formats;
  vector<int> sample_rates;
  vector<int> channels;
  vector<int> bit_depths;

  string ToString() const {
    stringstream stream;
    stream << "formats[";
    for (AudioFormat format : formats)
      stream << static_cast<int>(format) << ", ";
    stream << "SIZE[" << formats.size() << "]]";
    stream << "sample_rates[";
    for (int sample_rate : sample_rates)
      stream << sample_rate << ", ";
    stream << "SIZE[" << sample_rates.size() << "]], ";
    stream << "channels[";
    for (int channel : channels)
      stream << channel << ", ";
    stream << "SIZE[" << channels.size() << "]], ";
    stream << "bit_depths[";
    for (int bit_depth : bit_depths)
      stream << bit_depth << ", ";
    stream << "SIZE[" << bit_depths.size() << "]], ";
    return stream.str();
  }

  void ToParcel(Parcel* parcel) const {
    parcel->writeUint32(static_cast<uint32_t>(formats.size()));
    for (AudioFormat format : formats)
      parcel->writeInt32(static_cast<int32_t>(format));
    parcel->writeUint32(static_cast<uint32_t>(sample_rates.size()));
    for (int sample_rate : sample_rates)
      parcel->writeInt32(static_cast<int32_t>(sample_rate));
    parcel->writeUint32(static_cast<uint32_t>(channels.size()));
    for (int channel : channels)
      parcel->writeInt32(static_cast<int32_t>(channel));
    parcel->writeUint32(static_cast<uint32_t>(bit_depths.size()));
    for (int bit_depth : bit_depths)
      parcel->writeInt32(static_cast<int32_t>(bit_depth));
  }

  void FromParcel(const Parcel& parcel) {
    size_t number_of_elements = static_cast<size_t>(parcel.readUint32());
    for (size_t index = 0; index < number_of_elements; ++index)
      formats.push_back(static_cast<AudioFormat>(parcel.readInt32()));
    number_of_elements = static_cast<size_t>(parcel.readUint32());
    for (size_t index = 0; index < number_of_elements; ++index)
      sample_rates.push_back(static_cast<int>(parcel.readInt32()));
    number_of_elements = static_cast<size_t>(parcel.readUint32());
    for (size_t index = 0; index < number_of_elements; ++index)
      channels.push_back(static_cast<int>(parcel.readInt32()));
    number_of_elements = static_cast<size_t>(parcel.readUint32());
    for (size_t index = 0; index < number_of_elements; ++index)
      bit_depths.push_back(static_cast<int>(parcel.readInt32()));
  }
};

union DeviceSpecificCaps{
  VideoCaps video;
  AudioCaps audio;

  string ToString(DeviceType key) const {
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
    }
    return stream.str();
  }

  void ToParcel(DeviceType key, Parcel* parcel) const {
    parcel->writeInt32(static_cast<int32_t>(key));
    switch (key) {
      case DeviceType::kVideoIn:
      case DeviceType::kVideoOut:
        video.ToParcel(parcel);
        break;
      case DeviceType::kAudioIn:
      case DeviceType::kAudioOut:
        audio.ToParcel(parcel);
        break;
    }
  }

  void FromParcel(const Parcel& parcel) {
    DeviceType key = static_cast<DeviceType>(parcel.readInt32());
    switch (key) {
      case DeviceType::kVideoIn:
      case DeviceType::kVideoOut:
        video.FromParcel(parcel);
        break;
      case DeviceType::kAudioIn:
      case DeviceType::kAudioOut:
        audio.FromParcel(parcel);
        break;
    }
  }
};

struct DeviceCaps {
  DeviceType type;
  DeviceSpecificCaps caps;

  string ToString() const {
    stringstream stream;
    stream << "type[" << static_cast<int>(type) << "] ";
    stream << "caps[" << caps.ToString(type) << "] ";
    return stream.str();
  }

  void ToParcel(Parcel* parcel) const {
    parcel->writeInt32(static_cast<int32_t>(type));
    caps.ToParcel(type, parcel);
  }

  void FromParcel(const Parcel& parcel) {
    type = static_cast<DeviceType>(parcel.readInt32());
    caps.FromParcel(parcel);
  }
};

}; /* namespace qmmf */
