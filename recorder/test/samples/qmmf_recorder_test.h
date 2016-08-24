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

#include <map>

#include "recorder/test/samples/qmmf_recorder_test_wav.h"

#include <qmmf-sdk/qmmf_recorder.h>
#include <qmmf-sdk/qmmf_recorder_params.h>
#include <qmmf-sdk/qmmf_codec.h>

// Enable this define to dump YUV data from YUV track
#define DUMP_YUV_FRAMES

// Enable this define to dump encoded bit stream data.
#define DUMP_BITSTREAM

using namespace qmmf;
using namespace recorder;
using namespace android;

enum class VideoCodecType {
  kTypeAVC,
  kTypeHEVC
};

class RecorderTest {
 public:
  RecorderTest();

  ~RecorderTest();

  int32_t Connect();

  int32_t Disconnect();

  int32_t StartCamera();

  int32_t StopCamera();

  int32_t TakeSnapshot();

  int32_t Session4KAnd1080pYUVTracks();

  int32_t Session4KEncTrack(const VideoCodecType& type);

  int32_t Session1080pEncTrack(const VideoCodecType& type);

  int32_t Session4KYUVAnd1080pEncTracks(const VideoCodecType& type);

  int32_t SessionTwo1080pEncTracks(const VideoCodecType& type);

  void CreateAudioOnlySession();

  int32_t StartSession();

  int32_t StopSession();

  int32_t PauseSession();

  int32_t ResumeSession();

  int32_t DeleteSession();

  void SnapshotCb(uint32_t camera_id, uint32_t image_sequence_count,
                  BufferDescriptor buffer);

  void RecorderCallbackHandler(EventType event_type, void *event_data,
                               size_t event_data_size);

  void SessionCallbackHandler(EventType event_type,
                              void *event_data, size_t event_data_size);

  void AudioTrackDataCb(uint32_t track_id, std::vector<BufferDescriptor>
                        buffers, void *meta_param, TrackMetaParamType
                        meta_type, size_t meta_size);

  void AudioTrackEventCb(uint32_t track_id, EventType event_type,
                         void *event_data, size_t event_data_size);

  void VideoTrack4KYUVDataCb(uint32_t track_id,
                             std::vector<BufferDescriptor> buffers,
                             void *meta_param, TrackMetaParamType meta_type,
                             size_t meta_size);

  void VideoTrack4KYUVEventCb(uint32_t track_id, EventType event_type,
                              void *event_data, size_t event_data_size);

  void VideoTrack1080pYUVDataCb(uint32_t track_id,
                                std::vector<BufferDescriptor> buffers,
                                void *meta_param, TrackMetaParamType
                                meta_type, size_t meta_size);

  void VideoTrack1080pYUVEventCb(uint32_t track_id, EventType event_type,
                                 void *event_data, size_t event_data_size);

  void VideoTrack4KEncDataCb(uint32_t track_id,
                             std::vector<BufferDescriptor> buffers,
                             void *meta_param, TrackMetaParamType meta_type,
                             size_t meta_size);

  void VideoTrack4KEncEventCb(uint32_t track_id, EventType event_type,
                              void *event_data, size_t event_data_size);

  void VideoTrack1080pEncDataCb1(uint32_t track_id,
                                std::vector<BufferDescriptor> buffers,
                                void *meta_param, TrackMetaParamType
                                meta_type, size_t meta_size);

  void VideoTrack1080pEncDataCb2(uint32_t track_id,
                                std::vector<BufferDescriptor> buffers,
                                void *meta_param, TrackMetaParamType
                                meta_type, size_t meta_size);

  void VideoTrack1080pEncEventCb(uint32_t track_id, EventType event_type,
                                 void *event_data, size_t event_data_size);

#ifdef DUMP_BITSTREAM
  status_t DumpBitStream(std::vector<BufferDescriptor>& buffers,
                         int32_t file_fd);
#endif

#ifdef DUMP_YUV_FRAMES
  status_t DumpYUVFrame(uint32_t track_id, MetaInfo* meta_data,
                        BufferDescriptor buffer);
#endif

 private:

  Recorder recorder_;
  RecorderTestWav wav_;
  // <session_id, vector<track_ids> >
  std::map <uint32_t , std::vector<uint32_t> > sessions_;

  uint32_t camera_id_;
  // TODO: consolidate all data related to one track in separate class.
  int32_t  file_fd1_;
  int32_t  file_fd2_;
};

class CmdMenu
{
public:
    enum CommandType {
        CONNECT_CMD                       = '1',
        DISCONNECT_CMD                    = '2',
        START_CAMERA_CMD                  = '3',
        STOP_CAMERA_CMD                   = '4',
        CREATE_YUV_SESSION_CMD            = '5',
        CREATE_4KENC_AVC_SESSION_CMD      = '6',
        CREATE_4KENC_HEVC_SESSION_CMD     = '7',
        CREATE_1080pENC_AVC_SESSION_CMD   = '8',
        CREATE_1080pENC_HEVC_SESSION_CMD  = '9',
        CREATE_4KYUV_1080pENC_SESSION_CMD = 'V',
        CREATE_TWO_1080pENC_SESSION_CMD   = 'M',
        CREATE_AUD_SESSION_CMD            = 'K',
        START_SESSION_CMD                 = 'A',
        STOP_SESSION_CMD                  = 'B',
        TAKE_SNAPSHOT_CMD                 = 'S',
        PAUSE_SESSION_CMD                 = 'P',
        RESUME_SESSION_CMD                = 'R',
        DELETE_SESSION_CMD                = 'D',
        EXIT_CMD                          = 'X',
        INVALID_CMD                       = '0'
    };

    struct Command {
        Command( CommandType cmd)
        : cmd(cmd) {}
        Command()
        : cmd(INVALID_CMD) {}
        CommandType cmd;
    };

    CmdMenu(RecorderTest &ctx) :  ctx_(ctx) {};

    ~CmdMenu() {};

    Command GetCommand();

    void PrintMenu();

    RecorderTest &ctx_;
};
