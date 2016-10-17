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

#include <qmmf-sdk/qmmf_player.h>
#include <qmmf-sdk/qmmf_player_params.h>
#include "player/test/samples/qmmf_player_parser.h"
#include <pthread.h>

//#include <qmmf-sdk/qmmf_codec.h>

using namespace qmmf;
using namespace player;
using namespace android;

/*
enum class VideoCodecType {
  kTypeAVC,
  kTypeHEVC
};

enum class AudioCodecType {
  kTypeAAC,
  kTypeG711,
  kTypeAMR
};
*/

/*
typedef struct Event{
   PlayerState state;
};*/

enum PlayerClientState
{
   ERROR = 0,
   IDLE = 1 << 0,
   PREPARED = 1 << 1,
   STARTED = 1 << 2,
   PAUSED = 1 << 3,
   STOPPED =  1 << 4,
   PLAYBACK_COMPLETED = 1<< 5,
};

class PlayerTest
{
public:
    PlayerTest();

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

    void playercb(EventType event_type, void *event_data, size_t event_data_size);

    void audiotrackcb(EventType event_type, void *event_data, size_t
    event_data_size);

    void videotrackcb(EventType event_type, void *event_data, size_t
    event_data_size);

    static void* StartPlaying(void* ptr);

    char *            filename_;
    uint32_t          filetype_;

private:

    Player player_;
    std::map <uint32_t , std::vector<uint32_t> > sessions_;

    int32_t file_fd_;
    bool stopped_;
    pthread_t start_thread_id;

    AACfileIO*        aacfileIO_;
    G711fileIO*       g711fileIO_;
    AMRfileIO*        amrfileIO_;
};

class CmdMenu
{
public:
    enum CommandType {
        CONNECT_CMD                       = '1',
        DISCONNECT_CMD                    = '2',
        PREPARE_CMD                       = '3',
        START_CMD                         = '4',
        STOP_CMD                          = '5',
        PAUSE_CMD                         = '6',
        RESUME_CMD                        = '7',
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
