/*
* Copyright (c) 2017, The Linux Foundation. All rights reserved.
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


#include "qmmf_transcode_track.h"
#include "qmmf_transcode_params.h"

#pragma once


using ::qmmf::transcode::TranscoderTrack;
using ::qmmf::transcode::status_t;
using ::android::DefaultKeyedVector;

class TransCodeTest {

public:
  static TransCodeTest* Connect();

  status_t CreateTrack();

  status_t StartTrack();

  status_t StopTrack();

  status_t DeleteTrack();

  status_t SetTrackParams();

  static status_t Disconnect();

private:

  // map of track id's and corresponding Track Class
  DefaultKeyedVector<uint32_t, TranscoderTrack*> track_map_;
  static TransCodeTest*                          instance_;
};

class CmdMenu {

public:
  enum CommandType {
    CONNECT_CMD          = '1',
    CREATE_TRACK_CMD     = '2',
    START_TRACK_CMD      = '3',
    STOP_TRACK_CMD       = '4',
    DELETE_TRACK_CMD     = '5',
    DISCONNECT_CMD       = '6',
    EXIT_CMD             = 'X',
    INVALID_CMD          = '0'
  };

  struct Command {
    Command( CommandType cmd)
    : cmd(cmd) {}
    Command()
    : cmd(INVALID_CMD) {}
    CommandType cmd;
  };

  Command GetCommand();

  void PrintMenu();

  void PrintDynamicParams();

  void GetUserInput(char* file, uint32_t& track_id);
};



