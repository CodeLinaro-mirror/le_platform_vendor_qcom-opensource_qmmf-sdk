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

#include "qmmf_transcode_test.h"

TransCodeTest* TransCodeTest::instance_ = nullptr;

TransCodeTest* TransCodeTest::Connect() {
  if (!instance_) {
    instance_ = new TransCodeTest;
    if (instance_) {
      TEST_INFO("%s:%s Created Transcode Instance(%p) successfully",
          TAG, __func__, instance_);
      return instance_;
    } else {
      TEST_ERROR("%s:%s Failed to Crate Transcode(%p)", TAG, __func__,
          instance_);
      return nullptr;
    }
  } else {
    TEST_WARN("%s:%s Transcode is Already Created(%p)", TAG, __func__,
        instance_);
    return instance_;
  }
}

status_t TransCodeTest::Disconnect() {
  TEST_INFO("%s:%s Enter", TAG, __func__);
  if (instance_) {
    delete instance_;
    instance_ = nullptr;
  }
  TEST_INFO("%s:%s Exit", TAG, __func__);
  return 0;
}

status_t TransCodeTest::CreateTrack() {
  TEST_INFO("%s:%s Enter", TAG, __func__);
  status_t ret = 0;

  uint32_t     track_id;
  // char          track_file[MAX_FILE_NAME];

  // Read from the user input the track file name and track id
  track_id = 1;
  TranscoderTrack* track = new TranscoderTrack(nullptr);
  if (!track) {
    TEST_ERROR("%s:%s TranscoderTrack Creation Failed track_id(%u)", TAG,
      __func__, track_id);
    return ::android::NO_MEMORY;
  }

  track_map_.add(track_id,track);
  ret = track->PreparePipeline();
  if (ret != 0) {
    TEST_ERROR("%s:%s Failed to Prepare Pipeline for track_id(%u)",
        TAG, __func__, track_id);
    return ret;
  }

  TEST_INFO("%s:%s Exit", TAG, __func__);
  return ret;
}

status_t TransCodeTest::StartTrack() {
  TEST_INFO("%s:%s Enter", TAG, __func__);
  status_t ret = 0;
  uint32_t track_id;
  //Read the track id info from user input
  track_id = 1;
  TranscoderTrack *track = track_map_.valueFor(track_id);
  ret = track->Start();
  if (ret != 0) {
    TEST_ERROR("%s:%s Failed to Start Track track_id(%u)", TAG, __func__,
        track_id);
    return ret;
  }

  TEST_INFO("%s:%s Exit", TAG, __func__);
  return ret;
}

status_t TransCodeTest::StopTrack() {
  TEST_INFO("%s:%s Enter", TAG, __func__);
  status_t ret = 0;
  uint32_t track_id;
  //Read the track id info from user input
  track_id = 1;
  TranscoderTrack *track = track_map_.valueFor(track_id);
  ret = track->Stop();
  if (ret != 0) {
    TEST_ERROR("%s:%s Failed to Stop Track track_id(%u)", TAG, __func__,
        track_id);
    return ret;
  }

  TEST_INFO("%s:%s Exit", TAG, __func__);
  return ret;
}

status_t TransCodeTest::DeleteTrack() {
  TEST_INFO("%s:%s Enter", TAG, __func__);
  status_t ret = 0;
  uint32_t track_id;
  //Read the track id info from user input
  track_id = 1;
  TranscoderTrack *track = track_map_.valueFor(track_id);
  ret = track->Delete();
  if (ret != 0) {
    TEST_ERROR("%s:%s Failed to Delete Track track_id(%u)", TAG, __func__,
        track_id);
    return ret;
  }

  track_map_.removeItem(track_id);
  delete track;
  TEST_INFO("%s:%s Exit", TAG, __func__);
  return ret;
}

status_t TransCodeTest::SetTrackParams() {
  status_t ret = 0;
  TEST_INFO("%s:%s Enter", TAG, __func__);
  TEST_INFO("%s:%s Exit", TAG, __func__);
  return ret;
}

void CmdMenu::PrintMenu() {

  printf("\n\n=========== QIPCAM TEST MENU ===================\n\n");

  printf(" \n\nTransCode Test Application commands \n");
  printf(" -----------------------------\n");
  printf("   %c. Connect\n", CmdMenu::CONNECT_CMD);
  printf("   %c. Create Track\n", CmdMenu::CREATE_TRACK_CMD);
  printf("   %c. Start Track\n", CmdMenu::START_TRACK_CMD);
  printf("   %c. Stop Track\n", CmdMenu::STOP_TRACK_CMD);
  printf("   %c. Delete Track\n", CmdMenu::DELETE_TRACK_CMD);
  printf("   %c. Disconnect\n", CmdMenu::DISCONNECT_CMD);
  printf("   %c. Exit\n", CmdMenu::EXIT_CMD);
  printf("\n   Choice: ");
}

CmdMenu::Command CmdMenu::GetCommand() {
  PrintMenu();
  return CmdMenu::Command(static_cast<CmdMenu::CommandType>(getchar()));
}

int main() {

  TEST_INFO("%s:%s Enter", TAG, __func__);

  TransCodeTest* test_context_ptr = nullptr;
  CmdMenu cmd_menu;

  bool testRunning = true;

  while (testRunning) {
    CmdMenu::Command command = cmd_menu.GetCommand();

    switch (command.cmd) {
      case CmdMenu::CONNECT_CMD:
      {
        test_context_ptr = TransCodeTest::Connect();
        if (!test_context_ptr) {
          TEST_INFO("%s:%s exit from test", TAG, __func__);
          testRunning = false;
          break;
        }
      }
      break;
      case CmdMenu::CREATE_TRACK_CMD:
      {
        if (!test_context_ptr) {
          TEST_INFO("%s:%s exit from test", TAG, __func__);
          testRunning = false;
          break;
        }
        test_context_ptr->CreateTrack();
      }
      break;
      case CmdMenu::START_TRACK_CMD:
      {
        if (!test_context_ptr) {
          TEST_INFO("%s:%s exit from test", TAG, __func__);
          testRunning = false;
          break;
        }
        test_context_ptr->StartTrack();
      }
      break;
      case CmdMenu::STOP_TRACK_CMD:
      {
        if (!test_context_ptr) {
          TEST_INFO("%s:%s exit from test", TAG, __func__);
          testRunning = false;
          break;
        }
        test_context_ptr->StopTrack();
      }
      break;
      case CmdMenu::DELETE_TRACK_CMD:
      {
        if (!test_context_ptr) {
          TEST_INFO("%s:%s exit from test", TAG, __func__);
          testRunning = false;
          break;
        }
        test_context_ptr->DeleteTrack();
      }
      break;
      case CmdMenu::DISCONNECT_CMD:
      {
        if (!test_context_ptr) {
          TEST_INFO("%s:%s exit from test", TAG, __func__);
          testRunning = false;
          break;
        }
        TransCodeTest::Disconnect();
      }
      break;
      case CmdMenu::EXIT_CMD:
      {
        TEST_INFO("%s:%s exit from test", TAG, __func__);
        testRunning = false;
      }
      break;
      default:
      break;
    }
  }
  TEST_INFO("%s:%s Exit", TAG, __func__);
  return 0;
}
