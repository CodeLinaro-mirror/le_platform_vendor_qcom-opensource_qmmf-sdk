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

#define TAG "PlayerTest"

#include <fcntl.h>
#include <sys/mman.h>
#include <utils/Log.h>
#include <utils/String8.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "common/qmmf_common_utils.h"
#include "player/test/samples/qmmf_player_test.h"
#include "player/src/service/qmmf_player_common.h"

using namespace qmmf;
using namespace player;
using namespace android;


//#define DEBUG
#define TEST_INFO(fmt, args...)  ALOGD(fmt, ##args)
#define TEST_ERROR(fmt, args...) ALOGE(fmt, ##args)
#ifdef DEBUG
#define TEST_DBG  TEST_INFO
#else
#define TEST_DBG(...) ((void)0)
#endif

// Enable this define to dump bitstream from demuxer
#define DUMP_BITSTREAM

// Enable this define to dump YUV from decoder.
#define DUMP_YUV_FRAMES


// Enable this define to dump PCM from decoder.
#define DUMP_PCM_DATA

void PlayerTest::playercb(EventType event_type,
                    void *event_data,
                    size_t event_data_size)
{
  TEST_INFO("%s:%s: Enter", TAG, __func__);

  Event* ev = (Event *)event_data;

  TEST_INFO("%s:%s event_type is:: %d", TAG, __func__,event_type);
  TEST_INFO("%s:%s state is:: %d", TAG,__func__, ev->state);

  TEST_INFO("%s:%s: Exit", TAG, __func__);
}

void PlayerTest::audiotrackcb(EventType event_type,
    void *event_data, size_t event_data_size)
{
  TEST_INFO("%s:%s: Enter", TAG, __func__);
  TEST_INFO("%s:%s: Exit", TAG, __func__);
}

void PlayerTest::videotrackcb(EventType event_type,
    void *event_data, size_t event_data_size)
{
  TEST_INFO("%s:%s: Enter", TAG, __func__);
  TEST_INFO("%s:%s: Exit", TAG, __func__);
}

PlayerTest::PlayerTest():stopped_(false),filename_(NULL)
{
  TEST_INFO("%s:%s: Enter", TAG, __func__);
  TEST_INFO("%s:%s: Exit", TAG, __func__);
}

PlayerTest::~PlayerTest()
{
  TEST_INFO("%s:%s: Enter", TAG, __func__);
  TEST_INFO("%s:%s: Exit", TAG, __func__);
}

int32_t PlayerTest::Connect()
{
  TEST_INFO("%s:%s: Enter", TAG, __func__);
  PlayerCb player_cb_;

  player_cb_.event_cb = [&] (EventType event_type, void *event_data,
                  size_t event_data_size) {playercb(event_type,event_data,
                  event_data_size); };
  auto ret = player_.Connect(player_cb_);
  TEST_INFO("%s:%s: Exit", TAG, __func__);
  return ret;
}

int32_t PlayerTest::Disconnect()
{
  TEST_INFO("%s:%s: Enter", TAG, __func__);
  auto ret = player_.Disconnect();
  TEST_INFO("%s:%s: Exit", TAG, __func__);
  return ret;
}

int32_t PlayerTest::Prepare()
{
  TEST_INFO("%s:%s: Enter", TAG, __func__);

  auto result = 0;

  // Create Audio Track
  AudioTrackCreateParam audio_track_param_;
  memset(&audio_track_param_, 0x0, sizeof audio_track_param_);

  switch(filetype_)
  {
    case 1:
      aacfileIO_ = AACfileIO::createAACfileIOobj(filename_);
      result = aacfileIO_->Fillparams(&audio_track_param_);
      if(result != 0){
        TEST_INFO("%s:%s Could not fill the AAC params",TAG,__func__);
      }
      break;

    case 2:
      g711fileIO_ = G711fileIO::createG711fileIOobj(filename_);
      result = g711fileIO_->Fillparams(&audio_track_param_);
      if(result != 0){
        TEST_INFO("%s:%s Could not fill the G711 params",TAG,__func__);
      }
      break;

    case 3:
       amrfileIO_ = AMRfileIO::createAMRfileIOobj(filename_);
       result = amrfileIO_->Fillparams(&audio_track_param_);
       if(result != 0){
         TEST_INFO("%s:%s Could not fill the AMR params",TAG,__func__);
       }
      break;

    default:
      break;
  }

  uint32_t track_id_1 =1;
  TrackCb audio_track_cb_;

  audio_track_param_.out_device  = AudioOutSubtype::kBuiltIn;

  audio_track_cb_.event_cb = [&] (EventType event_type, void *event_data,
       size_t event_data_size) {audiotrackcb(event_type, event_data,
       event_data_size);};

  result = player_.CreateAudioTrack(track_id_1,audio_track_param_,audio_track_cb_);

  result = player_.Prepare();

  if (result != NO_ERROR)
    return -1;

  TEST_INFO("%s:%s: Exit", TAG, __func__);
  return result;
}

int32_t PlayerTest::Start()
{
  TEST_INFO("%s:%s: Enter", TAG, __func__);
  auto ret = 0;

  ret = player_.Start();
  stopped_ = false;

  pthread_create(&start_thread_id,NULL,PlayerTest::StartPlaying,(void *)this);

  TEST_INFO("%s:%s: Exit", TAG, __func__);
  return ret;
}

void * PlayerTest::StartPlaying(void *ptr)
{
  TEST_INFO("%s:%s: Enter", TAG, __func__);
  auto ret = 0;

  uint32_t track_id_1 = 1;
  uint32_t result;

  PlayerTest* playertest = static_cast<PlayerTest *>(ptr);

  while (!playertest->stopped_)
  {
     std::vector<TrackBuffer> buffers;

     TrackBuffer tb;
     memset(&tb,0x0,sizeof(tb));
     buffers.push_back(tb);

     ret = playertest->player_.DequeueInputBuffer(track_id_1,buffers);

     int32_t num_frames_read;
     uint32_t bytes_read;

     switch(playertest->filetype_)
     {
       case 1:
           //For AAC
           //this size is the size of buffer to which void*data points to and bytes_read is the filled length
           result = playertest->aacfileIO_->GetFrames((void*)buffers[0].data,buffers[0].size,&num_frames_read,&bytes_read);
           break;

       case 2:
           //For G711
           result = playertest->g711fileIO_->GetFrames((void*)buffers[0].data,buffers[0].size,&bytes_read);
           break;

       case 3:
           //For AMR
           result = playertest->amrfileIO_->GetFrames((void*)buffers[0].data,buffers[0].size,&num_frames_read,&bytes_read);
           break;

        default:
           break;
     }

     buffers[0].filled_size = bytes_read;

     if(result != 0){
        //EOS reached
        //jsut see how will you send EOS
        TEST_INFO("%s:%s: File read completed result is %d", TAG, __func__,  result);
        buffers[0].flag = 1;
        playertest->player_.Stop(true);
        playertest->stopped_ = true;
        break;
     }

    TEST_INFO("%s:%s: filled_size %d", TAG, __func__,  buffers[0].filled_size);
    TEST_INFO("%s:%s: buffer size %d", TAG, __func__, buffers[0].size);
    TEST_INFO("%s:%s: vaddr 0x%x", TAG, __func__, buffers[0].data);

    uint32_t val = 1;

    playertest->player_.QueueInputBuffer(track_id_1,buffers,(void*)&val,sizeof (uint32_t),TrackMetaBufferType::kNone);
  }

  TEST_INFO("%s:%s: Exit", TAG, __func__);
  return NULL;
}


int32_t PlayerTest::Stop()
{
  TEST_INFO("%s:%s: Enter", TAG, __func__);
  stopped_ = true;
  auto ret = player_.Stop(true);
  TEST_INFO("%s:%s: Exit", TAG, __func__);
  return ret;
}

int32_t PlayerTest::Pause()
{
  TEST_INFO("%s:%s: Enter", TAG, __func__);
  auto ret = player_.Pause();
  TEST_INFO("%s:%s: Exit", TAG, __func__);
  return ret;
}

int32_t PlayerTest::Resume()
{
  TEST_INFO("%s:%s: Enter", TAG, __func__);
  auto ret = player_.Resume();
  TEST_INFO("%s:%s: Exit", TAG, __func__);
  return ret;
}

int32_t PlayerTest::SetPosition()
{
  auto ret = 0;
  TEST_INFO("%s:%s: Enter", TAG, __func__);
  //int64_t seek_time;
  //auto ret = player_.SetPosition(seek_time);
  TEST_INFO("%s:%s: Exit", TAG, __func__);
  return ret;
}

int32_t PlayerTest::SetTrickMode()
{
  auto ret = 0;
  TEST_INFO("%s:%s: Enter", TAG, __func__);
  //auto ret = player_.SetTrickMode();
  TEST_INFO("%s:%s: Exit", TAG, __func__);
  return ret;
}

int32_t PlayerTest::GrabPicture()
{
  auto ret = 0;
  TEST_INFO("%s:%s: Enter", TAG, __func__);
  //player_.GrabPicture();
  TEST_INFO("%s:%s: Exit", TAG, __func__);
  return ret;
}

void CmdMenu::PrintMenu() {

  printf("\n\n=========== PLAYER TEST MENU ===================\n\n");

  printf(" \n\nPlayer Test Application commands \n");
  printf(" -----------------------------\n");
  printf("   %c. Connect\n", CmdMenu::CONNECT_CMD);
  printf("   %c. Disconnect\n", CmdMenu::DISCONNECT_CMD);
  printf("   %c. Prepare\n", CmdMenu::PREPARE_CMD);
  printf("   %c. Start\n", CmdMenu::START_CMD);
  printf("   %c. Stop\n", CmdMenu::STOP_CMD);
  printf("   %c. Pause\n", CmdMenu::PAUSE_CMD);
  printf("   %c. Resume\n", CmdMenu::RESUME_CMD);
  printf("   %c. Exit\n", CmdMenu::EXIT_CMD);
  printf("\n   Choice: ");
}

CmdMenu::Command CmdMenu::GetCommand() {
  PrintMenu();
  return CmdMenu::Command(static_cast<CmdMenu::CommandType>(getchar()));
}

int main(int argc,char *argv[]) {

  TEST_INFO("%s:%s: Enter", TAG, __func__);

  PlayerTest test_context;

  CmdMenu cmd_menu(test_context);

  int32_t exit_test = false;

  if(argc == 2) {
    test_context.filename_ = argv[1];
    char *extn = strrchr(argv[1], '.');

    TEST_INFO("%s: exten is: %s", TAG, extn);

    if (strcmp(extn, ".aac") == 0)
       test_context.filetype_ = 1;

    else if (strcmp(extn, ".g711") ==0)
         test_context.filetype_ = 2;

    else if (strcmp(extn, ".amr") == 0)
       test_context.filetype_ = 3;

    else {
        TEST_ERROR("%s:%s %s extn not supported, supported extn are",
        ".aac, .amr, .g711", TAG,__func__,extn);
         return -1;
        }
  } else {
      TEST_INFO("%s:%s Give some file to play, supported extn/format",
             "are .aac, .amr, .g711 ", TAG,__func__);
  }

  while (!exit_test) {

    CmdMenu::Command command = cmd_menu.GetCommand();
    switch (command.cmd) {

      case CmdMenu::CONNECT_CMD: {
        test_context.Connect();
      }
      break;
      case CmdMenu::DISCONNECT_CMD: {
        test_context.Disconnect();
      }
      break;
      case CmdMenu::PREPARE_CMD: {
        test_context.Prepare();
      }
      break;
      case CmdMenu::START_CMD: {
        test_context.Start();
      }
      break;
      case CmdMenu::STOP_CMD: {
        test_context.Stop();
      }
      break;
      case CmdMenu::PAUSE_CMD: {
        test_context.Pause();
      }
      break;
      case CmdMenu::RESUME_CMD: {
            test_context.Resume();
      }
      break;
      case CmdMenu::EXIT_CMD: {
        exit_test = true;
      }
      break;
      default:
        break;
    }
  }
  return 0;
}
