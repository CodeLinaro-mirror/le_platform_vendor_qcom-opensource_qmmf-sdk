/*
* Copyright (c) 2016-2019, 2021, The Linux Foundation. All rights reserved.
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
#include <mutex>
#include <vector>

#include <cutils/properties.h>
#include <cutils/trace.h>
#include <linux/input.h>

#include "common/utils/qmmf_condition.h"
#include "recorder/test/audio/qmmf_recorder_test_wav.h"

#include <qmmf-sdk/qmmf_recorder.h>
#include <qmmf-sdk/qmmf_recorder_params.h>
#include <qmmf-sdk/qmmf_device.h>

//#define DEBUG
// Logging related defines
#define TEST_INFO(fmt, args...)  ALOGD(fmt, ##args)
#define TEST_ERROR(fmt, args...) ALOGE(fmt, ##args)
#define TEST_WARN(fmt,args...)   ALOGW(fmt, ##args)
#ifdef DEBUG
#define TEST_DBG  TEST_INFO
#else
#define TEST_DBG(...) ((void)0)
#endif

using namespace qmmf;
using namespace recorder;
using namespace android;

enum class TrackType {
  kNone,
  kAudioPCM,
  kAudioPCMFP,
  kAudioPCMAS,
  kAudioAAC,
  kAudioAMR,
  kAudioG711,
  kAudioMPEGH,
};

struct TrackInfo {
  TrackType track_type;
  uint32_t  session_id;
  uint32_t  track_id;
  DeviceId  device_id;

  TrackInfo()
      : track_type(TrackType::kNone),
        session_id(-1),
        track_id(1),
        device_id(0) {}

  TrackInfo(TrackType track_type,
            uint32_t session_id,
            uint32_t track_id,
            DeviceId device_id)
      : track_type(track_type),
        session_id(session_id),
        track_id(track_id),
        device_id(device_id) {}
};

class TestTrack;
class CmdMenu;

class RecorderTest {
 public:
  RecorderTest();

  ~RecorderTest();

  status_t Connect();

  status_t Disconnect();

  status_t CreateAudioPCMTrack();

  status_t CreateAudio2PCMTrack();

  status_t CreateAudioSCOTrack();

  status_t CreateAudioPCMSCOTrack();

  status_t CreateAudioA2DPTrack();

  status_t CreateAudioPCMA2DPTrack();

  status_t CreateAudioPCMAMRTrack();

  status_t CreateAudioPCMFluenceTrack();

  status_t CreateAudioPCMAmbisonicTrack();

  status_t StartSession();

  status_t StopSession();

  status_t PauseSession();

  status_t ResumeSession();

  status_t DeleteSession();

  void RecorderEventCallbackHandler(EventType event_type, void *event_data,
                                    size_t event_data_size);

  void SessionCallbackHandler(EventType event_type,
                              void *event_data, size_t event_data_size);

  Recorder& GetRecorder() { return recorder_; }

 private:
  Recorder recorder_;

  friend class CmdMenu;

  std::map<uint32_t, std::vector<TestTrack*> > sessions_;
  typedef std::map<uint32_t, std::vector<TestTrack*> >::iterator session_iter_;

  uint32_t current_session_id_;

  QCondition   signal_;
  std::mutex   message_lock_;
  QCondition   signal_cb_;
  std::mutex   callback_lock_;
};

// Track can be types of Audio, this class is responsible for creating
// tracks, setting required parameters, registering the data/event callback,
// dumping the data for verification purpose etc.
class TestTrack {
 public:
  TestTrack(RecorderTest* recorder_test);

  ~TestTrack();

  TrackType& GetTrackType() { return track_info_.track_type; }

  uint32_t GetTrackId() { return track_info_.track_id; }

  status_t SetUp(TrackInfo& track_info);

  // Set up file to dump track data.
  status_t Prepare();

  // Clean up file.
  status_t CleanUp();

  const TrackInfo& GetTrackHandle(){return track_info_;}

 private:
  void TrackEventCB(uint32_t track_id, EventType event_type, void *event_data,
                    size_t event_data_size);

  void TrackDataCB(uint32_t track_id, std::vector<BufferDescriptor> buffers,
                   std::vector<MetaData> meta_buffers);

  TrackInfo track_info_;

  RecorderTest* recorder_test_;

  RecorderTestWav wav_output_;
};

class CmdMenu
{
public:
    enum CommandType {
        CONNECT_CMD                                     = '1',
        DISCONNECT_CMD                                  = '2',
        CREATE_PCM_AUD_SESSION_CMD                      = 'a',
        CREATE_2PCM_AUD_SESSION_CMD                     = 'b',
        CREATE_SCO_AUD_SESSION_CMD                      = 'c',
        CREATE_PCM_SCO_AUD_SESSION_CMD                  = 'd',
        CREATE_A2DP_AUD_SESSION_CMD                     = 'e',
        CREATE_PCM_A2DP_AUD_SESSION_CMD                 = 'f',
        CREATE_PCM_G7ll_AUD_SESSION_CMD                 = 'o',
        CREATE_PCMFL_AUD_SESSION_CMD                    = 'p',
        CREATE_PCMAS_AUD_SESSION_CMD                    = 'u',
        START_SESSION_CMD                               = 'A',
        STOP_SESSION_CMD                                = 'B',
        SET_PARAM_CMD                                   = 'T',
        PAUSE_SESSION_CMD                               = 'P',
        RESUME_SESSION_CMD                              = 'R',
        DELETE_SESSION_CMD                              = 'D',
        EXIT_CMD                                        = 'X',
        NEXT_CMD                                        = '\n',
        INVALID_CMD                                     = '0'
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

    Command GetCommand(bool& is_print_menu);

    void PrintMenu();

    RecorderTest &ctx_;
};
