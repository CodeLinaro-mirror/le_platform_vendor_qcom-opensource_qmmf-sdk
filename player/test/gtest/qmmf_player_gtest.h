/*
 * Copyright (c) 2016-2018, The Linux Foundation. All rights reserved.
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

#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <cutils/properties.h>
#include <gtest/gtest.h>
#include <qmmf-sdk/qmmf_player.h>
#include <qmmf-sdk/qmmf_player_params.h>

#include "player/test/gtest/qmmf_player_parser.h"

#define DEFAULT_ITERATION "1"
#define FILE_PATH_PREFIX  "/data/misc/qmmf/test."

class PlayerGtest : public ::testing::Test {
 public:
  PlayerGtest();
  ~PlayerGtest() {};

 protected:
  enum AudioFileType {
    kAAC,
    kAMR,
    kG711,
    kMax
  };

  enum class State {
    kRunning,
    kPaused,
    kStopped,
    kError,
  };

  static const ::std::string kCodecType[AudioFileType::kMax];

  void PlayerHandler(EventType event_type,
                     void *event_data,
                     size_t event_data_size);
  void AudioTrackHandler(uint32_t track_id,
                         EventType event_type,
                         void *event_data,
                         size_t event_data_size);
  void VideoTrackHandler(uint32_t track_id,
                         EventType event_type,
                         void *event_data,
                         size_t event_data_size);

  void GetGTestParams();
  void SetUp() override;
  void TearDown() override;

  static void ThreadEntry(PlayerGtest* player_gtest);
  void Thread();

  void Fail();

  int32_t Connect();
  int32_t Disconnect();

  int32_t ParseFile(AudioTrackCreateParam& audio_track_param);
  int32_t Prepare();
  int32_t Delete();

  int32_t Start();
  int32_t Stop();
  int32_t Pause();
  int32_t Resume();

  const ::testing::TestInfo* test_info_;

  Player            player_;

  AudioFileType     filetype_;
  bool              start_again_;
  uint32_t          iteration_count_;
  ::std::thread*    thread_;
  State             state_;
  ::std::mutex      lock_;

  AACfileIO*        aac_file_io_;
  G711fileIO*       g711_file_io_;
  AMRfileIO*        amr_file_io_;
};
