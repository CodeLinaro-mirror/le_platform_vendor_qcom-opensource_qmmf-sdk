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


#define TAG "VideoDecoderCore"


#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/msm_ion.h>

#include "player/src/service/qmmf_player_video_decoder_core.h"


namespace qmmf {
namespace player {


VideoDecoderCore* VideoDecoderCore::instance_ = nullptr;

VideoDecoderCore* VideoDecoderCore::CreateVideoDecoderCore()
{
  if(!instance_) {
     instance_ = new VideoDecoderCore();
  if(!instance_) {
    QMMF_ERROR("%s:%s: Can't Create VideoDecoderCore Instance", TAG, __func__);
    return nullptr;
  }
  }
  QMMF_INFO("%s:%s: VideoDecoderCore Instance Created Successfully(0x%x)", TAG,
       __func__, instance_);

  return instance_;
}

VideoDecoderCore::VideoDecoderCore(): ion_device_(-1)
{
  QMMF_INFO("%s:%s: Enter", TAG, __func__);
  QMMF_INFO("%s:%s: Exit", TAG, __func__);
}

VideoDecoderCore::~VideoDecoderCore()
{
  QMMF_INFO("%s:%s: Enter", TAG, __func__);
  QMMF_INFO("%s:%s: Exit", TAG, __func__);
}

status_t VideoDecoderCore::CreateVideoTrack(VideoTrackParams& params)
{
   QMMF_DEBUG("%s:%s: Enter", TAG, __func__);

   status_t ret = NO_ERROR;

  if(ion_device_ < 0) {
    ion_device_ = open("/dev/ion", O_RDONLY);
    assert(ion_device_ >=0 );
  }

  sp<VideoTrackDecoder> video_track_decoder_ = new VideoTrackDecoder(ion_device_);

  if (!video_track_decoder_.get()) {
    QMMF_ERROR("%s:%s: track_id(%d) Can't instantiate VideoTrackEncoder", TAG,
        __func__, params.track_id);
    return NO_MEMORY;
  }

  ret = video_track_decoder_->ConfigureTrackDecoder(params);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s:%s: track_id(%d) VideoTrackEncoder Init failed!", TAG, __func__,
        params.track_id);
    return BAD_VALUE;
  }

  video_track_decoders_.add(params.track_id, video_track_decoder_);

  QMMF_INFO("%s:%s: VideoTrackEncoder(0x%x) for track_id(%d) Instantiated!", TAG,
      __func__, video_track_decoder_.get(), params.track_id);

  QMMF_DEBUG("%s:%s: Exit", TAG, __func__);
  return ret;
}

status_t VideoDecoderCore::PrepareTrackPipeline(uint32_t track_id,const sp<VideoTrackSink>& video_track_sink)
{
  QMMF_DEBUG("%s:%s: Enter track_id(%d)", TAG, __func__, track_id);

  if (!isTrackValid(track_id)) {
   QMMF_ERROR("%s:%s: Invalid track_id(%d)", TAG, __func__, track_id);
   return BAD_VALUE;
  }

   sp<VideoTrackDecoder>  track_decoder = video_track_decoders_.valueFor(track_id);
   assert(track_decoder.get() != NULL);

   auto ret = track_decoder->PreparePipeline(video_track_sink);
   if (ret != NO_ERROR) {
   QMMF_INFO("%s:%s: track_id(%d) PreparePipeline failed!", TAG, __func__,
     track_id);
   return ret;
  }

  QMMF_INFO("%s:%s: track_id(%d) PreparePipeline Successful!", TAG,
     __func__, track_id);
  QMMF_DEBUG("%s:%s: Exit", TAG, __func__);
  return ret;
}

status_t VideoDecoderCore::DequeueTrackInputBuffer(uint32_t track_id,
                          std::vector<AVCodecBuffer>& buffers)
{
  QMMF_DEBUG("%s:%s: Enter track_id(%d)", TAG, __func__, track_id);

  if (!isTrackValid(track_id)) {
   QMMF_ERROR("%s:%s: Invalid track_id(%d)", TAG, __func__, track_id);
   return BAD_VALUE;
  }

  sp<VideoTrackDecoder>  track_decoder = video_track_decoders_.valueFor(track_id);
  assert(track_decoder.get() != NULL);

  auto ret = track_decoder->DequeueInputBuffer(buffers);
  if (ret != NO_ERROR) {
   QMMF_INFO("%s:%s: track_id(%d) DequeueInputBuffer failed!", TAG, __func__,
     track_id);
   return ret;
  }

  QMMF_INFO("%s:%s: track_id(%d) DequeueInputBuffer Successful!", TAG,
     __func__, track_id);
  QMMF_DEBUG("%s:%s: Exit", TAG, __func__);
  return ret;
}

status_t VideoDecoderCore::QueueTrackInputBuffer(uint32_t track_id,
                          std::vector<AVCodecBuffer>& buffers)
{
  QMMF_DEBUG("%s:%s: Enter track_id(%d)", TAG, __func__, track_id);

  if (!isTrackValid(track_id)) {
    QMMF_ERROR("%s:%s: Invalid track_id(%d)", TAG, __func__, track_id);
    return BAD_VALUE;
  }

   sp<VideoTrackDecoder>  track_decoder = video_track_decoders_.valueFor(track_id);
   assert(track_decoder.get() != NULL);

   auto ret = track_decoder->QueueInputBuffer(buffers);
   if (ret != NO_ERROR) {
    QMMF_INFO("%s:%s: track_id(%d) QueueInputBuffer failed!", TAG, __func__,
      track_id);
    return ret;
   }

   QMMF_INFO("%s:%s: track_id(%d) QueueInputBuffer Successful!", TAG,
      __func__, track_id);
   QMMF_DEBUG("%s:%s: Exit", TAG, __func__);
   return ret;
}

status_t VideoDecoderCore::StartTrackDecoder(uint32_t track_id)
{
  QMMF_DEBUG("%s:%s: Enter track_id(%d)", TAG, __func__, track_id);

  if (!isTrackValid(track_id)) {
   QMMF_ERROR("%s:%s: Invalid track_id(%d)", TAG, __func__, track_id);
   return BAD_VALUE;
  }

  sp<VideoTrackDecoder>  track_decoder = video_track_decoders_.valueFor(track_id);
  assert(track_decoder.get() != NULL);

  auto ret = track_decoder->StartDecoder();
  if (ret != NO_ERROR) {
   QMMF_INFO("%s:%s: track_id(%d) StartDecoder failed!", TAG, __func__,
     track_id);
   return ret;
  }

  QMMF_INFO("%s:%s: track_id(%d) StartDecoder Successful!", TAG,
     __func__, track_id);
  QMMF_DEBUG("%s:%s: Exit", TAG, __func__);
  return ret;
}

status_t VideoDecoderCore::StopTrackDecoder(uint32_t track_id)
{
  QMMF_DEBUG("%s:%s: Enter track_id(%d)", TAG, __func__, track_id);

  if (!isTrackValid(track_id)) {
   QMMF_ERROR("%s:%s: Invalid track_id(%d)", TAG, __func__, track_id);
   return BAD_VALUE;
  }

  sp<VideoTrackDecoder>  track_decoder = video_track_decoders_.valueFor(track_id);
  assert(track_decoder.get() != NULL);

  auto ret = track_decoder->StopDecoder();
  if (ret != NO_ERROR) {
    QMMF_INFO("%s:%s: track_id(%d) StopDecoder failed!", TAG, __func__,
      track_id);
    return ret;
  }

  QMMF_INFO("%s:%s: track_id(%d) StopDecoder Successful!", TAG,
      __func__, track_id);
  QMMF_DEBUG("%s:%s: Exit", TAG, __func__);
  return ret;
}

status_t VideoDecoderCore::PauseTrackDecoder(uint32_t track_id)
{
  QMMF_DEBUG("%s:%s: Enter track_id(%d)", TAG, __func__, track_id);

  if (!isTrackValid(track_id)) {
   QMMF_ERROR("%s:%s: Invalid track_id(%d)", TAG, __func__, track_id);
   return BAD_VALUE;
  }

  sp<VideoTrackDecoder>  track_decoder = video_track_decoders_.valueFor(track_id);
  assert(track_decoder.get() != NULL);

  auto ret = track_decoder->PauseDecoder();
  if (ret != NO_ERROR) {
   QMMF_INFO("%s:%s: track_id(%d) PauseDecoder failed!", TAG, __func__,
     track_id);
   return ret;
  }

  QMMF_INFO("%s:%s: track_id(%d) PauseDecoder Successful!", TAG,
     __func__, track_id);
  QMMF_DEBUG("%s:%s: Exit", TAG, __func__);
  return ret;
}

status_t VideoDecoderCore::ResumeTrackDecoder(uint32_t track_id)
{
  QMMF_DEBUG("%s:%s: Enter track_id(%d)", TAG, __func__, track_id);

  if (!isTrackValid(track_id)) {
    QMMF_ERROR("%s:%s: Invalid track_id(%d)", TAG, __func__, track_id);
    return BAD_VALUE;
  }

  sp<VideoTrackDecoder>  track_decoder = video_track_decoders_.valueFor(track_id);
  assert(track_decoder.get() != NULL);

  auto ret = track_decoder->ResumeDecoder();
  if (ret != NO_ERROR) {
   QMMF_INFO("%s:%s: track_id(%d) ResumeDecoder failed!", TAG, __func__,
     track_id);
   return ret;
  }

  QMMF_INFO("%s:%s: track_id(%d) ResumeDecoder Successful!", TAG,
     __func__, track_id);
  QMMF_DEBUG("%s:%s: Exit", TAG, __func__);
  return ret;
}

status_t VideoDecoderCore::SetVideoTrackDecoderParams(uint32_t track_id,
                                CodecParamType param_type, void* param,
                                uint32_t param_size)
{
  QMMF_DEBUG("%s:%s: Enter track_id(%d)", TAG, __func__, track_id);

  if (!isTrackValid(track_id)) {
      QMMF_ERROR("%s:%s: Invalid track_id(%d)", TAG, __func__, track_id);
      return BAD_VALUE;
  }

  sp<VideoTrackDecoder>  track_decoder = video_track_decoders_.valueFor(track_id);
  assert(track_decoder.get() != NULL);

  auto ret =  track_decoder->SetVideoDecoderParams(param_type, param, param_size);
  if (ret != NO_ERROR) {
   QMMF_INFO("%s:%s: track_id(%d) SetVideoDecoderParams failed!", TAG, __func__,
     track_id);
   return ret;
  }

  QMMF_INFO("%s:%s: track_id(%d) SetVideoDecoderParams Successful!", TAG,
     __func__, track_id);
  QMMF_DEBUG("%s:%s: Exit", TAG, __func__);
  return ret;
}

status_t VideoDecoderCore::DeleteTrackDecoder(uint32_t track_id)
{
  QMMF_DEBUG("%s:%s: Enter track_id(%d)", TAG, __func__, track_id);

  if (!isTrackValid(track_id)) {
     QMMF_ERROR("%s:%s: Invalid track_id(%d)", TAG, __func__, track_id);
     return BAD_VALUE;
  }

  sp<VideoTrackDecoder>  track_decoder = video_track_decoders_.valueFor(track_id);
  assert(track_decoder.get() != NULL);

  auto ret = track_decoder->DeleteDecoder();
  if (ret != NO_ERROR) {
   QMMF_INFO("%s:%s: track_id(%d) DeleteDecoder failed!", TAG, __func__,
     track_id);
   return ret;
  }

  QMMF_INFO("%s:%s: track_id(%d) DeleteDecoder Successful!", TAG,
     __func__, track_id);
  QMMF_DEBUG("%s:%s: Exit", TAG, __func__);
  return ret;
}

bool VideoDecoderCore::isTrackValid(uint32_t track_id)
{
  QMMF_INFO("%s: Number of Tracks exist = %d",__func__, video_track_decoders_.size());
  assert(video_track_decoders_.size() > 0);
  return video_track_decoders_.indexOfKey(track_id) >= 0 ? true : false;
}


/************************* Video Decoding ********************************/


VideoTrackDecoder::VideoTrackDecoder(int32_t ion_device):
    ion_device_(ion_device),eos_(false)
{
  QMMF_INFO("%s:%s: Enter", TAG, __func__);

  memset(&video_track_params_, 0x0, sizeof video_track_params_);
  QMMF_INFO("%s:%s: Exit (0x%x)", TAG, __func__, this);
}

VideoTrackDecoder::~VideoTrackDecoder()
{
  QMMF_INFO("%s:%s: Enter track_id(%d)", TAG, __func__, TrackId());

  for(auto& iter : input_buffer_list_) {

    if((iter).data) {
        munmap((iter).data, (iter).size);
        (iter).data = NULL;
    }
    if((iter).fd) {
        QMMF_INFO("%s:%s track_id(%d) (iter).fd =%d Free", TAG, __func__,
                                   TrackId(), (iter).fd);
        ioctl(ion_device_, ION_IOC_FREE, &((iter).handle));
        close((iter).fd);
        (iter).fd = 0;
    }
  }
  input_buffer_list_.clear();
  QMMF_INFO("%s:%s: Exit (0x%x)", TAG, __func__, this);
}

status_t VideoTrackDecoder::ConfigureTrackDecoder(VideoTrackParams& params)
{
  avcodec_ = new AVCodec();

  status_t ret = NO_ERROR;

  CodecCreateParam codec_param;
  memset(&codec_param, 0x0, sizeof(codec_param));

  codec_param.video_param.width        =  params.width;
  codec_param.video_param.height       =  params.height;
  codec_param.video_param.frame_rate   =   params.frame_rate;
  /*
  if (params.format == AudioFormat::kAAC) {
  codec_param.audio_param.codec_param.aac.bit_rate     = params.codec_param.aac.bit_rate;
  codec_param.audio_param.codec_param.aac.format       = params.codec_param.aac.format;
  codec_param.audio_param.codec_param.aac.frame_length = params.codec_param.aac.frame_length;
  codec_param.audio_param.codec_param.aac.mode         = params.codec_param.aac.mode;
  }*/

  ret = avcodec_->ConfigureCodec(CodecType::kVideoDecoder,codec_param);
  assert(ret == NO_ERROR);
  if(ret != NO_ERROR) {
  QMMF_ERROR("%s:%s track_id(%d) Failed to configure AVCodec!", TAG, __func__,
      params.track_id);
  return ret;
  }

  QMMF_INFO("%s:%s: Exit track_id(%d)", TAG, __func__, TrackId());
  return ret;
}

status_t VideoTrackDecoder::PreparePipeline(const sp<VideoTrackSink>& video_track_sink)
{
  QMMF_INFO("%s:%s: Enter track_id(%d)", TAG, __func__, TrackId());
  assert(avcodec_.get() != nullptr);

  status_t ret = NO_ERROR;

  //This function will get the port buffer requirment and will allocate buffer
  AllocInputPortBufs();

   ret = avcodec_->UseBuffer(kPortIndexInput, static_cast<void*>(this));
   assert(ret == NO_ERROR);
   if(ret != NO_ERROR) {
    QMMF_ERROR("%s:%s track_id(%d) UseBuffer Failed at input port!", TAG,
        __func__, TrackId());
   }

   //This function will get the port buffer requirment and will allocate buffer
   /*AllocOutputPortBufs();

   ret = avcodec_->UseBuffer(kPortIndexOutput, static_cast<void*>(audio_track_sink_));
   assert(ret == NO_ERROR);
   if(ret != NO_ERROR) {
   QMMF_ERROR("%s:%s track_id(%d) UseBuffer Failed at input port!", TAG,
         __func__, track_params.track_id);
   }*/

   for(auto& iter : input_buffer_list_) {
       QMMF_INFO("%s:%s: track_id(%d) Adding buffer fd(%d) to "
           "unfilled_frame_queue", TAG, __func__,TrackId() ,
           iter.fd);
       unfilled_frame_queue_.PushBack(iter);
  }

  QMMF_INFO("%s:%s: Exit track_id(%d)", TAG, __func__, TrackId());
  return ret;
}

//service will send unfilled buffer fd/pointer to the application to fill it
status_t VideoTrackDecoder::DequeueInputBuffer(std::vector<AVCodecBuffer>& buffers)
{
  QMMF_DEBUG("%s:%s: Enter", TAG, __func__);

  if(unfilled_frame_queue_.Size() <= 0) {
   QMMF_DEBUG("%s:%s track_id(%d) No Empty buffer available", TAG, __func__, TrackId());
   //Mutex::Autolock autoLock(lock_);
   //wait_for_frame_.wait(lock_);
  }

  StreamBuffer iter = *unfilled_frame_queue_.Begin();
  buffers[0].filled_length = (iter).fd;
  buffers[0].data = (iter).data;
  buffers[0].frame_length = (iter).size;

  QMMF_DEBUG("%s:%s track_id(%d) Sending buffer(0x%x) fd(%d) to client", TAG,
     __func__, TrackId(), (iter).data, (iter).fd);

  QMMF_DEBUG("%s:%s: Exit track_id(%d)", TAG, __func__, TrackId());
  return NO_ERROR;
}

//application will fill the data in the buffer and will send to service
status_t VideoTrackDecoder::QueueInputBuffer(std::vector<AVCodecBuffer>& buffers)
{
  QMMF_DEBUG("%s:%s: Enter", TAG, __func__);

  StreamBuffer iter = *unfilled_frame_queue_.Begin();
  /*(iter).fd = buffers[0].fd;
  (iter).data = buffers[0].data;
  (iter).size = buffers[0].size;
  */
  {
   Mutex::Autolock lock(queue_lock_);
   filled_frame_queue.PushBack(iter);
   wait_for_frame_.signal();
  }
  unfilled_frame_queue_.Erase(unfilled_frame_queue_.Begin());

  QMMF_DEBUG("%s:%s track_id(%d) Sending buffer(0x%x) fd(%d) to client", TAG,
     __func__, TrackId(), (iter).data, (iter).fd);

  QMMF_DEBUG("%s:%s: Exit track_id(%d)", TAG, __func__, TrackId());
  return NO_ERROR;
}

status_t VideoTrackDecoder::StartDecoder()
{
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

  QMMF_INFO("%s:%s: Exit track_id(%d)", TAG, __func__, TrackId());
  return ret;
}

status_t VideoTrackDecoder::StopDecoder()
{
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

  QMMF_INFO("%s:%s: Exit track_id(%d)", TAG, __func__, TrackId());
  return ret;
}

status_t VideoTrackDecoder::PauseDecoder()
{
  QMMF_INFO("%s:%s: Enter track_id(%d)", TAG, __func__, TrackId());

  assert(avcodec_.get() != nullptr);
  auto ret = avcodec_->PauseCodec();
  // Initial debug purpose.
  assert(ret == NO_ERROR);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s:%s: track_id(%d) PauseCodec failed!", TAG, __func__,
        TrackId());
    return ret;
  }

  QMMF_INFO("%s:%s: Exit track_id(%d)", TAG, __func__, TrackId());
  return ret;
}

status_t VideoTrackDecoder::ResumeDecoder()
{
  QMMF_INFO("%s:%s: Enter track_id(%d)", TAG, __func__, TrackId());

  assert(avcodec_.get() != nullptr);
  auto ret = avcodec_->ResumeCodec();
  // Initial debug purpose.
  assert(ret == NO_ERROR);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s:%s: track_id(%d) ResumeCodec failed!", TAG, __func__,
        TrackId());
    return ret;
  }

  QMMF_INFO("%s:%s: Exit track_id(%d)", TAG, __func__, TrackId());
  return ret;
}

status_t VideoTrackDecoder::SetVideoDecoderParams(CodecParamType param_type, void* param,
                                uint32_t param_size)
{
  QMMF_INFO("%s:%s: Enter track_id(%d)", TAG, __func__, TrackId());

  assert(avcodec_.get() != nullptr);
  auto ret = avcodec_->SetParameters(param_type,param,param_size);
  // Initial debug purpose.
  assert(ret == NO_ERROR);
  if (ret != NO_ERROR) {
   QMMF_ERROR("%s:%s: track_id(%d) ResumeCodec failed!", TAG, __func__,
       TrackId());
   return ret;
  }

  QMMF_INFO("%s:%s: Exit track_id(%d)", TAG, __func__, TrackId());
  return ret;
}


status_t VideoTrackDecoder::DeleteDecoder()
{
  QMMF_DEBUG("%s:%s: Enter", TAG, __func__);
  status_t ret = NO_ERROR;

  QMMF_DEBUG("%s:%s: Exit", TAG, __func__);
  return ret;
}


// this method provides an input buffer to the AVCodec
 status_t VideoTrackDecoder::Read(StreamBuffer& stream_buffer)
{


}

// this method is used by AVCodec to return buffer after encoding
 status_t VideoTrackDecoder::SignalBufferReturned(StreamBuffer& stream_buffer)
{


}

// this method is used by AVCodec to notify stop
 status_t VideoTrackDecoder::NotifyStatus(CodecInputPortStatus status)
{

}

status_t VideoTrackDecoder::AllocInputPortBufs()
{
  QMMF_INFO("%s:%s: Enter track_id(%d)", TAG, __func__, TrackId());
  int32_t ret = 0;
  uint32_t count, size;

  assert(avcodec_.get() != nullptr);
  ret = avcodec_->GetBufferRequirements(kPortIndexInput,  &count, &size);
  QMMF_DEBUG("%s:%s: BufferRequirements count(%d) size(%d)", TAG, __func__, count, size);
  assert(ret == NO_ERROR);
  //TODO: This hardcoding would be fixed by AVCodec layer
  count = 6;  //TODO

  assert(ion_device_ >= 0);
  int32_t ion_type = 0x1 << ION_IOMMU_HEAP_ID;
  void *vaddr      = NULL;

  struct ion_allocation_data alloc;
  struct ion_fd_data         ion_fddata;

  for(uint32_t i = 0; i < count; i++) {

    StreamBuffer buffer;
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

    //buffer.handle_data.handle = ion_fddata.handle;
    buffer.fd                 = ion_fddata.fd;
    buffer.size               = alloc.len;
    buffer.data               = vaddr;

    QMMF_INFO("%s:%s buffer.Fd(%d)", TAG, __func__, buffer.fd);
    QMMF_INFO("%s:%s buffer.size(%d)", TAG, __func__, buffer.size);
    QMMF_INFO("%s:%s buffer.vaddr(%p)", TAG, __func__, buffer.data);

    input_buffer_list_.push_back(buffer);
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

status_t VideoTrackDecoder::AllocOutputPortBufs()
{
    QMMF_INFO("%s:%s: Enter track_id(%d)", TAG, __func__, TrackId());
    int32_t ret = 0;
    uint32_t count, size;

    assert(avcodec_.get() != nullptr);
    ret = avcodec_->GetBufferRequirements(kPortIndexOutput,  &count, &size);
    QMMF_DEBUG("%s:%s: BufferRequirements count(%d) size(%d)", TAG, __func__, count, size);
    assert(ret == NO_ERROR);
    //TODO: This hardcoding would be fixed by AVCodec layer
    count = 11; //TODO

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


};//player
};//qmmf
