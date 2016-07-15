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

#include <utils/List.h>
#include <utils/Mutex.h>

#include "qmmf_recorder_params.h"
#include "qmmf_recorder_service_intf.h"
#include "qmmf_camera3_device_client.h"

/*
* Define LOG_LEVEL1 & 2 enable more debug logs.
*/
//#define LOG_LEVEL1
//#define LOG_LEVEL2

// QMMF_INFO, ERROR and WARN logs are enabled by default.
#define QMMF_INFO(fmt, args...)  ALOGD(fmt, ##args)
#define QMMF_ERROR(fmt, args...) ALOGE(fmt, ##args)
#define QMMF_WARN(fmt, args...)  ALOGW(fmt, ##args)

#ifdef LOG_LEVEL1
#define QMMF_LEVEL1(fmt, args...)  ALOGD(fmt, ##args)
#else
#define QMMF_LEVEL1(...) ((void)0)
#endif

#ifdef LOG_LEVEL2
#define QMMF_LEVEL2(fmt, args...)  ALOGD(fmt, ##args)
#else
#define QMMF_LEVEL2(...) ((void)0)
#endif

#define CAMERA_HAL_MODULE_PATH "/usr/lib/hw/camera.msm8953.so"
#define GRALLOC_MODULE_PATH    "/usr/lib/hw/gralloc.msm8953.so"

#define FRAME_DUMP_PATH        "/usr/data"

// Enable ENABLE_FRAME_DUMP to dump YUV frame at TrackSource level. it will
// Start dumping every 100th frame for all active tracks, and file name
// Would be track_(track_id)_(timestamp).yuv
//#define ENABLE_FRAME_DUMP

// Enable DEBUG_TRACK_FPS to print fps of all active video tracks.
#define DEBUG_TRACK_FPS
#define FPS_TIME_INTERVAL 3000000
//#define NO_FRAME_PROCESS

namespace qmmf {

namespace recorder {

using namespace cameraadaptor;

enum class TrackType {
    kVideo,
    kAudio
};

enum class CameraStreamType {
  kPreview,
  kVideo,
};

enum class CameraStreamFormat {
  kNV12,
  kNV21,
};

typedef struct CameraStreamDim {
    uint32_t width;
    uint32_t height;
} CameraStreamDim;

typedef std::function<void(uint32_t track_id, std::vector<BnTrackBuffer>
    buffers, void *meta_param, TrackMetaParamType meta_type, size_t meta_size)>
    buffer_callback;

typedef struct VideoTrackParams {
  uint32_t               track_id;
  std::vector<uint32_t>  camera_ids;
  uint32_t               width;
  uint32_t               height;
  uint32_t               frame_rate;
  VideoCodecType         codec_type;
  VideoCodecParam        codec_param;
  CameraStreamType       camera_stream_type;
  buffer_callback        data_cb;
} VideoTrackParams;

typedef struct CameraStreamParam {
  CameraStreamDim    cam_stream_dim;
  CameraStreamFormat cam_stream_format;
  CameraStreamType   cam_stream_type;
  uint32_t           frame_rate;
  uint32_t           id;
} CameraStreamParam;

typedef struct Buffer {
  CameraStreamParam  stream_param;
  StreamBuffer       stream_buffer;
} Buffer;

extern "C" void DebugCameraStartParams (const char* func,
                                        CameraStartParam* params);
extern "C" void DebugVideoTrackCreateParam (const char* _func_,
                                            VideoTrackCreateParam* params);
extern "C" void DebugVideoTrackParams (const char* _func_,
                                       VideoTrackParams* params);

// Thread safe Queue
template <class T>
class TSQueue
{
 public:
  typedef typename List<T>::iterator iterator;

  iterator begin() {
    Mutex::Autolock autoLock(lock_);
    return queue_.begin();
  }

  void PushBack(const T& item) {
    Mutex::Autolock autoLock(lock_);
    queue_.push_back(item);
  }

  int32_t Size() {
    Mutex::Autolock autoLock(lock_);
    return queue_.size();
  }

  bool Empty() {
   Mutex::Autolock autoLock(lock_);
   return queue_.empty();
  }

  iterator End() {
    Mutex::Autolock autoLock(lock_);
    return queue_.end();
  }

  void Erase(iterator it) {
    Mutex::Autolock autoLock(lock_);
    queue_.erase(it);
  }

  void Clear() {
    Mutex::Autolock autoLock(lock_);
    queue_.clear();
  }

 private:
  List<T> queue_;
  Mutex lock_;
};

// Thread safe KeyedVector
template <class T1, class T2>
class TSKeyedVector
{
 public:

  void Add(Buffer& buffer) {
      Mutex::Autolock autoLock(lock_);
      map_.add(buffer.stream_buffer.handle, 1);
  }

  uint32_t ValueFor(Buffer& buffer) {
      Mutex::Autolock autoLock(lock_);
      return map_.valueFor(buffer.stream_buffer.handle);
  }

  void RemoveItem(Buffer& buffer) {
      Mutex::Autolock autoLock(lock_);
      map_.removeItem(buffer.stream_buffer.handle);
  }

  int32_t Size() {
      Mutex::Autolock autoLock(lock_);
      return map_.size();
  }

  bool IsEmpty() {
       Mutex::Autolock autoLock(lock_);
       return map_.isEmpty();
  }

  void ReplaceValueFor(Buffer& buffer, uint32_t value) {
      Mutex::Autolock autoLock(lock_);
      map_.replaceValueFor(buffer.stream_buffer.handle, value);
  }

  void Clear() {
      Mutex::Autolock autoLock(lock_);
      map_.clear();
  }

 private:
  DefaultKeyedVector<T1, T2> map_;
  Mutex lock_;
};

}; //namespace recorder.

}; //namespace qmmf.
