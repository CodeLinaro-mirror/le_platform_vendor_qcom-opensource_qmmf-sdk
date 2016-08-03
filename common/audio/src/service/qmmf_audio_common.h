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
#include <functional>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#include <binder/Parcel.h>

namespace qmmf {
namespace common {
namespace audio {

using ::android::Parcel;
using ::std::function;
using ::std::string;
using ::std::stringstream;
using ::std::vector;

enum class AudioState {
  kNew, /* instantiated, unconnected and unconfigured */
  kConnect, /* connected and unconfigured */
  kIdle, /* configured and stopped */
  kRunning, /* streaming */
  kPaused, /* stopped with data retention */
};

/* handle to a specific audio client/service connection */
typedef int AudioHandle;

struct AudioHandleList {
  vector<AudioHandle> handles;

  string ToString() const {
    stringstream stream;
    for (AudioHandle handle : handles)
      stream << handle << ", ";
    stream << "SIZE[" << handles.size() << "]";
    return stream.str();
  }

  void ToParcel(Parcel* parcel) const {
    parcel->writeUint32(static_cast<uint32_t>(handles.size()));
    for (AudioHandle handle : handles)
      parcel->writeInt32(static_cast<int32_t>(handle));
  }

  void FromParcel(const Parcel& parcel) {
    size_t number_of_elements = static_cast<size_t>(parcel.readUint32());
    for (auto index = 0; index < number_of_elements; ++index) {
      AudioHandle handle;
      handles.push_back(static_cast<AudioHandle>(parcel.readInt32()));
    }
  }
};

typedef function<void(AudioHandle audio_handle, int error)> AudioErrorHandler;
typedef function<void(AudioHandle audio_handle,
                      const AudioBuffer& buffer)> AudioReadCompleteHandler;
typedef function<void(AudioHandle audio_handle,
                      const AudioBuffer& buffer)> AudioWriteCompleteHandler;

}; /* namespace audio */
}; /* namespace common */
}; /* namespace qmmf */
