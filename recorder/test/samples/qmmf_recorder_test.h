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

#include "qmmf_recorder.h"
#include "qmmf_recorder_params.h"
#include <map>

using namespace qmmf;
using namespace recorder;
using namespace android;

class RecorderTest
{
public:
    RecorderTest();

    ~RecorderTest();

    int32_t Connect();

    int32_t Disconnect();

    int32_t StartCamera();

    int32_t StopCamera();

    int32_t SessionWithTwoVideoTrack();

    int32_t CreateAudioOnlySession();

    int32_t CreateAudioVideoSession();

    int32_t StartSession();

    int32_t StopSession();

    int32_t PauseSession();

    int32_t ResumeSession();

    int32_t DeleteSession();

    void RecorderCallbackHandler(EventType event_type, void *event_data,
                                 size_t event_data_size);

    void SessionCallbackHandler(EventType event_type,
                                void *event_data, size_t event_data_size);

    void AudioTrackDataCb(uint32_t track_id, std::vector<TrackBuffer> buffers,
                          void *meta_param, TrackMetaParamType meta_type,
                          size_t meta_size, uint32_t buffer_pool_id);

    void AudioTrackEventCb(uint32_t track_id, EventType event_type,
                           void *event_data, size_t event_data_size);

    void VideoTrack4KDataCb(uint32_t track_id,
                            std::vector<TrackBuffer> buffers,
                            void *meta_param, TrackMetaParamType meta_type,
                            size_t meta_size);

    void VideoTrack4KEventCb(uint32_t track_id, EventType event_type,
                             void *event_data, size_t event_data_size);

    void VideoTrack1080pDataCb(uint32_t track_id,
                               std::vector<TrackBuffer> buffers,
                               void *meta_param, TrackMetaParamType meta_type,
                               size_t meta_size);

    void VideoTrack1080pEventCb(uint32_t track_id, EventType event_type,
                                void *event_data, size_t event_data_size);
private:

    Recorder recorder_;
    /*
    * <session_id, vector<track_ids> >
    */
    std::map <uint32_t , std::vector<uint32_t> > sessions_;
};

class CmdMenu
{
public:
    enum CommandType {
        CONNECT_CMD         = '1',
        DISCONNECT_CMD      = '2',
        START_CAMERA_CMD    = '3',
        STOP_CAMERA_CMD     = '4',
        CREATE_SESSION_CMD  = '5',
        START_SESSION_CMD   = '6',
        STOP_SESSION_CMD    = '7',
        PAUSE_SESSION_CMD   = '8',
        RESUME_SESSION_CMD  = '9',
        DELETE_SESSION_CMD  = 'D',
        EXIT_CMD            = 'X',
        INVALID_CMD         = '0'
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
