/*
* Copyright (c) 2016, 2019, The Linux Foundation. All rights reserved.
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

#define LOG_TAG "RecorderEncoderCore"

#include <memory>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <ion/ion.h>
#include <linux/dma-buf.h>
#include <linux/msm_ion.h>

#include "common/utils/qmmf_tools.h"
#include "recorder/src/service/qmmf_encoder_core.h"

namespace qmmf {

namespace recorder {

using ::std::make_shared;
using ::std::shared_ptr;
using ::std::vector;

static const int32_t kDebugTrackFps = 1<<0;

// kBitStreamHeaderSize is combined size of au delimiter,
// vps, sps, pps and frame start code size, as muxer
// changes each field start code.
#ifndef TARGET_ION_ABI_VERSION
static const uint32_t kBitStreamHeaderSize = 96;
#endif

const uint32_t TrackEncoder::kWaitNumFrames_ = 5; //frames
static const uint64_t kBufferWaitDuration = 5000000000; // 5 sec

EncoderCore* EncoderCore::instance_ = NULL;

EncoderCore* EncoderCore::CreateEncoderCore() {

  if(!instance_) {
    instance_ = new EncoderCore;
    if(!instance_) {
      QMMF_ERROR("%s: Can't Create EncoderCore Instance", __func__);
      return nullptr;
    }
  }
  QMMF_INFO("%s: EncoderCore Instance Created Successfully(0x%p)",
      __func__, instance_);
  return instance_;
}

EncoderCore::EncoderCore() : ion_device_(-1) {

  QMMF_GET_LOG_LEVEL();
  QMMF_KPI_GET_MASK();
  QMMF_KPI_DETAIL();
  QMMF_INFO("%s: Enter", __func__);
  QMMF_INFO("%s: Exit", __func__);
}

EncoderCore::~EncoderCore() {

  QMMF_INFO("%s: Enter", __func__);
  QMMF_KPI_DETAIL();
  if (!track_encoders_.empty()) {
    track_encoders_.clear();
  }
  instance_ = NULL;

  if (ion_device_ > 0) {
    ion_close(ion_device_);
    ion_device_ = -1;
  }
  QMMF_INFO("%s: Exit", __func__);
}

status_t EncoderCore::AddSource(const shared_ptr<TrackSource>& track_source,
                                VideoTrackParams& params) {

  QMMF_DEBUG("%s: Enter", __func__);
  assert(track_source.get() != nullptr);

  if(ion_device_ < 0) {
    QMMF_DEBUG("%s: Using Libion API", __func__);
    ion_device_ = ion_open();
    assert(ion_device_ >=0 );
  }

  shared_ptr<TrackEncoder> track_encoder =
      make_shared<TrackEncoder>(ion_device_);
  if (!track_encoder.get()) {
    QMMF_ERROR("%s: track_id(%x) Can't instantiate TrackEncoder",
        __func__, params.track_id);
    return NO_MEMORY;
  }

  auto ret = track_encoder->Init(track_source, track_encoder, params);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s: track_id(%x) TrackEncoder Init failed!", __func__,
        params.track_id);
    return BAD_VALUE;
  }

  std::lock_guard<std::mutex> l(encoder_list_lock_);
  track_encoders_.emplace(params.track_id, track_encoder);
  QMMF_INFO("%s: TrackEncoder(0x%p) for track_id(%x) Instantiated!",
      __func__, track_encoder.get(), params.track_id);

  QMMF_DEBUG("%s: Exit", __func__);
  return ret;
}

status_t EncoderCore::StartTrackEncoder(uint32_t track_id) {

  QMMF_DEBUG("%s: Enter track_id(%x)", __func__, track_id);
  QMMF_KPI_DETAIL();

  if (!isTrackValid(track_id)) {
    QMMF_ERROR("%s: Invalid track_id(%x)", __func__, track_id);
    return BAD_VALUE;
  }
  shared_ptr<TrackEncoder> track_encoder;
  {
    std::lock_guard<std::mutex> l(encoder_list_lock_);
    track_encoder = track_encoders_[track_id];
    assert(track_encoder.get() != NULL);
  }

  auto ret = track_encoder->Start();
  // Initial debug purpose.
  assert(ret == NO_ERROR);
  if (ret != NO_ERROR) {
    QMMF_INFO("%s: track_id(%x) TrackEncoder Start failed!", __func__,
      track_id);
    return ret;
  }

  QMMF_INFO("%s: track_id(%x) TrackEncoder Started Successfully!",
      __func__, track_id);
  QMMF_DEBUG("%s: Exit", __func__);
  return ret;
}

status_t EncoderCore::StopTrackEncoder(uint32_t track_id,
                                       bool is_force_cleanup) {

  QMMF_DEBUG("%s: Enter track_id(%x)", __func__, track_id);
  QMMF_KPI_DETAIL();

  if (!isTrackValid(track_id)) {
    QMMF_ERROR("%s: Invalid track_id(%x)", __func__, track_id);
    return BAD_VALUE;
  }
  shared_ptr<TrackEncoder> track_encoder;
  {
    std::lock_guard<std::mutex> l(encoder_list_lock_);
    track_encoder = track_encoders_[track_id];
    assert(track_encoder.get() != NULL);
  }

  auto ret = track_encoder->Stop(is_force_cleanup);
  // Initial debug purpose.
  assert(ret == NO_ERROR);
  if (ret != NO_ERROR) {
    QMMF_INFO("%s: track_id(%x) TrackEncoder Stop failed!", __func__,
      track_id);
    return ret;
  }

  QMMF_INFO("%s: track_id(%x) TrackEncoder Stopped Successfully!",
      __func__, track_id);
  QMMF_DEBUG("%s: Exit", __func__);
  return ret;
}

status_t EncoderCore::SetTrackEncoderParams(uint32_t track_id,
                                            CodecParamType param_type,
                                            void* param, uint32_t param_size) {

  QMMF_DEBUG("%s: Enter track_id(%x)", __func__, track_id);
  if (!isTrackValid(track_id)) {
    QMMF_ERROR("%s: Invalid track_id(%x)", __func__, track_id);
    return BAD_VALUE;
  }
  shared_ptr<TrackEncoder> track_encoder;
  {
    std::lock_guard<std::mutex> l(encoder_list_lock_);
    track_encoder = track_encoders_[track_id];
    assert(track_encoder.get() != NULL);
  }

  auto ret = track_encoder->SetParams(param_type, param, param_size);
  // Initial debug purpose.
  assert(ret == NO_ERROR);

  if (ret != NO_ERROR) {
    QMMF_INFO("%s: track_id(%x) TrackEncoder SetParams failed!",
        __func__, track_id);
    return ret;
  }

  QMMF_DEBUG("%s: Exit", __func__);
  return ret;
}

status_t EncoderCore::DeleteTrackEncoder(uint32_t track_id) {

  QMMF_DEBUG("%s: Enter track_id(%x)", __func__, track_id);
  QMMF_KPI_DETAIL();

  if (!isTrackValid(track_id)) {
    QMMF_ERROR("%s: Invalid track_id(%x)", __func__, track_id);
    return BAD_VALUE;
  }
  shared_ptr<TrackEncoder> track_encoder;
  {
    std::lock_guard<std::mutex> l(encoder_list_lock_);
    track_encoder = track_encoders_[track_id];
    assert(track_encoder.get() != nullptr);
  }

  auto ret = track_encoder->ReleaseHeaders();
  assert(ret == NO_ERROR);

  std::lock_guard<std::mutex> l(encoder_list_lock_);
  track_encoders_.erase(track_id);

  QMMF_INFO("%s: track_id(%x) TrackEncoder Deleted Successfully!",
      __func__, track_id);
  QMMF_DEBUG("%s: Exit", __func__);
  return NO_ERROR;

}

status_t EncoderCore::ReturnTrackBuffer(const uint32_t track_id,
                                        std::vector<BnBuffer> &buffers) {

  QMMF_DEBUG("%s: Enter track_id(%x)", __func__, track_id);

  if (!isTrackValid(track_id)) {
    QMMF_ERROR("%s: Invalid track_id(%x)", __func__, track_id);
    return BAD_VALUE;
  }
  shared_ptr<TrackEncoder> track_encoder;
  {
    std::lock_guard<std::mutex> l(encoder_list_lock_);
    track_encoder = track_encoders_[track_id];
    assert(track_encoder.get() != nullptr);
  }
  // Return buffer back to track encoder's output bitstream buffer queue.
  auto ret = track_encoder->OnBufferReturnFromClient(buffers);

  QMMF_DEBUG("%s: Exit track_id(%x)", __func__, track_id);
  return ret;
}

bool EncoderCore::isTrackValid(uint32_t track_id) {

  std::lock_guard<std::mutex> l(encoder_list_lock_);
  QMMF_DEBUG("%s: Number of Tracks exist = %d",__func__,
      track_encoders_.size());
  return track_encoders_.count(track_id) != 0 ? true : false;
}

status_t EncoderCore::FlushTrack(uint32_t track_id) {

  QMMF_DEBUG("%s: Enter track_id(%x)", __func__, track_id);

  if (!isTrackValid(track_id)) {
    QMMF_ERROR("%s: Invalid track_id(%x)", __func__, track_id);
    return NAME_NOT_FOUND;
  }
  shared_ptr<TrackEncoder> track_encoder;
  {
    std::lock_guard<std::mutex> l(encoder_list_lock_);
    track_encoder = track_encoders_[track_id];
    assert(track_encoder.get() != nullptr);
  }
  auto ret = track_encoder->Flush();

  QMMF_DEBUG("%s: Exit track_id(%x)", __func__, track_id);
  return ret;
}

TrackEncoder::TrackEncoder(int32_t ion_device)
    : ion_device_(ion_device),
      eos_atoutput_(false),
      is_force_cleanup_(false),
      num_bytes_(0),
      prevtv_{0, 0},
      count_(0),
      wait_duration_(kBufferWaitDuration) {

  QMMF_GET_LOG_LEVEL();
  QMMF_INFO("%s: Enter", __func__);

  char prop_val[PROPERTY_VALUE_MAX];
  property_get(PROP_DEBUG_FPS, prop_val, "1");
  debug_fps_ = atoi(prop_val);

  track_params_ = {};
  QMMF_INFO("%s: Exit (0x%p)", __func__, this);
}

TrackEncoder::~TrackEncoder() {

  QMMF_INFO("%s: Enter track_id(%x)", __func__, TrackId());

  int i = 0;
  for(auto& iter : output_buffer_list_) {
    if ((iter).data) {
      SyncEnd((iter).fd);
      munmap((iter).data, (iter).capacity);
      (iter).data = NULL;
    }

    if ((iter).fd) {
      QMMF_INFO("%s track_id(%x) (iter).fd =%d Free", __func__, TrackId(),
                (iter).fd);
      if (ion_is_legacy(ion_device_))
        ion_free(ion_device_, fd_ion_handle_map_[(iter).fd]);

      close((iter).fd);
      (iter).fd = 0;
    }
    ++i;
  }
  output_buffer_list_.clear();
  if(avcodec_ != nullptr) {
    delete avcodec_;
  }
  QMMF_INFO("%s: Exit (0x%p)", __func__, this);
}

status_t TrackEncoder::Init(const shared_ptr<TrackSource>& track_source,
                            const shared_ptr<TrackEncoder>& track_encoder,
                            VideoTrackParams& track_params) {

  QMMF_INFO("%s: Enter track_id(%x)", __func__, track_params.track_id);
  track_params_ = track_params;
  if (track_params.params.format_type == VideoFormat::kJPEG) {
#ifndef DISABLE_PP_JPEG
    avcodec_ = new JPEGEncoder();
#else
    QMMF_ERROR("%s: JPEG Postproc not supported", __func__);
    return -EINVAL;
#endif
  } else {
    avcodec_ = new AVCodec();
  }
  if(avcodec_ == nullptr) {
    QMMF_ERROR("%s: track_id(%x) AVCodec failed", __func__,
        track_params.track_id);
    return NO_MEMORY;
  }

  CodecParam codec_param{};

  codec_param.video_enc_param = track_params.params;

  QMMF_INFO("%s: track_id(%x) W(%d) H(%d) format_type(%d)", __func__,
      track_params.track_id, track_params.params.width,
      track_params.params.height, track_params.params.format_type);
  auto ret = 0;
  CodecMimeType mime_type =
      (track_params.params.format_type == VideoFormat::kJPEG)
          ? CodecMimeType::kMimeTypeJPEG
          : CodecMimeType::kMimeTypeVideoEncAVC;

  ret = avcodec_->ConfigureCodec(mime_type, codec_param);
  assert(ret == NO_ERROR);
  if(ret != NO_ERROR) {
    QMMF_ERROR("%s track_id(%x) Failed to configure AVCodec!", __func__,
        track_params.track_id);
    return ret;
  }

  // TODO: Modify UseBuffer Api to take sp pointer as a reference.
  vector<BufferDescriptor> dummy_list;
  ret = avcodec_->AllocateBuffer(kPortIndexInput, 0, 0,
                                 shared_ptr<ICodecSource>(track_source),
                                 dummy_list);
  assert(ret == NO_ERROR);
  if(ret != NO_ERROR) {
    QMMF_ERROR("%s track_id(%x) AllocateBuffer Failed at input port!",
        __func__, track_params.track_id);
    return ret;
  }

  //Output port configuration
  ret = AllocOutputPortBufs();
  assert(ret == NO_ERROR);
  if(ret != NO_ERROR) {
    QMMF_ERROR("%s track_id(%x) output buffer allocation failed!!",
        __func__, track_params.track_id);
    return ret;
  }

  ret = avcodec_->RegisterOutputBuffers(output_buffer_list_);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s track_id(%x) output buffers failed to register to AVCodec",
               __func__, track_params.track_id);
    return ret;
  }

  ret = avcodec_->AllocateBuffer(kPortIndexOutput, 0, 0,
                                 shared_ptr<ICodecSource>(track_encoder),
                                 dummy_list);
  if(ret != NO_ERROR) {
    QMMF_ERROR("%s track_id(%x) AllocateBuffer Failed at output port!",
        __func__, track_params.track_id);
    // TODO: Deallocate ouput port buffers.
    //ReleaseBuffer();
    return ret;
  }

  QMMF_INFO("%s: track_id(%x) AVCodec(0x%p) Instantiated!" , __func__,
      track_params.track_id, avcodec_);

  for (auto& buffer : output_buffer_list_) {
    QMMF_INFO("%s: track_id(%x) Adding buffer fd(%d) to "
        "output_free_buffer_queue list",  __func__, track_params.track_id,
        buffer.fd);
    std::lock_guard<std::mutex> lock(queue_lock_);
    output_free_buffer_queue_.push(buffer);
  }

#ifdef DUMP_BITSTREAM

  VideoFormat fmt_type = track_params.params.format_type;
  const char* type_string = (fmt_type == VideoFormat::kAVC) ? "h264" : "h265";
  std::string extension(type_string);
  std::string bitstream_filepath(FRAME_DUMP_PATH);
  bitstream_filepath += "/track_enc_";
  bitstream_filepath += std::to_string(track_params.track_id) + ".";
  bitstream_filepath += type_string;
  file_fd_ = open(bitstream_filepath.c_str(), O_CREAT | O_WRONLY | O_TRUNC,
       0655);
#endif

  auto output_frame_interval = 1000000.0 / track_params_.params.frame_rate;
  auto wait = output_frame_interval * 1000 * kWaitNumFrames_;
  wait_duration_ = wait < kBufferWaitDuration ? kBufferWaitDuration : wait;
  QMMF_INFO("%s: track_id(%x) wait_duration_:(%lld) ns",
      __func__, TrackId(), wait_duration_);

  QMMF_INFO("%s: Exit", __func__);
  return ret;
}

status_t TrackEncoder::Start() {

  QMMF_INFO("%s: Enter track_id(%x)", __func__, TrackId());
  std::lock_guard<std::mutex> lock(lock_);

  assert(avcodec_ != nullptr);
  auto ret = avcodec_->StartCodec(true);
  // Initial debug purpose.
  assert(ret == NO_ERROR);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s: track_id(%x) StartCodec failed!", __func__,
        TrackId());
    return ret;
  }

  eos_atoutput_ = false;

  QMMF_INFO("%s: Exit track_id(%x)", __func__, TrackId());
  return ret;
}

status_t TrackEncoder::Stop(bool is_force_cleanup) {

  QMMF_INFO("%s: Enter track_id(%x)", __func__, TrackId());
  std::lock_guard<std::mutex> lock(lock_);

  if (is_force_cleanup) {
    QMMF_INFO("%s track_id(%x) Force cleanup", __func__, TrackId());
    std::lock_guard<std::mutex> lock(queue_lock_);
    is_force_cleanup_ = true;

    for (auto& buffer : output_occupy_buffer_queue_) {
      output_free_buffer_queue_.push(buffer);
      wait_for_frame_.Signal();
    }
    output_occupy_buffer_queue_.clear();
  }
  assert(avcodec_ != nullptr);
  auto ret = avcodec_->StopCodec(true);
  // Initial debug purpose.
  assert(ret == NO_ERROR);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s: track_id(%x) StopCodec failed!", __func__, TrackId());
    return ret;
  }

  QMMF_INFO("%s: Exit track_id(%x)", __func__, TrackId());
  return ret;
}

status_t TrackEncoder::SetParams(CodecParamType param_type, void* param,
                                 uint32_t param_size) {

  QMMF_INFO("%s: Enter track_id(%x)", __func__, TrackId());
  assert(avcodec_ != nullptr);
  auto ret = avcodec_->SetParameters(param_type, param, param_size);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s: set parameter failed for track(%x)", __func__,
        TrackId());
  }

  if (param_type == CodecParamType::kFrameRateType) {
    float fps = *(static_cast<float*>(param));
    auto output_frame_interval = 1000000.0 / fps;
    std::lock_guard<std::mutex> autoLock(wait_duration_lock_);
    auto wait = output_frame_interval * 1000 * kWaitNumFrames_;
    wait_duration_ = wait < kBufferWaitDuration ? kBufferWaitDuration : wait;
    QMMF_INFO("%s: track_id(%x) wait_duration_:(%lld) ns",
        __func__, TrackId(), wait_duration_);
  }

  QMMF_INFO("%s: Exit track_id(%x)", __func__, TrackId());
  return ret;
}


status_t TrackEncoder::ReleaseHeaders() {

  QMMF_INFO("%s: Enter track_id(%x)", __func__, TrackId());
  assert(avcodec_ != nullptr);
  auto ret = avcodec_->ReleaseBuffer();
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s: ReleaseBuffer failed!", __func__);
  }
  QMMF_INFO("%s: Exit track_id(%x)", __func__, TrackId());
  return ret;
}
#ifndef TARGET_ION_ABI_VERSION
status_t TrackEncoder::SynchronizeCache(
    const ion_user_handle_t& ion_handle,
    const BufferDescriptor& buffer,
    const unsigned int flag) {
  QMMF_DEBUG("%s Enter track_id(%d)", __func__, TrackId());

  struct ion_flush_data flush_data;
  struct ion_custom_data custom_data;

  memset(&flush_data, 0x0, sizeof(flush_data));
  memset(&custom_data, 0x0, sizeof(custom_data));

  flush_data.vaddr = buffer.data;
  flush_data.fd = buffer.fd;
  flush_data.handle = ion_handle;
  flush_data.length = kBitStreamHeaderSize;
  custom_data.cmd = flag;
  custom_data.arg = reinterpret_cast<unsigned long>(&flush_data);
  QMMF_DEBUG("Cache %s: fd=%d handle=%d va=%p size=%d",
      (flag == ION_IOC_CLEAN_CACHES) ? "Clean" : "Invalidate", flush_data.fd,
      flush_data.handle, flush_data.vaddr, flush_data.length);
  auto ret = ioctl(ion_device_, ION_IOC_CUSTOM, &custom_data);
  if (ret < 0) {
    QMMF_ERROR("%s Cache %s failed", __func__,
        (flag == ION_IOC_CLEAN_CACHES) ? "Clean" : "Invalidate");
    return ret;
  }
  QMMF_DEBUG("%s Exit track_id(%d)", __func__, TrackId());
  return NO_ERROR;
}
#endif
status_t TrackEncoder::GetBuffer(BufferDescriptor& codec_buffer,
                                 void* client_data) {

  QMMF_DEBUG("%s: Enter track_id(%x)", __func__, TrackId());

  std::unique_lock<std::mutex> lk(queue_lock_);
  std::chrono::nanoseconds wait_time(GetWaitTime());

  // Give available free buffer to encoder to use on output port.
  while (output_free_buffer_queue_.empty()) {
    QMMF_DEBUG("%s: track_id(%x) No buffer available to notify,"
        " Wait for new buffer",  __func__, TrackId());

    auto ret = wait_for_frame_.WaitFor(lk, wait_time);
    if (ret != 0) {
      QMMF_ERROR("%s: track_id(%x) wait buffer timedout!",  __func__, TrackId());
      return TIMED_OUT;
    }
  }
  BufferDescriptor buffer = output_free_buffer_queue_.front();
  lk.unlock();

#ifndef TARGET_ION_ABI_VERSION
  auto ret = SynchronizeCache(fd_ion_handle_map_[buffer.fd], buffer,
                              ION_IOC_CLEAN_CACHES);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s: Cache Synchronization failed, error(%d)!", __func__, ret);
    return ret;
  }
#endif
  codec_buffer.fd = buffer.fd;
  codec_buffer.data = buffer.data;
  {
    std::lock_guard<std::mutex> lock(queue_lock_);
    output_occupy_buffer_queue_.push_back(buffer);
    output_free_buffer_queue_.pop();
  }

  QMMF_DEBUG("%s track_id(%x) Sending buffer(0x%p) fd(%d) for FTB",
      __func__, TrackId(), codec_buffer.data, codec_buffer.fd);

  QMMF_DEBUG("%s: Exit track_id(%x)", __func__, TrackId());
  return NO_ERROR;
}

status_t TrackEncoder::ReturnBuffer(BufferDescriptor& codec_buffer,
                                    void* client_data) {

  QMMF_DEBUG("%s: Enter track_id(%x)", __func__, TrackId());
  assert(codec_buffer.data != NULL);

  QMMF_VERBOSE("%s: track_id(%x) Received buffer(0x%p) from FBD",
      __func__, TrackId(), codec_buffer.data);

  // When the encoder flush is in progress then it will return buffers
  // as is i.e. with timestamp and size 0. If that is the case then
  // it is not a valid buffer and should not be notified to the client
  bool is_buffer_invalid = (codec_buffer.size == 0 ||
                            codec_buffer.timestamp == 0);

  if ((debug_fps_ & kDebugTrackFps) && !is_buffer_invalid) {
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    uint64_t time_diff = (uint64_t)((tv.tv_sec * 1000000 + tv.tv_usec) -
        (prevtv_.tv_sec * 1000000 + prevtv_.tv_usec));

    size_t size = codec_buffer.size;
    num_bytes_ += size;

    count_++;
    if (time_diff >= FPS_TIME_INTERVAL) {
      bool is_first_time = (prevtv_.tv_sec == 0 && prevtv_.tv_usec == 0);
      if (!is_first_time) {
        float framerate = (count_ * 1000000) / (float)time_diff;
        uint32_t bitrate = (num_bytes_ * 8/count_) * framerate;
        QMMF_INFO(" %s: track_id(%x): encoded fps: = %0.2f bitrate=%d",
                  __func__, TrackId(), framerate, bitrate);
      }
      prevtv_ = tv;
      count_ = 0;
      num_bytes_ = 0;
    }
  }

#ifdef DUMP_BITSTREAM
  DumpBitStream(codec_buffer);
#endif

#ifdef DONT_NOTIFY
  // This change is only for debug purpose, it will circulate buffers without
  // sending/mapping them to another process.
  std::list<BufferDescriptor>::iterator it =
      output_occupy_buffer_queue_.Begin();
  bool found = false;
  for (; it != output_occupy_buffer_queue_.End(); ++it) {
    QMMF_VERBOSE("%s track_id(%x) Checking match (0x%p)vs(0x%p) ",
        __func__, TrackId(), (*it).data,  codec_buffer.data);
    if (((*it).data) == (codec_buffer.data)) {
      QMMF_VERBOSE("%s track_id(%x) Buffer found", __func__, TrackId());
      output_free_buffer_queue_.PushBack(*it);
      output_occupy_buffer_queue_.Erase(it);
      wait_for_frame_.Signal();
      found = true;
      break;
    }
  }
  assert(found == true);
#else
  if (eos_atoutput_ == true || is_buffer_invalid) {
    //  If EOS or the buffer is invalid on output port then don't notify
    //  buffers to application, simply remove the buffer from output queue
    //  in input queue, note last buffer with EOS is already notified to
    //  application before setting eos_atoutput_ to true.
    {
      std::lock_guard<std::mutex> lock(queue_lock_);
      for (size_t idx = 0; idx < output_occupy_buffer_queue_.size(); ++idx) {
        BufferDescriptor& buffer = output_occupy_buffer_queue_[idx];

        if (buffer.data == codec_buffer.data) {
          if (eos_atoutput_) {
            QMMF_INFO("%s: track_id(%x) EOS is already done! moving buffer from"
                " Out to In queue!",  __func__, TrackId());
          } else {
            QMMF_INFO("%s: track_id(%x) Invalid buffer check if encoder is"
                "flushing! moving buffer from  Out to In queue!",
                __func__, TrackId());
          }
          output_free_buffer_queue_.push(buffer);
          output_occupy_buffer_queue_.erase(
              output_occupy_buffer_queue_.begin() + idx);
          break;
        }
      }
    }
  } else {
    NotifyBufferToClient(codec_buffer);
  }
#endif

  QMMF_DEBUG("%s: Exit track_id(%x)", __func__, TrackId());
  return NO_ERROR;
}

status_t TrackEncoder::NotifyPortEvent(PortEventType event_type,
                                       void* event_data) {

  QMMF_DEBUG("%s Enter", __func__);
  QMMF_DEBUG("%s Exit", __func__);
  return 0;
}

status_t TrackEncoder::OnBufferReturnFromClient(std::vector<BnBuffer>
                                                &bn_buffers) {

  QMMF_DEBUG("%s: Enter track_id(%x)", __func__, TrackId());

  //Buffer came back from client, now put this buffer in free queue.
  QMMF_DEBUG("%s: track_id(%x) Number of buffers(%d) returned from client",
      __func__, TrackId(), bn_buffers.size());

  std::lock_guard<std::mutex> lock(queue_lock_);
  if (is_force_cleanup_) {
    QMMF_DEBUG("%s: Force cleanup is triggered! Output occupy queue "
        "has been cleaned!",  __func__);
    return NO_ERROR;
  }

  for (auto& bn_buffer : bn_buffers) {
    bool match = false;
    QMMF_DEBUG("%s track_id(%x) output_occupy_buffer_queue_.size(%d)",
        __func__, TrackId(), output_occupy_buffer_queue_.size());

    for (size_t idx = 0; idx < output_occupy_buffer_queue_.size(); ++idx) {
      BufferDescriptor& buffer = output_occupy_buffer_queue_[idx];

      if (buffer.fd == static_cast<int32_t>(bn_buffer.buffer_id)) {
        QMMF_DEBUG("%s: track_id(%x) buffer_id(%d) found in list",
            __func__, TrackId(), bn_buffer.buffer_id);

        // Move buffer to free queue, and signal AVCodec's output thread if it
        // is waiting for buffer.
        output_free_buffer_queue_.push(buffer);
        // Erase buffer from occupy queue.
        output_occupy_buffer_queue_.erase(
            output_occupy_buffer_queue_.begin() + idx);

        wait_for_frame_.Signal();
        match = true;
        break;
      }
    }

    // Make sure all buffers are part of occupy queue.
    if (match == true) {
      QMMF_DEBUG("%s track_id(%x) output_occupy_buffer_queue_.size(%d)",
          __func__, TrackId(), output_occupy_buffer_queue_.size());
      QMMF_DEBUG("%s track_id(%x) output_free_buffer_queue_.size(%d)",
          __func__, TrackId(), output_free_buffer_queue_.size());
    } else {
      QMMF_ERROR("%s: track_id(%x) buffer_id(%d) not found in list",
          __func__, TrackId(), bn_buffer.buffer_id);
      return FAILED_TRANSACTION;
    }
  }
  QMMF_DEBUG("%s: Exit track_id(%x)", __func__, TrackId());
  return NO_ERROR;
}

void TrackEncoder::NotifyBufferToClient(BufferDescriptor& codec_buffer) {

  QMMF_DEBUG("%s: Enter track_id(%x)", __func__, TrackId());
  assert(track_params_.data_cb != nullptr);

  bool found = false;
  BnBuffer bn_buffer{};

  if(codec_buffer.flag & static_cast<uint32_t>(BufferFlags::kFlagEOS)) {
    eos_atoutput_ = true;
    QMMF_INFO("%s: EOS is received for track(%x)", __func__, TrackId());
  }

  {
    std::lock_guard<std::mutex> lock(queue_lock_);
    if (is_force_cleanup_) {
      QMMF_WARN("%s: Force cleanup is triggered! Client may not exist!",
          __func__);
      return;
    }
    for (auto& buffer : output_occupy_buffer_queue_) {
      QMMF_VERBOSE("%s: track_id(%x) Checking match (0x%p) vs (0x%p) ",
          __func__, TrackId(), buffer.data,  codec_buffer.data);

      if (buffer.data == codec_buffer.data) {
        QMMF_VERBOSE("%s: track_id(%x) fd(%d):size(%d):timestamp(%lld):"
            "capacity(%d)",  __func__, TrackId(), buffer.fd,
            codec_buffer.size, codec_buffer.timestamp, buffer.capacity);
        bn_buffer.ion_fd      = buffer.fd;
        bn_buffer.ion_meta_fd = -1;
        bn_buffer.size        = codec_buffer.size;
        bn_buffer.timestamp   = codec_buffer.timestamp;
        bn_buffer.width       = -1;
        bn_buffer.height      = -1;
        bn_buffer.buffer_id   = buffer.fd;
        bn_buffer.flag        = codec_buffer.flag;
        bn_buffer.capacity    = buffer.capacity;
        found = true;
        break;
      }
    }
  }
  assert(found == true);
  std::vector<BnBuffer> bn_buffers;
  bn_buffers.push_back(bn_buffer);

  MetaData meta_data{};
  meta_data.meta_flag = static_cast<uint32_t>(MetaParamType::kVideoFrameType);

  if (codec_buffer.flag & static_cast<uint32_t>(BufferFlags::kFlagIDRFrame))
    meta_data.video_frame_type_info |=
        static_cast<uint32_t>(BufferFlags::kFlagIDRFrame);

  if (codec_buffer.flag & static_cast<uint32_t>(BufferFlags::kFlagIFrame))
    meta_data.video_frame_type_info |=
        static_cast<uint32_t>(BufferFlags::kFlagIFrame);

  if (codec_buffer.flag & static_cast<uint32_t>(BufferFlags::kFlagPFrame))
    meta_data.video_frame_type_info =
        static_cast<uint32_t>(BufferFlags::kFlagPFrame);

  if (codec_buffer.flag & static_cast<uint32_t>(BufferFlags::kFlagBFrame))
    meta_data.video_frame_type_info =
        static_cast<uint32_t>(BufferFlags::kFlagBFrame);

  QMMF_VERBOSE("%s: Video Frame Type: %u\n", __func__,
      static_cast<uint32_t>(meta_data.video_frame_type_info));

  std::vector<MetaData> meta_buffers;
  meta_buffers.push_back(meta_data);

  track_params_.data_cb(bn_buffers, meta_buffers);

  QMMF_DEBUG("%s: Exit track_id(%x)", __func__, TrackId());

}

status_t TrackEncoder::AllocOutputPortBufs() {
  QMMF_INFO("%s: Enter track_id(%x)", __func__, TrackId());
  int32_t ret = 0;
  uint32_t count, size, count_prev;

  assert(avcodec_ != nullptr);
  ret = avcodec_->GetBufferRequirements(kPortIndexOutput, &count, &size);
  assert(ret == NO_ERROR);
  count_prev = count;
  // TODO: This hardcoding would be fixed by AVCodec layer
  count = OUTPUT_MAX_COUNT;

  // This Code changes buffer count in slice delivery mode.
  if (track_params_.params.format_type == VideoFormat::kAVC) {
    if (track_params_.params.codec_param.avc.slice_enabled) {
      count = count_prev;
    }
  }

  assert(ion_device_ >= 0);
  void* vaddr = NULL;
  uint32_t flags = ION_FLAG_CACHED;
  uint32_t heap_id_mask = ION_HEAP(ION_SYSTEM_HEAP_ID);

  for (uint32_t i = 0; i < count; i++) {
    BufferDescriptor buffer{};
    int32_t ion_handle = -1;
    size = (size + 4095) & (~4095);
    vaddr = NULL;

    if (!ion_is_legacy(ion_device_)) {
      ret = ion_alloc_fd(ion_device_, size, 0, heap_id_mask, flags, &buffer.fd);
      if (ret) {
        QMMF_ERROR("%s() ion_alloc_fd  command failed: %d[%s]", __func__, errno,
                   strerror(errno));
        return errno;
      }
    } else {
      ret = ion_alloc(ion_device_, size, 0, heap_id_mask, flags, &ion_handle);
      if (ret) {
        QMMF_ERROR("%s() ion_alloc  command failed: %d[%s]", __func__, errno,
                   strerror(errno));
        return errno;
      }
      ret = ion_share(ion_device_, ion_handle, &buffer.fd);
      if (ret) {
        QMMF_ERROR("%s() ion_share  command failed: %d[%s]", __func__, errno,
                   strerror(errno));
        return errno;
      }

      fd_ion_handle_map_.insert(::std::make_pair(buffer.fd, ion_handle));
    }

    vaddr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, buffer.fd, 0);
    if (vaddr == MAP_FAILED) {
      QMMF_ERROR("%s() unable to map buffer[%d]: %d[%s]", __func__, buffer.fd,
                 errno, strerror(errno));

      ret = close(buffer.fd);
      if (ret < 0) {
        QMMF_ERROR("%s() error closing mapping fd[%d]: %d[%s]", __func__,
                   buffer.fd, errno, strerror(errno));
        QMMF_ERROR("%s() [CRITICAL] ion fd has leaked", __func__);
      }
      return errno;
    }
    SyncStart(buffer.fd);

    buffer.capacity = size;
    buffer.data = vaddr;

    QMMF_INFO("%s buffer.Fd(%d)", __func__, buffer.fd);
    QMMF_DEBUG("%s buffer.capacity(%d)", __func__, buffer.capacity);
    QMMF_DEBUG("%s buffer.vaddr(%p)", __func__, buffer.data);

    output_buffer_list_.push_back(buffer);
  }

  QMMF_INFO("%s: Exit track_id(%x)", __func__, TrackId());
  return ret;
}

status_t TrackEncoder::Flush(){

  QMMF_DEBUG("%s: Enter track_id(%x)", __func__, TrackId());

  if (nullptr == avcodec_) {
    QMMF_ERROR("%s: AVCodec hasn't been initilized!", __func__);
    return NO_INIT;
  }
  auto ret = avcodec_->FlushCodec(kPortIndexInput);

  QMMF_DEBUG("%s: Enter track_id(%x)", __func__, TrackId());
  return ret;
}

#ifdef DUMP_BITSTREAM
void TrackEncoder::DumpBitStream(BufferDescriptor& codec_buffer) {

  QMMF_VERBOSE("%s: Enter track_id(%x)", __func__, TrackId());
  if(eos_atoutput_) {
    return;
  }
  if(file_fd_ > 0) {
    ssize_t exp_size = (ssize_t) codec_buffer.size;
    QMMF_INFO("%s Got encoded buffer of size(%d)", __func__,
        codec_buffer.size);

    if (exp_size != write(file_fd_, codec_buffer.data, codec_buffer.size)) {
      QMMF_INFO("Bad Write error (%d) %s", __func__, errno,
          strerror(errno));
      close(file_fd_);
      file_fd_ = -1;
    }
  } else {
    QMMF_ERROR("%s File is not open fd = %d", __func__, file_fd_);
  }

  if (codec_buffer.flag & static_cast<uint32_t>(BufferFlags::kFlagEOS)) {
    QMMF_ERROR("%s This is last buffer from encoder.close file",
        __func__);

    close(file_fd_);
    file_fd_ = -1;
  }
  QMMF_VERBOSE("%s: Enter track_id(%x)", __func__, TrackId());
}
#endif

uint64_t TrackEncoder::GetWaitTime(){
  std::lock_guard<std::mutex> l(wait_duration_lock_);
  return wait_duration_;
}

};  // namespace recorder

};  // namespace qmmf
