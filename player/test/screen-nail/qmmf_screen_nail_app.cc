/*
* Copyright (c) 2018, The Linux Foundation. All rights reserved.
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

#define LOG_TAG "ScreenNailApp"

#include <dirent.h>
#include <errno.h>

#include <chrono>

#include "qmmf_screen_nail_app.h"

using qmmf::AACMode;
using qmmf::AACFormat;
using qmmf::AudioFormat;
using qmmf::AudioOutSubtype;
using qmmf::avcodec::IAVCodec;
using qmmf::avcodec::ICodecSource;
using qmmf::avcodec::kPortIndexInput;
using qmmf::avcodec::kPortALL;
using qmmf::avcodec::kPortIndexOutput;
using qmmf::avcodec::PortEventType;
using qmmf::avcodec::PortreconfigData;
using qmmf::BufferDescriptor;
using qmmf::BufferFlags;
using qmmf::CodecMimeType;
using qmmf::player::AudioTrackCreateParam;
using qmmf::player::VideoCodecType;
using qmmf::player::VideoTrackCreateParam;
using qmmf::VideoFormat;
using qmmf::VideoOutSubtype;
using std::chrono::duration_cast;
using std::chrono::high_resolution_clock;
using std::chrono::microseconds;
using std::chrono::seconds;
using std::ios;
using std::lock_guard;
using std::make_shared;
using std::mutex;
using std::string;
using std::shared_ptr;
using std::static_pointer_cast;
using std::thread;
using std::unique_lock;
using std::vector;

using namespace screennailapp;

// Property to enable/disable to dump rgba
static const string prop_enable_rgba = "persist.qmmf.s.nail.dumprgba";

// Property to enable/disable to dump yuv
static const string prop_enable_dump_yuv = "persist.qmmf.s.nail.dumpyuv";

// Property to set the desired width for the jpeg image to be generated
static const string prop_jpeg_width = "persist.qmmf.s.nail.jpeg.width";

// Property to set the desired height for the jpeg image to be generated
static const string prop_jpeg_height = "persist.qmmf.s.nail.jpeg.height";

// Property to capture the desired frame for the generation of yuv and/or jpeg
// image
static const string prop_frame_number = "persist.qmmf.s.nail.framenumber";

// Property to set the fastcv operation mode
static const string prop_fastcv_mode = "persist.qmmf.s.nail.fastcvmode";

static const int default_frame = 1;
const int32_t end_of_frames = -1;

static inline uint32_t round_to(uint32_t val, uint32_t round_val) {
  return ((val + round_val - 1) & ~(round_val - 1));
}

VideoDecode::VideoDecode(const string& filename, const CaptureCb& cb,
                         const int32_t ion_device)
    : stop_decode_(false),
      cb_(cb),
      input_file_(filename),
      vidc_avcodec_(nullptr),
      ion_device_(ion_device),
      frame_number_(default_frame) {
  ALOGI("%s: Enter ", __func__);

  char prop_val[PROPERTY_VALUE_MAX];

  property_get(prop_frame_number.c_str(), prop_val, "0");
  if (atoi(prop_val) > 0) {
    frame_number_ = atoi(prop_val);
  }

  ALOGV("%s: frame number to be encoded: %llu", __func__, frame_number_);
  ALOGI("%s: Exit", __func__);
}

VideoDecode::~VideoDecode() {
  ALOGI("%s: Enter ", __func__);

  if (output_codec_src_ != nullptr) output_codec_src_ = nullptr;
  if (input_codec_src_ != nullptr) input_codec_src_ = nullptr;

  ReleaseBuffer(kPortIndexOutput);
  ReleaseBuffer(kPortIndexInput);

  if (vidc_avcodec_ != nullptr) delete vidc_avcodec_;
  if (stream_port_ != nullptr) delete stream_port_;
  if (demux_ != nullptr) delete demux_;

  ALOGI("%s: Exit ", __func__);
}

int32_t VideoDecode::InitDemux() {
  ALOGI("VideoDecode:%s: Enter ", __func__);
  if (!(input_file_.empty())) {
    stream_port_ =
        new CMM_MediaSourcePort(const_cast<char*>(input_file_.c_str()));
    if (stream_port_ == nullptr) {
      ALOGE("VideoDecode:%s: Failed to allocate demuxer", __func__);
      goto READ_FAILED;
    }
    CreateDataSource();
    auto ret = FillCodecParams();
    if (ret != 0) {
      ALOGE("VideoDecode:%s: Failed to get videodecoder params from demuxer",
            __func__);
      goto READ_FAILED;
    }
  } else {
    ALOGE("VideoDecode:%s: Input file name not found", __func__);
    goto READ_FAILED;
  }
  ALOGI("VideoDecode:%s: Exit", __func__);
  return 0;

READ_FAILED:
  if (stream_port_ != nullptr) delete stream_port_;
  if (vidc_avcodec_ != nullptr) delete vidc_avcodec_;
  close(ion_device_);
  ion_device_ = -1;
  ALOGI("VideoDecode:%s: Exit", __func__);
  return -1;
}

int32_t VideoDecode::Init() {
  ALOGI("VideoDecode:%s: Enter", __func__);

  vidc_avcodec_ = IAVCodec::CreateAVCodec();
  if (vidc_avcodec_ == nullptr) {
    ALOGE("VideoDecode:%s: avcodec creation failed", __func__);
    return -ENOMEM;
  }

  auto ret = vidc_avcodec_->ConfigureCodec(CodecMimeType::kMimeTypeVideoDecAVC,
                                      create_param_);
  if (ret != 0) {
    ALOGE("VideoDecode:%s: Failed to configure Codec", __func__);
    return ret;
  }

  ret = AllocateBuffer(kPortIndexInput);
  if (ret != 0) {
    ALOGE("VideoDecode:%s: Failed to allocate buffer on PORT_NAME(%d)",
          __func__, kPortIndexInput);
    ReleaseBuffer(kPortIndexInput);
    return ret;
  }

  ret = vidc_avcodec_->RegisterInputBuffers(input_buffer_list_);
  if (ret != 0) {
    ALOGE("VideoDecode:%s: input buffers failed to register to AVCodec",
          __func__);
    return ret;
  }

  input_codec_src_ = make_shared<InputCodecSourceImpl>(this);
  if (input_codec_src_.get() == nullptr) {
    ALOGE("VideoDecode:%s: failed to create output source", __func__);
    return -ENOMEM;
  }

  ret = vidc_avcodec_->AllocateBuffer(
      kPortIndexInput, 0, 0,
      static_pointer_cast<ICodecSource>(input_codec_src_), input_buffer_list_);
  if (ret != 0) {
    ALOGE("VideoDecode:%s: Failed to Call Allocate buffer on PORT_NAME(%d)",
          __func__, kPortIndexOutput);
    goto RELEASE_INPUT;
  }

  input_codec_src_->AddBufferList(input_buffer_list_);

  ret = AllocateBuffer(kPortIndexOutput);
  if (ret != 0) {
    ALOGE("VideoDecode:%s: Failed to allocate buffer on PORT_NAME(%d)",
          __func__, kPortIndexOutput);
    goto RELEASE_OUTPUT;
  }

  ret = vidc_avcodec_->RegisterOutputBuffers(output_buffer_list_);
  if (ret != 0) {
    ALOGE("VideoDecode:%s: output buffers failed to register to AVCodec",
          __func__);
    return ret;
  }

  output_codec_src_ = make_shared<OutputCodecSourceImpl>(
      input_file_.substr(0, input_file_.find(".")), this, cb_, frame_number_);
  if (output_codec_src_.get() == nullptr) {
    ALOGE("VideoDecode:%s: failed to create output source", __func__);
    return -ENOMEM;
  }

  ret = vidc_avcodec_->AllocateBuffer(
      kPortIndexOutput, 0, 0,
      static_pointer_cast<ICodecSource>(output_codec_src_),
      output_buffer_list_);

  if (ret != 0) {
    ALOGE("VideoDecode:%s: Failed to Call Allocate buffer on PORT_NAME(%d)",
        __func__, kPortIndexOutput);
    goto RELEASE_OUTPUT;
  }

  output_codec_src_->AddBufferList(output_buffer_list_);

  ALOGI("VideoDecode:%s: Exit", __func__);
  return ret;

RELEASE_OUTPUT:
  ReleaseBuffer(kPortIndexOutput);
RELEASE_INPUT:
  ReleaseBuffer(kPortIndexInput);
  vidc_avcodec_->ReleaseBuffer();
  ALOGI("VideoDecode:%s: Exit", __func__);
  return ret;
}

int32_t VideoDecode::StartDecode() {
  ALOGI("%s Enter", __func__);
  int32_t ret = 0;

  ret = vidc_avcodec_->StartCodec();
  assert(ret == 0);

  ALOGI("%s Exit", __func__);
  return ret;
}

int32_t VideoDecode::StopDecode() {
  ALOGI("%s Enter", __func__);
  int32_t ret = 0;

  ret = vidc_avcodec_->StopCodec(false);
  assert(ret == 0);

  ALOGI("%s Exit", __func__);
  return ret;
}

int32_t VideoDecode::AllocateBuffer(const uint32_t index) {
  ALOGI("VideoDecode:%s: Enter", __func__);
  int32_t ret = 0;

  assert(ion_device_ > 0);
  int32_t ionType = ION_HEAP(ION_IOMMU_HEAP_ID);

  uint32_t count, size;
  ret = vidc_avcodec_->GetBufferRequirements(index, &count, &size);
  if (ret != 0) {
    ALOGI("VideoDecode:%s: Failed to get Buffer Requirements on %s", __func__,
          PORT_NAME(index));
    return ret;
  }

  struct ion_allocation_data alloc;
  struct ion_fd_data ionFdData;
  struct ion_handle_data ionHandleData;

  if (index == kPortIndexInput) {
    void* vaddr = nullptr;

    for (uint32_t i = 0; i < count; i++) {
      BufferDescriptor buffer;
      memset(&buffer, 0x0, sizeof(buffer));
      memset(&alloc, 0x0, sizeof(ion_allocation_data));
      memset(&ionFdData, 0x0, sizeof(ion_fd_data));
      memset(&ionHandleData, 0x0, sizeof(ion_handle_data));

      alloc.len = size;
      alloc.len = (alloc.len + 4095) & (~4095);
      alloc.align = 4096;
      alloc.flags = ION_FLAG_CACHED;
      alloc.heap_id_mask = ionType;

      // Sends an ION allocation request to the system.
      ret = ioctl(ion_device_, ION_IOC_ALLOC, &alloc);
      if (ret < 0) {
        ALOGE("%s ION allocation failed", __func__);
        goto ION_ALLOC_FAILED;
      }

      ionFdData.handle = alloc.handle;
      ionHandleData.handle =  alloc.handle;

      // Send a request to create a file descriptor to use to share an
      // allocation
      ret = ioctl(ion_device_, ION_IOC_SHARE, &ionFdData);
      if (ret < 0) {
        ALOGE("%s ION map failed %s", __func__, strerror(errno));
        goto ION_MAP_FAILED;
      }

      vaddr = mmap(nullptr, alloc.len, PROT_READ | PROT_WRITE, MAP_SHARED,
                   ionFdData.fd, 0);
      if (vaddr == MAP_FAILED) {
        ALOGE("VideoDecode:%s:  ION mmap failed: %s (%d)", __func__,
              strerror(errno), errno);
        goto ION_MAP_FAILED;
      }
      input_ion_handle_data_.insert({ionFdData.fd, ionHandleData});

      buffer.fd = ionFdData.fd;
      buffer.capacity = alloc.len;
      buffer.size = alloc.len;
      buffer.data = vaddr;

      ALOGI("VideoDecode:%s: buffer.Fd(%d)", __func__, buffer.fd);
      ALOGI("VideoDecode:%s: buffer.capacity(%d)", __func__, buffer.capacity);
      ALOGI("VideoDecode:%s: buffer.vaddr(%p)", __func__, buffer.data);
      input_buffer_list_.push_back(buffer);
    }
  } else {
    void* vaddr = nullptr;

    for (uint32_t i = 0; i < count; i++) {
      BufferDescriptor buffer;
      vaddr = nullptr;

      memset(&buffer, 0x0, sizeof(buffer));
      memset(&alloc, 0x0, sizeof(ion_allocation_data));
      memset(&ionFdData, 0x0, sizeof(ion_fd_data));
      memset(&ionHandleData, 0x0, sizeof(ion_handle_data));

      alloc.len = size;
      alloc.len = (alloc.len + 4095) & (~4095);
      alloc.align = 4096;
      alloc.flags = ION_FLAG_CACHED;
      alloc.heap_id_mask = ionType;

      ret = ioctl(ion_device_, ION_IOC_ALLOC, &alloc);
      if (ret < 0) {
        ALOGE("VideoDecode:%s: ION allocation failed", __func__);
        goto ION_ALLOC_FAILED;
      }

      ionFdData.handle = alloc.handle;
      ionHandleData.handle = alloc.handle;

      ret = ioctl(ion_device_, ION_IOC_SHARE, &ionFdData);
      if (ret < 0) {
        ALOGE("VideoDecode:%s: ION map failed %s", __func__, strerror(errno));
        goto ION_MAP_FAILED;
      }

      vaddr = mmap(nullptr, alloc.len, PROT_READ | PROT_WRITE, MAP_SHARED,
                   ionFdData.fd, 0);
      if (vaddr == MAP_FAILED) {
        ALOGE("VideoDecode:%s: ION mmap failed: %s (%d)", __func__,
              strerror(errno), errno);
        goto ION_MAP_FAILED;
      }

      output_ion_handle_data_.insert({ionFdData.fd, ionHandleData});

      buffer.fd = ionFdData.fd;
      buffer.capacity = alloc.len;
      buffer.size = alloc.len;
      buffer.data = vaddr;

      ALOGI("VideoDecode:%s: buffer.Fd(%d)", __func__, buffer.fd);
      ALOGI("VideoDecode:%s: buffer.capacity(%d)", __func__, buffer.capacity);
      ALOGI("VideoDecode:%s: buffer.vaddr(%p)", __func__, buffer.data);
      output_buffer_list_.push_back(buffer);
    }
  }

  ALOGI("VideoDecode:%s: Exit", __func__);
  return ret;

ION_MAP_FAILED:
  memset(&ionHandleData, 0x0, sizeof(IonHandleData));
  ionHandleData.handle = ionFdData.handle;
  ioctl(ion_device_, ION_IOC_FREE, &ionHandleData);
ION_ALLOC_FAILED:
  close(ion_device_);
  ion_device_ = -1;
  ALOGE("VideoDecode:%s: ION Buffer allocation failed!", __func__);
  ALOGI("VideoDecode:%s: Exit", __func__);
  return -1;
}

void VideoDecode::ReleaseBuffer(const uint32_t index) {
  ALOGI("VideoDecode:%s: Enter ", __func__);

  assert(ion_device_ > 0);
  if (index == kPortIndexInput) {
    for (auto& iter : input_buffer_list_) {
      if ((iter).data) {
        munmap((iter).data, (iter).capacity);
        (iter).data = nullptr;
      }
      if ((iter).fd > 0) {
        auto it = input_ion_handle_data_.find(iter.fd);
        close((iter).fd);
        (iter).fd = -1;
        auto ret = ioctl(ion_device_, ION_IOC_FREE, &(it->second));
        if(ret < 0) {
          ALOGE("VideoDecode:%s: ION Free failed for Input-Port:(%d) %s",
              __func__, errno, strerror(errno));
        } else {
          ALOGD("VideoDecode:%s: ION Free successful for Input-Port", __func__);
        }
      }
    }
    input_buffer_list_.clear();
    input_ion_handle_data_.clear();
  }

  if (index == kPortIndexOutput) {
    for (auto& iter : output_buffer_list_) {
      if ((iter).data) {
        munmap((iter).data, (iter).capacity);
        (iter).data = nullptr;
      }
      if ((iter).fd > 0) {
        auto it = output_ion_handle_data_.find(iter.fd);
        close((iter).fd);
        (iter).fd = -1;
        auto ret = ioctl(ion_device_, ION_IOC_FREE, &(it->second));
        if(ret < 0) {
          ALOGE("VideoDecode:%s: ION Free failed for Output-Port:(%d) %s",
              __func__, errno, strerror(errno));
        } else {
          ALOGD("VideoDecode:%s: ION Free successful for Output-Port", __func__);
        }
      }
    }
    output_buffer_list_.clear();
    output_ion_handle_data_.clear();
  }

  ALOGI("VideoDecode:%s: Exit", __func__);
}

int32_t VideoDecode::Read(BufferDescriptor* stream_buffer,
                          const bool is_first_frame) {
  ALOGI("VideoDecode:%s: Enter ", __func__);
  FileSourceSampleInfo sample_info;
  FileSourceMediaStatus media_status = FILE_SOURCE_DATA_ERROR;
  memset(&sample_info, 0, sizeof(FileSourceSampleInfo));
  (track_info_).sVideo.sSampleBuf.ulLen =
      (track_info_).sVideo.sSampleBuf.ulMaxLen;
  if (is_first_frame == true) {
    uint32_t csd_data_size = 0;
    uint32_t status = demux_->m_pFileSource->GetFormatBlock(
        (track_info_).sVideo.ulTkId, nullptr, &csd_data_size);

    ALOGI("VideoDecode:%s: Video CSD data Size = %u", __func__, csd_data_size);
    assert(FILE_SOURCE_SUCCESS == status);

    status = demux_->m_pFileSource->GetFormatBlock(
        (track_info_).sVideo.ulTkId,
        reinterpret_cast<uint8_t*>(stream_buffer->data), &csd_data_size);

    media_status = demux_->GetNextMediaSample(
        (track_info_).sVideo.ulTkId,
        reinterpret_cast<uint8_t*>(stream_buffer->data) + csd_data_size,
        &((track_info_).sVideo.sSampleBuf.ulLen), sample_info);
    assert(FILE_SOURCE_SUCCESS == status);
    stream_buffer->size = csd_data_size + (track_info_).sVideo.sSampleBuf.ulLen;
  } else {
    media_status = demux_->GetNextMediaSample(
        (track_info_).sVideo.ulTkId,
        reinterpret_cast<uint8_t*>(stream_buffer->data),
        &((track_info_).sVideo.sSampleBuf.ulLen), sample_info);
    stream_buffer->size = (track_info_).sVideo.sSampleBuf.ulLen;
  }
  if (FILE_SOURCE_DATA_END == media_status) {
    ALOGI("VideoDecode:%s: File read completed", __func__);
    stream_buffer->size = 0;
    stream_buffer->flag = static_cast<uint32_t>(BufferFlags::kFlagEOS);
  }
  ALOGI("VideoDecode:%s: Exit ", __func__);
  return 0;
}

VideoDecode::InputCodecSourceImpl::InputCodecSourceImpl(VideoDecode* const vid)
    : input_video_decode_(vid), is_first_frame_(true) {
  ALOGI("VideoDecode:InputCodecSourceImpl:%s: Enter ", __func__);
  ALOGI("VideoDecode:InputCodecSourceImpl:%s: Exit ", __func__);
}

VideoDecode::InputCodecSourceImpl::~InputCodecSourceImpl() {
  ALOGI("VideoDecode:InputCodecSourceImpl:%s: Enter ", __func__);
  {
    lock_guard<mutex> lg(input_free_buffer_vector_lock_);
    input_free_buffer_vector_.clear();
  }
  {
    lock_guard<mutex> lg(input_occupy_buffer_map_lock_);
    input_occupy_buffer_map_.clear();
  }
  ALOGI("VideoDecode:InputCodecSourceImpl:%s: Exit ", __func__);
}

int32_t VideoDecode::InputCodecSourceImpl::GetBuffer(
    BufferDescriptor& stream_buffer, void* client_data) {
  ALOGI("Videodecode:InputCodecSourceImpl:%s: Enter ", __func__);

  int32_t ret;
  while (input_free_buffer_vector_.size() <= 0) {
    ALOGW("Videodecode:InputCodecSourceImpl:%s: No buffer available. Wait "
        "for new buffer", __func__);
    unique_lock<mutex> lock(wait_for_frame_lock_);
    wait_for_frame_.wait_for(lock, seconds(1));
  }
  BufferDescriptor buffer;
  {
    lock_guard<mutex> lg(input_free_buffer_vector_lock_);
    buffer = *input_free_buffer_vector_.begin();
  }
  assert(buffer.data != nullptr);
  ret = input_video_decode_->Read(&buffer, is_first_frame_);
  is_first_frame_ = false;
  if (ret != 0) {
    ALOGE("Videodecode:InputCodecSourceImpl:%s: Read failed", __func__);
    return ret;
  }
  stream_buffer = buffer;
  ALOGV("Videodecode:InputCodecSourceImpl:%s: Buffer descriptor: %s", __func__,
        stream_buffer.ToString().c_str());
  {
    lock_guard<mutex> lg(input_occupy_buffer_map_lock_);
    input_occupy_buffer_map_.insert({buffer.data, buffer});
  }
  {
    lock_guard<mutex> lg(input_free_buffer_vector_lock_);
    input_free_buffer_vector_.erase(input_free_buffer_vector_.begin());
  }
  if (stream_buffer.flag == static_cast<uint32_t>(BufferFlags::kFlagEOS)) {
    return end_of_frames;
  }
  ALOGI("Videodecode:InputCodecSourceImpl:%s: Exit ", __func__);
  return 0;
}

int32_t VideoDecode::InputCodecSourceImpl::ReturnBuffer(
    BufferDescriptor& stream_buffer, void* client_data) {
  ALOGI("Videodecode:InputCodecSourceImpl:%s: Enter", __func__);
  int32_t ret = 0;
  ALOGV("Videodecode:InputCodecSourceImpl:%s: Buffer-descriptor: %s", __func__,
        stream_buffer.ToString().c_str());
  {
    lock_guard<mutex> lg(input_occupy_buffer_map_lock_);
    auto it = input_occupy_buffer_map_.find(stream_buffer.data);
    if (it != input_occupy_buffer_map_.end()) {
      lock_guard<mutex> lg(input_free_buffer_vector_lock_);
      input_free_buffer_vector_.push_back(it->second);
      wait_for_frame_.notify_one();
      input_occupy_buffer_map_.erase(it);
    }
  }
  ALOGI("Videodecode:InputCodecSourceImpl:%s: Exit", __func__);
  return ret;
}

int32_t VideoDecode::InputCodecSourceImpl::NotifyPortEvent(
    PortEventType event_type, void* event_data) {
  ALOGI("Videodecode:InputCodecSourceImpl:%s: Enter", __func__);
  ALOGI("Videodecode:InputCodecSourceImpl:%s: Exit", __func__);
  return 0;
}

void VideoDecode::InputCodecSourceImpl::AddBufferList(
    const vector<BufferDescriptor>& list) {
  ALOGI("Videodecode:InputCodecSourceImpl:%s: Enter ", __func__);

  input_video_decode_->input_buffer_list_ = list;
  {
    lock_guard<mutex> lg(input_free_buffer_vector_lock_);
    input_free_buffer_vector_.clear();
  }
  {
    lock_guard<mutex> lg(input_occupy_buffer_map_lock_);
    input_occupy_buffer_map_.clear();
  }
  for (auto& iter : input_video_decode_->input_buffer_list_) {
    lock_guard<mutex> lg(input_free_buffer_vector_lock_);
    input_free_buffer_vector_.push_back(iter);
  }

  ALOGI("Videodecode:InputCodecSourceImpl:%s: Exit", __func__);
}

int32_t VideoDecode::FillCodecParams() {
  ALOGI("VideoDecode:%s: Enter", __func__);
  VideoTrackCreateParam video_track_param;

  memset(&video_track_param, 0x0, sizeof(video_track_param));

  if (track_type_ == TrackTypes::kAudioVideo ||
      track_type_ == TrackTypes::kVideoOnly) {
    if (track_info_.sVideo.ulCodecType == 11) {
      video_track_param.codec = VideoCodecType::kAVC;
    } else if (track_info_.sVideo.ulCodecType == 12) {
      video_track_param.codec = VideoCodecType::kHEVC;
    }

    video_track_param.frame_rate = track_info_.sVideo.fFrameRate;
    video_track_param.height = track_info_.sVideo.ulHeight;
    video_track_param.width = track_info_.sVideo.ulWidth;
    video_track_param.bitrate = track_info_.sVideo.ulBitRate;
    video_track_param.num_buffers = 1;

    ALOGI("VideoDecode:%s: height : %d width %d frame_rate %d, bitrate %d ",
          __func__, video_track_param.height, video_track_param.width,
          video_track_param.frame_rate, video_track_param.bitrate);
  }

  create_param_.video_dec_param = video_track_param;

  ALOGI("VideoDecode:%s: Exit", __func__);
  return 0;
}

int32_t VideoDecode::CreateDataSource() {
  ALOGI("VideoDecode:%s: Enter", __func__);

  int32_t err = MM_STATUS_ErrorNone;
  demux_ = CMM_MediaDemuxInt::New(*stream_port_, FILE_SOURCE_MPEG4);
  if (demux_ == nullptr) {
    ALOGE("VideoDecode:%s: DataSource Creation FAILURE!!", __func__);
    BAIL_ON_ERROR(MM_STATUS_ErrorDefault);
  }

  ALOGI("VideoDecode:%s: DataSource Creation SUCCESS!!", __func__);

  // Read file meta-data
  err = ReadMediaInfo();
  BAIL_ON_ERROR(err);
  ALOGI("VideoDecode:%s: Exit", __func__);
  return 0;
ERROR_BAIL:
  ALOGI("VideoDecode:%s: Exit", __func__);
  return err;
}

int32_t VideoDecode::ReadMediaInfo() {
  ALOGI("VideoDecode:%s: Enter", __func__);

  int32_t err = 0;
  FileSourceTrackIdInfoType track_list[MM_SOURCE_MAX_TRACKS];
  FileSourceMjMediaType mj_type = FILE_SOURCE_MJ_TYPE_UNKNOWN;
  FileSourceMnMediaType mn_type = FILE_SOURCE_MN_TYPE_UNKNOWN;
  FileSourceStatus fs_status = FILE_SOURCE_FAIL;
  track_type_ = TrackTypes::kAudioVideo;

  // Get total number of tracks available.
  track_info_.ulNumTracks = demux_->GetWholeTracksIDList(track_list);
  ALOGI("VideoDecode:%s: NumTracks = %u", __func__, track_info_.ulNumTracks);

  for (uint32 ulIdx = 0; ulIdx < track_info_.ulNumTracks; ulIdx++) {
    FileSourceTrackIdInfoType track_info = track_list[ulIdx];

    // Get MimeType
    fs_status = demux_->GetMimeType(track_info.id, mj_type, mn_type);
    if (FILE_SOURCE_SUCCESS != fs_status) {
      ALOGI("VideoDecode:%s: Unable to get MIME_TYPE = %u", __func__,
            fs_status);
      continue;
    }

    if (FILE_SOURCE_SUCCESS == fs_status) {
      if (FILE_SOURCE_MJ_TYPE_AUDIO == mj_type) {
        ALOGI("VideoDecode:%s: TRACK_AUDIO @MIME_TYPE = %u", __func__, mn_type);
        ALOGI("VideoDecode:%s id:%d ", __func__, track_info.id);
      } else if (FILE_SOURCE_MJ_TYPE_VIDEO == mj_type) {
        ALOGI("VideoDecode:%s: TRACK_VIDEO @MIME_TYPE = %u", __func__, mn_type);

        track_info_.sVideo.bTrackSelected = track_info.selected;

        ALOGI("VideoDecode:%s id:%d ", __func__, track_info.id);

        err = ReadVideoTrackMediaInfo(track_info.id, mn_type);
        if (track_info_.ulNumTracks == 1) {
          track_type_ = TrackTypes::kVideoOnly;
        }
      }
    } else {
      err = MM_STATUS_ErrorStreamCorrupt;
      ALOGE("VideoDecode:%s: Failed to identify Tracks Error= %u", __func__,
            fs_status);
      BAIL_ON_ERROR(err);
    }
  }

  ALOGI("VideoDecode:%s: Exit", __func__);
  return 0;
ERROR_BAIL:
  ALOGI("VideoDecode:%s: Exit", __func__);
  return err;
}

int32_t VideoDecode::ReadVideoTrackMediaInfo(
    const uint32 track_id, const FileSourceMnMediaType mn_type) {
  ALOGI("%s: Enter", __func__);

  MM_STATUS_TYPE err = MM_STATUS_ErrorNone;
  FileSourceStatus fs_status = FILE_SOURCE_FAIL;
  MediaTrackInfo media_info;
  memset(&media_info, 0, sizeof(MediaTrackInfo));

  track_info_.sVideo.sSampleBuf.ulMaxLen =
      demux_->GetTrackMaxFrameBufferSize(track_id);
  fs_status = demux_->GetMediaTrackInfo(track_id, &media_info);
  if (FILE_SOURCE_SUCCESS == fs_status) {
    track_info_.sVideo.ulTkId = track_id;
    track_info_.sVideo.ulCodecType = media_info.videoTrackInfo.videoCodec;
    track_info_.sVideo.ulWidth = media_info.videoTrackInfo.frameWidth;
    track_info_.sVideo.ulHeight = media_info.videoTrackInfo.frameHeight;
    track_info_.sVideo.fFrameRate = media_info.videoTrackInfo.frameRate;
    track_info_.sVideo.ulBitRate = media_info.videoTrackInfo.bitRate;
    track_info_.sVideo.ullDuration = media_info.videoTrackInfo.duration;
    track_info_.sVideo.ulTimeScale = media_info.videoTrackInfo.timeScale;

    ALOGI("%s:Video CodecType is = %u ", __func__,
          track_info_.sVideo.ulCodecType);

    ALOGI("%s: TkId = %u, Input-Width = %u,  Input-Height = %u, FrameRate = %f,"
          " bitrate = %u duration = %llu", __func__, track_id,
          track_info_.sVideo.ulWidth, track_info_.sVideo.ulHeight,
          track_info_.sVideo.fFrameRate, track_info_.sVideo.ulBitRate,
          track_info_.sVideo.ullDuration);

    // Get CSD data len
    fs_status = demux_->GetFormatBlock(track_id, nullptr,
                                       &track_info_.sVideo.sCSD.ulLen, FALSE);
    BAIL_ON_ERROR(fs_status);
    if (0 != track_info_.sVideo.sCSD.ulLen) {
      ALOGI("%s: CSD Len = %u", __func__, track_info_.sVideo.sCSD.ulLen);

      track_info_.sVideo.sCSD.pucData = reinterpret_cast<uint8*>(
          MM_Malloc(sizeof(uint8) * track_info_.sVideo.sCSD.ulLen));
      if (track_info_.sVideo.sCSD.pucData == nullptr) {
        err = MM_STATUS_ErrorMemAllocFail;
        ALOGE("%s CSD Alloc failure", __func__);
        BAIL_ON_ERROR(err);
      }
      fs_status =
          demux_->GetFormatBlock(track_id, track_info_.sVideo.sCSD.pucData,
                                 &track_info_.sVideo.sCSD.ulLen, FALSE);
      BAIL_ON_ERROR(fs_status);
    }
  } else {
    BAIL_ON_ERROR(fs_status);
  }

  ALOGI("%s: Exit", __func__);

ERROR_BAIL:
  if (FILE_SOURCE_SUCCESS != fs_status) {
    err = MM_STATUS_ErrorDefault;
  }

  ALOGE("%s: Return Status %u", __func__, err);
  ALOGI("%s: Exit", __func__);
  return err;
}

VideoDecode::OutputCodecSourceImpl::OutputCodecSourceImpl(
    const string& filename, VideoDecode* const vid, const CaptureCb& cb,
    const uint64_t frame_number)
    : frame_number_(frame_number),
      frame_counter_(0),
      cb_(cb),
      output_video_decode_(vid) {
  ALOGI("VideoDecode:OutputCodecSourceImpl:%s: Enter", __func__);
  ALOGI("VideoDecode:OutputCodecSourceImpl:%s: Exit", __func__);
}

VideoDecode::OutputCodecSourceImpl::~OutputCodecSourceImpl() {
  ALOGI("VideoDecode:OutputCodecSourceImpl:%s: Enter", __func__);
  {
    lock_guard<mutex> lg(output_free_buffer_vector_lock_);
    output_free_buffer_vector_.clear();
  }
  {
    lock_guard<mutex> lg(output_occupy_buffer_map_lock_);
    output_occupy_buffer_map_.clear();
  }
  ALOGI("VideoDecode:OutputCodecSourceImpl:%s: Exit", __func__);
}

int32_t VideoDecode::OutputCodecSourceImpl::GetBuffer(
    BufferDescriptor& codec_buffer, void* client_data) {
  ALOGI("VideoDecode:OutputCodecSourceImpl:%s: Enter", __func__);

  int32_t ret = 0;
  while (output_free_buffer_vector_.size() <= 0) {
    ALOGW("VideoDecode:OutputCodecSourceImpl:%s: No buffer available to notify."
          " Wait for new buffer", __func__);
    unique_lock<mutex> lock(wait_for_frame_lock_);
    wait_for_frame_.wait_for(lock, seconds(1));
  }
  {
    lock_guard<mutex> lg(output_free_buffer_vector_lock_);
    codec_buffer = *output_free_buffer_vector_.begin();
  }
  ALOGV("%s:OutputPort-videodecode:%s", __func__,
        codec_buffer.ToString().c_str());
  {
    lock_guard<mutex> lg(output_occupy_buffer_map_lock_);
    output_occupy_buffer_map_.insert({codec_buffer.fd, codec_buffer});
  }
  {
    lock_guard<mutex> lg(output_free_buffer_vector_lock_);
    output_free_buffer_vector_.erase(output_free_buffer_vector_.begin());
  }
  ALOGI("VideoDecode:OutputCodecSourceImpl:%s: Exit", __func__);
  return ret;
}

int32_t VideoDecode::OutputCodecSourceImpl::ReturnBuffer(
    BufferDescriptor& codec_buffer, void* client_data) {
  ALOGI("VideoDecode:OutputCodecSourceImpl:%s: Enter", __func__);

  int32_t ret = 0;

  frame_counter_++;
  ALOGV("VideoDecode:OutputCodecSourceImpl:%s: frame_number: %d, "
        "frame_counter: %d", __func__, frame_number_, frame_counter_);
  ALOGV("VideoDecode:OutputCodecSourceImpl:%s: Buffer-Descriptor:%s", __func__,
        codec_buffer.ToString().c_str());

  {
    lock_guard<mutex> lg(output_occupy_buffer_map_lock_);
    auto it = output_occupy_buffer_map_.find(codec_buffer.fd);

    if (it != output_occupy_buffer_map_.end()) {
      if (frame_number_ == frame_counter_) {
        auto ret = cb_(it->second,
                output_video_decode_->create_param_.video_dec_param.width,
                output_video_decode_->create_param_.video_dec_param.height);
        if(ret != 0) {
          ALOGE("VideoDecode:OutputCodecSourceImpl:%s: Call back failed",
                 __func__);
          assert(0);
        }
        output_video_decode_->SetStopDecode(true);
      }
      {
        lock_guard<mutex> lg(output_free_buffer_vector_lock_);
        output_free_buffer_vector_.push_back(it->second);
      }
      output_occupy_buffer_map_.erase(it);
      wait_for_frame_.notify_one();
    }
  }

  if (codec_buffer.flag == static_cast<uint32_t>(BufferFlags::kFlagEOS)) {
    output_video_decode_->SetStopDecode(true);
    ALOGI("VideoDecode:OutputCodecSourceImpl:%s: This is last buffer from "
          "decoder.Close file", __func__);
  }
  ALOGI("VideoDecode:OutputCodecSourceImpl:%s: Exit", __func__);
  return ret;
}

int32_t VideoDecode::OutputCodecSourceImpl::NotifyPortEvent(
    PortEventType event_type, void* event_data) {
  ALOGI("VideoDecode:OutputCodecSourceImpl:%s: Enter", __func__);

  int32_t ret = 0;
  switch (event_type) {
    case PortEventType::kPortStatus:
      break;
    case PortEventType::kPortSettingsChanged:
      switch (static_cast<PortreconfigData*>(event_data)->reconfig_type) {
        case PortreconfigData::PortReconfigType::kCropParametersChanged:
          break;
        case PortreconfigData::PortReconfigType::kBufferRequirementsChanged:
          ALOGI("VideoDecode:OutputCodecSourceImpl:%s: Releasing Video decode "
                "output_buffer_list_", __func__);
          output_video_decode_->ReleaseBuffer(kPortIndexOutput);
          assert(output_video_decode_->output_buffer_list_.empty());
          ALOGI("VideoDecode:OutputCodecSourceImpl:%s: Allocating New set of "
                "Buffers", __func__);
          ret = output_video_decode_->AllocateBuffer(kPortIndexOutput);
          if (ret != 0) {
            ALOGE("VideoDecode:OutputCodecSourceImpl:%s: Failed to allocate new "
                  "video decode output buffers", __func__);
            return ret;
          }

          ret = output_video_decode_->vidc_avcodec_->RegisterOutputBuffers(
              output_video_decode_->output_buffer_list_);
          if (ret != 0) {
            ALOGE("VideoDecode:OutputCodecSourceImpl:%s: output buffers failed "
                  "to register to AVCodec", __func__);
            return ret;
          }
          {
            lock_guard<mutex> lg(output_free_buffer_vector_lock_);
            output_free_buffer_vector_.clear();
          }
          ALOGD("VideoDecode:OutputCodecSourceImpl:%s: Cleared o/p free buffer "
                "vector", __func__);
          {
            lock_guard<mutex> lg(output_occupy_buffer_map_lock_);
            output_occupy_buffer_map_.clear();
          }
          ALOGD("VideoDecode:OutputCodecSourceImpl:%s: Cleared o/p occupy buffer "
                "map", __func__);
          for (auto& iter : output_video_decode_->output_buffer_list_) {
            ALOGD("VideoDecode:OutputCodecSourceImpl:%s: Pushing new buffer into "
                  "o/p buffer list", __func__);
            lock_guard<mutex> lg(output_free_buffer_vector_lock_);
            output_free_buffer_vector_.push_back(iter);
          }
          wait_for_frame_.notify_one();
          break;
        default:
          return -1;
      }
      break;
    default:
      return -1;
  }
  ALOGI("VideoDecode:OutputCodecSourceImpl:%s: Exit", __func__);
  return 0;
}

void VideoDecode::OutputCodecSourceImpl::AddBufferList(
    const vector<BufferDescriptor>& list) {
  ALOGI("VideoDecode:OutputCodecSourceImpl:%s: Enter ", __func__);

  output_video_decode_->output_buffer_list_ = list;
  {
    lock_guard<mutex> lg(output_free_buffer_vector_lock_);
    output_free_buffer_vector_.clear();
  }
  {
    lock_guard<mutex> lg(output_occupy_buffer_map_lock_);
    output_occupy_buffer_map_.clear();
  }
  for (auto& iter : output_video_decode_->output_buffer_list_) {
    lock_guard<mutex> lg(output_free_buffer_vector_lock_);
    output_free_buffer_vector_.push_back(iter);
  }

  ALOGI("VideoDecode:OutputCodecSourceImpl:%s: Exit", __func__);
}

JpegEncode::JpegEncode(const string& filename, const int32_t ion_device)
    : init_(false),
      width_(960),
      height_(720),
      ion_device_(ion_device),
      output_file_(filename) {
  ALOGI("%s: Enter ", __func__);
  char prop_val[PROPERTY_VALUE_MAX];
  property_get(prop_jpeg_width.c_str(), prop_val, "0");
  if (!(atoi(prop_val) == 0)) {
    width_ = atoi(prop_val);
  }
  property_get(prop_jpeg_height.c_str(), prop_val, "0");
  if (!(atoi(prop_val) == 0)) {
    height_ = atoi(prop_val);
  }
  ALOGI("%s: Jpeg width:%d", __func__, width_);
  ALOGI("%s: Jpeg height:%d", __func__, height_);
  ALOGI("%s: Exit ", __func__);
}

JpegEncode::~JpegEncode() {
  ALOGI("%s: Enter ", __func__);
  ReleaseBuffer();
  if (jpeg_avcodec_ != nullptr) delete jpeg_avcodec_;
  ALOGI("%s: Exit ", __func__);
}

int32_t JpegEncode::Init(const BufferDescriptor& buffer) {
  ALOGI("JpegEncode:%s: Enter ", __func__);
  jpeg_avcodec_ = IAVCodec::CreateAVCodec(CodecMimeType::kMimeTypeJPEG);
  if (jpeg_avcodec_ == nullptr) {
    ALOGE("JpegEncode:%s: avcodec creation failed", __func__);
    return -ENOMEM;
  }

  create_param_.video_enc_param.format_type = VideoFormat::kJPEG;
  create_param_.video_enc_param.width = width_;
  create_param_.video_enc_param.height = height_;

  auto ret = jpeg_avcodec_->ConfigureCodec(CodecMimeType::kMimeTypeJPEG,
                                           create_param_);
  if (ret != 0) {
    ALOGE("JpegEncode:%s: Failed to configure Codec", __func__);
    return ret;
  }

  ret = AllocateBuffer(kPortIndexInput);
  if (ret != 0) {
    ALOGE("JpegEncode:%s: Failed to allocate buffer on PORT_NAME(%d)", __func__,
          kPortIndexInput);
    ReleaseBuffer();
    return ret;
  }
  ret = jpeg_avcodec_->RegisterInputBuffers(input_buffer_list_);
  if (ret != 0) {
    ALOGE("%s input buffers failed to register to AVCodec", __func__);
    return ret;
  }
  input_codec_src_ = make_shared<InputCodecSourceImpl>(buffer, this);
  if (input_codec_src_.get() == nullptr) {
    ALOGE("JpegEncode:%s: failed to create output source", __func__);
    return -ENOMEM;
  }

  ret = jpeg_avcodec_->AllocateBuffer(
      kPortIndexInput, 0, 0,
      static_pointer_cast<ICodecSource>(input_codec_src_), input_buffer_list_);
  if (ret != 0) {
    ALOGE("%s Failed to Call Allocate buffer on PORT_NAME(%d)", __func__,
          kPortIndexOutput);
    goto RELEASE;
  }

  ret = AllocateBuffer(kPortIndexOutput);
  if (ret != 0) {
    ALOGE("JpegEncode:%s: Failed to allocate buffer on PORT_NAME(%d)", __func__,
          kPortIndexOutput);
    goto RELEASE;
  }
  ret = jpeg_avcodec_->RegisterOutputBuffers(output_buffer_list_);
  if (ret != 0) {
    ALOGE("%s input buffers failed to register to AVCodec", __func__);
    return ret;
  }
  output_codec_src_ = make_shared<OutputCodecSourceImpl>(output_file_, this);
  if (output_codec_src_.get() == nullptr) {
    ALOGE("JpegEncode:%s: failed to create output source", __func__);
    return -ENOMEM;
  }

  ret = jpeg_avcodec_->AllocateBuffer(
      kPortIndexOutput, 0, 0,
      static_pointer_cast<ICodecSource>(output_codec_src_),
      output_buffer_list_);
  if (ret != 0) {
    ALOGE("%s Failed to Call Allocate buffer on PORT_NAME(%d)", __func__,
          kPortIndexOutput);
    goto RELEASE;
  }
  ALOGI("JpegEncode:%s: Exit", __func__);
  return ret;

RELEASE:
  ReleaseBuffer();
  jpeg_avcodec_->ReleaseBuffer();
  ALOGI("JpegEncode:%s: Exit", __func__);
  return ret;
}

int32_t JpegEncode::AllocateBuffer(const uint32_t index) {
  ALOGI("JpegEncode:%s: Enter", __func__);
  int32_t ret = 0;
  uint32_t count, size;
  ret = jpeg_avcodec_->GetBufferRequirements(index, &count, &size);
  assert(ion_device_ > 0);
  int32_t ionType = ION_HEAP(ION_IOMMU_HEAP_ID);

  struct ion_allocation_data alloc;
  struct ion_fd_data ionFdData;
  struct ion_handle_data ionHandleData;

  if (index == kPortIndexInput) {
    size = VENUS_Y_STRIDE(COLOR_FMT_NV12, create_param_.video_enc_param.width) *
      VENUS_Y_SCANLINES(COLOR_FMT_NV12, create_param_.video_enc_param.height) *
      3 / 2;
    BufferDescriptor buffer;
    memset(&buffer, 0x0, sizeof(buffer));
    memset(&alloc, 0x0, sizeof(ion_allocation_data));
    memset(&ionFdData, 0x0, sizeof(ion_fd_data));
    memset(&ionHandleData, 0x0, sizeof(ion_handle_data));

    ALOGE("Width:%d, height:%d, size:%d", create_param_.video_enc_param.width,
          create_param_.video_enc_param.height, size);
    alloc.len = size;
    alloc.len = (alloc.len + 4095) & (~4095);
    alloc.align = 4096;
    alloc.flags = ION_FLAG_CACHED;
    alloc.heap_id_mask = ionType;

    ret = ioctl(ion_device_, ION_IOC_ALLOC, &alloc);
    if (ret < 0) {
      ALOGE("JpegEncode:%s:Input-Port: ION allocation failed", __func__);
      goto ION_ALLOC_FAILED;
    }

    ionFdData.handle = alloc.handle;
    ionHandleData.handle =  alloc.handle;
    ret = ioctl(ion_device_, ION_IOC_SHARE, &ionFdData);
    if (ret < 0) {
      ALOGE("JpegEncode:%s:Input-Port: ION map failed %s", __func__,
            strerror(errno));
      goto ION_MAP_FAILED;
    }

    input_ion_handle_data_.insert({ionFdData.fd, ionHandleData});

    private_handle_t* meta_handle = new private_handle_t(
        static_cast<int>(ionFdData.fd), static_cast<unsigned int>(alloc.len),
        private_handle_t::PRIV_FLAGS_FRAMEBUFFER, 1,
        HAL_PIXEL_FORMAT_NV12_ENCODEABLE,
        VENUS_Y_STRIDE(COLOR_FMT_NV12, create_param_.video_enc_param.width),
        round_to(create_param_.video_enc_param.height, 32));
    if (meta_handle == nullptr) {
      ALOGE("JpegEncode:%s: failed to allocated metabuffer handle", __func__);
      return -ENOMEM;
    }

    ALOGI("JpegEncode:%s:  buffer native handle(%p)", __func__, meta_handle);

    meta_handle->unaligned_width = create_param_.video_enc_param.width;
    meta_handle->unaligned_height = create_param_.video_enc_param.height;

    ALOGI("JpegEncode:%s: fd = %d offset = %u size = %u width = %d "
        "height = %d unaligned_width = %d unaligned_height = %d",
        __func__, meta_handle->fd, meta_handle->offset, meta_handle->size,
        meta_handle->width, meta_handle->height, meta_handle->unaligned_width,
        meta_handle->unaligned_height);

    buffer.fd = ionFdData.fd;
    buffer.size = alloc.len;
    buffer.data = meta_handle;

    input_buffer_list_.push_back(buffer);
  } else {
    void* vaddr = nullptr;
    BufferDescriptor buffer;

    memset(&buffer, 0x0, sizeof(buffer));
    memset(&alloc, 0x0, sizeof(ion_allocation_data));
    memset(&ionFdData, 0x0, sizeof(ion_fd_data));
    memset(&ionHandleData, 0x0, sizeof(ion_handle_data));

    alloc.len = size;
    alloc.len = (alloc.len + 4095) & (~4095);
    alloc.align = 4096;
    alloc.flags = ION_FLAG_CACHED;
    alloc.heap_id_mask = ionType;

    ret = ioctl(ion_device_, ION_IOC_ALLOC, &alloc);
    if (ret < 0) {
      ALOGE("JpegEncode:%s:output-Port: ION allocation failed", __func__);
      goto ION_ALLOC_FAILED;
    }

    ionFdData.handle = alloc.handle;
    ionHandleData.handle = alloc.handle;
    ret = ioctl(ion_device_, ION_IOC_SHARE, &ionFdData);
    if (ret < 0) {
      ALOGE("JpegEncode:%s:output-Port: ION map failed %s", __func__,
            strerror(errno));
      goto ION_MAP_FAILED;
    }

    vaddr = mmap(nullptr, alloc.len, PROT_READ | PROT_WRITE, MAP_SHARED,
                 ionFdData.fd, 0);
    if (vaddr == MAP_FAILED) {
      ALOGE("JpegEncode:%s:output-Port: ION mmap failed: %s (%d)", __func__,
            strerror(errno), errno);
      goto ION_MAP_FAILED;
    }

    output_ion_handle_data_.insert({ionFdData.fd, ionHandleData});

    buffer.fd = ionFdData.fd;
    buffer.capacity = alloc.len;
    buffer.size = alloc.len;
    buffer.data = vaddr;

    ALOGI("JpegEncode:%s:output-Port: buffer.Fd(%d)", __func__, buffer.fd);
    ALOGI("JpegEncode:%s:output-Port: buffer.capacity(%d)", __func__,
          buffer.capacity);
    ALOGI("JpegEncode:%s:output-Port: buffer.vaddr(%p)", __func__, buffer.data);
    output_buffer_list_.push_back(buffer);
  }

  ALOGI("JpegEncode:%s: Exit", __func__);
  return ret;

ION_MAP_FAILED:
  memset(&ionHandleData, 0x0, sizeof(IonHandleData));
  ionHandleData.handle = ionFdData.handle;
  ioctl(ion_device_, ION_IOC_FREE, &ionHandleData);
ION_ALLOC_FAILED:
  close(ion_device_);
  ion_device_ = -1;
  ALOGE("JpegEncode:%s: ION Buffer allocation failed!", __func__);
  ALOGI("JpegEncode:%s: Exit", __func__);
  return -1;
}

void JpegEncode::ReleaseBuffer() {
  ALOGI("JpegEncode:%s: Enter ", __func__);
  assert(ion_device_ > 0);

  for (auto& iter : input_buffer_list_) {
    auto it = output_ion_handle_data_.find(iter.fd);
    private_handle_t* meta_handle =
        reinterpret_cast<private_handle_t*>((iter).data);
    if (meta_handle->fd) {
      close(meta_handle->fd);
      meta_handle->fd = -1;
      delete meta_handle;
      meta_handle = nullptr;
    }
    iter.data = nullptr;
    auto ret = ioctl(ion_device_, ION_IOC_FREE, &(it->second));
    if(ret < 0) {
      ALOGE("JpegEncode:%s: ION Free failed for Input-Port(%d) %s",
          __func__, errno, strerror(errno));
    } else {
      ALOGD("VideoDecode:%s: ION Free successful for Input-Port", __func__);
    }
  }
  input_buffer_list_.clear();
  input_ion_handle_data_.clear();

  for (auto& iter : output_buffer_list_) {
    if ((iter).data) {
      munmap((iter).data, (iter).capacity);
      (iter).data = nullptr;
    }
    if ((iter).fd > 0) {
      auto it = output_ion_handle_data_.find(iter.fd);
      close((iter).fd);
      (iter).fd = -1;
      auto ret = ioctl(ion_device_, ION_IOC_FREE, &(it->second));
      if(ret < 0) {
        ALOGE("JpegEncode:%s:Output-Port: ION Free failed for Output-Port(%d) %s",
            __func__, errno, strerror(errno));
      } else {
        ALOGD("JpegEncode:%s: ION Free successful for Output-Port", __func__);
      }
    }
  }
  output_buffer_list_.clear();
  output_ion_handle_data_.clear();

  ALOGI("JpegEncode:%s: Exit", __func__);
}

int32_t JpegEncode::StartEncode() {
  ALOGI("JpegEncode:%s: Enter", __func__);
  auto ret = jpeg_avcodec_->StartCodec();
  if (ret != 0) {
    ALOGE("JpegEncode:%s: Start jpeg encoding failed", __func__);
  }
  ALOGI("JpegEncode:%s: Exit", __func__);
  return ret;
}

JpegEncode::InputCodecSourceImpl::InputCodecSourceImpl(
    const qmmf::BufferDescriptor& yuv_buf, JpegEncode* const jpg)
    : ip_jpeg_(jpg) {
  ALOGI("JpegEncode:InputCodecSourceImpl:%s: Enter ", __func__);
  auto it = (ip_jpeg_->input_buffer_list_).begin();
  private_handle_t* meta = reinterpret_cast<private_handle_t*>(it->data);
  void* buf_vaadr = nullptr;
  buf_vaadr = mmap(nullptr, meta->size, PROT_READ | PROT_WRITE, MAP_SHARED,
                   meta->fd, 0);
  if (buf_vaadr == MAP_FAILED) {
    ALOGE("JpegEncode:InputCodecSourceImpl:%s: MAP_FAILED", __func__);
    assert(0);
  }
  memcpy(buf_vaadr, yuv_buf.data, meta->size);
  munmap(buf_vaadr, meta->size);

  ALOGV("JpegEncode:%s: fd = %d offset = %u size = %u width = %d height = %d ",
      __func__, meta->fd, meta->offset, meta->size, meta->width, meta->height);
  ALOGV("JpegEncode:InputCodecSourceImpl:%s: Buffer-desciptor: yuv-buffer: %s",
      __func__, yuv_buf.ToString().c_str());
  ALOGI("JpegEncode:InputCodecSourceImpl:%s: Exit ", __func__);
}

JpegEncode::InputCodecSourceImpl::~InputCodecSourceImpl() {
  ALOGI("JpegEncode:InputCodecSourceImpl:%s: Enter ", __func__);
  ALOGI("JpegEncode:InputCodecSourceImpl:%s: Exit ", __func__);
}

int32_t JpegEncode::InputCodecSourceImpl::GetBuffer(
    BufferDescriptor& stream_buffer, void* client_data) {
  ALOGI("JpegEncode:InputCodecSourceImpl:%s: Enter ", __func__);

  stream_buffer = *(ip_jpeg_->input_buffer_list_).begin();

  ALOGV("JpegEncode:InputCodecSourceImpl:%s: Buffer-descriptor: %s", __func__,
        stream_buffer.ToString().c_str());
  ALOGI("JpegEncode:InputCodecSourceImpl:%s: Exit ", __func__);
  return -1;
}

int32_t JpegEncode::InputCodecSourceImpl::ReturnBuffer(
    BufferDescriptor& stream_buffer, void* client_data) {
  ALOGI("JpegEncode:InputCodecSourceImpl:%s: Enter ", __func__);

  ALOGV("Videodecode:InputCodecSourceImpl:%s: Buffer-descriptor: %s", __func__,
        stream_buffer.ToString().c_str());
  vector<BufferDescriptor>::iterator it = ip_jpeg_->input_buffer_list_.begin();
  for (; it != ip_jpeg_->input_buffer_list_.end(); ++it) {
    if (it->data == stream_buffer.data) {
      munmap(it->data, it->capacity);
      it->data = nullptr;
      break;
    }
  }
  ALOGI("JpegEncode:InputCodecSourceImpl:%s: Exit ", __func__);
  return 0;
}

int32_t JpegEncode::InputCodecSourceImpl::NotifyPortEvent(
    PortEventType event_type, void* event_data) {
  ALOGI("JpegEncode:InputCodecSourceImpl:%s: Enter ", __func__);
  ALOGI("JpegEncode:InputCodecSourceImpl:%s: Exit ", __func__);
  return 0;
}

JpegEncode::OutputCodecSourceImpl::OutputCodecSourceImpl(string filename,
                                                         JpegEncode* const jpg)
    : op_jpeg_(jpg) {
  ALOGI("JpegEncode:OutputCodecSourceImpl:%s: Enter ", __func__);
  jpg_file_.open(filename.append(".jpg"), ios::out | ios::app | ios::binary);
  if (!jpg_file_.is_open()) {
    ALOGE("JpegEncode:OutputCodecSourceImpl:%s Failed to open o/p file",
          __func__);
    assert(0);
  }
  ALOGI("JpegEncode:OutputCodecSourceImpl:%s: Exit ", __func__);
}

JpegEncode::OutputCodecSourceImpl::~OutputCodecSourceImpl() {
  ALOGI("JpegEncode:OutputCodecSourceImpl:%s: Enter ", __func__);
  if (jpg_file_.is_open()) {
    jpg_file_.close();
  }
  ALOGI("JpegEncode:OutputCodecSourceImpl:%s: Exit ", __func__);
}

int32_t JpegEncode::OutputCodecSourceImpl::GetBuffer(
    BufferDescriptor& codec_buffer, void* client_data) {
  ALOGI("JpegEncode:OutputCodecSourceImpl:%s: Enter ", __func__);
  codec_buffer = *(op_jpeg_->output_buffer_list_).begin();
  ALOGI("JpegEncode:OutputCodecSourceImpl:%s: Exit ", __func__);
  return 0;
}

int32_t JpegEncode::OutputCodecSourceImpl::ReturnBuffer(
    BufferDescriptor& codec_buffer, void* client_data) {
  ALOGI("JpegEncode:OutputCodecSourceImpl:%s: Enter ", __func__);
  ALOGV("JpegEncode:OutputCodecSourceImpl:%s: Buffer-Descriptor:%s", __func__,
        codec_buffer.ToString().c_str());
  if (jpg_file_.is_open()) {
    DumpJpeg(codec_buffer);
  }

  vector<BufferDescriptor>::iterator it = op_jpeg_->output_buffer_list_.begin();
  for (; it != op_jpeg_->output_buffer_list_.end(); ++it) {
    if (it->data == codec_buffer.data) {
      munmap(it->data, it->capacity);
      it->data = nullptr;
      break;
    }
  }
  ALOGI("JpegEncode:OutputCodecSourceImpl:%s: Exit ", __func__);
  return 0;
}

int32_t JpegEncode::OutputCodecSourceImpl::NotifyPortEvent(
    PortEventType event_type, void* event_data) {
  ALOGI("JpegEncode:OutputCodecSourceImpl:%s: Enter ", __func__);
  ALOGI("JpegEncode:OutputCodecSourceImpl:%s: Exit ", __func__);
  return 0;
}

void JpegEncode::OutputCodecSourceImpl::DumpJpeg(
    const BufferDescriptor& codec_buffer) {
  ALOGI("JpegEncode:OutputCodecSourceImpl:%s: Enter ", __func__);

  int exp_size = static_cast<int>(codec_buffer.size);
  jpg_file_.write(static_cast<char*>(codec_buffer.data), exp_size);
  jpg_file_.close();

  ALOGI("JpegEncode:OutputCodecSourceImpl:%s: Exit ", __func__);
}

ScreenNail::ScreenNail(string filename)
    : vidc_(nullptr),
      jpeg_(nullptr),
      ion_device_(-1),
      flag_(0),
      input_width_(0),
      input_height_(0),
      rescale_width_(960),
      rescale_height_(720) {
  ALOGI("%s: Enter ", __func__);
  ion_device_ = open("/dev/ion", O_RDONLY);
  if (ion_device_ <= 0) {
    ALOGE("%s: Ion dev open failed %s", __func__, strerror(errno));
    ion_device_ = -1;
  }

  char prop_val[PROPERTY_VALUE_MAX];
  property_get(prop_enable_rgba.c_str(), prop_val, "0");
  if (atoi(prop_val) == 1) {
    string name = filename;
    rgba_file_.open(name.append(".rgba"), ios::out | ios::app);
    if (!rgba_file_.is_open()) {
      ALOGE("%s: Failed to open o/p rgba file", __func__);
      assert(0);
    }
  }

  property_get(prop_enable_dump_yuv.c_str(), prop_val, "0");
  if (atoi(prop_val) == 1) {
    yuv_file_.open(filename.append(".yuv"), ios::out | ios::app);
    if (!yuv_file_.is_open()) {
      ALOGE("%s: Failed to open o/p file", __func__);
      assert(0);
    }
  }

  property_get(prop_jpeg_width.c_str(), prop_val, "0");
  if (!(atoi(prop_val) == 0)) {
    rescale_width_ = atoi(prop_val);
  }

  property_get(prop_jpeg_height.c_str(), prop_val, "0");
  if (!(atoi(prop_val) == 0)) {
    rescale_height_ = atoi(prop_val);
   }
  ALOGI("%s: Exit", __func__);
}

ScreenNail::~ScreenNail() {
  ALOGI("%s: Enter ", __func__);
  if (jpeg_ != nullptr) jpeg_ = nullptr;
  if (vidc_ != nullptr) vidc_ = nullptr;
  close(ion_device_);
  ion_device_ = -1;
  if (rgba_file_.is_open()) {
    rgba_file_.close();
  }
  if (yuv_file_.is_open()) {
    yuv_file_.close();
  }
  if (flag_ == 1) {
    if (rescale_buffer_.data != nullptr) {
      free(rescale_buffer_.data);
      rescale_buffer_.data = nullptr;
    }
  }
  ALOGI("%s: Exit", __func__);
}

int32_t ScreenNail::Init(meta_info* info) {
  ALOGI("ScreenNail:%s: Enter for thread:%d", __func__, info->tid);

  CaptureCb cb;
  cb = [&](const BufferDescriptor& buffer, uint32_t width,
          uint32_t height) -> int32_t { return Capture(buffer, width, height); };

  vidc_ = make_shared<VideoDecode>(info->filename, cb, ion_device_);
  assert(vidc_ != nullptr);
  jpeg_ = make_shared<JpegEncode>(
      (info->filename).substr(0, info->filename.find(".")), ion_device_);
  assert(jpeg_ != nullptr);
  auto ret = vidc_->InitDemux();
  if (ret != 0) {
    ALOGE("%s: Video Decode Demux Initialisation failed for thread: %d",
          __func__, info->tid);
    return ret;
  }
  ret = vidc_->Init();
  if (ret != 0) {
    ALOGE("%s: Video Decode Initialisation failed for thread:%d", __func__,
          info->tid);
    return ret;
  }
  ret = vidc_->StartDecode();
  if (ret != 0) {
    ALOGE("%s: Video Decode start failed:%d", __func__, info->tid);
    return ret;
  }
  ALOGI("ScreenNail:%s: Exit for thread:%d", __func__, info->tid);
  return 0;
}

void ScreenNail::CheckStatus(int tid) {
  ALOGI("ScreenNail:%s: Enter for thread:%d", __func__, tid);
  int count = 0;
  int32_t ret = 0;
  while (count != 2) {
    if (jpeg_->GetEncodeInitStatus() == true) {
      ret = jpeg_->StartEncode();
      jpeg_->SetEncodeInit(false);
      if (ret != 0) {
        ALOGE("%s:Jpeg Encode Start failed for thread:%d", __func__, tid);
      }
      count++;
    }
    if (vidc_->GetStopDecodeStatus() == true) {
      ret = vidc_->StopDecode();
      if (ret != 0) {
        ALOGE("%s: Video Decode stop failed for thread:%d", __func__, tid);
      }
      vidc_->SetStopDecode(false);
      count++;
    }
  }
  ALOGI("ScreenNail:%s: Exit for thread:%d", __func__, tid);
}

int32_t ScreenNail::Capture(const BufferDescriptor& buffer,
                           uint32_t input_width, uint32_t input_height) {
  ALOGI("ScreenNail:%s: Enter ", __func__);
  input_width_ = input_width;
  input_height_ = input_height;
  memset(&rescale_buffer_, 0x0, sizeof(rescale_buffer_));
  int32_t status;
  if (input_width_ == rescale_width_ && input_height_ == rescale_height_) {
    rescale_buffer_ = buffer;
    flag_ = 0;
  } else {
    status = InitRescalar();
    if (status != 0) {
      ALOGE("ScreenNail:%s Failed to Initialize rescalar", __func__);
      assert(0);
    }
    rescale_buffer_.size = VENUS_Y_STRIDE(COLOR_FMT_NV12, rescale_width_) *
                           VENUS_Y_SCANLINES(COLOR_FMT_NV12, rescale_height_) *
                           3 / 2;
    rescale_buffer_.data = malloc(rescale_buffer_.size);
    status = Rescale(buffer, &rescale_buffer_);
    if (status != 0) {
      ALOGE("ScreenNail:%s: Failed to rescale", __func__);
      assert(0);
    }
    flag_ = 1;
  }

  auto ret = jpeg_->Init(rescale_buffer_);
  if (ret != 0) {
    ALOGE("ScreenNail:%s: Jpeg init failed", __func__);
    assert(0);
  }
  jpeg_->SetEncodeInit(true);

  if (yuv_file_.is_open()) {
    DumpYUV(rescale_buffer_);
  }

  if (rgba_file_.is_open()) {
    BufferDescriptor rgba_buffer;
    memset(&rgba_buffer, 0x0, sizeof(rgba_buffer));
    rgba_buffer.size = rescale_width_ * rescale_height_ * 4;
    rgba_buffer.data = malloc(rgba_buffer.size);
    ColorConvert(rescale_buffer_, &rgba_buffer);
    DumpRGBA(rgba_buffer);
    free(rgba_buffer.data);
    rgba_buffer.data = nullptr;
  }
  ALOGI("ScreenNail:%s: Exit", __func__);
  return 0;
}

int32_t ScreenNail::InitRescalar() {
  ALOGI("ScreenNail:%s: Enter ", __func__);
  char prop_val[PROPERTY_VALUE_MAX];
  auto mode = FASTCV_OP_LOW_POWER;
  property_get(prop_fastcv_mode.c_str(), prop_val, "0");
  if (atoi(prop_val) > 0) {
    switch (atoi(prop_val)) {
      case 1:
        mode = FASTCV_OP_PERFORMANCE;
        break;
      case 2:
        mode = FASTCV_OP_CPU_OFFLOAD;
        break;
      case 3:
        mode = FASTCV_OP_CPU_PERFORMANCE;
        break;
    }
  }
  int status = fcvSetOperationMode(mode);

  ALOGI("ScreenNail:%s: Set the fcvSetOperationMode", __func__);
  if (0 != status) {
    ALOGE("ScreenNail:%s: Unable to set FastCV operation mode", __func__);
    return -1;
  }
  ALOGI("ScreenNail:%s: Exit ", __func__);
  return 0;
}

int32_t ScreenNail::Rescale(const BufferDescriptor& in_buffer,
                            BufferDescriptor* out_buffer) {
  ALOGI("ScreenNail:%s: Enter ", __func__);
  uint8_t *in_buffer_y, *out_buffer_y;
  uint8_t *in_buffer_uv, *out_buffer_uv;
  size_t in_stride_y, out_stride_y;
  size_t in_scanline_y, out_scanline_y;
  size_t in_plane_y_len, out_plane_y_len;

  in_buffer_y = reinterpret_cast<uint8_t*>(in_buffer.data);

  in_stride_y = VENUS_Y_STRIDE(COLOR_FMT_NV12, input_width_);
  in_scanline_y = VENUS_Y_SCANLINES(COLOR_FMT_NV12, input_height_);
  in_plane_y_len = in_stride_y * in_scanline_y;

  in_buffer_uv = in_buffer_y + in_plane_y_len;

  out_buffer_y = reinterpret_cast<uint8_t*>(out_buffer->data);

  out_stride_y = VENUS_Y_STRIDE(COLOR_FMT_NV12, rescale_width_);
  out_scanline_y = VENUS_Y_SCANLINES(COLOR_FMT_NV12, rescale_height_);
  out_plane_y_len = out_stride_y * out_scanline_y;

  out_buffer_uv = out_buffer_y + out_plane_y_len;

  ALOGD("ScreenNail:%s: Calling fcvScaleu8_v2() for rescaling", __func__);
  auto start_time = high_resolution_clock::now();
  auto ret =
      fcvScaleu8_v2(in_buffer_y, input_width_, input_height_, in_stride_y,
                    out_buffer_y, rescale_width_, rescale_height_, out_stride_y,
                    FASTCV_INTERPOLATION_TYPE_AREA, FASTCV_BORDER_REPLICATE, 0);
  if (ret != 0) {
    ALOGE("ScreenNail:%s: fcvScaleu8_v2() failed", __func__);
    return ret;
  }

  ALOGD("ScreenNail:%s: Calling fcvScaleDownMNInterleaveu8() for rescaling",
        __func__);
  fcvScaleDownMNInterleaveu8(
      in_buffer_uv, input_width_ >> 1, input_height_ >> 1, in_stride_y,
      out_buffer_uv, rescale_width_ >> 1, rescale_height_ >> 1, out_stride_y);

  auto end_time = high_resolution_clock::now();

  auto diff = duration_cast<microseconds>(end_time - start_time).count();

  ALOGI("ScreenNail:%s: Time taken for rescale: %llu microseconds", __func__,
        diff);
  ALOGI("ScreenNail:%s: Exit ", __func__);
  return 0;
}

void ScreenNail::ColorConvert(const BufferDescriptor& in_buffer,
                              BufferDescriptor* out_buffer) {
  ALOGI("ScreenNail:%s: Enter", __func__);
  uint8_t *in_buffer_y, *out_buffer_rgba;
  uint8_t* in_buffer_uv;
  size_t in_stride_y, in_stride_uv, out_stride_y;
  size_t in_scanline_y;
  size_t in_plane_y_len;

  in_buffer_y = reinterpret_cast<uint8_t*>(in_buffer.data);

  in_stride_y = VENUS_Y_STRIDE(COLOR_FMT_NV12, rescale_width_);
  in_stride_uv = in_stride_y;
  in_scanline_y = VENUS_Y_SCANLINES(COLOR_FMT_NV12, rescale_height_);
  in_plane_y_len = in_stride_y * in_scanline_y;

  in_buffer_uv = in_buffer_y + in_plane_y_len;

  out_buffer_rgba = reinterpret_cast<uint8_t*>(out_buffer->data);

  out_stride_y = round_to(rescale_width_ * 4, 8);

  auto start_time = high_resolution_clock::now();

  fcvColorYCbCr420PseudoPlanarToRGBA8888u8(
      in_buffer_y, in_buffer_uv, rescale_width_, rescale_height_, in_stride_y,
      in_stride_uv, out_buffer_rgba, out_stride_y);

  auto end_time = high_resolution_clock::now();

  auto diff = duration_cast<microseconds>(end_time - start_time).count();

  ALOGI("ScreenNail:%s: Time taken for color conversion: %llu microseconds",
        __func__, diff);

  ALOGI("ScreenNail:%s: Exit", __func__);
}

void ScreenNail::DumpRGBA(const BufferDescriptor& codec_buffer) {
  ALOGI("ScreenNail:%s: Enter", __func__);

  ALOGV("ScreenNail:%s: Buffer-Descriptor:%s", __func__,
        codec_buffer.ToString().c_str());
  rgba_file_.write(static_cast<char*>(codec_buffer.data), codec_buffer.size);
  rgba_file_.close();

  ALOGI("ScreenNail:%s: Exit", __func__);
}

void ScreenNail::DumpYUV(const BufferDescriptor& codec_buffer) {
  ALOGI("ScreenNail:%s: Enter", __func__);

  ALOGV("ScreenNail:%s: Buffer-Descriptor:%s", __func__,
        codec_buffer.ToString().c_str());
  yuv_file_.write(static_cast<char*>(codec_buffer.data), codec_buffer.size);
  yuv_file_.close();

  ALOGI("ScreenNail:%s: Exit", __func__);
}


static void* Transcode(void* arg) {
  meta_info* info = static_cast<meta_info*>(arg);
  ALOGI("%s: Enter thread: %d", __func__, info->tid);
  ScreenNail* nail =
          new ScreenNail((info->filename).substr(0, info->filename.find(".")));
  auto ret = nail->Init(info);
  if( ret != 0) {
    ALOGE("ScreenNail:%s: ScreenNail init failed", __func__);
    assert(0);
  }
  nail->CheckStatus(info->tid);
  ALOGI("%s: Exit thread:%d", __func__, info->tid);
  return nullptr;
}

static bool IsFileFormatValid(const string& filename) {
  ALOGI("%s: Enter", __func__);
  if (filename.size() < 4) {
    ALOGE("%s:File is not having a proper name or is not an mp4 file",
          __func__);
    ALOGI("%s: Exit", __func__);
    return false;
  }
  if ((filename.compare(filename.size() - 4, string::npos, ".mp4") != 0) &&
      (filename.compare(filename.size() - 4, string::npos, ".MP4") != 0)) {
    ALOGE("%s:File is not mp4 file", __func__);
    ALOGI("%s: Exit", __func__);
    return false;
  }
  ALOGI("%s: Exit", __func__);
  return true;
}

static void ParseDir(const string& path, vector<string>* filenames) {
  ALOGI("%s: Enter", __func__);
  DIR* dp;
  int i = 0;
  struct dirent* ep;
  dp = opendir(path.c_str());
  ALOGV("%s: Directory path:%s", __func__, path.c_str());
  if (dp != nullptr) {
    while ((ep = readdir(dp)) != nullptr) {
      if (IsFileFormatValid(string(ep->d_name))) {
        string temp = path + string(ep->d_name);
        filenames->push_back(temp);
        ALOGV("%s: File name:%s", __func__, ep->d_name);
        ALOGV("%s: File name with complete path:%s", __func__, temp.c_str());
        i++;
      }
    }
    (void)closedir(dp);
  } else {
    perror("Couldn't open the directory");
    return;
  }
  ALOGI("%s: There's %d mp4 files in the current directory", __func__, i);

  ALOGI("%s: Exit", __func__);
  return;
}

int main(int argc, char* argv[]) {
  ALOGI("%s: Enter", __func__);
  if (argc == 1) {
    printf("Please provide a folder containing mp4 files as an input\n");
    printf("Usage: screen_nail <folder-containing-mp4-files>\n");
    return -1;
  } else if (argc > 2) {
    printf(
        "Number of arguments mismatch: Please provide one path (containing "
        "all mp4 files) as input\n");
    printf("Usage: screen_nail <folder-containing-mp4-files>\n");
    return -1;
  }

  vector<string> filenames;
  ParseDir(argv[1], &filenames);
  int num_threads = filenames.size();
  if (num_threads == 0) {
    printf("There are no mp4 files in the directory\n");
    return -1;
  }
  meta_info* info = new meta_info[num_threads];
  thread* thread_id = new thread[num_threads];
  for (int i = 0; i < num_threads; i++) {
    info[i].tid = i;
    info[i].filename = filenames[i];
    thread_id[i] = thread(Transcode, &info[i]);
  }

  for (int i = 0; i < num_threads; i++) {
    thread_id[i].join();
  }

  delete[] info;
  delete[] thread_id;
  filenames.clear();
  ALOGI("%s: Exit", __func__);
  return 0;
}