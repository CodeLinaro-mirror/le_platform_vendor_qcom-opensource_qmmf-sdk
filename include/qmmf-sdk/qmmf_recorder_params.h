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
#include <iomanip>
#include <functional>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

#include <camera/CameraMetadata.h>

#include "qmmf-sdk/qmmf_buffer.h"
#include "qmmf-sdk/qmmf_codec.h"
#include "qmmf-sdk/qmmf_device.h"

namespace qmmf {

namespace recorder {

#define MAX_IN_DEVICES 4

#define SENSOR_VENDOR_MODE_OFFSET (24)
#define SENSOR_VENDOR_MODE_MASK (0xff)

typedef int32_t status_t;

enum class EventType { kError, kStateChanged };

typedef std::function<void(EventType event_type, void *event_data,
                           size_t event_data_size)> EventCb;

/// \brief Recorder callback is called to notify non track
/// and non session specific event notifications
///
/// Only error event types are expected as of now
struct RecorderCb {
  EventCb event_cb;
};

/// \brief Session cb is used to return state changes i.e. to indicate
/// start, stop, pause state transition completions
struct SessionCb {
  EventCb event_cb;
};

enum class MetaParamType {
  kNone,
  kCamBufMetaData,
  kVideoCrop,
  kMultipleFrame
};

/// \brief Both data and event callbacks should be set by the client.
/// event_cb is called to notify track specific errors and data_cb
/// to notify availability of output data from track to clients
///
/// TrackMetaParam in data cb is an optional parameter. This parameter is
/// expected
/// to be used in case multiple frames are passed in the same buffer and
/// in that case meta_param can describe the respective frame offsets
/// and timestamps in the buffer
/// When the data cb is called by recorder, the buffer ownership is transfered
/// to
/// client. To return the buffer back to recoder, clients should call
/// ReturnTrackBuffer
/// API. However, clients needs to ensure that buffers returned within the frame
/// rate
/// of track - else recording pipeline will stall.
/// Track event_cb returns async error events and data_cb returns periodic
/// data
struct TrackCb {
  std::function<void(uint32_t track_id, ::std::vector<BufferDescriptor> buffers,
                     void *meta_param, MetaParamType meta_type,
                     size_t meta_size)>
      data_cb;
  std::function<void(uint32_t track_id, EventType event_type, void *event_data,
                     size_t event_data_size)>
      event_cb;
};

/// @brief Createtime parameters for audio track
///
/// Audio output device is used for routing audio to output
/// to external devices say through HDMI. In all other usecases
/// out_device will be set to AUDIO_DEVICE_NONE
struct AudioTrackCreateParam {
  ::std::vector<DeviceId> in_devices;
  uint32_t                sample_rate;
  uint32_t                channels;
  uint32_t                bit_depth;
  AudioFormat             format;
  AudioCodecParams        codec_params;
  DeviceId                out_device;
  uint32_t                flags;

  ::std::string ToString() const {
    ::std::stringstream stream;
    stream << "in_devices[";
    for (const DeviceId device : in_devices)
      stream << device << ", ";
    stream << "SIZE[" << in_devices.size() << "]], ";
    stream << "sample_rate[" << sample_rate << "] ";
    stream << "channels[" << channels << "] ";
    stream << "bit_depth[" << bit_depth << "] ";
    stream << "format["
           << static_cast<::std::underlying_type<AudioFormat>::type>(format)
           << "] ";
    stream << "codec_params[" << codec_params.ToString(format) << "] ";
    stream << "out_device[" << out_device << "] ";
    stream << "flags[" << flags << "]";
    return stream.str();
  }
};

/// \brief create time parameters for a video track
/// For 360 degree capture, camera_id vector should contain the id of
/// multiple cameras involved in 360 capture
/// \param low_power_mode: true indicates that track to be output by VFE
///        (Video Front End) camera block without any post-processing.
///        Currently atmost one track can be a low_power_mode track.
/// \TODO: define VideoOutDevice
struct VideoTrackCreateParam {
  uint32_t         camera_id;
  uint32_t         width;
  uint32_t         height;
  uint32_t         frame_rate;
  VideoFormat      format_type;
  VideoCodecParams codec_param;
  uint32_t         out_device;
  bool             low_power_mode;

  ::std::string ToString() const {
    ::std::stringstream stream;
    stream << "camera_id[" << camera_id << "] ";
    stream << "width[" << width << "] ";
    stream << "height[" << height << "] ";
    stream << "frame_rate[" << frame_rate << "] ";
    stream << "format_type["
           << static_cast<::std::underlying_type<VideoFormat>::type>
                         (format_type)
           << "] ";
    stream << "codec_params[" << codec_param.ToString(format_type) << "] ";
    stream << "out_device[" << out_device << "]";
    return stream.str();
  }
};


/// \brief Result callback passed to StartCamera API
///
/// Optional result callback which will get triggered
/// by service once there is at least one started session
/// which includes a video track.
typedef std::function<void(uint32_t camera_id,
                           const android::CameraMetadata &res)> CameraResultCb;

/// \brief Parameters passed to StartCamera API
///
/// When the zsl mode is set to true during StartCamera, recorder
/// would start capturing images of resolution max_snapshot_width
/// and max_snapshot_height from camera at the frame_rate specified.
/// In non-zsl mode, snapshot resolution and frame rate parameter is
/// ignored.
/// flags provide a mechanism to provide a custom initialization
/// parameter to camera
struct CameraStartParam {
  bool     zsl_mode;
  uint32_t zsl_queue_depth;
  uint32_t zsl_width;
  uint32_t zsl_height;
  uint32_t frame_rate;
  uint32_t flags;

  ::std::string ToString() const {
    ::std::stringstream stream;
    stream << "zsl_mode[" << ::std::boolalpha << zsl_mode << ::std::noboolalpha
           << "]";
    stream << "zsl_queue_depth[" << zsl_queue_depth << "] ";
    stream << "zsl_width[" << zsl_width << "] ";
    stream << "zsl_height[" << zsl_height << "] ";
    stream << "frame_rate[" << frame_rate << "] ";
    stream << "flags[" << flags << "]";
    return stream.str();
  };

  void setSensorVendorMode(int32_t sensor_vendor_mode) {
    flags &= ~(SENSOR_VENDOR_MODE_MASK);
    flags |= sensor_vendor_mode << SENSOR_VENDOR_MODE_OFFSET;
  };

  int32_t getSensorVendorMode() const {
    return flags >> SENSOR_VENDOR_MODE_OFFSET;
  }
};

/// \brief For thumbnail images only kJPEG is supported
/// For YUV and Bayer formats, quality is ignored
struct ImageParam {
  uint32_t    width;
  uint32_t    height;
  uint32_t    image_quality;
  ImageFormat image_format;

  ::std::string ToString() const {
    ::std::stringstream stream;
    stream << "width[" << width << "]";
    stream << "height[" << height << "] ";
    stream << "image_quality[" << image_quality << "] ";
    stream << "image_format["
           << static_cast<::std::underlying_type<ImageFormat>::type>
                         (image_format)
           << "]";
    return stream.str();
  }
};

/// \brief Advance configuration for image capture
///
/// \param sensor_frame_skip_interval: When multiple images needs to be captured
///       clients can set the sample_rate of capture through this parameter.
///       If this value is set to 1, every alternate image is captured,
///       if 2, every 3rd image is captured and so on_event_id
/// \param with_exif: Applies only when image codec is set to JPEG. If set to
///       true, EXIF along with with thumbnails are embedded with the image.
///       Else thumbnails and camera meta information is send separately through
///       metadata
/// \param with_camera_meta: Applies only with with_exif is set to false. In
///       this case, camera metadata is send separately
/// \param with_raw: Enables clients to take a RAW image along with JPEG. This
///       can be set to true only when ImageFormat is NOT RAW
///
/// \param raw_image_type: Could be either of RDI RAW or IDEAL Raw
/// \param thumbnail_image_param: Thumbnail image characteristics. This is
/// vector since a single image can contain more than one thumbnail
struct ImageCaptureConfig {
  uint32_t sensor_frame_skip_interval;
  bool with_exif;
  bool with_camera_meta;
  bool with_raw;
  ImageFormat raw_image_format;
  ::std::vector<ImageParam> thumbnail_image_param;

  ::std::string ToString() const {
    ::std::stringstream stream;
    stream << "sensor_frame_skip_interval[" << sensor_frame_skip_interval
           << "] ";
    stream << "with_exif[" << ::std::boolalpha << with_exif
           << ::std::noboolalpha << "] ";
    stream << "with_camera_meta[" << ::std::boolalpha << with_camera_meta
           << ::std::noboolalpha << "] ";
    stream << "with_raw[" << ::std::boolalpha << with_raw << ::std::noboolalpha
           << "] ";
    stream << "raw_image_format["
           << static_cast<::std::underlying_type<ImageFormat>::type>
                         (raw_image_format)
           << "] ";
    stream << "thumbnail_image_param[";
    for (const ImageParam image_param : thumbnail_image_param)
      stream << image_param.ToString() << ", ";
    stream << "SIZE[" << thumbnail_image_param.size() << "]]";
    return stream.str();
  }
};

typedef std::function<void(uint32_t camera_id, uint32_t image_sequence_count,
                           BufferDescriptor buffer, void *meta_param,
                           MetaParamType meta_type, size_t meta_size)>
    ImageCaptureCb;

};
};  // namespace qmmf::recorder
