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

#define TAG "VideoSink"

#include "player/src/service/qmmf_player_video_sink.h"

#include <memory>

namespace qmmf {
namespace player {

using ::qmmf::avcodec::AVCodec;
using ::qmmf::avcodec::CodecBuffer;
using ::qmmf::avcodec::CodecParam;
using ::qmmf::avcodec::CodecPortStatus;
using ::std::make_shared;
using ::std::shared_ptr;

VideoSink* VideoSink::instance_ = nullptr;

VideoSink* VideoSink::CreateVideoSink()
{
  QMMF_DEBUG("%s:%s Enter ", TAG, __func__);

  if(!instance_) {
      instance_ = new VideoSink();
      if(!instance_) {
          QMMF_ERROR("%s:%s: Can't Create VideoSink Instance!", TAG, __func__);
          return NULL;
        }
      }

  QMMF_INFO("%s:%s: VideoSink Instance Created Successfully(0x%x)", TAG,
      __func__, instance_);

  QMMF_DEBUG("%s:%s Exit", TAG, __func__);
  return instance_;
}

VideoSink::VideoSink()
{
  QMMF_DEBUG("%s:%s Enter ", TAG, __func__);
  QMMF_DEBUG("%s:%s Exit", TAG, __func__);
}

VideoSink::~VideoSink()
{
  QMMF_DEBUG("%s:%s Enter ", TAG, __func__);
  if (!video_track_sinks.isEmpty()) {
    video_track_sinks.clear();
  }
  instance_ = NULL;
  QMMF_DEBUG("%s:%s Exit", TAG, __func__);
}

status_t VideoSink::CreateTrackSink(uint32_t track_id,
                                          VideoTrackParams& track_param)
{
  QMMF_DEBUG("%s:%s Enter ", TAG, __func__);
  shared_ptr<VideoTrackSink> track_sink;

  if (track_param.params.out_device == VideoOutSubtype::kHDMI)
    track_sink =  make_shared<VideoTrackSink>();

  video_track_sinks.add(track_id,track_sink);
  QMMF_DEBUG("%s:%s Exit", TAG, __func__);
}

const shared_ptr<VideoTrackSink>& VideoSink::GetTrackSink(uint32_t track_id)
{
  QMMF_DEBUG("%s:%s Enter ", TAG, __func__);
  int32_t idx = video_track_sinks.indexOfKey(track_id);
  assert(idx >= 0);
  return video_track_sinks.valueFor(track_id);
  QMMF_DEBUG("%s:%s Exit", TAG, __func__);
}

status_t VideoSink::StartTrackSink(uint32_t track_id)
{
  QMMF_DEBUG("%s:%s Enter ", TAG, __func__);
  shared_ptr<VideoTrackSink> track_sink = video_track_sinks.valueFor(track_id);
  assert(track_sink.get() != NULL);

  auto ret = track_sink->StartSink();
  if (ret != NO_ERROR) {
    QMMF_INFO("%s:%s: track_id(%d) StartSink failed!", TAG, __func__,
      track_id);
    return ret;
  }

  QMMF_INFO("%s:%s: track_id(%d) StartSink Successful!", TAG,
    __func__, track_id);

  QMMF_DEBUG("%s:%s Exit", TAG, __func__);
  return ret;
}

status_t VideoSink::StopTrackSink(uint32_t track_id)
{
  QMMF_DEBUG("%s:%s Enter ", TAG, __func__);
  shared_ptr<VideoTrackSink> track_sink = video_track_sinks.valueFor(track_id);
  assert(track_sink.get() != NULL);

  auto ret = track_sink->StopSink();
  if (ret != NO_ERROR) {
    QMMF_INFO("%s:%s: track_id(%d) StopSink failed!", TAG, __func__,
      track_id);
    return ret;
  }

  QMMF_INFO("%s:%s: track_id(%d) StopSink Successful!", TAG,
    __func__, track_id);

  QMMF_DEBUG("%s:%s Exit", TAG, __func__);
  return ret;
}

status_t VideoSink::DeleteTrackSink(uint32_t track_id)
{
  QMMF_DEBUG("%s:%s Enter ", TAG, __func__);
  shared_ptr<VideoTrackSink> track_sink = video_track_sinks.valueFor(track_id);
  assert(track_sink.get() != NULL);

  auto ret = track_sink->DeleteSink();
  if (ret != NO_ERROR) {
    QMMF_INFO("%s:%s: track_id(%d) DeleteSink failed!", TAG, __func__,
      track_id);
    return ret;
  }

  video_track_sinks.removeItem(track_id);

  QMMF_INFO("%s:%s: track_id(%d) DeleteSink Successful!", TAG,
    __func__, track_id);

  QMMF_DEBUG("%s:%s Exit", TAG, __func__);
  return ret;
}

VideoTrackSink::VideoTrackSink():stopplayback_(false)
{
  QMMF_DEBUG("%s:%s Enter ", TAG, __func__);
#ifdef DUMP_YUV_FRAMES
  file_fd_ = open("/data/video_track.yuv", O_CREAT | O_WRONLY | O_TRUNC, 0655);
  if(file_fd_ < 0) {
    QMMF_ERROR("%s:%s Failed to open o/p yuv dump file ", TAG, __func__);
  }
#endif
  QMMF_DEBUG("%s:%s Exit", TAG, __func__);
}


VideoTrackSink::~VideoTrackSink()
{
  QMMF_DEBUG("%s:%s Enter ", TAG, __func__);
  QMMF_DEBUG("%s:%s Exit", TAG, __func__);
}

status_t VideoTrackSink::Init(VideoTrackParams& track_param)
{
  QMMF_INFO("%s:%s: Enter track_id(%d)", TAG, __func__, track_param.track_id);

  track_params_.track_id = track_param.track_id;


  QMMF_INFO("%s:%s: Exit track_id(%d)", TAG, __func__, TrackId());

  return NO_ERROR;
}

status_t VideoTrackSink::StartSink()
{
  QMMF_DEBUG("%s:%s: Enter track_id(%d)", TAG, __func__, TrackId());
  auto ret = 0;
  stopplayback_ = false;
  QMMF_DEBUG("%s:%s: Exit track_id(%d)", TAG, __func__, TrackId());
  return ret;
}

status_t VideoTrackSink::StopSink()
{
  QMMF_DEBUG("%s:%s: Enter track_id(%d)", TAG, __func__, TrackId());
  auto ret = 0;
  stopplayback_ = true;
  QMMF_DEBUG("%s:%s: Exit track_id(%d)", TAG, __func__, TrackId());
  return ret;
}

status_t VideoTrackSink::DeleteSink()
{
  QMMF_DEBUG("%s:%s: Enter track_id(%d)", TAG, __func__, TrackId());
  auto ret = 0;

  QMMF_DEBUG("%s:%s: Exit track_id(%d)", TAG, __func__, TrackId());
  return ret;
}

void VideoTrackSink::AddBufferList(Vector<CodecBuffer>& list) {
  QMMF_DEBUG("%s:%s: Enter track_id(%d)", TAG, __func__, TrackId());

  output_buffer_list_ = list;
  output_free_buffer_queue_.Clear();
  output_occupy_buffer_queue_.Clear();

  //decoded buffer queue
  for(auto& iter : output_buffer_list_) {
          QMMF_INFO("%s:%s: track_id(%d) Adding buffer fd(%d) to "
              "output_free_buffer_queue_", TAG, __func__,TrackId() ,
              iter.fd);
          output_free_buffer_queue_.PushBack(iter);
          BufInfo bufinfo_temp;
          bufinfo_temp.vaddr = iter.pointer;
          buf_info_map.add(iter.fd,bufinfo_temp);
  }

    for(uint32_t j = 0; j < buf_info_map.size(); j++) {
      QMMF_VERBOSE("%s:%s: buf_info_map:idx(%d) :key(%d) :fd:%d :data:"
          "0x%x", TAG, __func__, j, buf_info_map.keyAt(j), buf_info_map[j].buf_id,
          buf_info_map[j].vaddr);
    }

  QMMF_DEBUG("%s:%s: Exit track_id(%d)", TAG, __func__, TrackId());
}

status_t VideoTrackSink::GetBuffer(BufferDescriptor& codec_buffer,
                                   void* client_data)
{
  QMMF_DEBUG("%s:%s: Enter track_id(%d)", TAG, __func__, TrackId());
  // Give available free buffer to decoder to use on output port.

  if(output_free_buffer_queue_.Size() <= 0) {
    QMMF_DEBUG("%s:%s track_id(%d) No buffer available to notify,"
      " Wait for new buffer", TAG, __func__, TrackId());
    Mutex::Autolock autoLock(wait_for_frame_lock_);
    wait_for_frame_.wait(wait_for_frame_lock_);
  }

  CodecBuffer iter = *output_free_buffer_queue_.Begin();
  codec_buffer.fd = (iter).fd;
  codec_buffer.data = (iter).pointer;
  output_free_buffer_queue_.Erase(output_free_buffer_queue_.Begin());
  {
    Mutex::Autolock lock(queue_lock_);
    output_occupy_buffer_queue_.PushBack(iter);
  }
  QMMF_DEBUG("%s:%s track_id(%d) Sending buffer(0x%x) fd(%d) for FTB", TAG,
      __func__, TrackId(), codec_buffer.data, codec_buffer.fd);

  QMMF_DEBUG("%s:%s: Exit track_id(%d)", TAG, __func__, TrackId());
  return NO_ERROR;
}

status_t VideoTrackSink::ReturnBuffer(BufferDescriptor& codec_buffer,
                                      void* client_data)
{
  QMMF_DEBUG("%s:%s: Enter track_id(%d)", TAG, __func__, TrackId());
  status_t ret = 0;

  assert(codec_buffer.data != NULL);

  QMMF_VERBOSE("%s:%s: track_id(%d) Received buffer(0x%x) from FBD", TAG,
     __func__, TrackId(), codec_buffer.data);

#ifdef DUMP_YUV_FRAMES
  DumpYUVData(codec_buffer);
#endif


  List<CodecBuffer>::iterator it = output_occupy_buffer_queue_.Begin();
  bool found = false;
  for (; it != output_occupy_buffer_queue_.End(); ++it) {
    QMMF_VERBOSE("%s:%s track_id(%d) Checking match %d vs %d ", TAG,
       __func__, TrackId(), (*it).fd,  codec_buffer.fd);
   if (((*it).fd) == (codec_buffer.fd)) {
     QMMF_VERBOSE("%s:%s track_id(%d) Buffer found", TAG, __func__, TrackId());
     output_free_buffer_queue_.PushBack(*it);
     output_occupy_buffer_queue_.Erase(it);
     wait_for_frame_.signal();
     found = true;
     break;
   }
  }
  assert(found == true);

  QMMF_DEBUG("%s:%s: Exit track_id(%d)", TAG, __func__, TrackId());
  return ret;
}

status_t VideoTrackSink::NotifyPortStatus(CodecPortStatus status)
{
  QMMF_DEBUG("%s:%s Enter track_id(%d)", TAG, __func__, TrackId());

  QMMF_DEBUG("%s:%s Exit track_id(%d)", TAG, __func__, TrackId());
  return NO_ERROR;
}

#ifdef DUMP_YUV_FRAMES
void VideoTrackSink::DumpYUVData(BufferDescriptor& codec_buffer) {
  QMMF_VERBOSE("%s:%s: Enter track_id(%d)", TAG, __func__, TrackId());
  /*if(eos_atoutput_) {
    return;
  }*/

  if (file_fd_ > 0) {
    ssize_t exp_size = (ssize_t) codec_buffer.size;
    QMMF_INFO("%s:%s Got decoded buffer of size(%d)", TAG, __func__,
        codec_buffer.size);

    BufInfo bufinfo;
    memset(&bufinfo, 0x0, sizeof bufinfo);

    bufinfo = buf_info_map.valueFor(codec_buffer.fd);

    uint8_t*vaddr = static_cast<uint8_t*>(bufinfo.vaddr);

    uint32_t bytes_written;

    uint8_t  *pSrc = vaddr;
    bytes_written  = write(file_fd_, pSrc,
        (track_params_.params.height*track_params_.params.width*3)/2);
    if(bytes_written != codec_buffer.size) {
      QMMF_ERROR("Bytes written != %d and written = %u",codec_buffer.size,
          bytes_written);
    }
  }

  if(codec_buffer.flag & OMX_BUFFERFLAG_EOS) {
    QMMF_ERROR("%s:%s This is last buffer from decoder.close file", TAG,
        __func__);
    close(file_fd_);
    file_fd_ = -1;
  }

  QMMF_VERBOSE("%s:%s: Exit track_id(%d)", TAG, __func__, TrackId());
}
#endif

};  // namespace player
};  // namespace qmmf
