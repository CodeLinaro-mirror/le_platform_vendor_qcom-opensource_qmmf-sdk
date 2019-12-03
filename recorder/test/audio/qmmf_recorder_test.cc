/*
* Copyright (c) 2016-2019, The Linux Foundation. All rights reserved.
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

#define LOG_TAG "RecorderAudioTest"

#include <string>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <utils/Log.h>
#include <assert.h>

#include "recorder/test/audio/qmmf_recorder_test.h"
#include "recorder/test/audio/qmmf_recorder_test_wav.h"
#include "recorder/test/audio/qmmf_recorder_test_amr.h"
#include "recorder/test/audio/qmmf_recorder_test_mpegh.h"
#include "common/utils/qmmf_log.h"

using ::std::mutex;
using ::std::unique_lock;
using ::std::vector;

static const char* kDefaultAudioFilenamePrefix =
    "/data/misc/qmmf/recorder_test_audio";

RecorderTest::RecorderTest() {
  TEST_INFO("%s: Enter", __func__);
  TEST_INFO("%s: Exit", __func__);
}

RecorderTest::~RecorderTest() {
  TEST_INFO("%s: Enter", __func__);
  TEST_INFO("%s: Exit", __func__);
}

status_t RecorderTest::Connect() {
  TEST_INFO("%s: Enter", __func__);

  RecorderCb recorder_status_cb;
  recorder_status_cb.event_cb = [&] (EventType event_type, void *event_data,
      size_t event_data_size) { RecorderEventCallbackHandler(event_type,
      event_data, event_data_size); };

  auto ret = recorder_.Connect(recorder_status_cb);

  TEST_INFO("%s: Exit", __func__);
  return ret;
}

status_t RecorderTest::Disconnect() {
  TEST_INFO("%s: Enter", __func__);
  auto ret = recorder_.Disconnect();
  TEST_INFO("%s: Exit", __func__);
  return ret;
}

status_t RecorderTest::CreateAudioPCMTrack() {
  TEST_INFO("%s: Enter", __func__);

  SessionCb session_status_cb;
  session_status_cb.event_cb = [&] ( EventType event_type, void *event_data,
      size_t event_data_size) { SessionCallbackHandler(event_type,
      event_data, event_data_size); };

  uint32_t session_id;
  auto ret = recorder_.CreateSession(session_status_cb, &session_id);
  TEST_INFO("%s: sessions_id = %d", __func__, session_id);

  std::vector<TestTrack*> tracks;

  TestTrack *audio_pcm_track = new TestTrack(this);
  TrackInfo info{};
  info.track_id   = 101;
  info.track_type = TrackType::kAudioPCM;
  info.session_id = session_id;
  info.device_id = static_cast<DeviceId>(AudioDeviceId::kBuiltIn);

  ret = audio_pcm_track->SetUp(info);
  assert(ret == 0);
  tracks.push_back(audio_pcm_track);
  sessions_.insert(std::make_pair(session_id, tracks));

  TEST_INFO("%s: Exit", __func__);
  return ret;
}

status_t RecorderTest::CreateAudio2PCMTrack() {
  TEST_INFO("%s: Enter", __func__);

  SessionCb session_status_cb;
  session_status_cb.event_cb = [&] ( EventType event_type, void *event_data,
      size_t event_data_size) { SessionCallbackHandler(event_type,
      event_data, event_data_size); };

  uint32_t session_id;
  auto ret = recorder_.CreateSession(session_status_cb, &session_id);
  TEST_INFO("%s: sessions_id = %d", __func__, session_id);

  std::vector<TestTrack*> tracks;

  TestTrack *audio_pcm_track1 = new TestTrack(this);
  TrackInfo info{};
  info.track_id   = 101;
  info.track_type = TrackType::kAudioPCM;
  info.session_id = session_id;
  info.device_id = static_cast<DeviceId>(AudioDeviceId::kBuiltIn);

  ret = audio_pcm_track1->SetUp(info);
  assert(ret == 0);
  tracks.push_back(audio_pcm_track1);

  TestTrack *audio_pcm_track2 = new TestTrack(this);
  info.track_id   = 102;

  ret = audio_pcm_track2->SetUp(info);
  assert(ret == 0);
  tracks.push_back(audio_pcm_track2);
  sessions_.insert(std::make_pair(session_id, tracks));

  TEST_INFO("%s: Exit", __func__);
  return ret;
}

status_t RecorderTest::CreateAudioSCOTrack() {
  TEST_INFO("%s: Enter", __func__);

  SessionCb session_status_cb;
  session_status_cb.event_cb = [&] ( EventType event_type, void *event_data,
      size_t event_data_size) { SessionCallbackHandler(event_type,
      event_data, event_data_size); };

  uint32_t session_id;
  auto ret = recorder_.CreateSession(session_status_cb, &session_id);
  TEST_INFO("%s: sessions_id = %d", __func__, session_id);

  std::vector<TestTrack*> tracks;

  TestTrack *audio_sco_track = new TestTrack(this);
  TrackInfo info{};
  info.track_id   = 101;
  info.track_type = TrackType::kAudioPCM;
  info.session_id = session_id;
  info.device_id = static_cast<DeviceId>(AudioDeviceId::kBlueToothSCO);

  ret = audio_sco_track->SetUp(info);
  assert(ret == 0);
  tracks.push_back(audio_sco_track);
  sessions_.insert(std::make_pair(session_id, tracks));

  TEST_INFO("%s: Exit", __func__);
  return ret;
}

status_t RecorderTest::CreateAudioPCMSCOTrack() {
  TEST_INFO("%s: Enter", __func__);

  SessionCb session_status_cb;
  session_status_cb.event_cb = [&] ( EventType event_type, void *event_data,
      size_t event_data_size) { SessionCallbackHandler(event_type,
      event_data, event_data_size); };

  uint32_t session_id;
  auto ret = recorder_.CreateSession(session_status_cb, &session_id);
  TEST_INFO("%s: sessions_id = %d", __func__, session_id);

  std::vector<TestTrack*> tracks;

  TestTrack *audio_pcm_track = new TestTrack(this);
  TrackInfo info{};
  info.track_id   = 101;
  info.track_type = TrackType::kAudioPCM;
  info.session_id = session_id;
  info.device_id = static_cast<DeviceId>(AudioDeviceId::kBuiltIn);

  ret = audio_pcm_track->SetUp(info);
  assert(ret == 0);
  tracks.push_back(audio_pcm_track);

  TestTrack *audio_sco_track = new TestTrack(this);
  info.track_id   = 102;
  info.device_id = static_cast<DeviceId>(AudioDeviceId::kBlueToothSCO);

  ret = audio_sco_track->SetUp(info);
  assert(ret == 0);
  tracks.push_back(audio_sco_track);
  sessions_.insert(std::make_pair(session_id, tracks));

  TEST_INFO("%s: Exit", __func__);
  return ret;
}

status_t RecorderTest::CreateAudioA2DPTrack() {
  TEST_INFO("%s: Enter", __func__);

  SessionCb session_status_cb;
  session_status_cb.event_cb = [&] ( EventType event_type, void *event_data,
      size_t event_data_size) { SessionCallbackHandler(event_type,
      event_data, event_data_size); };

  uint32_t session_id;
  auto ret = recorder_.CreateSession(session_status_cb, &session_id);
  TEST_INFO("%s: sessions_id = %d", __func__, session_id);

  std::vector<TestTrack*> tracks;

  TestTrack *audio_a2dp_track = new TestTrack(this);
  TrackInfo info{};
  info.track_id   = 101;
  info.track_type = TrackType::kAudioPCM;
  info.session_id = session_id;
  info.device_id = static_cast<DeviceId>(AudioDeviceId::kBlueToothA2DP);

  ret = audio_a2dp_track->SetUp(info);
  assert(ret == 0);
  tracks.push_back(audio_a2dp_track);
  sessions_.insert(std::make_pair(session_id, tracks));

  TEST_INFO("%s: Exit", __func__);
  return ret;
}

status_t RecorderTest::CreateAudioPCMA2DPTrack() {
  TEST_INFO("%s: Enter", __func__);

  SessionCb session_status_cb;
  session_status_cb.event_cb = [&] ( EventType event_type, void *event_data,
      size_t event_data_size) { SessionCallbackHandler(event_type,
      event_data, event_data_size); };

  uint32_t session_id;
  auto ret = recorder_.CreateSession(session_status_cb, &session_id);
  TEST_INFO("%s: sessions_id = %d", __func__, session_id);

  std::vector<TestTrack*> tracks;

  TestTrack *audio_pcm_track = new TestTrack(this);
  TrackInfo info{};
  info.track_id   = 101;
  info.track_type = TrackType::kAudioPCM;
  info.session_id = session_id;
  info.device_id = static_cast<DeviceId>(AudioDeviceId::kBuiltIn);

  ret = audio_pcm_track->SetUp(info);
  assert(ret == 0);
  tracks.push_back(audio_pcm_track);

  TestTrack *audio_a2dp_track = new TestTrack(this);
  info.track_id   = 102;
  info.device_id = static_cast<DeviceId>(AudioDeviceId::kBlueToothA2DP);

  ret = audio_a2dp_track->SetUp(info);
  assert(ret == 0);
  tracks.push_back(audio_a2dp_track);
  sessions_.insert(std::make_pair(session_id, tracks));

  TEST_INFO("%s: Exit", __func__);
  return ret;
}

status_t RecorderTest::CreateAudioAACTrack() {
  TEST_INFO("%s: Enter", __func__);

  SessionCb session_status_cb;
  session_status_cb.event_cb = [&] ( EventType event_type, void *event_data,
      size_t event_data_size) { SessionCallbackHandler(event_type,
      event_data, event_data_size); };

  uint32_t session_id;
  auto ret = recorder_.CreateSession(session_status_cb, &session_id);
  TEST_INFO("%s: sessions_id = %d", __func__, session_id);

  std::vector<TestTrack*> tracks;

  TestTrack *audio_aac_track = new TestTrack(this);
  TrackInfo info{};
  info.track_id   = 101;
  info.track_type = TrackType::kAudioAAC;
  info.session_id = session_id;
  info.device_id = static_cast<DeviceId>(AudioDeviceId::kBuiltIn);

  ret = audio_aac_track->SetUp(info);
  assert(ret == 0);
  tracks.push_back(audio_aac_track);
  sessions_.insert(std::make_pair(session_id, tracks));

  TEST_INFO("%s: Exit", __func__);
  return ret;
}

status_t RecorderTest::CreateAudio2AACTrack() {
  TEST_INFO("%s: Enter", __func__);

  SessionCb session_status_cb;
  session_status_cb.event_cb = [&] ( EventType event_type, void *event_data,
      size_t event_data_size) { SessionCallbackHandler(event_type,
      event_data, event_data_size); };

  uint32_t session_id;
  auto ret = recorder_.CreateSession(session_status_cb, &session_id);
  TEST_INFO("%s: sessions_id = %d", __func__, session_id);

  std::vector<TestTrack*> tracks;

  TestTrack *audio_aac_track1 = new TestTrack(this);
  TrackInfo info{};
  info.track_id   = 101;
  info.track_type = TrackType::kAudioAAC;
  info.session_id = session_id;
  info.device_id = static_cast<DeviceId>(AudioDeviceId::kBuiltIn);

  ret = audio_aac_track1->SetUp(info);
  assert(ret == 0);
  tracks.push_back(audio_aac_track1);

  TestTrack *audio_aac_track2 = new TestTrack(this);
  info.track_id   = 102;

  ret = audio_aac_track2->SetUp(info);
  assert(ret == 0);
  tracks.push_back(audio_aac_track2);
  sessions_.insert(std::make_pair(session_id, tracks));

  TEST_INFO("%s: Exit", __func__);
  return ret;
}

status_t RecorderTest::CreateAudioPCMAACTrack() {
  TEST_INFO("%s: Enter", __func__);

  SessionCb session_status_cb;
  session_status_cb.event_cb = [&] ( EventType event_type, void *event_data,
      size_t event_data_size) { SessionCallbackHandler(event_type,
      event_data, event_data_size); };

  uint32_t session_id;
  auto ret = recorder_.CreateSession(session_status_cb, &session_id);
  TEST_INFO("%s: sessions_id = %d", __func__, session_id);

  std::vector<TestTrack*> tracks;

  TestTrack *audio_pcm_track = new TestTrack(this);
  TrackInfo info{};
  info.track_id   = 101;
  info.track_type = TrackType::kAudioPCM;
  info.session_id = session_id;
  info.device_id = static_cast<DeviceId>(AudioDeviceId::kBuiltIn);

  ret = audio_pcm_track->SetUp(info);
  assert(ret == 0);
  tracks.push_back(audio_pcm_track);

  TestTrack *audio_aac_track = new TestTrack(this);
  info.track_id   = 102;
  info.track_type = TrackType::kAudioAAC;

  ret = audio_aac_track->SetUp(info);
  assert(ret == 0);
  tracks.push_back(audio_aac_track);
  sessions_.insert(std::make_pair(session_id, tracks));

  TEST_INFO("%s: Exit", __func__);
  return ret;
}

status_t RecorderTest::CreateAudioAMRTrack() {
  TEST_INFO("%s: Enter", __func__);

  SessionCb session_status_cb;
  session_status_cb.event_cb = [&] ( EventType event_type, void *event_data,
      size_t event_data_size) { SessionCallbackHandler(event_type,
      event_data, event_data_size); };

  uint32_t session_id;
  auto ret = recorder_.CreateSession(session_status_cb, &session_id);
  TEST_INFO("%s: sessions_id = %d", __func__, session_id);

  std::vector<TestTrack*> tracks;

  TestTrack *audio_amr_track = new TestTrack(this);
  TrackInfo info{};
  info.track_id   = 101;
  info.track_type = TrackType::kAudioAMR;
  info.session_id = session_id;
  info.device_id = static_cast<DeviceId>(AudioDeviceId::kBuiltIn);

  ret = audio_amr_track->SetUp(info);
  assert(ret == 0);
  tracks.push_back(audio_amr_track);
  sessions_.insert(std::make_pair(session_id, tracks));

  TEST_INFO("%s: Exit", __func__);
  return ret;
}

status_t RecorderTest::CreateAudio2AMRTrack() {
  TEST_INFO("%s: Enter", __func__);

  SessionCb session_status_cb;
  session_status_cb.event_cb = [&] ( EventType event_type, void *event_data,
      size_t event_data_size) { SessionCallbackHandler(event_type,
      event_data, event_data_size); };

  uint32_t session_id;
  auto ret = recorder_.CreateSession(session_status_cb, &session_id);
  TEST_INFO("%s: sessions_id = %d", __func__, session_id);

  std::vector<TestTrack*> tracks;

  TestTrack *audio_amr_track1 = new TestTrack(this);
  TrackInfo info{};
  info.track_id   = 101;
  info.track_type = TrackType::kAudioAMR;
  info.session_id = session_id;
  info.device_id = static_cast<DeviceId>(AudioDeviceId::kBuiltIn);

  ret = audio_amr_track1->SetUp(info);
  assert(ret == 0);
  tracks.push_back(audio_amr_track1);

  TestTrack *audio_amr_track2 = new TestTrack(this);
  info.track_id   = 102;

  ret = audio_amr_track2->SetUp(info);
  assert(ret == 0);
  tracks.push_back(audio_amr_track2);
  sessions_.insert(std::make_pair(session_id, tracks));

  TEST_INFO("%s: Exit", __func__);
  return ret;
}

status_t RecorderTest::CreateAudioPCMAMRTrack() {
  TEST_INFO("%s: Enter", __func__);

  SessionCb session_status_cb;
  session_status_cb.event_cb = [&] ( EventType event_type, void *event_data,
      size_t event_data_size) { SessionCallbackHandler(event_type,
      event_data, event_data_size); };

  uint32_t session_id;
  auto ret = recorder_.CreateSession(session_status_cb, &session_id);
  TEST_INFO("%s: sessions_id = %d", __func__, session_id);

  std::vector<TestTrack*> tracks;

  TestTrack *audio_pcm_track = new TestTrack(this);
  TrackInfo info{};
  info.track_id   = 101;
  info.track_type = TrackType::kAudioPCM;
  info.session_id = session_id;
  info.device_id = static_cast<DeviceId>(AudioDeviceId::kBuiltIn);

  ret = audio_pcm_track->SetUp(info);
  assert(ret == 0);
  tracks.push_back(audio_pcm_track);

  TestTrack *audio_amr_track = new TestTrack(this);
  info.track_id   = 102;
  info.track_type = TrackType::kAudioAMR;

  ret = audio_amr_track->SetUp(info);
  assert(ret == 0);
  tracks.push_back(audio_amr_track);
  sessions_.insert(std::make_pair(session_id, tracks));

  TEST_INFO("%s: Exit", __func__);
  return ret;
}

status_t RecorderTest::CreateAudioG711Track() {
  TEST_INFO("%s: Enter", __func__);

  SessionCb session_status_cb;
  session_status_cb.event_cb = [&] ( EventType event_type, void *event_data,
      size_t event_data_size) { SessionCallbackHandler(event_type,
      event_data, event_data_size); };

  uint32_t session_id;
  auto ret = recorder_.CreateSession(session_status_cb, &session_id);
  TEST_INFO("%s: sessions_id = %d", __func__, session_id);

  std::vector<TestTrack*> tracks;

  TestTrack *audio_g711_track = new TestTrack(this);
  TrackInfo info{};
  info.track_id   = 101;
  info.track_type = TrackType::kAudioG711;
  info.session_id = session_id;
  info.device_id = static_cast<DeviceId>(AudioDeviceId::kBuiltIn);

  ret = audio_g711_track->SetUp(info);
  assert(ret == 0);
  tracks.push_back(audio_g711_track);
  sessions_.insert(std::make_pair(session_id, tracks));

  TEST_INFO("%s: Exit", __func__);
  return ret;
}

status_t RecorderTest::CreateAudio2G711Track() {
  TEST_INFO("%s: Enter", __func__);

  SessionCb session_status_cb;
  session_status_cb.event_cb = [&] ( EventType event_type, void *event_data,
      size_t event_data_size) { SessionCallbackHandler(event_type,
      event_data, event_data_size); };

  uint32_t session_id;
  auto ret = recorder_.CreateSession(session_status_cb, &session_id);
  TEST_INFO("%s: sessions_id = %d", __func__, session_id);

  std::vector<TestTrack*> tracks;

  TestTrack *audio_g711_track1 = new TestTrack(this);
  TrackInfo info{};
  info.track_id   = 101;
  info.track_type = TrackType::kAudioG711;
  info.session_id = session_id;
  info.device_id = static_cast<DeviceId>(AudioDeviceId::kBuiltIn);

  ret = audio_g711_track1->SetUp(info);
  assert(ret == 0);
  tracks.push_back(audio_g711_track1);

  TestTrack *audio_g711_track2 = new TestTrack(this);
  info.track_id   = 102;

  ret = audio_g711_track2->SetUp(info);
  assert(ret == 0);
  tracks.push_back(audio_g711_track2);
  sessions_.insert(std::make_pair(session_id, tracks));

  TEST_INFO("%s: Exit", __func__);
  return ret;
}

status_t RecorderTest::CreateAudioPCMG711Track() {
  TEST_INFO("%s: Enter", __func__);

  SessionCb session_status_cb;
  session_status_cb.event_cb = [&] ( EventType event_type, void *event_data,
      size_t event_data_size) { SessionCallbackHandler(event_type,
      event_data, event_data_size); };

  uint32_t session_id;
  auto ret = recorder_.CreateSession(session_status_cb, &session_id);
  TEST_INFO("%s: sessions_id = %d", __func__, session_id);

  std::vector<TestTrack*> tracks;

  TestTrack *audio_pcm_track = new TestTrack(this);
  TrackInfo info{};
  info.track_id   = 101;
  info.track_type = TrackType::kAudioPCM;
  info.session_id = session_id;
  info.device_id = static_cast<DeviceId>(AudioDeviceId::kBuiltIn);

  ret = audio_pcm_track->SetUp(info);
  assert(ret == 0);
  tracks.push_back(audio_pcm_track);

  TestTrack *audio_g711_track = new TestTrack(this);
  info.track_id   = 102;
  info.track_type = TrackType::kAudioG711;

  ret = audio_g711_track->SetUp(info);
  assert(ret == 0);
  tracks.push_back(audio_g711_track);
  sessions_.insert(std::make_pair(session_id, tracks));

  TEST_INFO("%s: Exit", __func__);
  return ret;
}

status_t RecorderTest::CreateAudioPCMFluenceTrack() {
  TEST_INFO("%s: Enter", __func__);

  SessionCb session_status_cb;
  session_status_cb.event_cb = [&] ( EventType event_type, void *event_data,
      size_t event_data_size) { SessionCallbackHandler(event_type,
      event_data, event_data_size); };

  uint32_t session_id;
  auto ret = recorder_.CreateSession(session_status_cb, &session_id);
  TEST_INFO("%s: sessions_id = %d", __func__, session_id);

  std::vector<TestTrack*> tracks;

  TestTrack *audio_pcm_track = new TestTrack(this);
  TrackInfo info;
  memset(&info, 0x0, sizeof info);
  info.track_id   = 101;
  info.track_type = TrackType::kAudioPCMFP;
  info.session_id = session_id;
  info.device_id = static_cast<DeviceId>(AudioDeviceId::kBuiltIn);

  ret = audio_pcm_track->SetUp(info);
  assert(ret == 0);
  tracks.push_back(audio_pcm_track);
  sessions_.insert(std::make_pair(session_id, tracks));

  TEST_INFO("%s: Exit", __func__);
  return ret;
}

status_t RecorderTest::CreateAudioPCMAmbisonicTrack() {
  TEST_INFO("%s: Enter", __func__);

  SessionCb session_status_cb;
  session_status_cb.event_cb = [&] ( EventType event_type, void *event_data,
      size_t event_data_size) { SessionCallbackHandler(event_type,
      event_data, event_data_size); };

  uint32_t session_id;
  auto ret = recorder_.CreateSession(session_status_cb, &session_id);
  TEST_INFO("%s: sessions_id = %d", __func__, session_id);

  std::vector<TestTrack*> tracks;

  TestTrack *audio_pcm_track = new TestTrack(this);
  TrackInfo info;
  memset(&info, 0x0, sizeof info);
  info.track_id   = 101;
  info.track_type = TrackType::kAudioPCMAS;
  info.session_id = session_id;
  info.device_id = static_cast<DeviceId>(AudioDeviceId::kBuiltIn);

  ret = audio_pcm_track->SetUp(info);
  assert(ret == 0);
  tracks.push_back(audio_pcm_track);
  sessions_.insert(std::make_pair(session_id, tracks));

  TEST_INFO("%s: Exit", __func__);
  return ret;
}

status_t RecorderTest::CreateAudioMPEGHTrack() {
  TEST_INFO("%s: Enter", __func__);

  SessionCb session_status_cb;
  session_status_cb.event_cb = [&] ( EventType event_type, void *event_data,
      size_t event_data_size) { SessionCallbackHandler(event_type,
      event_data, event_data_size); };

  uint32_t session_id;
  auto ret = recorder_.CreateSession(session_status_cb, &session_id);
  TEST_INFO("%s: sessions_id = %d", __func__, session_id);

  std::vector<TestTrack*> tracks;

  TestTrack *audio_mpegh_track = new TestTrack(this);
  TrackInfo info{};
  info.track_id   = 101;
  info.track_type = TrackType::kAudioMPEGH;
  info.session_id = session_id;
  info.device_id = static_cast<DeviceId>(AudioDeviceId::kBuiltIn);

  ret = audio_mpegh_track->SetUp(info);
  assert(ret == 0);
  tracks.push_back(audio_mpegh_track);
  sessions_.insert(std::make_pair(session_id, tracks));

  TEST_INFO("%s: Exit", __func__);
  return ret;
}

status_t RecorderTest::StartSession() {
  TEST_INFO("%s: Enter", __func__);
  session_iter_ it = sessions_.begin();

  // Prepare tracks: setup files to dump track data, event etc.
  for (auto track : it->second)
    track->Prepare();
  uint32_t session_id = it->first;
  auto result = recorder_.StartSession(session_id);
  assert(result == NO_ERROR);

  TEST_INFO("%s: Enter", __func__);
  return NO_ERROR;
}

status_t RecorderTest::StopSession() {
  TEST_INFO("%s: Enter", __func__);
  session_iter_ it = sessions_.begin();

  uint32_t session_id = it->first;
  auto result = recorder_.StopSession(session_id, true /*flush buffers*/);
  assert(result == NO_ERROR);

  for (auto track : it->second)
    track->CleanUp();
  TEST_INFO("%s: Exit", __func__);
  return NO_ERROR;
}

status_t RecorderTest::PauseSession() {
  TEST_INFO("%s: Enter", __func__);
  session_iter_ it = sessions_.begin();
  uint32_t session_id = it->first;
  auto ret = recorder_.PauseSession(session_id);
  assert(ret == 0);
  TEST_INFO("%s: Exit", __func__);

  return NO_ERROR;
}

status_t RecorderTest::ResumeSession() {
  TEST_INFO("%s: Enter", __func__);
  session_iter_ it = sessions_.begin();
  uint32_t session_id = it->first;
  auto ret = recorder_.ResumeSession(session_id);
  assert(ret == 0);
  TEST_INFO("%s: Exit", __func__);

  return NO_ERROR;
}

status_t RecorderTest::DeleteSession() {
  TEST_INFO("%s: Enter", __func__);
  session_iter_ it = sessions_.begin();
  uint32_t session_id = it->first;
  // Delete all the tracks associated to session.
  status_t ret;
  for (auto track : it->second) {
      assert(track != nullptr);
      ret = recorder_.DeleteAudioTrack(session_id, track->GetTrackId());
      assert(ret == 0);
      delete track;
      track = nullptr;
  }
  // Once all tracks are deleted successfully delete session.
  ret = recorder_.DeleteSession(session_id);
  sessions_.erase(it);

  TEST_INFO("%s: Exit", __func__);
  return 0;
}

void RecorderTest::RecorderEventCallbackHandler(EventType event_type,
                                                void *event_data,
                                                size_t event_data_size) {
  TEST_INFO("%s: Enter", __func__);
  if (event_type == EventType::kServerDied) {
    // qmmf-server died, reason could be non recoverable FATAL error,
    // qmmf-server runs as a daemon and gets restarted automatically, on death
    // event application can cleanup all its resources and connect again.
    TEST_WARN("%s: Recorder Service died!", __func__);
  }
  TEST_INFO("%s: Exit", __func__);
}

void RecorderTest::SessionCallbackHandler(EventType event_type,
                                          void *event_data,
                                          size_t event_data_size) {
  TEST_INFO("%s: Enter", __func__);
  TEST_INFO("%s: Exit", __func__);
}

TestTrack::TestTrack(RecorderTest* recorder_test)
    : recorder_test_(recorder_test) {
  TEST_DBG("%s: Enter", __func__);
  track_info_ = {};
  TEST_DBG("%s: Exit", __func__);
}

TestTrack::~TestTrack() {
  TEST_DBG("%s: Enter", __func__);
  TEST_DBG("%s: Exit", __func__);
}

status_t TestTrack::SetUp(TrackInfo& track_info) {
  TEST_DBG("%s: Enter", __func__);
  int32_t ret = NO_ERROR;
  assert(recorder_test_ != nullptr);

  // Create AudioTrack
  AudioTrackCreateParam audio_track_params{};
  audio_track_params.in_devices_num = 0;
  audio_track_params.in_devices[audio_track_params.in_devices_num++] =
      track_info.device_id;
  audio_track_params.sample_rate = 48000;
  audio_track_params.channels    = 1;
  audio_track_params.bit_depth   = 16;
  audio_track_params.flags       = 0;

  switch (track_info.track_type) {
    case TrackType::kAudioPCM:
      audio_track_params.format = AudioFormat::kPCM;
      break;
    case TrackType::kAudioPCMFP:
      audio_track_params.format = AudioFormat::kPCM;
      ::std::string("record_fluence").copy(audio_track_params.profile,
                    strlen("record_fluence"));
      audio_track_params.sample_rate = 16000;
      break;
    case TrackType::kAudioPCMAS:
      audio_track_params.format = AudioFormat::kPCM;
      ::std::string("record_ambisonic").copy(audio_track_params.profile,
                    strlen("record_ambisonic"));
      audio_track_params.channels = 4;
      break;
    case TrackType::kAudioAAC:
      audio_track_params.format = AudioFormat::kAAC;
      audio_track_params.codec_params.aac.format = AACFormat::kADTS;
      audio_track_params.codec_params.aac.mode = AACMode::kAALC;
      break;
    case TrackType::kAudioAMR:
      audio_track_params.format = AudioFormat::kAMR;
      audio_track_params.codec_params.amr.isWAMR = false;
      audio_track_params.sample_rate = 8000;
      break;
    case TrackType::kAudioG711:
      audio_track_params.format = AudioFormat::kG711;
      audio_track_params.codec_params.g711.mode = G711Mode::kALaw;
      audio_track_params.sample_rate = 8000;
      break;
    case TrackType::kAudioMPEGH:
      audio_track_params.format = AudioFormat::kMPEGH;
      ::std::string("record_ambisonic").copy(audio_track_params.profile,
                    strlen("record_ambisonic"));
      audio_track_params.channels = 4;
      audio_track_params.codec_params.mpegh.bit_rate = 307200;
      break;
    default:
      assert(0);
      break;
  }
  TrackCb audio_track_cb;
  audio_track_cb.data_cb =
      [this] (uint32_t track_id, std::vector<BufferDescriptor> buffers,
              std::vector<MetaData> meta_buffers)
              -> void {
        TrackDataCB(track_id, buffers, meta_buffers);
      };

  audio_track_cb.event_cb =
      [this] (uint32_t track_id, EventType event_type, void *event_data,
              size_t event_data_size) -> void {
        TrackEventCB(track_id, event_type, event_data, event_data_size);
      };

  ret = recorder_test_->GetRecorder().CreateAudioTrack(track_info.session_id,
            track_info.track_id, audio_track_params, audio_track_cb);
  assert(ret == NO_ERROR);

  switch (track_info.track_type) {
    case TrackType::kAudioPCM:
    case TrackType::kAudioPCMFP:
    case TrackType::kAudioPCMAS:
    case TrackType::kAudioG711:
      // Configure .wav output.
      ret = wav_output_.Configure(kDefaultAudioFilenamePrefix,
                                  track_info.track_id, audio_track_params);
      assert(ret == NO_ERROR);
      break;
    case TrackType::kAudioAAC:
      // Configure .aac output.
      ret = aac_output_.Configure(kDefaultAudioFilenamePrefix,
                                  track_info.track_id, audio_track_params);
      assert(ret == NO_ERROR);
      break;
    case TrackType::kAudioAMR:
      // Configure .amr output.
      ret = amr_output_.Configure(kDefaultAudioFilenamePrefix,
                                  track_info.track_id, audio_track_params);
      assert(ret == NO_ERROR);
      break;
    case TrackType::kAudioMPEGH:
      // Configure .mhas output.
      ret = mpegh_output_.Configure(kDefaultAudioFilenamePrefix,
                                  track_info.track_id, audio_track_params);
      assert(ret == NO_ERROR);
      break;
    default:
      assert(0);
      break;
  }
  track_info_ = track_info;

  TEST_DBG("%s: Exit", __func__);
  return ret;
}

// Set up file to dump track data.
status_t TestTrack::Prepare() {

  TEST_DBG("%s: Enter", __func__);
  int32_t ret = NO_ERROR;

  if (track_info_.track_type == TrackType::kAudioPCM ||
      track_info_.track_type == TrackType::kAudioPCMFP ||
      track_info_.track_type == TrackType::kAudioPCMAS ||
      track_info_.track_type == TrackType::kAudioG711) {
    ret = wav_output_.Open();
    assert(ret == NO_ERROR);
  } else if (track_info_.track_type == TrackType::kAudioAAC) {
    ret = aac_output_.Open();
    assert(ret == NO_ERROR);
  } else if (track_info_.track_type == TrackType::kAudioAMR) {
    ret = amr_output_.Open();
    assert(ret == NO_ERROR);
  } else if (track_info_.track_type == TrackType::kAudioMPEGH) {
    ret = mpegh_output_.Open();
    assert(ret == NO_ERROR);
  }
  TEST_DBG("%s: Exit", __func__);
  return ret;
}

// Clean up file.
status_t TestTrack::CleanUp() {

  TEST_DBG("%s: Enter", __func__);
  int32_t ret = NO_ERROR;
  switch (track_info_.track_type) {
    case TrackType::kAudioPCM:
    case TrackType::kAudioPCMFP:
    case TrackType::kAudioPCMAS:
    case TrackType::kAudioG711:
      wav_output_.Close();
      break;
    case TrackType::kAudioAAC:
      aac_output_.Close();
      break;
    case TrackType::kAudioAMR:
      amr_output_.Close();
      break;
    case TrackType::kAudioMPEGH:
      mpegh_output_.Close();
      break;
    default:
      break;
  }
  TEST_DBG("%s: Exit", __func__);
  return ret;
}

void TestTrack::TrackEventCB(uint32_t track_id, EventType event_type,
                             void *event_data, size_t event_data_size) {

  TEST_DBG("%s: Enter", __func__);
  TEST_DBG("%s: Exit", __func__);
}

void TestTrack::TrackDataCB(uint32_t track_id, std::vector<BufferDescriptor>
                            buffers, std::vector<MetaData> meta_buffers) {

  TEST_DBG("%s: Enter track_id(%dd)", __func__, track_id);
  assert (recorder_test_ != nullptr);
  int32_t ret = 0;

  switch (track_info_.track_type) {
    case TrackType::kAudioPCM:
    case TrackType::kAudioPCMFP:
    case TrackType::kAudioPCMAS:
    case TrackType::kAudioG711:
      for (const BufferDescriptor& buffer : buffers) {
        ret = wav_output_.Write(buffer);
        assert(ret == 0);
      }
    break;
    case TrackType::kAudioAAC:
      for (const BufferDescriptor& buffer : buffers) {
        if (buffer.flag & static_cast<uint32_t>(BufferFlags::kFlagEOS))
          break;
        ret = aac_output_.Write(buffer);
        assert(ret == 0);
      }
    break;
    case TrackType::kAudioAMR:
      for (const BufferDescriptor& buffer : buffers) {
        if (buffer.flag & static_cast<uint32_t>(BufferFlags::kFlagEOS))
          break;
        ret = amr_output_.Write(buffer);
        assert(ret == 0);
      }
    break;
    case TrackType::kAudioMPEGH:
      for (const BufferDescriptor& buffer : buffers) {
        if (buffer.flag & static_cast<uint32_t>(BufferFlags::kFlagEOS))
          break;
        ret = mpegh_output_.Write(buffer);
        assert(ret == 0);
      }
    break;
    default:
    break;
  }
  // Return buffers back to service.
  ret = recorder_test_->GetRecorder().ReturnTrackBuffer(track_info_.session_id,
                                                        track_id, buffers);
  assert(ret == 0);
  TEST_DBG("%s: Exit", __func__);
}

void CmdMenu::PrintMenu() {
  printf("\n\n========= QMMF RECORDER AUDIO TEST MENU ================\n\n");

  printf(" Recorder Audio Test Application commands\n");
  printf(" ----------------------------------------\n");
  printf("   %c. Connect\n", CmdMenu::CONNECT_CMD);
  printf("   %c. Disconnect\n", CmdMenu::DISCONNECT_CMD);
  printf("   %c. Create Session: (PCM mono,16,48KHz)\n",
      CmdMenu::CREATE_PCM_AUD_SESSION_CMD);
  printf("   %c. Create Session: (PCM mono,16,48KHz + PCM mono,16,48KHz)\n",
      CmdMenu::CREATE_2PCM_AUD_SESSION_CMD);
  printf("   %c. Create Session: (SCO mono,16,48KHz)\n",
      CmdMenu::CREATE_SCO_AUD_SESSION_CMD);
  printf("   %c. Create Session: (PCM mono,16,48KHz + SCO mono,16,48KHz)\n",
      CmdMenu::CREATE_PCM_SCO_AUD_SESSION_CMD);
#ifdef ENABLE_A2DP_USECASE
  printf("   %c. Create Session: (A2DP mono,16,48KHz)\n",
      CmdMenu::CREATE_A2DP_AUD_SESSION_CMD);
  printf("   %c. Create Session: (PCM mono,16,48KHz + A2DP mono,16,48KHz)\n",
      CmdMenu::CREATE_PCM_A2DP_AUD_SESSION_CMD);
#endif
  printf("   %c. Create Session: (AAC mono)\n",
      CmdMenu::CREATE_AAC_AUD_SESSION_CMD);
  printf("   %c. Create Session: (AAC mono + AAC mono)\n",
      CmdMenu::CREATE_2AAC_AUD_SESSION_CMD);
  printf("   %c. Create Session: (PCM mono,16,48KHz + AAC mono)\n",
    CmdMenu::CREATE_PCM_AAC_AUD_SESSION_CMD);
  printf("   %c. Create Session: (AMR mono)\n",
      CmdMenu::CREATE_AMR_AUD_SESSION_CMD);
  printf("   %c. Create Session: (AMR mono + AMR mono)\n",
      CmdMenu::CREATE_2AMR_AUD_SESSION_CMD);
  printf("   %c. Create Session: (PCM mono,16,8KHz + AMR mono)\n",
      CmdMenu::CREATE_PCM_AMR_AUD_SESSION_CMD);
  printf("   %c. Create Session: (G711 mono)\n",
      CmdMenu::CREATE_G7ll_AUD_SESSION_CMD);
  printf("   %c. Create Session: (G711 mono + G711 mono)\n",
      CmdMenu::CREATE_2G7ll_AUD_SESSION_CMD);
  printf("   %c. Create Session: (PCM mono,16,8KHz + G711 mono)\n",
      CmdMenu::CREATE_PCM_G7ll_AUD_SESSION_CMD);
  printf("   %c. Create Session: (PCM mono,16,16KHz,FluencePro)\n",
      CmdMenu::CREATE_PCMFL_AUD_SESSION_CMD);
  printf("   %c. Create Session: (PCM 4ch,16,48KHz,Ambisonic)\n",
      CmdMenu::CREATE_PCMAS_AUD_SESSION_CMD);
  printf("   %c. Create Session: (MPEGH 4ch,16,48KHz)\n",
      CmdMenu::CREATE_MPEGH_AUD_SESSION_CMD);
  printf("   %c. Start Session\n", CmdMenu::START_SESSION_CMD);
  printf("   %c. Stop Session\n", CmdMenu::STOP_SESSION_CMD);
  printf("   %c. Pause Session\n", CmdMenu::PAUSE_SESSION_CMD);
  printf("   %c. Resume Session\n", CmdMenu::RESUME_SESSION_CMD);
  printf("   %c. Delete Session\n", CmdMenu::DELETE_SESSION_CMD);
  printf("   %c. Exit\n", CmdMenu::EXIT_CMD);
  printf("\n   Choice: ");
}

CmdMenu::Command CmdMenu::GetCommand(bool& is_print_menu) {
  if (is_print_menu) {
    PrintMenu();
    is_print_menu = false;
  }
  return CmdMenu::Command(static_cast<CmdMenu::CommandType>(getchar()));
}

int main(int argc,char *argv[]) {
  QMMF_GET_LOG_LEVEL();
  TEST_INFO("%s: Enter", __func__);

  RecorderTest test_context;

  CmdMenu cmd_menu(test_context);
  bool is_print_menu = true;
  int32_t exit_test = false;

  while (!exit_test) {
    CmdMenu::Command command = cmd_menu.GetCommand(is_print_menu);
    switch (command.cmd) {

      case CmdMenu::CONNECT_CMD: {
        test_context.Connect();
      }
      break;
      case CmdMenu::DISCONNECT_CMD: {
        test_context.Disconnect();
      }
      break;
      case CmdMenu::CREATE_PCM_AUD_SESSION_CMD: {
          test_context.CreateAudioPCMTrack();
      }
      break;
      case CmdMenu::CREATE_2PCM_AUD_SESSION_CMD: {
          test_context.CreateAudio2PCMTrack();
      }
      break;
      case CmdMenu::CREATE_SCO_AUD_SESSION_CMD: {
          test_context.CreateAudioSCOTrack();
      }
      break;
      case CmdMenu::CREATE_PCM_SCO_AUD_SESSION_CMD: {
          test_context.CreateAudioPCMSCOTrack();
      }
      break;
#ifdef ENABLE_A2DP_USECASE
      case CmdMenu::CREATE_A2DP_AUD_SESSION_CMD: {
          test_context.CreateAudioA2DPTrack();
      }
      break;
      case CmdMenu::CREATE_PCM_A2DP_AUD_SESSION_CMD: {
          test_context.CreateAudioPCMA2DPTrack();
      }
      break;
#endif
      case CmdMenu::CREATE_AAC_AUD_SESSION_CMD: {
          test_context.CreateAudioAACTrack();
      }
      break;
      case CmdMenu::CREATE_2AAC_AUD_SESSION_CMD: {
          test_context.CreateAudio2AACTrack();
      }
      break;
      case CmdMenu::CREATE_PCM_AAC_AUD_SESSION_CMD: {
          test_context.CreateAudioPCMAACTrack();
      }
      break;
      case CmdMenu::CREATE_AMR_AUD_SESSION_CMD: {
          test_context.CreateAudioAMRTrack();
      }
      break;
      case CmdMenu::CREATE_2AMR_AUD_SESSION_CMD: {
          test_context.CreateAudio2AMRTrack();
      }
      break;
      case CmdMenu::CREATE_PCM_AMR_AUD_SESSION_CMD: {
          test_context.CreateAudioPCMAMRTrack();
      }
      break;
      case CmdMenu::CREATE_G7ll_AUD_SESSION_CMD: {
          test_context.CreateAudioG711Track();
      }
      break;
      case CmdMenu::CREATE_2G7ll_AUD_SESSION_CMD: {
          test_context.CreateAudio2G711Track();
      }
      break;
      case CmdMenu::CREATE_PCM_G7ll_AUD_SESSION_CMD: {
          test_context.CreateAudioPCMG711Track();
      }
      break;
      case CmdMenu::CREATE_PCMFL_AUD_SESSION_CMD: {
          test_context.CreateAudioPCMFluenceTrack();
      }
      break;
      case CmdMenu::CREATE_PCMAS_AUD_SESSION_CMD: {
          test_context.CreateAudioPCMAmbisonicTrack();
      }
      break;
      case CmdMenu::CREATE_MPEGH_AUD_SESSION_CMD: {
          test_context.CreateAudioMPEGHTrack();
      }
      break;
      case CmdMenu::START_SESSION_CMD: {
        test_context.StartSession();
      }
      break;
      case CmdMenu::STOP_SESSION_CMD: {
        test_context.StopSession();
      }
      break;
      case CmdMenu::PAUSE_SESSION_CMD: {
        test_context.PauseSession();
      }
      break;
      case CmdMenu::RESUME_SESSION_CMD: {
        test_context.ResumeSession();
      }
      break;
      case CmdMenu::DELETE_SESSION_CMD: {
        test_context.DeleteSession();
      }
      break;
      case CmdMenu::EXIT_CMD: {
        exit_test = true;
      }
      break;
      case CmdMenu::NEXT_CMD: {
        is_print_menu = true;
      }
      break;
      default:
        break;
    }
  }
  return 0;
}
