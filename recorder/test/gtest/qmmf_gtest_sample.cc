/*
* Copyright (c) 2016-2021, The Linux Foundation. All rights reserved.
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

#define LOG_TAG "VideoGTest"

#include "recorder/test/gtest/qmmf_gtest.h"

using namespace qcamera;


/*
* SessionWithSingleStream:
*   This test will test Single stream of a configurable
*   resolution and format.
*   If EIS and SHDR is enabled, then they are also applied.
*   If Snapshot stream is on, Snapshot will also be taken.
* API test sequence:
*  - StartCamera [Check for EIS, SHDR]
*  - CreateSession
*  - CreateVideoTrack
*  - StartSession
*  - Check for SnapShot stream
*  - If Snapshot Stream  is on
*  - { ConfigImageCapture
*  -   CaptureImage
*  -   CancelCaptureImage }
*  - StopSession
*  - DeleteVideoTrack
*  - DeleteSession
*  - StopCamera
*/

TEST_F(VideoGtest, SessionWithSingleStream) {
  std::cout << "\n---------- Run Test ----------" <<
      test_info_->test_case_name() << "." << test_info_->name()<< std::endl;

  auto ret = Init();
  ASSERT_TRUE(ret == NO_ERROR);

  // Extract Parameter of First Video Stream.
  uint32_t video_track_1 = kFirstStreamID;
  VideoStreamInfo stream = stream_info_map_[video_track_1];
  uint32_t width = stream.width;
  uint32_t height = stream.height;
  uint32_t linkStatus = 0;
  VideoFormat format = stream.format;
  float fps = stream.fps;

  CameraExtraParam camera_extra_param;

  SetCameraExtraParam(camera_extra_param);

  ret = recorder_.StartCamera(camera_id_, camera_fps_, camera_extra_param);
  ASSERT_TRUE(ret == NO_ERROR);
  ret =  recorder_.GetCameraParam(camera_id_, static_info_);
  ASSERT_TRUE(ret == NO_ERROR);
  linkStatus = GetLinkStatus();
  if(linkStatus == 0xF){
    height = height * 4;
  }else if (linkStatus == 0xE || linkStatus == 0xD ||
            linkStatus == 0xB || linkStatus == 0x7){
    height = height * 3;
  }else if(linkStatus == 0xC || linkStatus == 0xA ||
           linkStatus == 0x9 || linkStatus == 0x6 ||
           linkStatus == 0x5 || linkStatus == 0x3){
    height = height * 2;
  }
  stream.width = height;
  snap_height_ = height;
  stream_info_map_.emplace(kFirstStreamID, stream);
  PrintStreamInfo(kFirstStreamID);

  for (uint32_t i = 1; i <= iteration_count_; i++) {
    std::cout
        << "###############################################################"
        << std::endl;

    std::cout << "Ruunnig Test Iteration: " << i << "/" << iteration_count_
              << std::endl;

    TEST_INFO("%s: Running Test(%s) iteration = %d ", __func__,
              test_info_->name(), i);

    SessionCb session_status_cb = CreateSessionStatusCb();
    uint32_t session_id;

    ret = recorder_.CreateSession(session_status_cb, &session_id);
    ASSERT_TRUE(session_id > 0);
    ASSERT_TRUE(ret == NO_ERROR);

    if (dump_bitstream_.IsEnabled() &&
        (format == VideoFormat::kAVC || format == VideoFormat::kHEVC)) {
      // Dump Encoded Streams
      StreamDumpInfo dumpinfo = {format, session_id, video_track_1, width,
                                 height};
      ret = dump_bitstream_.SetUp(dumpinfo);
      ASSERT_TRUE(ret == NO_ERROR);
    }

    // Configure Single Video Stream
    VideoTrackCreateParam video_track_param{camera_id_, format, width, height,
                                            fps};

    TrackCb video_track_cb;
    video_track_cb.data_cb = [&, session_id](
        uint32_t track_id, std::vector<BufferDescriptor> buffers,
        std::vector<MetaData> meta_buffers) {
      if (format == VideoFormat::kYUY2) {
        VideoTrackYUVDataCb(session_id, track_id, buffers, meta_buffers);
      } else {
        VideoTrackEncDataCb(session_id, track_id, buffers, meta_buffers);
      }
    };

    video_track_cb.event_cb = [&](uint32_t track_id, EventType event_type,
                                  void *event_data, size_t event_data_size) {
      VideoTrackEventCb(track_id, event_type, event_data, event_data_size);
    };

    ret = recorder_.CreateVideoTrack(session_id, video_track_1,
                                     video_track_param, video_track_cb);
    ASSERT_TRUE(ret == NO_ERROR);

    std::vector<uint32_t> track_ids;
    track_ids.push_back(video_track_1);
    sessions_.insert(std::make_pair(session_id, track_ids));

    // Configure Snapshot Stream.
    if (is_snap_stream_on_) {
      ConfigureImageParam();
    }

    // Start Session
    ret = recorder_.StartSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    sleep(record_duration_);

    // Now Take Snapshot.
    if (is_snap_stream_on_) {
      TakeSnapshot();
    }

    ret = recorder_.StopSession(session_id, false);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteVideoTrack(session_id, video_track_1);
    ASSERT_TRUE(ret == NO_ERROR);

    ret = recorder_.DeleteSession(session_id);
    ASSERT_TRUE(ret == NO_ERROR);

    ClearSessions();
    dump_bitstream_.CloseAll();
  }

  ret = recorder_.StopCamera(camera_id_);
  ASSERT_TRUE(ret == NO_ERROR);

  ret = DeInit();
  ASSERT_TRUE(ret == NO_ERROR);

  std::cout <<"---------- Test Completed ----------\n" <<
      test_info_->test_case_name() << "." << test_info_->name();
}

