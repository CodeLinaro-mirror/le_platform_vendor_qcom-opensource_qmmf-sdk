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

// The basic types of audio and video devices.
enum class DeviceType {
    kVideoIn,
    kVideoOut,
    kAudioIn,
    kAudioOut,
};

// The specific subtypes of video input devices.
enum class VideoInSubtype {
    kNone = 0,
    kCamera,
    kDefault
};

// The specific subtypes of video output devices.
enum class VideoOutSubtype {
    kNone = 0,
    kLCD,
    kHDMI,
    kDefault
};

// The specific subtypes of audio input devices.
enum class AudioInSubtype {
    kNone = 0,
    kBuiltIn,
    kHeadSet,
    kBlueTooth,
    kHDMI,
    kSPDIF,
    kLine,
    kUSB,
    kDefault
};

// The specific subtypes of audio output devices.
enum class AudioOutSubtype {
    kNone = 0,
    kBuiltIn,
    kHeadPhone,
    kBlueTooth,
    kHDMI,
    kSPDIF,
    kLine,
    kUSB,
    kDefault
};

// The ID string that functions as a handle to a specific device.
typedef std::string DeviceID;

// Describes a particular device.
typedef struct DeviceInfo {
    DeviceType type;
    union {
        VideoInSubtype video_in;
        VideoOutSubtype video_out;
        AudioInSubtype audio_in;
        AudioOutSubtype audio_out;
    } subtype;
    DeviceID id;
} DeviceInfo;

// Represented by width then height
typedef std::tuple<uint32_t, uint32_t> Dimension;

typedef struct VideoCaps {
    std::vector<Dimension> dimensions;
    std::vector<uint32_t> frame_rates;
    std::vector<ImageFormat> format;
} VideoCaps;

typedef struct AudioCaps {
    std::vector<AudioFormat> formats;
    std::vector<uint32_t> sample_rates;
    std::vector<uint32_t> channels;
    std::vector<uint32_t> bit_depths;
} AudioCaps;

typedef struct DeviceCaps {
    DeviceType type;
    union {
        VideoCaps video_in;
        VideoCaps video_out;
        AudioCaps audio_in;
        AudioCaps audio_out;
    } caps;
} DeviceCaps;

} //namespace qmmf
