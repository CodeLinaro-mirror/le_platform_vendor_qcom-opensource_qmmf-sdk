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
#include <string>
#include <qmmf-sdk/qmmf_player.h>
#include <qmmf-sdk/qmmf_player_params.h>
#include "player/test/samples/qmmf_player_parser.h"
#include <pthread.h>
#include "player/test/demuxer/qmmf_demuxer_mediadata_def.h"
#include "player/test/demuxer/qmmf_demuxer_intf.h"
#include "player/test/demuxer/qmmf_demuxer_sourceport.h"
#include <fstream>


using namespace qmmf;
using namespace player;
using namespace android;

enum class AudioFileType{
  kAAC,
  kAMR,
  kG711
};

class PlayerTest {
 public:
  PlayerTest();

  PlayerTest(char* filename_);

  ~PlayerTest();

  int32_t Connect();

  int32_t Disconnect();

  int32_t Prepare();

  int32_t Start();

  int32_t Stop();

  int32_t Pause();

  int32_t Resume();

  int32_t SetPosition();

  int32_t SetTrickMode();

  int32_t GrabPicture();

  int32_t Delete();

  void playercb(EventType event_type, void *event_data,
                size_t event_data_size);

  void audiotrackcb(EventType event_type, void *event_data,
                    size_t event_data_size);

  void videotrackcb(EventType event_type, void *event_data,
                    size_t event_data_size);

  static void* StartPlayingAudio(void* ptr);

  static void* StartPlayingVideo(void* ptr);

  int32_t StopPlaying();

  uint32_t CreateDataSource();

  uint32_t ReadMediaInfo();

  uint32_t  ReadAudioTrackMediaInfo(uint32 ulTkId,
                                    FileSourceMnMediaType eMnType);

  uint32_t ReadVideoTrackMediaInfo(uint32 ulTkId,
                                  FileSourceMnMediaType eMnType);

  char *            filename_;
  AudioFileType     filetype_;

 private:

  int32_t ParseFile(AudioTrackCreateParam& audio_track_param_,
                    VideoTrackCreateParam& video_track_param_);

  Player player_;
  std::map <uint32_t , std::vector<uint32_t> > sessions_;


  Mutex                           state_lock;
  Condition                       wait_for_state_change_;

  bool                            stopped_;
  bool                            stop_playing_;
  bool                            start_again_;
  pthread_t                       audio_thread_id_;
  pthread_t                       video_thread_id_;

  MM_TRACK_INFOTYPE               m_sTrackInfo_;
  CMM_MediaSourcePort*            m_pIStreamPort_;
  CMM_MediaDemuxInt*              m_pDemux_;
  int                             fileCount_audio_;
  int                             fileCount_video_;
  ofstream                        srcFile_audio_;
  ofstream                        srcFile_video_;

  uint32_t                        audio_track_id_;
  uint32_t                        video_track_id_;
  bool                            audioFirstFrame_;
  bool                            videoFirstFrame_;

 std::map<uint32_t, const char*>  statemap_;
 const char*                      PlayerTestEvent[2];
};

class CmdMenu {
 public:
  enum CommandType {
      CONNECT_CMD                       = '1',
      DISCONNECT_CMD                    = '2',
      PREPARE_CMD                       = '3',
      START_CMD                         = '4',
      STOP_CMD                          = '5',
      PAUSE_CMD                         = '6',
      RESUME_CMD                        = '7',
      DELETE_CMD                        = '8',
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

  CmdMenu(PlayerTest &ctx) :  ctx_(ctx) {};

  ~CmdMenu() {};

  Command GetCommand();

  void PrintMenu();

  PlayerTest &ctx_;
};
