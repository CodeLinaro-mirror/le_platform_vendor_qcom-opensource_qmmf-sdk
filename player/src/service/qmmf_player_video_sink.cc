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
#define ALIGNED_WIDTH(x) ((x)+((x%64)?(64-(x%64)):0))

namespace qmmf {
namespace player {

using ::qmmf::avcodec::AVCodec;
using ::qmmf::avcodec::CodecBuffer;
using ::qmmf::avcodec::CodecParam;
using ::qmmf::avcodec::CodecPortStatus;
using ::std::make_shared;
using ::std::shared_ptr;

VideoSink* VideoSink::instance_ = nullptr;

VideoSink* VideoSink::CreateVideoSink() {
  QMMF_DEBUG("%s:%s Enter ", TAG, __func__);

  if(!instance_) {
      instance_ = new VideoSink();
      if(!instance_) {
          QMMF_ERROR("%s:%s: Can't Create VideoSink Instance!", TAG, __func__);
          return NULL;
        }
      }

  QMMF_INFO("%s:%s: VideoSink Instance Created Successfully(0x%p)", TAG,
      __func__, instance_);

  QMMF_DEBUG("%s:%s Exit", TAG, __func__);
  return instance_;
}

VideoSink::VideoSink() {
  QMMF_DEBUG("%s:%s Enter ", TAG, __func__);
  QMMF_DEBUG("%s:%s Exit", TAG, __func__);
}

VideoSink::~VideoSink() {
  QMMF_DEBUG("%s:%s Enter ", TAG, __func__);
  if (!video_track_sinks.isEmpty()) {
    video_track_sinks.clear();
  }
  instance_ = NULL;
  QMMF_DEBUG("%s:%s Exit", TAG, __func__);
}

status_t VideoSink::CreateTrackSink(uint32_t track_id,
                                    VideoTrackParams& track_param) {
  QMMF_DEBUG("%s:%s Enter ", TAG, __func__);
  shared_ptr<VideoTrackSink> track_sink;

  if (track_param.params.out_device == VideoOutSubtype::kHDMI)
    track_sink =  make_shared<VideoTrackSink>();

  video_track_sinks.add(track_id,track_sink);
  track_sink->Init(track_param);

  QMMF_DEBUG("%s:%s Exit", TAG, __func__);
  return NO_ERROR;
}

const shared_ptr<VideoTrackSink>& VideoSink::GetTrackSink(
    uint32_t track_id) {
  QMMF_DEBUG("%s:%s Enter ", TAG, __func__);
  int32_t idx = video_track_sinks.indexOfKey(track_id);
  assert(idx >= 0);
  return video_track_sinks.valueFor(track_id);
  QMMF_DEBUG("%s:%s Exit", TAG, __func__);
}

status_t VideoSink::StartTrackSink(uint32_t track_id) {
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

status_t VideoSink::StopTrackSink(uint32_t track_id) {
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

status_t VideoSink::DeleteTrackSink(uint32_t track_id) {
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

VideoTrackSink::VideoTrackSink()
    : stopplayback_(false), paused_(false),
      decoded_frame_number_(0), display_started_(0),
      playback_speed_(TrickModeSpeed::kSpeed_1x),
      playback_dir_(TrickModeDirection::kForward),
      current_time_(0), prev_time_(0),
      displayed_frames_(0) {
  QMMF_DEBUG("%s:%s Enter ", TAG, __func__);
#ifdef DUMP_YUV_FRAMES
  file_fd_ = open("/data/video_track.yuv", O_CREAT | O_WRONLY | O_TRUNC, 0655);
  if(file_fd_ < 0) {
    QMMF_ERROR("%s:%s Failed to open o/p yuv dump file ", TAG, __func__);
  }
#endif
  QMMF_DEBUG("%s:%s Exit", TAG, __func__);
}

VideoTrackSink::~VideoTrackSink() {
  QMMF_DEBUG("%s:%s Enter ", TAG, __func__);
  QMMF_DEBUG("%s:%s Exit", TAG, __func__);
}

status_t VideoTrackSink::Init(VideoTrackParams& track_param) {
  QMMF_INFO("%s:%s: Enter track_id(%d)", TAG, __func__, track_param.track_id);

  track_params_.track_id = track_param.track_id;
  track_params_.params = track_param.params;

  auto ret = CreateDisplay(display::DisplayType::kPrimary, track_param);
  if (ret != 0) {
    QMMF_ERROR("%s:%s CreateDisplay Failed!!", TAG, __func__);
    return ret;
  }

  QMMF_INFO("%s:%s: Exit track_id(%d)", TAG, __func__, TrackId());

  return NO_ERROR;
}

status_t VideoTrackSink::StartSink() {
  QMMF_DEBUG("%s:%s: Enter track_id(%d)", TAG, __func__, TrackId());
  auto ret = 0;
  stopplayback_ = false;
  QMMF_DEBUG("%s:%s: Exit track_id(%d)", TAG, __func__, TrackId());
  return ret;
}

status_t VideoTrackSink::StopSink() {
  QMMF_DEBUG("%s:%s: Enter track_id(%d)", TAG, __func__, TrackId());
  auto ret = 0;
  stopplayback_ = true;
  QMMF_DEBUG("%s:%s: Total number of video frames decoded %d", TAG, __func__,
      decoded_frame_number_);
  QMMF_DEBUG("%s:%s: Total number of video frames displayed %d", TAG, __func__,
       displayed_frames_);

  decoded_frame_number_ = 0;
  displayed_frames_ = 0;
  current_time_ = 0;
  prev_time_ = 0;

  QMMF_DEBUG("%s:%s: Exit track_id(%d)", TAG, __func__, TrackId());
  return ret;
}

status_t VideoTrackSink::PauseSink() {
  QMMF_DEBUG("%s:%s: Enter track_id(%d)", TAG, __func__, TrackId());
  auto ret = 0;

  paused_ = true;

  QMMF_DEBUG("%s:%s: Exit track_id(%d)", TAG, __func__, TrackId());
  return ret;
}

status_t VideoTrackSink::ResumeSink() {
  QMMF_DEBUG("%s:%s: Enter track_id(%d)", TAG, __func__, TrackId());
  auto ret = 0;

  paused_ = false;

  QMMF_DEBUG("%s:%s: Exit track_id(%d)", TAG, __func__, TrackId());
  return ret;
}

status_t VideoTrackSink::DeleteSink() {
  QMMF_DEBUG("%s:%s: Enter track_id(%d)", TAG, __func__, TrackId());
  auto ret = 0;

  ret = DeleteDisplay(display::DisplayType::kPrimary);
  if (ret != 0) {
    QMMF_ERROR("%s:%s DeleteDisplay Failed!!", TAG, __func__);
    return ret;
  }

  QMMF_DEBUG("%s:%s: Exit track_id(%d)", TAG, __func__, TrackId());
  return ret;
}

status_t VideoTrackSink::SetTrickMode(TrickModeSpeed speed,
                                      TrickModeDirection direction) {
  QMMF_DEBUG("%s:%s: Enter track_id(%d)", TAG, __func__, TrackId());
  QMMF_DEBUG("%s:%s: Speed (%u) Dir (%u)", TAG, __func__,
      static_cast<uint32_t>(speed), static_cast<uint32_t>(direction));

  playback_speed_ = speed;
  playback_dir_ = direction;

  QMMF_DEBUG("%s:%s: Exit track_id(%d)", TAG, __func__, TrackId());
  return NO_ERROR;
}

void VideoTrackSink::AddBufferList(Vector<CodecBuffer>& list) {
  QMMF_DEBUG("%s:%s: Enter track_id(%d)", TAG, __func__, TrackId());

  output_buffer_list_ = list;
  output_free_buffer_queue_.Clear();
  output_occupy_buffer_queue_.Clear();

  //decoded buffer queue
  for (auto& iter : output_buffer_list_) {
    QMMF_INFO("%s:%s: track_id(%d) Adding buffer fd(%d) to "
        "output_free_buffer_queue_", TAG, __func__,TrackId() ,
        iter.fd);
    output_free_buffer_queue_.PushBack(iter);
    BufInfo bufinfo_temp;
    bufinfo_temp.vaddr = iter.pointer;
    buf_info_map.add(iter.fd,bufinfo_temp);
  }

  for (uint32_t j = 0; j < buf_info_map.size(); j++) {
    QMMF_VERBOSE("%s:%s: buf_info_map:idx(%d) :key(%d) :fd:%d :data:"
        "0x%p", TAG, __func__, j, buf_info_map.keyAt(j), buf_info_map[j].buf_id,
        buf_info_map[j].vaddr);
  }

  QMMF_DEBUG("%s:%s: Exit track_id(%d)", TAG, __func__, TrackId());
}

status_t VideoTrackSink::GetBuffer(BufferDescriptor& codec_buffer,
                                   void* client_data) {
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
  QMMF_DEBUG("%s:%s track_id(%d) Sending buffer(0x%p) fd(%d) for FTB", TAG,
      __func__, TrackId(), codec_buffer.data, codec_buffer.fd);

  QMMF_DEBUG("%s:%s: Exit track_id(%d)", TAG, __func__, TrackId());
  return NO_ERROR;
}

status_t VideoTrackSink::ReturnBuffer(BufferDescriptor& codec_buffer,
                                      void* client_data) {
  QMMF_DEBUG("%s:%s: Enter track_id(%d)", TAG, __func__, TrackId());
  status_t ret = 0;

  assert(codec_buffer.data != NULL);

  QMMF_VERBOSE("%s:%s: track_id(%d) Received buffer(0x%p) from FBD", TAG,
     __func__, TrackId(), codec_buffer.data);

  if (!(stopplayback_ || (codec_buffer.flag & OMX_BUFFERFLAG_EOS) ||
      !(codec_buffer.size) || paused_)) {
    ++decoded_frame_number_;

    QMMF_DEBUG("%s:%s: track_id(%d) For decoded video frame number %d"
        " timestamps is %llu ",TAG, __func__, TrackId(), decoded_frame_number_,
        codec_buffer.timestamp);
      current_time_ = codec_buffer.timestamp;

 #ifdef DUMP_YUV_FRAMES
    DumpYUVData(codec_buffer);
#endif

    if (SkipFrame()) {
      QMMF_DEBUG("%s:%s:Skipping frame number %d to display", TAG, __func__,
           decoded_frame_number_);
      prev_time_ = current_time_;
      ReturnBufferToCodec(codec_buffer);
      return NO_ERROR;
    } else {

      if (prev_time_ != 0) {
        QMMF_VERBOSE("%s:%s Sleeping for %0.2f ms", TAG, __func__,
            (float)((current_time_ - prev_time_)/(float)1000));
        usleep(current_time_ - prev_time_);
      }

      ret = PushFrameToDisplay(codec_buffer);
      if (ret != 0) {
        QMMF_ERROR("%s:%s PushFrameToDisplay Failed!!", TAG, __func__);
      }

      ++displayed_frames_;
      QMMF_DEBUG("%s:%s: track_id(%d) displayed video frame number %d",
          TAG, __func__, TrackId(), decoded_frame_number_);
    }
    prev_time_ = current_time_;
  }

  ReturnBufferToCodec(codec_buffer);

  QMMF_DEBUG("%s:%s: Exit track_id(%d)", TAG, __func__, TrackId());
  return ret;
}

status_t VideoTrackSink::ReturnBufferToCodec(
    BufferDescriptor& codec_buffer) {
  QMMF_DEBUG("%s:%s: Enter track_id(%d)", TAG, __func__, TrackId());

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
  return NO_ERROR;
}

status_t VideoTrackSink::SkipFrame() {
  QMMF_DEBUG("%s:%s: Enter track_id(%d)", TAG, __func__, TrackId());

  if (playback_speed_ == TrickModeSpeed::kSpeed_1x) {
    return false;

  } else if (playback_speed_ == TrickModeSpeed::kSpeed_2x) {
    return (((decoded_frame_number_%2) == 0)? false: true);

  } else if (playback_speed_ == TrickModeSpeed::kSpeed_4x) {
    return (((decoded_frame_number_%4) == 0)? false: true);

  } else if (playback_speed_ == TrickModeSpeed::kSpeed_8x) {
    return (((decoded_frame_number_%8) == 0)? false: true);
  }

  QMMF_DEBUG("%s:%s: Exit track_id(%d)", TAG, __func__, TrackId());
  return NO_ERROR;
}

status_t VideoTrackSink::NotifyPortStatus(CodecPortStatus status) {
  QMMF_DEBUG("%s:%s Enter track_id(%d)", TAG, __func__, TrackId());

  QMMF_DEBUG("%s:%s Exit track_id(%d)", TAG, __func__, TrackId());
  return NO_ERROR;
}

status_t VideoTrackSink::CreateDisplay(
    display::DisplayType display_type,
    VideoTrackParams& track_param) {
  QMMF_INFO("%s:%s: Enter", TAG, __func__);
  int32_t res;
  DisplayCb  display_status_cb;

  display_= new Display();
  assert(display_ != nullptr);
  res = display_->Connect();
  if (res != 0) {
    QMMF_ERROR("%s:%s Connect Failed!!", TAG, __func__);
    delete display_;
    display_ = nullptr;
    return res;
  }

  display_status_cb.EventCb = [&] ( DisplayEventType event_type,
      void *event_data, size_t event_data_size) { DisplayCallbackHandler
      (event_type, event_data, event_data_size); };

  display_status_cb.VSyncCb = [&] ( int64_t time_stamp)
      { DisplayVSyncHandler(time_stamp); };

  res = display_->CreateDisplay(display_type, display_status_cb);
  if (res != 0) {
    QMMF_ERROR("%s:%s CreateDisplay Failed!!", TAG, __func__);
    display_->Disconnect();
    delete display_;
    display_ = nullptr;
    return res;
  }

  memset(&surface_config, 0x0, sizeof surface_config);

  surface_config.width = track_param.params.width;
  surface_config.height = track_param.params.height;
  surface_config.format = SurfaceFormat::kFormatYCbCr420SPVenusUbwc;
  surface_config.buffer_count = track_param.params.num_buffers;
  surface_config.cache = 0;
  surface_config.use_buffer = 1;
  res = display_->CreateSurface(surface_config, &surface_id_);
  if (res != 0) {
    QMMF_ERROR("%s:%s CreateSurface Failed!!", TAG, __func__);
    DeleteDisplay(display_type);
    return res;
  }
  display_started_ = 1;

  surface_param_.src_rect = { 0.0, 0.0, (float)track_param.params.width,
      (float)track_param.params.height };
  surface_param_.dst_rect = { 0.0, 0.0, (float)track_param.params.width,
      (float)track_param.params.height };
  surface_param_.surface_blending =
      SurfaceBlending::kBlendingCoverage;
  surface_param_.surface_flags.cursor = 0;
  surface_param_.frame_rate=track_param.params.frame_rate;
  surface_param_.z_order = 0;
  surface_param_.solid_fill_color = 0;
  surface_param_.surface_transform.rotation = 0.0f;
  surface_param_.surface_transform.flip_horizontal = 0;
  surface_param_.surface_transform.flip_vertical = 0;

  QMMF_INFO("%s:%s: Exit", TAG, __func__);
  return res;

}

status_t VideoTrackSink::DeleteDisplay(display::DisplayType display_type) {
  QMMF_INFO("%s:%s: Enter", TAG, __func__);
  int32_t res = 0;

  if (display_started_ == 1) {
    display_started_ =0;
    res = display_->DestroySurface(surface_id_);
    if (res != 0) {
      QMMF_ERROR("%s:%s DestroySurface Failed!!", TAG, __func__);
    }

    res = display_->DestroyDisplay(display_type);
    if (res != 0) {
      QMMF_ERROR("%s:%s DestroyDisplay Failed!!", TAG, __func__);
    }

    res = display_->Disconnect();

    if (display_ != nullptr) {
      QMMF_INFO("%s:%s: DELETE display_:%p", TAG, __func__, display_);
      delete display_;
      display_ = nullptr;
    }
  }
  QMMF_INFO("%s:%s: Exit", TAG, __func__);
  return res;
}

void VideoTrackSink::DisplayCallbackHandler(DisplayEventType
    event_type, void *event_data, size_t event_data_size) {
  QMMF_DEBUG("%s:%s Enter ", TAG, __func__);
  QMMF_DEBUG("%s:%s Exit ", TAG, __func__);
}

void VideoTrackSink::DisplayVSyncHandler(int64_t time_stamp) {
  QMMF_VERBOSE("%s:%s: Enter", TAG, __func__);
  QMMF_VERBOSE("%s:%s: Exit", TAG, __func__);
}

status_t VideoTrackSink::PushFrameToDisplay(BufferDescriptor& codec_buffer) {
  BufInfo bufinfo;
  memset(&bufinfo, 0x0, sizeof bufinfo);

  bufinfo = buf_info_map.valueFor(codec_buffer.fd);

   uint8_t*vaddr = (uint8_t*)bufinfo.vaddr;
   if (display_started_) {
     surface_buffer_.plane_info[0].ion_fd = codec_buffer.fd;
     surface_buffer_.buf_id =0;
     surface_buffer_.format = SurfaceFormat::kFormatYCbCr420SPVenusUbwc;
     surface_buffer_.plane_info[0].stride = ALIGNED_WIDTH(surface_config.width);
     surface_buffer_.plane_info[0].size = codec_buffer.size;
     surface_buffer_.plane_info[0].width = surface_config.width;
     surface_buffer_.plane_info[0].height = surface_config.height;
     surface_buffer_.plane_info[0].offset = codec_buffer.offset;
     surface_buffer_.plane_info[0].buf = (void*)vaddr;

     auto ret = display_->QueueSurfaceBuffer(surface_id_, surface_buffer_,
         surface_param_);
     if(ret != 0) {
       QMMF_ERROR("%s:%s QueueSurfaceBuffer Failed!!", TAG, __func__);
       return ret;
     }

     ret = display_->DequeueSurfaceBuffer( surface_id_, surface_buffer_);
     if(ret != 0) {
       QMMF_ERROR("%s:%s DequeueSurfaceBuffer Failed!!", TAG, __func__);
       return ret;
     }
   }

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
    uint32_t size = (track_params_.params.height*track_params_.params.width*3)/2;

    bytes_written  = write(file_fd_, pSrc, size);
    if(bytes_written != size) {
      QMMF_ERROR("Bytes written != %u and written = %u", size, bytes_written);
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
