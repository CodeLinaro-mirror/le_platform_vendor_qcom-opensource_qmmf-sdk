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
#include <vector>

#include "qmmf-sdk/qmmf_codec.h"

namespace qmmf {

namespace recorder {

#define MAX_STRING_LENGTH 128
#define MAX_IN_DEVICES    4

typedef int32_t status_t;

enum class EventType {
  kError,
  kStateChanged
};

typedef std::function<void(EventType event_type, void *event_data,
                         size_t event_data_size)> EventCb;
// Recorder Class specific callbacks

// There are separate callbacks for Sessions and Tracks
// Recorder callback will be called to notify non track and non
// session specific event notifications.
// Only error event types are expected as of now
typedef struct RecorderCb {
  EventCb event_cb;
} RecorderCb;

// Session specific callbacks

// Session cb will be mostly used to return state changes - to indicate
// start, stop, pause state transition completions
typedef struct SessionCb {
  EventCb event_cb;
} SessionCb;

typedef struct TrackBuffer {
  void    *data;
  size_t   size;
  int64_t  timestamp;
  uint32_t flag;
  uint32_t buf_id;
  size_t   capacity;
} TrackBuffer;

enum class TrackMetaParamType {
  kNone,
  kCamBufMetaData,
  kVideoCrop,
  kMultipleFrame
};

enum class TrackBufferFlags {
  kFlagEOS         = (1 << 1),
  kFlagCodecConfig = (1 << 2)
};

// Track specfic callbacks

// Both data and event callbacks should be set by the client
// event_cb is called to notify track specific errors and data_cb
// to notify data.
// TrackMetaParam in data cb is an optional parameter. This parameter is expected
// to be used in case multiple frames are passed in the same buffer and
// in that case meta_param can describe the respective frame offsets
// and timestamps in the buffer
// When the data cb is called by recorder, the buffer ownership is transfered to
// client. To return the buffer back to recoder, clients should call ReturnTrackBuffer
// API. However, clients needs to ensure that buffers returned within the frame rate
// of track - else recording pipeline will stall.
// Track event_cb returns async error events and data_cb returns periodic
// data
typedef struct TrackCb {
  std::function<void(uint32_t track_id, std::vector<TrackBuffer> buffers,
      void *meta_param, TrackMetaParamType meta_type, size_t meta_size)>
      data_cb;
  std::function<void(uint32_t track_id, EventType event_type, void *event_data
      , size_t event_data_size)> event_cb;
} TrackCb;

// Capture Image callback.
typedef std::function<void(void *buffer, uint32_t buffer_size)> CaptureImageCb;

// @brief Create time parameters for audio track

// Audio output device is used for routing audio to output
// to external devices say through HDMI. In all other usecases
// out_device will be set to AUDIO_DEVICE_NONE
//
typedef struct AudioTrackCreateParam {
  //TODO: define AudioInputDevice??
  int32_t          in_device[MAX_IN_DEVICES];
  int32_t          num_in_devices;
  uint32_t         sample_rate;
  uint32_t         channels;
  uint32_t         bit_depth;
  AudioFormat      format_type;
  AudioCodecParams codec_param;
  //TODO: define AudioOutDevice
  int32_t          out_device;
  uint32_t         flags;
} AudioTrackCreateParam;

// create time parameters for a video track
// For 360 degree capture, camera_id vector should contain the id of
// multiple cameras involved in 360 capture
typedef struct VideoTrackCreateParam {
  uint32_t           camera_ids[MAX_IN_DEVICES];
  uint32_t           num_cameras;
  uint32_t           width;
  uint32_t           height;
  uint32_t           frame_rate;
  VideoFormat        format_type;
  VideoCodecParams   codec_param;
  //TODO: define VideoOutDevice
  uint32_t            out_device;
} VideoTrackCreateParam;

// Parameters passed to StartCamera API
// When the zsl mode is set to true during StartCamera, recorder
// would start capturing images of resolution max_snapshot_width
// and max_snapshot_height from camera at the frame_rate specified.
// In non-zsl mode, snapshot resolution and frame rate parameter is
// ignored.
// flags provide a mechanism to provide a custom initialization
// parameter to camera
typedef struct CameraStartParam {
  bool     zsl_mode;
  uint32_t zsl_queue_depth;
  uint32_t zsl_width;
  uint32_t zsl_height;
  uint32_t frame_rate;
  uint32_t flags;
} CameraStartParam;

typedef struct ThumbnailParam {
  uint32_t width;
  uint32_t height;
  uint32_t quality;
} ThumbnailParam;

// For thumbnail images only kJPEG is supported
// For YUV and Bayer formats, quality is ignored
typedef struct ImageInfo {
  uint32_t    width;
  uint32_t    height;
  uint32_t    image_quality;
  ImageFormat image_format;
} ImageInfo;

// @brief Detail configuration for ImageCapture

// num_images: Total number of images to capture. If this value is set to -1,
//      take image continues until CancelImageCapture is called
// main_image_param: width, height, quality, format info for main image
// thumbnail_image_param : This is a vector to represent thumbnail info in a image
//       Do note that number of thumbnails supported would be limited based on platform
// sensor_frame_skip_interval: When multiple images needs to be captured clients can set the
//      sample_rate of capture through this parameter. If this value is set to 1, every
//      alternate image is captured, if 2, every 3rd image is captured and so on
// with_exif: Applies only when image codec is set to JPEG. If set to true, EXIF along with
//      with thumbnails are embedded with the image. Else thumbnails and camera meta information
//      is send separately through metadata
// with_camera_meta: Applies only with with_exif is set to false. In this case, camera metadata is
//      send separately
// with_raw: Enables clients to take a RAW image along with JPEG. This can
//       be set to true only when ImageFormat is NOT RAW
// raw_image_type: Could be either of RDI RAW or IDEAL Raw
struct ImageParam {
  uint32_t               num_images;
  ImageInfo              main_image_param;
  uint32_t               sensor_frame_skip_interval;
  bool                   with_exif;
  bool                   with_camera_meta;
  bool                   with_raw;
  ImageFormat            raw_image_format;
  std::vector<ImageInfo> thumbnail_image_param;
};

// tuple describing structure of image data callback. First parameter
// is the type of imagemetadata, second size of metadata buffer and
// third pointer to buffer
typedef std::tuple <ImageMetaDataType, size_t, void *> ImageCbDataInfo;

// Callback for ImageCapture API. Each vector element is a tuple representing
// type of data (main image, thumbnail image etc), size of the buffer and a
// buffer pointer
typedef std::function < void(uint32_t image_sequence_count,
                           std::vector <ImageCbDataInfo> image_data
                          ) > ImageCaptureCb;

// @brief Camera sensor/ISP specific parameters
enum class CameraParamType {
  kSaturation,
  kContrast,
  kBrightness,
  kAEMode,
  kAntiBandingMode,
  kAEExposureCompensation,
  kExposureTime,
  kAELock,
  kAETargetFpsRange,
  kISOMode,
  kAWBMode,
  kAWBLock,
  kNoiseReductionMode,
  kControlAERegion,
  kVideoHDRMode,
  kUserCustomMode
};

enum class OverlayType {
  kDateType,
  kUserText,
  kStaticImage,
  kBoundingBox,
  kPrivacyMask
};

enum class OverlayLocationType {
  kTopLEft,
  kTopRight,
  kCenter,
  kBottomLeft,
  kBottomRight,
  kNone
};

enum class OverlayTimeType {
  kHHMMSS_24HR,
  kHHMMSS_AMPM,
  kHHMM_24HR,
  kHHMM_AMPM
};

enum class OverlayDateType {
  kYYYYMMDD,
  kMMDDYYYY
};

typedef struct OverlayDateTimeType {
  OverlayTimeType time_type;
  OverlayDateType date_type;
} OverlayDateAndTimeType;

typedef struct BoundingBox {
  int32_t startX;
  int32_t startY;
  int32_t width;
  int32_t height;
  char    box_name[MAX_STRING_LENGTH];
} BoundingBox;

typedef struct OverlayImageInfo {
  char     image_location[MAX_STRING_LENGTH];
  int32_t  width;
  int32_t  height;
} OverlayImageInfo;

typedef struct OverlayParam {
  OverlayType type;
  OverlayLocationType location;
  uint32_t textColor;
  union {
    OverlayDateTimeType date_time_type;
    char                user_text[MAX_STRING_LENGTH];
    OverlayImageInfo    image_info;
    BoundingBox         bounding_box;
  };
} OverlayParam;

};
}; //namespace qmmf::recorder

