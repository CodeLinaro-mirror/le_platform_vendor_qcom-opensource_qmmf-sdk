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
#include <functional>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#include <binder/Parcel.h>

#include "include/qmmf-sdk/qmmf_recorder_params.h"

namespace qmmf {
namespace recorder {

using ::android::Parcel;
using ::std::setbase;
using ::std::string;
using ::std::stringstream;

struct AudioTrackCreateParamI : public AudioTrackCreateParam {
  AudioTrackCreateParamI(){}
  AudioTrackCreateParamI(AudioTrackCreateParam& base)
      : AudioTrackCreateParam(base) {}
  AudioTrackCreateParamI(const AudioTrackCreateParam& base)
      : AudioTrackCreateParam(const_cast<AudioTrackCreateParam&>(base)) {}

  void ToParcel(Parcel* parcel) const {
    parcel->writeInt32(num_in_devices);
    for (auto index = 0; index < num_in_devices; ++index) {
      parcel->writeInt32(in_device[index]);
    }
    parcel->writeUint32(sample_rate);
    parcel->writeUint32(channels);
    parcel->writeUint32(bit_depth);
    parcel->writeInt32(static_cast<int32_t>(format_type));
    //codec_param.ToParcel(format_type, parcel); FIXME: ToParcel method is removed
    // from CodecParam
    parcel->writeInt32(out_device);
    parcel->writeUint32(flags);
  }

  void FromParcel(const Parcel& parcel) {
    num_in_devices = parcel.readInt32();
    for (auto index = 0; index < num_in_devices; ++index) {
      in_device[index] = parcel.readInt32();
    }
    sample_rate = parcel.readUint32();
    channels = parcel.readUint32();
    bit_depth = parcel.readUint32();
    format_type = static_cast<AudioFormat>(parcel.readInt32());
    //codec_param.FromParcel(parcel); FIXME: FromParcel method is removed
    // from CodecParam
    out_device = parcel.readInt32();
    flags = parcel.readUint32();
  }
};

}; // recorder
}; // qmmf

