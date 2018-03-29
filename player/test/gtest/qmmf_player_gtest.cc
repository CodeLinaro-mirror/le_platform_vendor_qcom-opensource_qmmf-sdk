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

#define LOG_TAG "PlayerGTest"

#include "player/test/gtest/qmmf_player_gtest.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <utils/Log.h>
#include <utils/String8.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "common/utils/qmmf_common_utils.h"
#include "player/test/gtest/qmmf_player_parser.h"

#define DEBUG
#define TEST_INFO(fmt, args...)  ALOGD(fmt, ##args)
#define TEST_ERROR(fmt, args...) ALOGE(fmt, ##args)
#define TEST_WARN(fmt, args...) ALOGE(fmt, ##args)
#ifdef DEBUG
#define TEST_DBG  TEST_INFO
#else
#define TEST_DBG(...) ((void)0)
#endif

const ::std::string PlayerGtest::kCodecType[AudioFileType::kMax] = {
  "aac",
  "amr",
  "g711"
};

PlayerGtest::PlayerGtest()
  : start_again_(false),
    thread_(nullptr),
    state_(State::kStopped) {
  QMMF_GET_LOG_LEVEL();
}

void PlayerGtest::GetGTestParams()
{
  char prop[PROPERTY_VALUE_MAX];
  memset(prop, 0, sizeof(prop));
  property_get("persist.player.gtest.iteration", prop, DEFAULT_ITERATION);
  iteration_count_ = static_cast<uint32_t>(atoi(prop));
}

void PlayerGtest::PlayerHandler(EventType event_type,
                                void* event_data,
                                size_t event_data_size) {
  TEST_INFO("%s: Enter", __func__);
  TEST_INFO("%s event_type[%d]", __func__,
            static_cast<int32_t>(event_type));

  if (event_type == EventType::kStopped) {
    if (thread_ != nullptr) {
      thread_->join();
      delete thread_;
      thread_ = nullptr;
    }

    fprintf(stderr, "\nPlayback has finished.\n");
  }

  TEST_INFO("%s: Exit", __func__);
}

void PlayerGtest::AudioTrackHandler(uint32_t track_id,
                                    EventType event_type,
                                    void* event_data,
                                    size_t event_data_size) {
  TEST_INFO("%s: Enter", __func__);
  TEST_INFO("%s event_type[%d]", __func__,
            static_cast<int32_t>(event_type));
  TEST_INFO("%s track_id[%u]", __func__, track_id);
  TEST_INFO("%s: Exit", __func__);
}

void PlayerGtest::VideoTrackHandler(uint32_t track_id,
                                    EventType event_type,
                                    void* event_data,
                                    size_t event_data_size) {
  TEST_INFO("%s: Enter", __func__);
  TEST_INFO("%s event_type[%d]", __func__,
            static_cast<int32_t>(event_type));
  TEST_INFO("%s track_id[%u]", __func__, track_id);
  TEST_INFO("%s: Exit", __func__);
}

void PlayerGtest::Fail() {
  TEST_INFO("%s: Enter", __func__);

  if (thread_ != nullptr) {
    {
      std::lock_guard<std::mutex> lock(lock_);
      state_ = State::kStopped;
    }

    thread_->join();
    delete thread_;
    thread_ = nullptr;
  }

  ASSERT_TRUE(false);

  TEST_INFO("%s: Exit", __func__);
}

void PlayerGtest::SetUp() {
  TEST_INFO("%s Enter ", __func__);

  test_info_ = ::testing::UnitTest::GetInstance()->current_test_info();

  GetGTestParams();
  filetype_ = AudioFileType::kAAC;
  start_again_ = false;

  TEST_INFO("%s Exit ", __func__);
}

void PlayerGtest::TearDown() {
  TEST_INFO("%s Enter ", __func__);
  TEST_INFO("%s Exit ", __func__);
}

/*
 * ConnectToService: This tests the Connect/Disconnect APIs.
 *
 * API test sequence:
 *  - Connect
 *  - Disconnect
 */
TEST_F(PlayerGtest, ConnectToService) {
  fprintf(stderr, "\n---------- Run Test %s.%s ------------\n",
          test_info_->test_case_name(), test_info_->name());

  for(uint32_t i = 1; i <= iteration_count_; i++) {
    fprintf(stderr, "test iteration = %d/%d\n", i, iteration_count_);
    TEST_INFO("%s: Running Test(%s) iteration = %d ",
              __func__, test_info_->name(), i);

    auto result = Connect();
    if (result != NO_ERROR) Fail();
    sleep(3);

    result = Disconnect();
    if (result != NO_ERROR) Fail();
  }

  fprintf(stderr, "---------- Test Completed %s.%s ----------\n",
          test_info_->test_case_name(), test_info_->name());
}

/*
 * AllocDeallocBufAudioPlayback: This tests Start/Stop/Pause/Resume APIs.
 * This will allocate/deallocate buffers in every iteration allocated for a
 * particular codec.
 *
 * Api test sequence:
 *  - loop Start {
 *  - CreateAudioTrack
 *  - Prepare
 *  - Start
 *
 *  - loop Start {
 *    - Pause
 *    - Resume
 *  - } loop End
 *
 *  - Stop
 *  - DeleteAudioTrack
 *  - } loop End
 */
TEST_F(PlayerGtest, AllocDeallocBufAudioPlayback) {
  fprintf(stderr, "\n---------- Run Test %s.%s ------------\n",
          test_info_->test_case_name(), test_info_->name());

  auto result = Connect();
  if (result != NO_ERROR) Fail();

  for(uint32_t k = 0; k < AudioFileType::kMax; k++) {
    fprintf(stderr, "\n-----Iterations are runnning for %s codec type ------\n",
            kCodecType[k].c_str());
    filetype_ = static_cast<AudioFileType>(k);

    for(uint32_t i = 1; i <= iteration_count_; i++) {
      fprintf(stderr, "test iteration = %d/%d\n", i, iteration_count_);
      TEST_INFO("%s: Running Test(%s) iteration = %d ",
                __func__, test_info_->name(), i);

      result = Prepare();
      if (result != NO_ERROR) Fail();
      sleep(2);

      result = Start();
      if (result != NO_ERROR) Fail();
      sleep(5);

      for (uint32_t j = 1; j<= 2; j++) {
        result = Pause();
        if (result != NO_ERROR) Fail();
        sleep(4);

        result = Resume();
        if (result != NO_ERROR) Fail();
        sleep(4);
      }

      result = Stop();
      if (result != NO_ERROR) Fail();
      sleep(2);

      result = Delete();
      if (result != NO_ERROR) Fail();
      sleep(2);
    }
  }

  result = Disconnect();
  if (result != NO_ERROR) Fail();

  fprintf(stderr, "---------- Test Completed %s.%s ----------\n",
          test_info_->test_case_name(), test_info_->name());
}

/*
 * StartStopAudioPlayback: This tests Start/Stop/Pause/Resume APIs.
 * This will allocate/deallocate buffers once and use them for all
 * iterations for particular codec.
 *
 * Api test sequence:
 *  - CreateAudioTrack
 *  - Prepare
 *  - loop Start {
 *    - Start
 *    - loop Start {
 *      - Pause
 *      - Resume
 *    - } loop End
 *    - Stop
 *  } loop End
 *  - DeleteAudioTrack
 */
TEST_F(PlayerGtest, StartStopAudioPlayback) {
  fprintf(stderr, "\n---------- Run Test %s.%s ------------\n",
          test_info_->test_case_name(), test_info_->name());

  auto result = Connect();
  if (result != NO_ERROR) Fail();

  for(uint32_t k = 0; k < AudioFileType::kMax; k++) {
    fprintf(stderr, "\n-----Iterations are runnning for %s codec type ------\n",
            kCodecType[k].c_str());
    filetype_ = static_cast<AudioFileType>(k);

    result = Prepare();
    if (result != NO_ERROR) Fail();
    sleep(2);

    for(uint32_t i = 1; i <= iteration_count_; i++) {
      fprintf(stderr, "test iteration = %d/%d\n", i, iteration_count_);
      TEST_INFO("%s: Running Test(%s) iteration = %d ",
                __func__, test_info_->name(), i);

      result = Start();
      if (result != NO_ERROR) Fail();
      sleep(5);

      for (uint32_t j = 1; j<= 2; j++) {
        result = Pause();
        if (result != NO_ERROR) Fail();
        sleep(4);

        result = Resume();
        if (result != NO_ERROR) Fail();
        sleep(4);
      }

      result = Stop();
      if (result != NO_ERROR) Fail();
      sleep(2);
    }

    result = Delete();
    if (result != NO_ERROR) Fail();
    sleep(2);
  }

  result = Disconnect();
  if (result != NO_ERROR) Fail();

  fprintf(stderr, "---------- Test Completed %s.%s ----------\n",
          test_info_->test_case_name(), test_info_->name());
}

/*
 * RepeatedAudioPlayback: This tests Start/Stop/Pause/Resume APIs.
 * This will be the same as the StartStopAudioPlayback test; but instead of
 * being stopped by the Stop command, it will be stopped at the EOF and will
 * restart playback again.
 *
 * Api test sequence:
 *  - CreateAudioTrack
 *  - Prepare
 *  - loop Start {
 *    - Start
 *    - loop Start {
 *      - Pause
 *      - Resume
 *    - } loop End
 *    - Stop AT EOF
 *  - } loop End
 *  - DeleteAudioTrack
 */
TEST_F(PlayerGtest, RepeatedAudioPlayback) {
  fprintf(stderr, "\n---------- Run Test %s.%s ------------\n",
          test_info_->test_case_name(), test_info_->name());

  auto result = Connect();
  if (result != NO_ERROR) Fail();

  for(uint32_t k = 0; k < AudioFileType::kMax; k++) {
    fprintf(stderr, "\n-----Iterations are runnning for %s codec type ------\n",
            kCodecType[k].c_str());
    filetype_ = static_cast<AudioFileType>(k);

    result = Prepare();
    if (result != NO_ERROR) Fail();
    sleep(2);

    for(uint32_t i = 1; i <= iteration_count_; i++) {
      fprintf(stderr, "test iteration = %d/%d\n", i, iteration_count_);
      TEST_INFO("%s: Running Test(%s) iteration = %d ",
                __func__, test_info_->name(), i);

      result = Start();
      if (result != NO_ERROR) Fail();
      sleep(5);

      for (uint32_t j = 1; j<= 2; j++) {
        result = Pause();
        if (result != NO_ERROR) Fail();
        sleep(4);

        result = Resume();
        if (result != NO_ERROR) Fail();
        sleep(4);
      }

      while (true) {
        {
          std::lock_guard<std::mutex> lock(lock_);
          if (state_ == State::kStopped || state_ == State::kError) break;
        }
        sleep(1);
      }

      result = Stop();
      if (result != NO_ERROR) Fail();
      sleep(2);
    }

    result = Delete();
    if (result != NO_ERROR) Fail();
    sleep(2);
  }

  result = Disconnect();
  if (result != NO_ERROR) Fail();

  fprintf(stderr, "---------- Test Completed %s.%s ----------\n",
          test_info_->test_case_name(), test_info_->name());
}

int32_t PlayerGtest::Connect() {
  TEST_INFO("%s: Enter", __func__);

  PlayerCb callback;
  callback.event_cb = [this](EventType event_type,
                             void *event_data,
                             size_t event_data_size) {
    PlayerHandler(event_type, event_data, event_data_size);
  };

  status_t result = player_.Connect(callback);
  if (result != NO_ERROR) {
    TEST_ERROR("%s: Connect() returned error[%d]", __func__, result);
    return result;
  }

  TEST_INFO("%s: Exit", __func__);
  return result;
}

int32_t PlayerGtest::Disconnect() {
  TEST_INFO("%s: Enter", __func__);

  status_t result = player_.Disconnect();
  if (result != NO_ERROR)
    TEST_ERROR("%s: Disconnect() returned error[%d]", __func__, result);

  TEST_INFO("%s: Exit", __func__);
  return result;
}

int32_t PlayerGtest::Prepare() {
  TEST_INFO("%s: Enter", __func__);
  std::lock_guard<std::mutex> lock(lock_);
  status_t result;

  AudioTrackCreateParam audio_track_param;
  memset(&audio_track_param, 0x0, sizeof audio_track_param);

  result = ParseFile(audio_track_param);
  if (result != NO_ERROR) {
    TEST_ERROR("%s: ParseFile() returned error[%d]", __func__, result);
    return result;
  }

  uint32_t track_id = 1;

  audio_track_param.out_device = AudioOutSubtype::kBuiltIn;

  TrackCb callback;
  callback.event_cb = [this] (uint32_t track_id,
                              EventType event_type,
                              void *event_data,
                              size_t event_data_size) {
    AudioTrackHandler(track_id, event_type, event_data, event_data_size);
  };

  result = player_.CreateAudioTrack(track_id, audio_track_param, callback);
  if (result != NO_ERROR) {
    TEST_ERROR("%s: CreateAudioTrack() returned error[%d]", __func__, result);
    return result;
  }

  result = player_.Prepare();
  if (result != NO_ERROR) {
    TEST_ERROR("%s: Prepare() returned error[%d]", __func__, result);
    return result;
  }

  start_again_ = false;

  TEST_INFO("%s: Exit", __func__);
  return result;
}

int32_t PlayerGtest::ParseFile(AudioTrackCreateParam& audio_track_param) {
  TEST_INFO("%s: Enter", __func__);
  auto result = 0;

  switch(filetype_)
  {
    case AudioFileType::kAAC:
      aac_file_io_ = new AACfileIO(FILE_PATH_PREFIX "aac");
      result = aac_file_io_->Fillparams(&audio_track_param);
      if (result != NO_ERROR)
        TEST_INFO("%s Could not fill the AAC params", __func__);
      break;

    case AudioFileType::kG711:
      g711_file_io_ = new G711fileIO(FILE_PATH_PREFIX "wav");
      result = g711_file_io_->Fillparams(&audio_track_param);
      if (result != NO_ERROR)
        TEST_INFO("%s Could not fill the G711 params", __func__);
      break;

    case AudioFileType::kAMR:
      amr_file_io_ = new AMRfileIO(FILE_PATH_PREFIX "amr");
      result = amr_file_io_->Fillparams(&audio_track_param);
      if (result != NO_ERROR)
        TEST_INFO("%s Could not fill the AMR params", __func__);
      break;

    default:
      break;
  }

  TEST_INFO("%s: Exit", __func__);
  return result;
}

int32_t PlayerGtest::Delete() {
  TEST_INFO("%s: Enter", __func__);

  if (thread_ != nullptr) {
    thread_->join();
    delete thread_;
    thread_ = nullptr;
  }

  const uint32_t track_id = 1;
  status_t result = player_.DeleteAudioTrack(track_id);
  if (result != NO_ERROR)
    TEST_ERROR("%s: Prepare() returned error[%d]", __func__, result);

  TEST_INFO("%s: Exit", __func__);
  return result;
}

int32_t PlayerGtest::Start() {
  TEST_INFO("%s: Enter", __func__);
  status_t result;

  if(start_again_)
  {
    AudioTrackCreateParam audio_track_param;
    memset(&audio_track_param, 0x0, sizeof audio_track_param);

    result = ParseFile(audio_track_param);
    if (result != NO_ERROR) {
      TEST_ERROR("%s: ParseFile() returned error[%d]", __func__, result);
      return result;
    }
  }

  result = player_.Start();
  if (result != NO_ERROR) {
    TEST_ERROR("%s: Start() returned error[%d]", __func__, result);
    return result;
  }

  {
    std::lock_guard<std::mutex> lock(lock_);
    state_ = State::kRunning;
  }

  thread_ = new thread(PlayerGtest::ThreadEntry, this);
  if (thread_ == nullptr) {
    TEST_ERROR("%s: could not start thread", __func__);
    return -1;
  }

  TEST_INFO("%s: Exit", __func__);
  return result;
}

void PlayerGtest::ThreadEntry(PlayerGtest* player_gtest) {
  QMMF_DEBUG("%s() TRACE", __func__);

  player_gtest->Thread();
}

void PlayerGtest::Thread() {
  TEST_INFO("%s: Enter", __func__);
  auto ret = 0;
  uint32_t track_id = 1;
  uint32_t result = 0;

  while (true) {
    {
      std::lock_guard<std::mutex> lock(lock_);
      if (state_ == State::kPaused) continue;
      if (state_ == State::kStopped || state_ == State::kError) break;
    }

    std::vector<TrackBuffer> buffers;
    TrackBuffer tb;
    memset(&tb, 0x0, sizeof tb);
    buffers.push_back(tb);

    ret = player_.DequeueInputBuffer(track_id, buffers);
    if (ret != NO_ERROR) {
      TEST_ERROR("%s: DequeueInputBuffer() returned error[%d]",
                 __func__, result);
      std::lock_guard<std::mutex> lock(lock_);
      state_ = State::kError;
      break;
    }

    for (TrackBuffer& buffer : buffers) {
      int32_t num_frames_read = 0;
      uint32_t bytes_read = 0;

      switch (filetype_)
      {
        case AudioFileType::kAAC:
          result = aac_file_io_->GetFrames(buffer.data, buffer.size,
                                           &num_frames_read, &bytes_read);
          break;
        case AudioFileType::kG711:
          result = g711_file_io_->GetFrames(buffer.data, buffer.size,
                                            &bytes_read);
          break;
        case AudioFileType::kAMR:
          result = amr_file_io_->GetFrames(buffer.data, buffer.size,
                                           &num_frames_read, &bytes_read);
          break;
        default:
          TEST_ERROR("%s: invalid filetype in switch[%d]",
                     __func__, static_cast<int>(filetype_));
          break;
      }
      buffer.filled_size = bytes_read;

      if (result != 0) {
        // EOF reached
        TEST_INFO("%s: File read completed result is %d",
                  __func__, result);
        buffer.flag |= static_cast<uint32_t>(BufferFlags::kFlagEOS);

        {
          std::lock_guard<std::mutex> lock(lock_);
          state_ = State::kStopped;
        }
      }

      TEST_DBG("%s: filled_size %d", __func__, buffer.filled_size);
      TEST_DBG("%s: buffer size %d", __func__, buffer.size);
      TEST_DBG("%s: vaddr 0x%p", __func__, buffer.data);

      ret = player_.QueueInputBuffer(track_id, buffers, nullptr, 0,
                                     TrackMetaBufferType::kNone);
      if (ret != NO_ERROR) {
        TEST_ERROR("%s: QueueInputBuffer() returned error[%d]",
                   __func__, result);
        std::lock_guard<std::mutex> lock(lock_);
        state_ = State::kError;
        break;
      }
    }
    buffers.clear();
  }

  switch (filetype_) {
    case AudioFileType::kAAC: delete aac_file_io_; break;
    case AudioFileType::kG711: delete g711_file_io_; break;
    case AudioFileType::kAMR: delete amr_file_io_; break;
    default: break;
  }

  start_again_ = true;

  TEST_INFO("%s: Exit", __func__);
}

int32_t PlayerGtest::Stop() {
  TEST_INFO("%s: Enter", __func__);

  bool thread_failed = false;
  {
    std::lock_guard<std::mutex> lock(lock_);
    if (state_ == State::kError)
      thread_failed = true;
    state_ = State::kStopped;
  }
  if (thread_failed) Fail();

  status_t result = player_.Stop();
  if (result != NO_ERROR)
    TEST_ERROR("%s: Stop() returned error[%d]", __func__, result);

  if (thread_ != nullptr) {
    thread_->join();
    delete thread_;
    thread_ = nullptr;
  }

  TEST_INFO("%s: Exit", __func__);
  return result;
}

int32_t PlayerGtest::Pause() {
  TEST_INFO("%s: Enter", __func__);

  bool thread_failed = false;
  {
    std::lock_guard<std::mutex> lock(lock_);
    if (state_ == State::kError)
      thread_failed = true;
    else if (state_ == State::kRunning)
      state_ = State::kPaused;
  }
  if (thread_failed) Fail();

  auto result = player_.Pause();
  if (result != NO_ERROR) {
    TEST_ERROR("%s: Pause() returned error[%d]", __func__, result);
    return result;
  }

  TEST_INFO("%s: Exit", __func__);
  return result;
}

int32_t PlayerGtest::Resume()
{
  TEST_INFO("%s: Enter", __func__);

  bool thread_failed = false;
  {
    std::lock_guard<std::mutex> lock(lock_);
    if (state_ == State::kError)
      thread_failed = true;
    else if (state_ == State::kPaused)
      state_ = State::kRunning;
  }
  if (thread_failed) Fail();

  auto result = player_.Resume();
  if (result != NO_ERROR) {
    TEST_ERROR("%s: Resume() returned error[%d]", __func__, result);
    return result;
  }

  TEST_INFO("%s: Exit", __func__);
  return result;
}
