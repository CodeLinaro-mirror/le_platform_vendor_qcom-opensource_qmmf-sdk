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

/*
 * Contains the definitions of enums, structs, etc. used by the audio API
 */

#pragma once

#include <cstdint>
#include <functional>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#include <binder/Parcel.h>

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
using ::std::vector;

struct AudioBuffer {
  void* data;
  int ion_fd;
  int buffer_id;
  int capacity;
  int size;
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
      parcel->writeFileDescriptor(ion_fd);
    else
      parcel->writeInt32(static_cast<int32_t>(ion_fd));
    parcel->writeInt32(static_cast<int32_t>(buffer_id));
    parcel->writeInt32(static_cast<int32_t>(capacity));
    parcel->writeInt32(static_cast<int32_t>(size));
    parcel->writeInt64(timestamp);
    parcel->writeUint32(flags);
  }

  void FromParcel(const Parcel& parcel, bool readFileDescriptor) {
    data = reinterpret_cast<void *>(parcel.readIntPtr());
    if (readFileDescriptor && data == nullptr)
      ion_fd = parcel.readFileDescriptor();
    else
      ion_fd = static_cast<int>(parcel.readInt32());
    buffer_id = static_cast<int>(parcel.readInt32());
    capacity = static_cast<int>(parcel.readInt32());
    size = static_cast<int>(parcel.readInt32());
    timestamp = parcel.readInt64();
    flags = parcel.readUint32();
  }
};

struct AudioBufferList {
  vector<AudioBuffer> list;

  string ToString() const {
    stringstream stream;
    stream << "list[";
    for (const AudioBuffer& buffer : list)
      stream << buffer.ToString() << ", ";
    stream << "SIZE[" << list.size() << "]]";
    return stream.str();
  }

  void ToParcel(Parcel* parcel, bool writeFileDescriptor) const {
    parcel->writeUint32(static_cast<uint32_t>(list.size()));
    for (const AudioBuffer& buffer : list)
      buffer.ToParcel(parcel, writeFileDescriptor);
  }

  void FromParcel(const Parcel& parcel, bool readFileDescriptor) {
    size_t number_of_elements = static_cast<size_t>(parcel.readUint32());
    for (auto index = 0; index < number_of_elements; ++index) {
      AudioBuffer buffer;
      buffer.FromParcel(parcel, readFileDescriptor);
      list.push_back(buffer);
    }
  }
};

enum class AudioEventType {
  kError,
  kBuffer,
};

union AudioEventData {
  int error; /* kError */
  AudioBuffer buffer; /* kBuffer */

  string ToString(AudioEventType key) const {
    stringstream stream;
    switch (key) {
      case AudioEventType::kError:
        stream << "error[" << error << "]";
        break;
      case AudioEventType::kBuffer:
        stream << "buffer[" << buffer.ToString() << "]";
        break;
    }
    return stream.str();
  }

  void ToParcel(AudioEventType key, Parcel* parcel) const {
    parcel->writeInt32(static_cast<int32_t>(key));
    switch (key) {
      case AudioEventType::kError:
        parcel->writeInt32(static_cast<int32_t>(error));
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
        error = static_cast<int>(parcel.readInt32());
        break;
      case AudioEventType::kBuffer:
        buffer.FromParcel(parcel, false);
        break;
    }
  }

  /* needed for unions with non-trivial members */
  AudioEventData() : error(0) {}
  AudioEventData(int _error) : error(_error) {}
  AudioEventData(const AudioBuffer& _buffer) : buffer(_buffer) {}
  ~AudioEventData() {}
};

typedef function<void(AudioEventType event_type,
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
  int num_channels;
  int sample_rate;  /* rate in Hz */
  int sample_size;  /* size in bits */
  CodecID codec;
  AudioFormat codec_type;
  AudioCodecParams codec_params;
  uint32_t flags;

  string ToString() const {
    stringstream stream;
    stream << "format[" << static_cast<int>(format) << "] ";
    stream << "num_channels[" << num_channels << "] ";
    stream << "sample_rate[" << sample_rate << "] ";
    stream << "sample_size[" << sample_size << "]";
    stream << "codec[" << codec << "]";
    stream << "codec_type[" << static_cast<int>(codec_type) << "]";
    stream << "codec_params[" << codec_params.ToString(codec_type) << "]";
    stream << "flags[" << setbase(16) << flags << setbase(10) << "]";
    return stream.str();
  }

  void ToParcel(Parcel* parcel) const {
    parcel->writeInt32(static_cast<int32_t>(format));
    parcel->writeInt32(static_cast<int32_t>(num_channels));
    parcel->writeInt32(static_cast<int32_t>(sample_rate));
    parcel->writeInt32(static_cast<int32_t>(sample_size));
    parcel->writeInt32(static_cast<int32_t>(codec));
    parcel->writeInt32(static_cast<int32_t>(codec_type));
    codec_params.ToParcel(codec_type, parcel);
    parcel->writeUint32(flags);
  }

  void FromParcel(const Parcel& parcel) {
    format = static_cast<AudioFormat>(parcel.readInt32());
    num_channels = static_cast<int>(parcel.readInt32());
    sample_rate = static_cast<int>(parcel.readInt32());
    sample_size = static_cast<int>(parcel.readInt32());
    codec = static_cast<CodecID>(parcel.readInt32());
    codec_type = static_cast<AudioFormat>(parcel.readInt32());
    codec_params.FromParcel(parcel);
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
  DeviceID id;

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
    id = static_cast<DeviceID>(parcel.readInt32());
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
  int volume; /* kVolume */
  AudioParamDeviceData device; /* kDevice */
  AudioParamCustomData custom; /* kCustom */

  string ToString(AudioParamType key) const {
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
    }
    return stream.str();
  }

  void ToParcel(AudioParamType key, Parcel* parcel) const {
    parcel->writeInt32(static_cast<int32_t>(key));
    switch (key) {
      case AudioParamType::kVolume:
        parcel->writeInt32(static_cast<int32_t>(volume));
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
        volume = static_cast<int>(parcel.readInt32());
        break;
      case AudioParamType::kDevice:
        device.FromParcel(parcel);
        break;
      case AudioParamType::kCustom:
        custom.FromParcel(parcel);
        break;
    }
  }

  /* needed for unions with non-trivial members */
  AudioParamData() : volume(0) {}
  AudioParamData(int _volume) : volume(_volume) {}
  AudioParamData(const AudioParamDeviceData& _device) : device(_device) {}
  AudioParamData(const AudioParamCustomData& _custom) : custom(_custom) {}
  ~AudioParamData() {}
};

}; /* namespace audio */
}; /* namespace common */
}; /* namespace qmmf */
