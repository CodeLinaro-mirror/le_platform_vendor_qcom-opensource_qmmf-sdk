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

#include <memory>

#include "common/codecadaptor/src/qmmf_avcodec.h"
#include "player/src/service/qmmf_player_common.h"
#include <utils/KeyedVector.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/msm_ion.h>
#include <unistd.h>
#include "qmmf-sdk/qmmf_display.h"
#include "qmmf-sdk/qmmf_display_params.h"

using ::qmmf::display::DisplayEventType;
using ::qmmf::display::DisplayType;
using ::qmmf::display::Display;
using ::qmmf::display::DisplayCb;
using ::qmmf::display::SurfaceBuffer;
using ::qmmf::display::SurfaceParam;
using ::qmmf::display::SurfaceConfig;
using ::qmmf::display::SurfaceBlending;
using ::qmmf::display::SurfaceFormat;

namespace qmmf {
namespace player {

class VideoTrackSink;

class VideoSink {
 public:

  static VideoSink* CreateVideoSink();

  ~VideoSink();

  status_t CreateTrackSink(uint32_t track_id,
                                  VideoTrackParams& track_param);

  const ::std::shared_ptr<VideoTrackSink>& GetTrackSink(uint32_t track_id);

  status_t StartTrackSink(uint32_t track_id);

  status_t StopTrackSink(uint32_t track_id);

  status_t DeleteTrackSink(uint32_t track_id);

 private:
  VideoSink();

  static VideoSink* instance_;

  // Map of track it and TrackSinks.
  DefaultKeyedVector<uint32_t, ::std::shared_ptr<VideoTrackSink>> video_track_sinks;
};


class VideoTrackSink : public ::qmmf::avcodec::ICodecSource {
 public:

  VideoTrackSink();

  ~VideoTrackSink();

  status_t Init(VideoTrackParams& param);

  status_t StartSink();

  status_t StopSink();

  status_t PauseSink();

  status_t ResumeSink();

  status_t DeleteSink();

  status_t SetTrickMode(TrickModeSpeed speed, TrickModeDirection direction);

  void AddBufferList(Vector<::qmmf::avcodec::CodecBuffer>& list);

  status_t GetBuffer(BufferDescriptor& codec_buffer,
                     void* client_data) override;

  status_t ReturnBuffer(BufferDescriptor& codec_buffer,
                        void* client_data) override;

  status_t NotifyPortStatus(::qmmf::avcodec::CodecPortStatus status) override;

  status_t CreateDisplay(display::DisplayType display_type,
      VideoTrackParams& track_param);

  status_t DeleteDisplay(display::DisplayType display_type);

  void DisplayCallbackHandler(display::DisplayEventType event_type,
      void *event_data, size_t event_data_size);

  void DisplayVSyncHandler(int64_t time_stamp);

 private:

  int32_t TrackId() { return track_params_.track_id; }

  VideoTrackParams        track_params_;

  Vector<::qmmf::avcodec::CodecBuffer>  output_buffer_list_;
  TSQueue<::qmmf::avcodec::CodecBuffer> output_free_buffer_queue_;
  TSQueue<::qmmf::avcodec::CodecBuffer> output_occupy_buffer_queue_;

  Mutex                   wait_for_frame_lock_;
  Condition               wait_for_frame_;
  Mutex                   queue_lock_;
  bool                    stopplayback_;
  bool                    paused_;
  uint32_t                decoded_frame_number_;

  Display*   display_;
  uint32_t   surface_id_;
  SurfaceParam surface_param_;
  SurfaceBuffer surface_buffer_;
  SurfaceConfig surface_config;
  bool display_started_;

  typedef struct BufInfo {
    // FD at service
    uint32_t buf_id;

    // Memory mapped buffer.
    void*    vaddr;
  } BufInfo;

  //map<fd , buf_info>
  DefaultKeyedVector<int32_t, BufInfo> buf_info_map;

#ifdef DUMP_YUV_FRAMES
  int32_t               file_fd_;
  void DumpYUVData(BufferDescriptor& codec_buffer);
#endif

  status_t PushFrameToDisplay(BufferDescriptor& codec_buffer);

  status_t SkipFrame();

  status_t ReturnBufferToCodec(BufferDescriptor& codec_buffer);

  TrickModeSpeed           playback_speed_;
  TrickModeDirection       playback_dir_;
  uint64_t                 current_time_;
  uint64_t                 prev_time_;
  uint32_t                 displayed_frames_;
};

};  // namespace player
};  // namespace qmmf
