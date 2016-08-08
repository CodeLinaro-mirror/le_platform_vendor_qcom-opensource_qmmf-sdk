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

#define TAG "RecorderEncoderCore"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/msm_ion.h>

#include "recorder/src/service/qmmf_encoder_core.h"

namespace qmmf {

namespace recorder {

EncoderCore* EncoderCore::instance_ = NULL;

EncoderCore* EncoderCore::CreateEncoderCore() {

  if(!instance_) {
    instance_ = new EncoderCore;
    if(!instance_) {
      QMMF_ERROR("%s:%s: Can't Create EncoderCore Instance", TAG, __func__);
      return nullptr;
    }
  }
  QMMF_INFO("%s:%s: EncoderCore Instance Created Successfully(0x%x)", TAG,
      __func__, instance_);
  return instance_;
}

EncoderCore::EncoderCore() : ion_device_(-1) {

  QMMF_INFO("%s:%s: Enter", TAG, __func__);
  QMMF_INFO("%s:%s: Exit", TAG, __func__);
}

EncoderCore::~EncoderCore() {

  QMMF_INFO("%s:%s: Enter", TAG, __func__);
  if (!track_encoders_.isEmpty()) {
    track_encoders_.clear();
  }
  instance_ = NULL;

  if (ion_device_ > 0) {
    close(ion_device_);
    ion_device_ = -1;
  }
  QMMF_INFO("%s:%s: Exit", TAG, __func__);
}

status_t EncoderCore::AddSource(const sp<TrackSource>& track_source,
                                VideoTrackParams& params) {

  QMMF_DEBUG("%s:%s: Enter", TAG, __func__);
  assert(track_source.get() != nullptr);

  if(ion_device_ < 0) {
    ion_device_ = open("/dev/ion", O_RDONLY);
    assert(ion_device_ >=0 );
  }

  sp<TrackEncoder> track_encoder = new TrackEncoder(ion_device_);
  if (!track_encoder.get()) {
    QMMF_ERROR("%s:%s: track_id(%d) Can't instantiate TrackEncoder", TAG,
        __func__, params.track_id);
    return NO_MEMORY;
  }

  auto ret = track_encoder->Init(track_source, params);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s:%s: track_id(%d) TrackEncoder Init failed!", TAG, __func__,
        params.track_id);
    return BAD_VALUE;
  }

  track_encoders_.add(params.track_id, track_encoder);
  QMMF_INFO("%s:%s: TrackEncoder(0x%x) for track_id(%d) Instantiated!", TAG,
      __func__, track_encoder.get(), params.track_id);

  QMMF_DEBUG("%s:%s: Exit", TAG, __func__);
  return ret;
}

status_t EncoderCore::StartTrackEncoder(uint32_t track_id) {

  QMMF_DEBUG("%s:%s: Enter track_id(%d)", TAG, __func__, track_id);

  if (!isTrackValid(track_id)) {
    QMMF_ERROR("%s:%s: Invalid track_id(%d)", TAG, __func__, track_id);
    return BAD_VALUE;
  }
  sp<TrackEncoder> track_encoder = track_encoders_.valueFor(track_id);
  assert(track_encoder.get() != NULL);

  auto ret = track_encoder->Start();
  // Initial debug purpose.
  assert(ret == NO_ERROR);
  if (ret != NO_ERROR) {
    QMMF_INFO("%s:%s: track_id(%d) TrackEncoder Start failed!", TAG, __func__,
      track_id);
    return ret;
  }

  QMMF_INFO("%s:%s: track_id(%d) TrackEncoder Started Successfully!", TAG,
      __func__);
  QMMF_DEBUG("%s:%s: Exit", TAG, __func__);
  return ret;
}

status_t EncoderCore::StopTrackEncoder(uint32_t track_id) {

  QMMF_DEBUG("%s:%s: Enter track_id(%d)", TAG, __func__, track_id);

  if (!isTrackValid(track_id)) {
    QMMF_ERROR("%s:%s: Invalid track_id(%d)", TAG, __func__, track_id);
    return BAD_VALUE;
  }
  sp<TrackEncoder> track_encoder = track_encoders_.valueFor(track_id);
  assert(track_encoder.get() != NULL);

  auto ret = track_encoder->Stop();
  // Initial debug purpose.
  assert(ret == NO_ERROR);
  if (ret != NO_ERROR) {
    QMMF_INFO("%s:%s: track_id(%d) TrackEncoder Stop failed!", TAG, __func__,
      track_id);
    return ret;
  }

  QMMF_INFO("%s:%s: track_id(%d) TrackEncoder Stopped Successfully!", TAG,
      __func__);
  QMMF_DEBUG("%s:%s: Exit", TAG, __func__);
  return ret;
}

status_t EncoderCore::SetTrackEncoderParams(uint32_t track_id,
                                            VideoTrackParamType param_type,
                                            void* param, uint32_t param_size) {

  QMMF_DEBUG("%s:%s: Enter track_id(%d)", TAG, __func__, track_id);
  if (!isTrackValid(track_id)) {
    QMMF_ERROR("%s:%s: Invalid track_id(%d)", TAG, __func__, track_id);
    return BAD_VALUE;
  }
  sp<TrackEncoder> track_encoder = track_encoders_.valueFor(track_id);
  assert(track_encoder.get() != NULL);

  auto ret = track_encoder->SetParams(param_type, param, param_size);
  // Initial debug purpose.
  assert(ret == NO_ERROR);

  if (ret != NO_ERROR) {
    QMMF_INFO("%s:%s: track_id(%d) TrackEncoder SetParams failed!", TAG,
        __func__, track_id);
    return ret;
  }

  QMMF_DEBUG("%s:%s: Exit", TAG, __func__);
  return ret;
}

status_t EncoderCore::DeleteTrackEncoder(uint32_t track_id) {

  QMMF_DEBUG("%s:%s: Enter track_id(%d)", TAG, __func__, track_id);

  if (!isTrackValid(track_id)) {
    QMMF_ERROR("%s:%s: Invalid track_id(%d)", TAG, __func__, track_id);
    return BAD_VALUE;
  }
  sp<TrackEncoder> track_encoder = track_encoders_.valueFor(track_id);
  assert(track_encoder.get() != nullptr);

  auto ret = track_encoder->ReleaseHeaders();
  assert(ret == NO_ERROR);

  track_encoders_.removeItem(track_id);

  QMMF_INFO("%s:%s: track_id(%d) TrackEncoder Deleted Successfully!", TAG,
      __func__);
  QMMF_DEBUG("%s:%s: Exit", TAG, __func__);
  return NO_ERROR;

}

status_t EncoderCore::ReturnTrackBuffer(const uint32_t track_id,
                                        std::vector<BnTrackBuffer> &buffers) {

  QMMF_DEBUG("%s:%s: Enter track_id(%d)", TAG, __func__, track_id);

  if (!isTrackValid(track_id)) {
    QMMF_ERROR("%s:%s: Invalid track_id(%d)", TAG, __func__, track_id);
    return BAD_VALUE;
  }
  sp<TrackEncoder> track_encoder = track_encoders_.valueFor(track_id);
  assert(track_encoder.get() != nullptr);

  // Return buffer back to track encoder's output bitstream buffer queue.
  auto ret = track_encoder->OnBufferReturnFromClient(buffers);

  QMMF_DEBUG("%s:%s: Exit track_id(%d)", TAG, __func__, track_id);
  return ret;
}

bool EncoderCore::isTrackValid(uint32_t track_id) {

  QMMF_INFO("%s: Number of Tracks exist = %d",__func__, track_encoders_.size());
  assert(track_encoders_.size() > 0);
  return track_encoders_.indexOfKey(track_id) >= 0 ? true : false;
}

TrackEncoder::TrackEncoder(int32_t ion_device)
    : ion_device_(ion_device), eos_atoutput_(false) {

  QMMF_INFO("%s:%s: Enter", TAG, __func__);

  memset(&track_params_, 0x0, sizeof track_params_);
  QMMF_INFO("%s:%s: Exit (0x%x)", TAG, __func__, this);
}

TrackEncoder::~TrackEncoder() {

  QMMF_INFO("%s:%s: Enter", TAG, __func__);

  for(auto& iter : output_buffer_list_) {

    if((iter).pointer) {
        munmap((iter).pointer, (iter).frame_length);
        (iter).pointer = NULL;
    }
    if((iter).fd) {
        QMMF_INFO("%s:%s track_id(%d) (iter).fd =%d Free", TAG, __func__,
                                   TrackId(), (iter).fd);
        ioctl(ion_device_, ION_IOC_FREE, &((iter).handle_data));
        close((iter).fd);
        (iter).fd = 0;
    }
  }
  output_buffer_list_.clear();
  QMMF_INFO("%s:%s: Exit (0x%x)", TAG, __func__, this);
}

status_t TrackEncoder::Init(const sp<TrackSource>& track_source,
                            VideoTrackParams& params) {

  QMMF_INFO("%s:%s: Enter track_id(%d)", TAG, __func__, params.track_id);
  track_params_ = params;

  avcodec_ = new AVCodec();
  if(!avcodec_.get()) {
    QMMF_ERROR("%s:%s: track_id(%d) AVCodec failed", TAG, __func__,
        params.track_id);
    return NO_MEMORY;
  }

  CodecCreateParam codec_param;
  memset(&codec_param, 0x0, sizeof(codec_param));

  codec_param.video_param.width        = params.width;
  codec_param.video_param.height       = params.height;
  codec_param.video_param.frame_rate   = params.frame_rate;
  codec_param.video_param.format_type  = params.format_type;
  codec_param.video_param.codec_param  = params.codec_param;

  QMMF_INFO("%s:%s: W(%d) H(%d) format_type(%d)", TAG, __func__, params.width,
      params.height, params.format_type);

  codec_param.event_cb = [&] (OMX_EVENTTYPE event, OMX_U32 data1,
      OMX_U32 data2) { EventCallback(event, data1, data2);};

  auto ret = avcodec_->ConfigureCodec(CodecType::kVideoEncoder, codec_param);
  assert(ret == NO_ERROR);
  if(ret != NO_ERROR) {
    QMMF_ERROR("%s:%s track_id(%d) Failed to configure AVCodec!", TAG, __func__,
        params.track_id);
    return ret;
  }

  // TODO: Modify UseBuffer Api to take sp pointer as a reference.
  ret = avcodec_->UseBuffer(kPortIndexInput,
                            static_cast<void*>(track_source.get()));
  assert(ret == NO_ERROR);
  if(ret != NO_ERROR) {
    QMMF_ERROR("%s:%s track_id(%d) UseBuffer Failed at input port!", TAG,
        __func__, params.track_id);
    return ret;
  }

  //Output port configuration
  ret = AllocOutputPortBufs();
  assert(ret == NO_ERROR);
  if(ret != NO_ERROR) {
    QMMF_ERROR("%s:%s track_id(%d) output buffer allocation failed!!", TAG,
        __func__, params.track_id);
    return ret;
  }
  ret = avcodec_->UseBuffer(kPortIndexOutput, static_cast<void*>(this));
  if(ret != NO_ERROR) {
    QMMF_ERROR("%s:%s track_id(%d) UseBuffer Failed at output port!", TAG,
        __func__, params.track_id);
    // TODO: Deallocate ouput port buffers.
    //ReleaseBuffer();
    return ret;
  }

  QMMF_INFO("%s:%s: track_id(%d) AVCodec(0x%x) Instantiated!" , TAG, __func__,
      params.track_id, avcodec_.get());

  for(auto& iter : output_buffer_list_) {
      QMMF_INFO("%s:%s:  Adding buffer fd(%d) to output_free_buffer_queue_ list"
          , TAG, __func__, iter.fd);
      output_free_buffer_queue_.PushBack(iter);
  }

#ifdef DUMP_BITSTREAM
  String8 bitstream_filepath;
  const char* type_string = (params.format_type == VideoFormat::kAVC) ? "h264"
      : "h265";
  String8 extension(type_string);
  bitstream_filepath.appendFormat(FRAME_DUMP_PATH"/track_enc_%d.%s",
      params.track_id, type_string);
  file_fd_ = open(bitstream_filepath.string(), O_CREAT | O_WRONLY | O_TRUNC,
       0655);
#endif
  QMMF_INFO("%s:%s: Exit", TAG, __func__);
  return ret;
}

status_t TrackEncoder::Start() {

  QMMF_INFO("%s:%s: Enter track_id(%d)", TAG, __func__, TrackId());

  assert(avcodec_.get() != nullptr);
  auto ret = avcodec_->StartCodec();
  // Initial debug purpose.
  assert(ret == NO_ERROR);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s:%s: track_id(%d) StartCodec failed!", TAG, __func__,
        TrackId());
    return ret;
  }

  eos_atoutput_ = false;

  QMMF_INFO("%s:%s: Exit track_id(%d)", TAG, __func__, TrackId());
  return ret;
}

status_t TrackEncoder::Stop() {

  QMMF_INFO("%s:%s: Enter track_id(%d)", TAG, __func__, TrackId());

  assert(avcodec_.get() != nullptr);
  auto ret = avcodec_->StopCodec();
  // Initial debug purpose.
  assert(ret == NO_ERROR);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s:%s: track_id(%d) StopCodec failed!", TAG, __func__,
        TrackId());
    return ret;
  }

  assert(output_free_buffer_queue_.Size() == output_buffer_list_.size());
  assert(output_occupy_buffer_queue_.Size() == 0);
  QMMF_INFO("%s:%s: Exit track_id(%d)", TAG, __func__, TrackId());
  return ret;
}

status_t TrackEncoder::SetParams(VideoTrackParamType param_type, void* param,
                                 uint32_t param_size) {

  QMMF_INFO("%s:%s: Enter track_id(%d)", TAG, __func__, TrackId());

  QMMF_INFO("%s:%s: Exit", TAG, __func__);
}


status_t TrackEncoder::ReleaseHeaders() {

  QMMF_INFO("%s:%s: Enter track_id(%d)", TAG, __func__, TrackId());
  assert(avcodec_ != nullptr);
  auto ret = avcodec_->ReleaseBuffer();
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s:%s: ReleaseBuffer failed!", TAG, __func__);
  }
  QMMF_INFO("%s:%s: Exit track_id(%d)", TAG, __func__, TrackId());
  return ret;
}

status_t TrackEncoder::GetBuffer(CodecBuffer& codec_buffer) {

  QMMF_DEBUG("%s:%s: Enter track_id(%d)", TAG, __func__, TrackId());
  // Give available free buffer to encoder to use on output port.

  if(output_free_buffer_queue_.Size() <= 0) {
    QMMF_DEBUG("%s:%s track_id(%d) No buffer available to notify,"
      " Wait for new buffer", TAG, __func__, TrackId());
    Mutex::Autolock autoLock(lock_);
    wait_for_frame_.wait(lock_);
    //TODO: change simple wait to relative wait.
  }

  CodecBuffer iter = *output_free_buffer_queue_.Begin();
  codec_buffer.fd = (iter).fd;
  codec_buffer.pointer = (iter).pointer;
  output_free_buffer_queue_.Erase(output_free_buffer_queue_.Begin());
  {
    Mutex::Autolock lock(queue_lock_);
    output_occupy_buffer_queue_.PushBack(iter);
  }
  QMMF_VERBOSE("%s:%s track_id(%d) Sending buffer(0x%x) fd(%d) for FTB", TAG,
      __func__, TrackId(), codec_buffer.pointer, codec_buffer.fd);

  QMMF_DEBUG("%s:%s: Exit track_id(%d)", TAG, __func__, TrackId());
  return NO_ERROR;
}

status_t TrackEncoder::ReturnBuffer(CodecBuffer& codec_buffer) {

  QMMF_DEBUG("%s:%s: Enter track_id(%d)", TAG, __func__, TrackId());
  assert(codec_buffer.pointer != NULL);

  QMMF_VERBOSE("%s:%s: track_id(%d) Received buffer(0x%x) from FBD", TAG,
      __func__, TrackId(), codec_buffer.pointer);

#ifdef DUMP_BITSTREAM
  DumpBitStream(codec_buffer);
#endif

#ifdef DONT_NOTIFY
  // This change is only for debug purpose, it will circulate buffers without
  // sending/mapping them to another process.
  List<CodecBuffer>::iterator it = output_occupy_buffer_queue_.Begin();
  bool found = false;
  for (; it != output_occupy_buffer_queue_.End(); ++it) {
    QMMF_VERBOSE("%s:%s Checking match (0x%x)vs(0x%x) ", TAG, __func__,
        (*it).pointer,  codec_buffer.pointer);
    if (((*it).pointer) == (codec_buffer.pointer)) {
      QMMF_VERBOSE("%s:%s Buffer found", TAG, __func__);
      output_free_buffer_queue_.PushBack(*it);
      output_occupy_buffer_queue_.Erase(it);
      wait_for_frame_.signal();
      found = true;
      break;
    }
  }
  assert(found == true);
#else
  if (eos_atoutput_ == true) {
    //  If EOS happend on output port then don't notify buffers to application.
    //  simply remove the buffer from output queue in input queue, note last
    //  buffer with EOS is already notified to application before setting
    //  eos_atoutput_ to true.
    {
      Mutex::Autolock lock(queue_lock_);
      List<CodecBuffer>::iterator it = output_occupy_buffer_queue_.Begin();
      for (; it != output_occupy_buffer_queue_.End(); ++it) {
        if (((*it).pointer) == (codec_buffer.pointer)) {
          QMMF_INFO("%s:%s EOS is already done! moving buffer from Out to In"
              " queue!", TAG, __func__);
          output_free_buffer_queue_.PushBack(*it);
          output_occupy_buffer_queue_.Erase(it);
          break;
        }
      }
    }
  } else {
    NotifyBufferToClient(codec_buffer);
  }
#endif

  QMMF_DEBUG("%s:%s: Exit track_id(%d)", TAG, __func__, TrackId());
}

status_t TrackEncoder::OnBufferReturnFromClient(std::vector<BnTrackBuffer>
                                            &bn_buffers) {

  QMMF_DEBUG("%s:%s: Enter track_id(%d)", TAG, __func__, TrackId());
  int32_t ret = NO_ERROR;

  //Buffer came back from client, now put this buffer in free queue.
  QMMF_DEBUG("%s:%s: Number of buffers(%d) returned from client", TAG,
      __func__, bn_buffers.size());

  assert(output_occupy_buffer_queue_.Size() > 0);

  for (auto& iter : bn_buffers) {
    List<CodecBuffer>::iterator it = output_occupy_buffer_queue_.Begin();
    bool match = false;
    QMMF_VERBOSE("%s:%s output_occupy_buffer_queue_.size(%d)", TAG, __func__,
        output_occupy_buffer_queue_.Size());
    {
      Mutex::Autolock lock(queue_lock_);
      for (; it != output_occupy_buffer_queue_.End(); ++it) {
        if ((*it).fd == iter.buffer_id) {
          QMMF_VERBOSE("%s:%s: buffer_id(%d) found in list", TAG, __func__,
              iter.buffer_id);
          // Move buffer to free queue, and signal AVCodec's output thread if it
          // is waiting for buffer.
          output_free_buffer_queue_.PushBack((*it));
          // Erase buffer from occupy queue.
          output_occupy_buffer_queue_.Erase(it);
          wait_for_frame_.signal();
          match = true;
        }
      }
      // Make sure all buffers are part of occupy queue.
      assert(match == true);
      QMMF_VERBOSE("%s:%s output_occupy_buffer_queue_.size(%d)", TAG, __func__,
          output_occupy_buffer_queue_.Size());
      QMMF_VERBOSE("%s:%s output_free_buffer_queue_.size(%d)", TAG, __func__,
          output_free_buffer_queue_.Size());
    }
  }
  QMMF_DEBUG("%s:%s: Exit track_id(%d)", TAG, __func__, TrackId());
  return ret;
}

void TrackEncoder::NotifyBufferToClient(CodecBuffer& codec_buffer) {

  QMMF_DEBUG("%s:%s: Enter track_id(%d)", TAG, __func__, TrackId());
  assert(track_params_.data_cb != nullptr);

  bool found = false;
  BnTrackBuffer bn_buffer;
  memset(&bn_buffer, 0x0, sizeof bn_buffer);

  uint32_t flags = 0x0;
  if(codec_buffer.flag & OMX_BUFFERFLAG_EOS) {
    flags |= static_cast<uint32_t>(TrackBufferFlags::kFlagEOS);
    eos_atoutput_ = true;
  }
  //TODO: Add CodecConfig flag too.

  {
    Mutex::Autolock lock(queue_lock_);
    List<CodecBuffer>::iterator it = output_occupy_buffer_queue_.Begin();
    for (; it != output_occupy_buffer_queue_.End(); ++it) {
      QMMF_VERBOSE("%s:%s Checking match (0x%x) vs (0x%x) ", TAG, __func__,
          (*it).pointer,  codec_buffer.pointer);
      if (((*it).pointer) ==  (codec_buffer.pointer)) {
        QMMF_VERBOSE("%s:%s fd(%d):filled_length(%d):ts(%lld):frame_length(%d)",
            TAG, __func__,  (*it).fd, codec_buffer.filled_length,
            codec_buffer.ts, (*it).frame_length);
        bn_buffer.ion_fd    = (*it).fd;
        bn_buffer.size      = codec_buffer.filled_length;
        bn_buffer.timestamp = codec_buffer.ts;
        bn_buffer.width     = -1;
        bn_buffer.height    = -1;
        bn_buffer.buffer_id = (*it).fd;
        bn_buffer.flag      = flags;
        bn_buffer.capacity  = (*it).frame_length;
        found = true;
        break;
      }
    }
  }
  assert(found == true);
  std::vector<BnTrackBuffer> bn_buffers;
  bn_buffers.push_back(bn_buffer);
  track_params_.data_cb(TrackId(), bn_buffers, nullptr,
      TrackMetaParamType::kNone, 0);

  QMMF_DEBUG("%s:%s: Exit track_id(%d)", TAG, __func__, TrackId());

}

status_t TrackEncoder::AllocOutputPortBufs() {

  QMMF_INFO("%s:%s: Enter track_id(%d)", TAG, __func__, TrackId());
  int32_t ret = 0;
  uint32_t count, size;

  assert(avcodec_.get() != nullptr);
  ret = avcodec_->GetBufferRequirements(kPortIndexOutput,  &count, &size);
  assert(ret == NO_ERROR);
  //TODO: This hardcoding would be fixed by AVCodec layer
  count = OUTPUT_MAX_COUNT;

  assert(ion_device_ >= 0);
  int32_t ion_type = 0x1 << ION_IOMMU_HEAP_ID;
  void *vaddr      = NULL;

  struct ion_allocation_data alloc;
  struct ion_fd_data         ion_fddata;

  for(uint32_t i = 0; i < count; i++) {

    CodecBuffer buffer;
    vaddr = NULL;
    memset(&buffer, 0x0, sizeof(buffer));
    memset(&alloc, 0x0, sizeof(ion_allocation_data));
    memset(&ion_fddata, 0x0, sizeof(ion_fddata));

    alloc.len = size;
    alloc.len = (alloc.len + 4095) & (~4095);
    alloc.align = 4096;
    alloc.flags = ION_FLAG_CACHED;
    alloc.heap_id_mask = ion_type;

    ret = ioctl(ion_device_, ION_IOC_ALLOC, &alloc);
    if (ret < 0) {
      QMMF_ERROR("%s:%s ION allocation failed!", TAG, __func__);
      goto ION_ALLOC_FAILED;
    }

    ion_fddata.handle = alloc.handle;
    ret = ioctl(ion_device_, ION_IOC_SHARE, &ion_fddata);
    if (ret < 0) {
        QMMF_ERROR("%s:%s ION map failed %s", TAG, __func__, strerror(errno));
        goto ION_MAP_FAILED;
    }

    vaddr = mmap(NULL, alloc.len, PROT_READ  | PROT_WRITE, MAP_SHARED,
                 ion_fddata.fd, 0);

    if (vaddr == MAP_FAILED) {
        QMMF_ERROR("%s:%s  ION mmap failed: %s (%d)", TAG, __func__,
            strerror(errno), errno);
        goto ION_MAP_FAILED;
    }

    buffer.handle_data.handle = ion_fddata.handle;
    buffer.fd                 = ion_fddata.fd;
    buffer.frame_length       = alloc.len;
    buffer.pointer            = vaddr;

    QMMF_INFO("%s:%s buffer.Fd(%d)", TAG, __func__, buffer.fd);
    QMMF_INFO("%s:%s buffer.size(%d)", TAG, __func__, buffer.frame_length);
    QMMF_INFO("%s:%s buffer.vaddr(%p)", TAG, __func__, buffer.pointer);

    output_buffer_list_.push_back(buffer);
  }

  QMMF_INFO("%s:%s: Exit track_id(%d)", TAG, __func__, TrackId());
  return ret;

ION_MAP_FAILED:
  struct ion_handle_data ionHandleData;
  memset(&ionHandleData, 0x0, sizeof(ionHandleData));
  ionHandleData.handle = ion_fddata.handle;
  ioctl(ion_device_, ION_IOC_FREE, &ionHandleData);
ION_ALLOC_FAILED:
  QMMF_ERROR("%s:%s ION Buffer allocation failed!", TAG, __func__);
  return -1;
}

void TrackEncoder::EventCallback(OMX_EVENTTYPE event, OMX_U32 data1,
                                 OMX_U32 data2) {
  QMMF_INFO("%s:%s: Enter track_id(%d)", TAG, __func__, TrackId());
  QMMF_INFO("%s:%s: Exit track_id(%d)", TAG, __func__, TrackId());
}

#ifdef DUMP_BITSTREAM
void TrackEncoder::DumpBitStream(CodecBuffer& codec_buffer) {

  QMMF_VERBOSE("%s:%s: Enter track_id(%d)", TAG, __func__, TrackId());
  if(eos_atoutput_) {
    return;
  }
  if(file_fd_ > 0) {
    ssize_t exp_size = (ssize_t) codec_buffer.filled_length;
    QMMF_INFO("%s:%s Got encoded buffer of size(%d)", TAG, __func__,
        codec_buffer.filled_length);

    if (exp_size != write(file_fd_, codec_buffer.pointer,
       codec_buffer.filled_length)) {

      QMMF_INFO("%s:%s: Bad Write error (%d) %s", TAG, __func__, errno,
          strerror(errno));
      close(file_fd_);
      file_fd_ = -1;
    }
  } else {
    QMMF_ERROR("%s:%s File is not open fd = %d", TAG, __func__, file_fd_);
  }

  if(codec_buffer.flag & OMX_BUFFERFLAG_EOS) {
    QMMF_ERROR("%s:%s This is last buffer from encoder.close file", TAG,
        __func__);

    close(file_fd_);
    file_fd_ = -1;
  }
  QMMF_VERBOSE("%s:%s: Enter track_id(%d)", TAG, __func__, TrackId());
}
#endif

};  // namespace recorder

};  // namespace qmmf