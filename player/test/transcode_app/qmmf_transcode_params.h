/*
* Copyright (c) 2017, The Linux Foundation. All rights reserved.
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


#include "qmmf-sdk/qmmf_codec.h"
#include "qmmf-sdk/qmmf_avcodec.h"
#include "qmmf-sdk/qmmf_avcodec_params.h"
#include "qmmf-sdk/qmmf_buffer.h"
#include "qmmf-sdk/qmmf_player_params.h"
#include "qmmf-sdk/qmmf_recorder_params.h"
#include "common/qmmf_common_utils.h"

#include <linux/msm_ion.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <utils/Log.h>
#include <fcntl.h>
#include <assert.h>
#include <unistd.h>
#include <pthread.h>

#include <cutils/native_handle.h>

#include <memory>

#define DEBUG_PIPE

#define TAG "TranscodeAPP"

#pragma once

// Remove comment markers to define LOG_LEVEL_DEBUG for debugging-related logs
// #define TEST_LOG_LEVEL_DEBUG

// Remove comment markers to define LOG_LEVEL_VERBOSE for complete logs
// #define TEST_LOG_LEVEL_VERBOSE

// INFO, ERROR and WARN logs are enabled by default
#define TEST_INFO(fmt, args...)  ALOGD(fmt, ##args)
#define TEST_WARN(fmt, args...)  ALOGW(fmt, ##args)
#define TEST_ERROR(fmt, args...) ALOGE(fmt, ##args)

#ifdef TEST_LOG_LEVEL_DEBUG
#define TEST_DEBUG(fmt, args...)  ALOGD(fmt, ##args)
#else
#define TEST_DEBUG(...) ((void)0)
#endif

#ifdef TEST_LOG_LEVEL_VERBOSE
#define TEST_VERBOSE(fmt, args...)  ALOGD(fmt, ##args)
#else
#define TEST_VERBOSE(...) ((void)0)
#endif

namespace qmmf {
namespace transcode {

typedef int32_t status_t;

#define MAX_FILE_NAME 100

#define NOT_REQUIRED 0x0

#define EOS_FLAG 0x1

#define FRAME_RATE_PERIOD 3000000.0

// #define DEBUG_PIPE_SPEED

#ifdef DEBUG_PIPE_SPEED
#define PIPE_FRAME_RATE_PERIOD 1000000.0
#else
#define PIPE_FRAME_RATE_PERIOD 0xFFFFFFFFFFFFFFFF
#endif

typedef  struct ion_allocation_data IonHandleData;
typedef  struct ion_fd_data IonFdData;

enum class TransCodeType {
  kVideoDecodeVideoEncode,
  kVideoEncodeVideoDecode,
  kImageDecodeVideoEncode,
};

typedef enum BufferOwner {
  kTransCoderCore,
  kTransCoderSink,
  kTransCoderPipeIn,
  kTransCoderPipeOut,
}BufferOwner;

#define OWNER_INDEX(owner) (owner == BufferOwner::kTransCoderCore ? 0x00000000:\
                            owner == BufferOwner::kTransCoderSink ? 0x01000000:\
                            owner == BufferOwner::kTransCoderPipeIn ? 0x10000000:\
                            owner == BufferOwner::kTransCoderPipeOut ? 0x11000000:\
                            0x11111111)

enum class TrackTypes{
  kAudioVideo,
  kAudioOnly,
  kVideoOnly,
  kInvalid,
};

using ::qmmf::avcodec::kPortIndexInput;
using ::qmmf::avcodec::kPortIndexOutput;
using ::qmmf::avcodec::kPortALL;
using ::qmmf::avcodec::CodecParam;
using ::qmmf::avcodec::IAVCodec;
using ::qmmf::avcodec::ICodecSource;
using ::qmmf::avcodec::PortreconfigData;
using ::qmmf::avcodec::PortEventType;
using ::qmmf::avcodec::CodecPortStatus;
using ::qmmf::BufferDescriptor;
using ::qmmf::CodecType;
using ::qmmf::CodecMimeType;
using ::std::shared_ptr;
using ::std::weak_ptr;
using ::std::make_shared;
using ::std::vector;
using ::std::enable_shared_from_this;

typedef struct BufInfo {
  uint32_t capacity;
  uint32_t buf_size;
  int32_t fd;
  void* vaddr;
  IonHandleData ion_handle_;
}BufInfo;

enum class AVCodecBufferType {
  kNormal,
  kNativeHandle,
};

template<class T>
class TSVariable {
public:
  TSVariable() {
    Mutex::Autolock autoLock(lock_);
    memset(&data, 0x0, sizeof(data));
  }

  void SetData(T t) {
    Mutex::Autolock autoLock(lock_);
    data = t;
  }

  T GetData() {
    Mutex::Autolock autoLock(lock_);
    return data;
  }

  void add(T t) {
    Mutex::Autolock autoLock(lock_);
    data = data + t;
  }

  void subtract(T t) {
    Mutex::Autolock autoLock(lock_);
    data = data - t;
  }
private:
  T data;
  Mutex lock_;
};

typedef struct TransCodeParams {
  CodecParam      core_params_;
  CodecParam      sink_params_;
  CodecType       core_codec_type;
  CodecType       sink_codec_type;
  TransCodeType   track_type_;
  char            track_file_[MAX_FILE_NAME];
  char            input_file_[MAX_FILE_NAME];
  char            output_file_[MAX_FILE_NAME];

  ::std::string ToString() const {
    ::std::stringstream stream;
    stream << "TrackFile[" << track_file_ << "] ";
    stream << "Inputfile[" << input_file_ << "] ";
    stream << "OutputFile[" << output_file_ << "] ";
    stream << "TransCodeType["
           << static_cast<::std::underlying_type<TransCodeType>::type>(track_type_)
           << "] ";
    stream << "CoreCodecType["
           << static_cast<::std::underlying_type<CodecType>::type>(core_codec_type)
           << "] ";
    stream << "SinkCodecType["
           << static_cast<::std::underlying_type<CodecType>::type>(sink_codec_type)
           << "] ";
    stream << "CoreParams["
           << (core_codec_type == CodecType::kVideoDecoder ? core_params_.video_dec_param.ToString() :
               core_codec_type == CodecType::kVideoEncoder ? core_params_.video_enc_param.ToString() :
               "UnknownCodecType")
           << "] ";
    stream << "SinkParams["
           << (sink_codec_type == CodecType::kVideoDecoder ? sink_params_.video_dec_param.ToString() :
               sink_codec_type == CodecType::kVideoEncoder ? sink_params_.video_enc_param.ToString() :
               "UnknownCodecType")
           << "]";
    return stream.str();
  }
}TransCodeParams;

class TransCodeBuffer{

public:
  // For local variable declarations in a function of this type

  TransCodeBuffer();

  TransCodeBuffer& operator=(const TransCodeBuffer& rhs);

  TransCodeBuffer(const TransCodeBuffer& obj);

  TransCodeBuffer(BufferOwner _owner, uint32_t _buf_id);

  ~TransCodeBuffer();

  status_t Allocate(uint32_t size);

  status_t Release();

  inline status_t getAVCodecBuffer(AVCodecBufferType type,
                                 BufferDescriptor& buffer) {
    if (type == AVCodecBufferType::kNormal) {
      buffer.data = buf_info_.vaddr;
      buffer.fd = buf_info_.fd;
      buffer.buf_id = buf_id_;
      buffer.size = filled_size_;
      buffer.capacity = buf_info_.capacity;
      buffer.offset = offset_;
      buffer.timestamp = timestamp_;
      buffer.flag = flag_;
    } else if (type == AVCodecBufferType::kNativeHandle){
      buffer.data = static_cast<void*>(meta_handle_);
      buffer.fd = buf_info_.fd;
      buffer.buf_id = buf_id_;
      buffer.size = filled_size_;
      buffer.capacity = buf_info_.capacity;
      buffer.offset = offset_;
      buffer.timestamp = timestamp_;
      buffer.flag = flag_;
    } else {
      TEST_ERROR("%s:%s Unknown AVCodecBufferType", TAG, __func__);
      return -1;
    }
    return 0;
  }

  inline void UpdateTransCodeBuffer(BufferDescriptor& buffer) {
    timestamp_ = buffer.timestamp;
    flag_ = buffer.flag;
    filled_size_ = buffer.size;
    offset_ = buffer.offset;
  }

  inline void* data() {return buf_info_.vaddr;}

  inline uint64_t& Ts() {return timestamp_;}

  inline uint32_t& FilledSize() {return filled_size_;}

  inline uint32_t& Flag() {return flag_;}

  inline uint32_t& Offset() {return offset_;}

  inline uint32_t getID() {return buf_id_;}

  inline int32_t Fd() {return buf_info_.fd;}

  inline BufferOwner& Owner() {return owner_;}

  inline void*  MetaHandle() {return static_cast<void*>(meta_handle_);}

private:
  native_handle_t  *meta_handle_;
  BufferOwner      owner_;
  BufInfo          buf_info_;
  uint32_t         buf_id_;
  uint64_t         timestamp_;
  uint32_t         flag_;
  uint32_t         filled_size_;
  uint32_t         offset_;
  static int32_t   ion_device_;
  static TSVariable<int32_t> num_instances_;
};

status_t AllocateBuffers(vector<TransCodeBuffer>& list,
                        shared_ptr<IAVCodec>& _avcodec,
                        BufferOwner _owner, const uint32_t& port_index_);

status_t ReleaseBuffers(vector<TransCodeBuffer>& list);

};  //namespace transcode
};  //namespace qmmf
