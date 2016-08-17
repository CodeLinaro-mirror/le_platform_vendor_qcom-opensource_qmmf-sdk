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

//
// Contains the definitions of enums, structs, etc. used by the audio API
//

#pragma once

#include <cstdint>
#include <functional>
#include <iomanip>
#include <sstream>
#include <string>
#include <type_traits>

#include <binder/Parcel.h>

#include "common/qmmf_codec_internal.h"
#include "include/qmmf-sdk/qmmf_codec.h"
#include "include/qmmf-sdk/qmmf_device.h"

namespace qmmf {
namespace common {
namespace audio {

using ::android::Parcel;
using ::std::boolalpha;
using ::std::function;
using ::std::noboolalpha;
using ::std::setbase;
using ::std::string;
using ::std::stringstream;
using ::std::underlying_type;

struct AudioBuffer {
  void* data;
  int32_t ion_fd;
  int32_t buffer_id;
  int32_t capacity;
  int32_t size;
  int64_t timestamp;
  uint32_t flags;

  string ToString() const {
    stringstream stream;
    stream << "data[" << data << "] ";
    stream << "ion_fd[" << ion_fd << "] ";
    stream << "buffer_id[" << buffer_id << "] ";
    stream << "capacity[" << capacity << "] ";
    stream << "size[" << size << "] ";
    stream << "timestamp[" << timestamp << "] ";
    stream << "flags[" << setbase(16) << flags << setbase(10) << "]";
    return stream.str();
  }

  void ToParcel(Parcel* parcel, bool writeFileDescriptor) const {
    if (ion_fd == -1)
      parcel->writeInt32(reinterpret_cast<intptr_t>(data));
    else
      parcel->writeInt32(reinterpret_cast<intptr_t>(nullptr));
    if (writeFileDescriptor && ion_fd != -1)
      parcel->writeFileDescriptor(static_cast<int>(ion_fd));
    else
      parcel->writeInt32(ion_fd);
    parcel->writeInt32(buffer_id);
    parcel->writeInt32(capacity);
    parcel->writeInt32(size);
    parcel->writeInt64(timestamp);
    parcel->writeUint32(flags);
  }

  void FromParcel(const Parcel& parcel, bool readFileDescriptor) {
    data = reinterpret_cast<void *>(parcel.readIntPtr());
    if (readFileDescriptor && data == nullptr)
      ion_fd = static_cast<int32_t>(parcel.readFileDescriptor());
    else
      ion_fd = parcel.readInt32();
    buffer_id = parcel.readInt32();
    capacity = parcel.readInt32();
    size = parcel.readInt32();
    timestamp = parcel.readInt64();
    flags = parcel.readUint32();
  }
};

enum class AudioEventType {
  kError,
  kBuffer,
};

union AudioEventData {
  int32_t error; // kError
  AudioBuffer buffer; // kBuffer

  string ToString(const AudioEventType key) const {
    stringstream stream;
    switch (key) {
      case AudioEventType::kError:
        stream << "error[" << error << "]";
        break;
      case AudioEventType::kBuffer:
        stream << "buffer[" << buffer.ToString() << "]";
        break;
      default:
        stream << "Invalid Key["
               << static_cast<underlying_type<AudioEventType>::type>(key)
               << "]";
        break;
    }
    return stream.str();
  }

  void ToParcel(const AudioEventType key, Parcel* parcel) const {
    parcel->writeInt32(static_cast<underlying_type<AudioEventType>::type>(key));
    switch (key) {
      case AudioEventType::kError:
        parcel->writeInt32(error);
        break;
      case AudioEventType::kBuffer:
        buffer.ToParcel(parcel, false);
        break;
    }
  }

  void FromParcel(const Parcel& parcel) {
    AudioEventType key = static_cast<AudioEventType>(parcel.readInt32());
    switch (key) {
      case AudioEventType::kError:
        error = parcel.readInt32();
        break;
      case AudioEventType::kBuffer:
        buffer.FromParcel(parcel, false);
        break;
    }
  }

  // needed for unions with non-trivial members
  AudioEventData() : error(0) {}
  AudioEventData(int _error) : error(_error) {}
  AudioEventData(const AudioBuffer& _buffer) : buffer(_buffer) {}
  ~AudioEventData() {}
};

typedef function<void(const AudioEventType event_type,
                      const AudioEventData& event_data)> AudioEventHandler;

enum class AudioEndPointType {
  kSource,
  kSink,
};

enum class AudioFlagBitPosition {
  kLowLatencyBit = 0,
};

struct AudioMetadata {
  AudioFormat format;
  int32_t num_channels;
  int32_t sample_rate;  // rate in Hz
  int32_t sample_size;  // size in bits
  CodecId codec;
  AudioCodecParams codec_params;
  uint32_t flags;

  string ToString() const {
    stringstream stream;
    stream << "format["
           << static_cast<underlying_type<AudioFormat>::type>(format) << "] ";
    stream << "num_channels[" << num_channels << "] ";
    stream << "sample_rate[" << sample_rate << "] ";
    stream << "sample_size[" << sample_size << "]";
    stream << "codec[" << codec << "]";
    stream << "codec_params[" << codec_params.ToString(format) << "]";
    stream << "flags[" << setbase(16) << flags << setbase(10) << "]";
    return stream.str();
  }

  void ToParcel(Parcel* parcel) const {
    parcel->writeInt32(static_cast<underlying_type<AudioFormat>::type>(format));
    parcel->writeInt32(num_channels);
    parcel->writeInt32(sample_rate);
    parcel->writeInt32(sample_size);
    parcel->writeInt32(static_cast<int32_t>(codec));
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
    parcel->writeUint32(flags);
  }

  void FromParcel(const Parcel& parcel) {
    format = static_cast<AudioFormat>(parcel.readInt32());
    num_channels = parcel.readInt32();
    sample_rate = parcel.readInt32();
    sample_size = parcel.readInt32();
    codec = static_cast<CodecId>(parcel.readInt32());
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
    flags = parcel.readUint32();
  }
};

enum class AudioParamType {
  kVolume,
  kDevice,
  kCustom,
};

struct AudioParamDeviceData {
  bool enable;
  DeviceId id;

  string ToString() const {
    stringstream stream;
    stream << "enable[" << boolalpha << enable << noboolalpha << "] ";
    stream << "id[" << id << "]";
    return stream.str();
  }

  void ToParcel(Parcel* parcel) const {
    parcel->writeInt32(static_cast<int32_t>(enable));
    parcel->writeInt32(static_cast<int32_t>(id));
  }

  void FromParcel(const Parcel& parcel) {
    enable = static_cast<bool>(parcel.readInt32());
    id = static_cast<DeviceId>(parcel.readInt32());
  }
};

struct AudioParamCustomData {
  string key;
  string value;

  string ToString() const {
    stringstream stream;
    stream << "key[" << key << "] ";
    stream << "value[" << value << "]";
    return stream.str();
  }

  void ToParcel(Parcel* parcel) const {
    parcel->writeCString(key.c_str());
    parcel->writeCString(value.c_str());
  }

  void FromParcel(const Parcel& parcel) {
    key = parcel.readCString();
    value = parcel.readCString();
  }
};

union AudioParamData {
  int32_t volume; /* kVolume */
  AudioParamDeviceData device; /* kDevice */
  AudioParamCustomData custom; /* kCustom */

  string ToString(const AudioParamType key) const {
    stringstream stream;
    switch (key) {
      case AudioParamType::kVolume:
        stream << "error[" << volume << "]";
        break;
      case AudioParamType::kDevice:
        stream << "device[" << device.ToString() << "]";
        break;
      case AudioParamType::kCustom:
        stream << "custom[" << custom.ToString() << "]";
        break;
      default:
        stream << "Invalid Key["
               << static_cast<underlying_type<AudioParamType>::type>(key)
               << "]";
        break;
    }
    return stream.str();
  }

  void ToParcel(const AudioParamType key, Parcel* parcel) const {
    parcel->writeInt32(static_cast<underlying_type<AudioParamType>::type>(key));
    switch (key) {
      case AudioParamType::kVolume:
        parcel->writeInt32(volume);
        break;
      case AudioParamType::kDevice:
        device.ToParcel(parcel);
        break;
      case AudioParamType::kCustom:
        custom.ToParcel(parcel);
        break;
    }
  }

  void FromParcel(const Parcel& parcel) {
    AudioParamType key = static_cast<AudioParamType>(parcel.readInt32());
    switch (key) {
      case AudioParamType::kVolume:
        volume = parcel.readInt32();
        break;
      case AudioParamType::kDevice:
        device.FromParcel(parcel);
        break;
      case AudioParamType::kCustom:
        custom.FromParcel(parcel);
        break;
    }
  }

  // needed for unions with non-trivial members
  AudioParamData() : volume(0) {}
  AudioParamData(int _volume) : volume(_volume) {}
  AudioParamData(const AudioParamDeviceData& _device) : device(_device) {}
  AudioParamData(const AudioParamCustomData& _custom) : custom(_custom) {}
  ~AudioParamData() {}
};

}; // namespace audio
}; // namespace common
}; // namespace qmmf
