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

#include <fcntl.h>
#include <dirent.h>
#include <functional>
#include <gtest/gtest.h>
#include <vector>
#include <map>
#include <cutils/properties.h>

#include <qmmf-sdk/qmmf_recorder.h>
#include <qmmf-sdk/qmmf_recorder_params.h>

using namespace qmmf;
using namespace recorder;
using namespace android;

#define DEFAULT_YUV_DUMP_FREQ  "200"
#define DEFAULT_ITERATIONS     "50"

// Prop to enable YUV data dumping from YUV track
#define PROP_DUMP_YUV_FRAMES   "persist.qmmf.recgtest.dumpyuv"
// Prop to enable encoded bitstream data dumping
#define PROP_DUMP_BITSTREAM    "persist.qmmf.recgtest.dumpstrm"
// Prop to set frequency of YUV data dumping
#define PROP_DUMP_YUV_FREQ     "persist.qmmf.recgtest.dumpfreq"
// Prop to set no of iterations
#define PROP_N_ITERATIONS      "persist.qmmf.recgtest.iter"

typedef struct StreamDumpInfo {
  int32_t       file_fd;
  VideoFormat   format;
  uint32_t      track_id;
  int32_t       width;
  int32_t       height;
} StreamDumpInfo;

class DumpBitStream {
 public:
  DumpBitStream() : is_enabled(false) {};

  ~DumpBitStream() {};

  int32_t SetUp(StreamDumpInfo& dumpinfo);

  void Close(int32_t file_fd) { if (file_fd > 0) close(file_fd); }

  status_t Dump(std::vector<BufferDescriptor>& buffers,
                int32_t file_fd);

  bool is_enabled;
};

class RecorderGtest : public ::testing::Test {
 public:
  RecorderGtest() : recorder_() {};

  ~RecorderGtest() {};

 protected:
  const ::testing::TestInfo* test_info_;

  void SetUp() override;

  void TearDown() override;

  int32_t Init();

  int32_t DeInit();

  void ClearSessions();

  void RecorderCallbackHandler(EventType event_type, void *event_data,
                               size_t event_data_size);

  void SessionCallbackHandler(EventType event_type,
                              void *event_data,
                              size_t event_data_size);

  void CameraResultCallbackHandler(uint32_t camera_id,
                                   const CameraMetadata &result);

  void VideoTrackYUVDataCb(uint32_t track_id, std::vector<BufferDescriptor>
                           buffers, std::vector<MetaData> meta_buffers);

  void VideoTrackOneEncDataCb(uint32_t track_id, std::vector<BufferDescriptor>
                              buffers, std::vector<MetaData> meta_buffers);

  void VideoTrackTwoEncDataCb(uint32_t track_id, std::vector<BufferDescriptor>
                              buffers, std::vector<MetaData> meta_buffers);

  void VideoTrackThreeEncDataCb(uint32_t track_id, std::vector<BufferDescriptor>
                                buffers, std::vector<MetaData> meta_buffers);

  void VideoTrackEventCb(uint32_t track_id, EventType event_type,
                         void *event_data, size_t event_data_size);

  void SnapshotCb(uint32_t camera_id, uint32_t image_sequence_count,
                  BufferDescriptor buffer, MetaData meta_data);

  status_t QueueVideoFrame(VideoFormat format_type,
                           const uint8_t *buffer, size_t size,
                           int64_t timestamp, AVQueue *que);

  void VideoCachedDataCb(uint32_t track_id,
                         std::vector<BufferDescriptor> buffers,
                         std::vector<MetaData> meta_buffers,
                         VideoFormat format_type,
                         AVQueue *que);

  status_t DumpQueue(AVQueue *queue, int32_t file_fd);

  Recorder              recorder_;
  uint32_t              camera_id_;
  uint32_t              iteration_count_;
  std::vector<uint32_t> camera_ids_;
  CameraStartParam      camera_start_params_;
  RecorderCb            recorder_status_cb_;
  int32_t               track1_bitstream_filefd_;
  int32_t               track2_bitstream_filefd_;
  int32_t               track3_bitstream_filefd_;
  std::map <uint32_t , std::vector<uint32_t> > sessions_;

  DumpBitStream         dump_bitstream_;
  bool                  is_dump_yuv_enabled_;
  uint32_t              dump_yuv_freq_;
};

