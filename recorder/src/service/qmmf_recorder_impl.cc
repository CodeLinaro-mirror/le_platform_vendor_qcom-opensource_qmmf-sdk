/*
 * Copyright (c) 2016-2020, The Linux Foundation. All rights reserved.
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

#define LOG_TAG "RecorderImpl"

#include "recorder/src/service/qmmf_recorder_impl.h"

#include <functional>
#include <future>

#include "recorder/src/client/qmmf_recorder_params_internal.h"

#ifdef LOG_LEVEL_KPI
volatile uint32_t kpi_debug_level = BASE_KPI_FLAG;
#endif

namespace qmmf {

namespace recorder {

RecorderImpl* RecorderImpl::instance_ = nullptr;

RecorderImpl* RecorderImpl::CreateRecorder() {

  if (!instance_) {
    instance_ = new RecorderImpl;
    if (!instance_) {
      QMMF_ERROR("%s: Can't Create Recorder Instance!", __func__);
      return NULL;
    }
  }
  QMMF_INFO("%s: Recorder Instance Created Successfully(0x%p)",
      __func__, instance_);
  return instance_;
}

RecorderImpl::RecorderImpl()
  : unique_session_id_(0),
    camera_source_(nullptr),
    encoder_core_(nullptr)
#ifdef PULSE_AUDIO_ENABLE
    ,audio_source_(nullptr),
    audio_encoder_core_(nullptr)
#endif
{

    QMMF_GET_LOG_LEVEL();
    QMMF_KPI_GET_MASK();
    QMMF_KPI_DETAIL();
    QMMF_INFO("%s: Enter", __func__);
    QMMF_INFO("%s: Exit", __func__);
  }

RecorderImpl::~RecorderImpl() {

  QMMF_KPI_DETAIL();
  QMMF_INFO("%s: Enter", __func__);

  if (camera_source_) {
    delete camera_source_;
    camera_source_ = nullptr;
  }
  if (encoder_core_) {
    delete encoder_core_;
    encoder_core_ = nullptr;
  }
#ifdef PULSE_AUDIO_ENABLE
  if (audio_source_) {
    delete audio_source_;
    audio_source_ = nullptr;
  }
  if (audio_encoder_core_) {
    delete audio_encoder_core_;
    audio_encoder_core_ = nullptr;
  }
#endif
  instance_ = nullptr;
  QMMF_INFO("%s: Exit (0x%p)", __func__, this);
}

status_t RecorderImpl::Init(const RemoteCallbackHandle& remote_cb_handle) {

  QMMF_INFO("%s: Enter", __func__);
  QMMF_KPI_DETAIL();

  assert(remote_cb_handle != nullptr);
  remote_cb_handle_ = remote_cb_handle;

  camera_source_ = CameraSource::CreateCameraSource();
  if (!camera_source_) {
    QMMF_ERROR("%s: Can't Create CameraSource Instance!", __func__);
    return NO_MEMORY;
  }
  QMMF_INFO("%s: CameraSource Instance Created Successfully!",
      __func__);

  encoder_core_ = EncoderCore::CreateEncoderCore();
  if (!encoder_core_) {
    QMMF_ERROR("%s: Can't Create EncoderCore Instance!", __func__);
    return NO_MEMORY;
  }
  QMMF_INFO("%s: EncoderCore Instance Created Successfully!",
      __func__);
#ifdef PULSE_AUDIO_ENABLE
  audio_source_ = AudioSource::CreateAudioSource();
  if (!audio_source_) {
    QMMF_ERROR("%s: Can't Create AudioSource Instance!", __func__);
    return NO_MEMORY;
  }
  QMMF_INFO("%s: AudioSource Instance Created Successfully!",
      __func__);

  audio_encoder_core_ = AudioEncoderCore::CreateAudioEncoderCore();
  if (!audio_encoder_core_) {
    QMMF_ERROR("%s: Can't Create AudioEncoderCore Instance!", __func__);
    return NO_MEMORY;
  }
  QMMF_INFO("%s: AudioEncoderCore Instance Created Successfully!",
      __func__);
#endif
  QMMF_INFO("%s: Exit", __func__);
  return NO_ERROR;
}

status_t RecorderImpl::DeInit() {

  QMMF_INFO("%s: Enter", __func__);
  QMMF_KPI_DETAIL();

  if (camera_source_) {
    delete camera_source_;
    camera_source_ = nullptr;
  }
  if (encoder_core_) {
    delete encoder_core_;
    encoder_core_ = nullptr;
  }
#ifdef PULSE_AUDIO_ENABLE
  if (audio_source_) {
    delete audio_source_;
    audio_source_ = nullptr;
  }
  if (audio_encoder_core_) {
    delete audio_encoder_core_;
    audio_encoder_core_ = nullptr;
  }
#endif
  QMMF_INFO("%s: Exit", __func__);
  return NO_ERROR;
}

status_t RecorderImpl::RegisterClient(const uint32_t client_id) {

  QMMF_INFO("%s: Enter client_id(%d)", __func__, client_id);

  std::lock_guard<std::mutex> session_lock(client_session_lock_);
  if (client_session_map_.count(client_id) != 0) {
    QMMF_WARN("%s: Client is already connected !!", __func__);
    return NO_ERROR;
  }
  client_session_map_.emplace(client_id, SessionTrackMap());
  QMMF_INFO("%s: client_session_map_.size(%d)", __func__,
      client_session_map_.size());

  auto const& session_track_map = client_session_map_[client_id];
  QMMF_INFO("%s: session_track_map.size(%d)", __func__,
      session_track_map.size());

  std::lock_guard<std::mutex> status_lock(client_state_lock_);
  client_state_.emplace(client_id, ClientState::kAlive);
  QMMF_INFO("%s: client_status_map_.size(%d)", __func__,
      client_cameraid_map_.size());

  std::lock_guard<std::mutex> camera_lock(camera_map_lock_);
  client_cameraid_map_.emplace(client_id, std::map<uint32_t, bool>());
  QMMF_INFO("%s: Exit client_id(%d)", __func__, client_id);
  return NO_ERROR;
}

status_t RecorderImpl::DeRegisterClient(const uint32_t client_id,
                                        bool force_cleanup) {

  QMMF_INFO("%s: Enter client_id(%d)", __func__, client_id);

  status_t ret = NO_ERROR;
  if (!IsClientValid(client_id)) {
    QMMF_ERROR("%s: Client(%u) is not connected!", __func__, client_id);
    return BAD_VALUE;
  }

  if (!force_cleanup) {
    QMMF_WARN("%s Resource belogs to client(%d) are not released!",
        __func__, client_id);
    std::unique_lock<std::mutex> lk(camera_map_lock_);
    if (client_cameraid_map_[client_id].empty()) {
      client_cameraid_map_.erase(client_id);
    }
    return NO_ERROR;
  }

  // This is the case when client is dead before releasing its acquired
  // resources, service is trying to free up his resources to avoid
  // affecting other connected clients, worst case if this doesn't help
  // then micro restart is the only option left.
  QMMF_INFO("%s: triggering force cleanup for dead client(%d)",
      __func__, client_id);
  {
    // Raise the client_died_ flag in order to signal the audio/video
    // track callbacks to return the buffers from where they originated.
    std::lock_guard<std::mutex> lock(client_state_lock_);
    client_state_[client_id] = ClientState::kDead;
  }

  {
    // Cleanup client sessions.
    std::unique_lock<std::mutex> lk(client_session_lock_);
    auto session_track_map = client_session_map_[client_id];
    lk.unlock();

    for (auto session : session_track_map) {
      auto session_id = session.first;

      ret = StopSession(client_id, session_id, false, true);
      if (ret != NO_ERROR) {
        QMMF_WARN("%s: Client(%u): Session(%u) internal stop failed!,",
            __func__, client_id, session_id);
        // Carry-on even stop session fails.
      }
      auto track = session.second.rbegin();
      while (track != session.second.rend()) {
        uint32_t client_track_id  = track->first;
        TrackInfo track_info      = track->second;
        uint32_t service_track_id = track_info.track_id;

        QMMF_INFO("%s: Track to Delete, client_id(%d):session_id(%d), "
            "client_track_id(%d):service_track_id(%x)", __func__,
            client_id, session_id, client_track_id, service_track_id);

        if (track_info.type == TrackType::kVideo) {
          ret = DeleteVideoTrack(client_id, session_id, client_track_id);
        }
#ifdef PULSE_AUDIO_ENABLE
        else {
          ret = DeleteAudioTrack(client_id, session_id, client_track_id);
        }
#endif
        // Carry-on even delete track fails.
        ++track;
      }
      ret = DeleteSession(client_id, session_id);
      if (ret != NO_ERROR) {
        QMMF_WARN("%s: Internal delete session is failed! client_id(%d):"
            "session_id(%d)", __func__, client_id, session_id);
      }
    }
    QMMF_INFO("%s: Number of sessions(%d) left after cleanup for"
        " client(%d)!", __func__, session_track_map.size(), client_id);

    lk.lock();
    client_session_map_.erase(client_id);
  }

  {
    // Close the cameras owned by the client.
    std::unique_lock<std::mutex> lk(camera_map_lock_);
    auto cameras = client_cameraid_map_[client_id];
    lk.unlock();

    QMMF_INFO("%s: Client(%u) Cameras %d", __func__, client_id, cameras.size());
    for (auto camera : cameras) {
      auto camera_id = camera.first;
      ret = StopCamera(client_id, camera_id);
      if (ret != NO_ERROR) {
        QMMF_INFO("%s: Client(%u): Camera ID(%d) close failed!", __func__,
            client_id, camera_id);
        // Go ahead with removing camera id from map.
      }
    }

    lk.lock();
    client_cameraid_map_.erase(client_id);
  }

  std::lock_guard<std::mutex> lock(client_state_lock_);
  client_state_.erase(client_id);

  QMMF_INFO("%s: Exit client_id(%d)", __func__, client_id);
  return NO_ERROR;
}

status_t RecorderImpl::StartCamera(const uint32_t client_id,
                                   const uint32_t camera_id,
                                   const float frame_rate,
                                   const CameraExtraParam& extra_param,
                                   bool enable_result_cb) {

  QMMF_DEBUG("%s: Enter", __func__);
  QMMF_KPI_DETAIL();

  if (!IsClientValid(client_id)) {
    QMMF_ERROR("%s: Client(%u) is not connected!", __func__, client_id);
    return BAD_VALUE;
  }

  bool owned = IsCameraOwned(client_id, camera_id);

  CameraSlaveMode camera_slave_mode = {};
  if (extra_param.Exists(QMMF_CAMERA_SLAVE_MODE)) {
    size_t entry_count = extra_param.EntryCount(QMMF_CAMERA_SLAVE_MODE);
    if (entry_count == 1) {
      extra_param.Fetch(QMMF_CAMERA_SLAVE_MODE, camera_slave_mode, 0);
    }
  }

  if ((camera_slave_mode.mode == SlaveMode::kSlave) && !owned) {
    QMMF_WARN("%s Client(%u): Camera(%u) hasn't been opened yet,"
        " operation not allowed!", __func__, client_id, camera_id);
    return NAME_NOT_FOUND;
  } else if ((camera_slave_mode.mode == SlaveMode::kSlave) && owned) {
    QMMF_INFO("%s Client(%u): Camera(%u) is already owned by another client,"
        " using camera in slave mode!", __func__, client_id, camera_id);
    std::lock_guard<std::mutex> lock(camera_map_lock_);
    client_cameraid_map_[client_id].emplace(camera_id, false);
    return NO_ERROR;
  } else if (owned) {
    QMMF_WARN("%s Client(%u): Camera(%u) is already owned by another client,"
        " operation not allowed!", __func__, client_id, camera_id);
    return INVALID_OPERATION;
  }

  if (IsCameraValid(client_id, camera_id)) {
    QMMF_WARN("%s Client(%u): Camera(%u) has been already started,"
        " operation not allowed!", __func__, client_id, camera_id);
    return INVALID_OPERATION;
  }

  assert(camera_source_ != nullptr);
  ResultCb cb = [&] (uint32_t camera_id, const CameraMetadata &result) {
    CameraResultCb(camera_id, result);
  };

  ErrorCb errcb = [&] (RecorderErrorData &error) { CameraErrorCb(error); };

  auto ret = camera_source_->StartCamera(camera_id, frame_rate, extra_param,
                                         enable_result_cb ? cb : nullptr,
                                         errcb);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s: StartCamera Failed!!", __func__);
    return BAD_VALUE;
  }

  std::lock_guard<std::mutex> lock(camera_map_lock_);

  // Notify all clients, except this one, that the camera has been opened.
  for (auto it : client_cameraid_map_) {
    auto& client = it.first;
    remote_cb_handle_(client)->NotifyRecorderEvent(
        EventType::kCameraOpened,
        const_cast<void*>(reinterpret_cast<const void*>(&camera_id)),
        sizeof(uint32_t));
  }

  client_cameraid_map_[client_id].emplace(camera_id, true);

  FlushCb flushcb = [&](const uint32_t camera_id) { CameraFlushCb(camera_id); };
  ret = camera_source_->SetFlushCb(camera_id, flushcb);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s: SetFlushCb Failed!!", __func__);
    return BAD_VALUE;
  }

  QMMF_INFO("%s: Number of clients connected(%d)", __func__,
      client_cameraid_map_.size());

  for (auto iter : client_cameraid_map_) {
    auto cameras = iter.second;
    auto client = iter.first;

    QMMF_INFO("%s client_id(%d): number of cameras(%d) owned!",
        __func__, client, cameras.size());
    for (auto camera : cameras) {
      auto& camid = camera.first;
      QMMF_INFO("%s: client_id(%d): camera_id(%d)", __func__, client, camid);
    }
  }
  QMMF_DEBUG("%s: Exit", __func__);
  return NO_ERROR;
}

status_t RecorderImpl::StopCamera(const uint32_t client_id,
                                  const uint32_t camera_id) {

  QMMF_DEBUG("%s: Enter", __func__);
  QMMF_KPI_DETAIL();

  if (!IsClientValid(client_id)) {
    QMMF_ERROR("%s: Client(%u) is not connected!", __func__, client_id);
    return BAD_VALUE;
  }

  if (IsCameraOwned(client_id, camera_id)) {
    QMMF_WARN("%s Client(%u): Camera(%u) is not owned by this client,"
        " closing camera in slave mode!", __func__, client_id, camera_id);
    std::lock_guard<std::mutex> lock(camera_map_lock_);
    client_cameraid_map_[client_id].erase(camera_id);
    slave_camera_closed_.SignalAll();
    return NO_ERROR;
  }

  if (!IsCameraValid(client_id, camera_id)) {
    QMMF_ERROR("%s Client(%u): Camera(%u) is not owned by this client,"
        " operation not allowed!", __func__, client_id, camera_id);
    return INVALID_OPERATION;
  }
  assert(camera_source_ != nullptr);

  // Notify all clients, except this one, that the camera is about to be closed.
  for (auto it : client_cameraid_map_) {
    auto& client = it.first;
    if (client != client_id) {
      remote_cb_handle_(client)->NotifyRecorderEvent(
          EventType::kCameraClosing,
          const_cast<void*>(reinterpret_cast<const void*>(&camera_id)),
          sizeof(uint32_t));
    }
  }

  {
    std::unique_lock<std::mutex> lk(camera_map_lock_);
    std::chrono::milliseconds timeout(1000);

    // Wait until all slave camera clients have closed their connections.
    auto ret = slave_camera_closed_.WaitFor(lk, timeout, [&]() {
      for (auto const& it : client_cameraid_map_) {
        auto const& cameras = it.second;
        if ((cameras.count(camera_id) != 0) && !cameras.at(camera_id)) {
          return false;
        }
      }
      return true;
    });
    if (ret != 0) {
      QMMF_ERROR("%s: Failed, slave camera clients still active!!", __func__);
      return TIMED_OUT;
    }
  }

  auto ret = camera_source_->StopCamera(camera_id);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s: StopCamera Failed!!", __func__);
    return BAD_VALUE;
  }

  std::lock_guard<std::mutex> lock(camera_map_lock_);
  client_cameraid_map_[client_id].erase(camera_id);

  // Notify all clients, except this one, that the camera has been closed.
  for (auto it : client_cameraid_map_) {
    auto& client = it.first;
    remote_cb_handle_(client)->NotifyRecorderEvent(
        EventType::kCameraClosed,
        const_cast<void*>(reinterpret_cast<const void*>(&camera_id)),
        sizeof(uint32_t));
  }

  QMMF_INFO("%s client_id(%d): number of cameras(%d)", __func__,
      client_id, client_cameraid_map_[client_id].size());

  QMMF_DEBUG("%s: Exit", __func__);
  return NO_ERROR;
}

status_t RecorderImpl::CreateSession(const uint32_t client_id,
                                     uint32_t *session_id) {

  QMMF_DEBUG("%s: Enter", __func__);
  QMMF_KPI_DETAIL();

  if (!IsClientValid(client_id)) {
    QMMF_ERROR("%s: Client(%u) is not connected!", __func__, client_id);
    return BAD_VALUE;
  }

  if (!camera_source_) {
    QMMF_ERROR("%s: Can't Create Session! Connect Should be called before"
        " Calling CreateSession", __func__);
    return NO_INIT;
  }
  std::lock_guard<std::mutex> lock(client_session_lock_);
  ++unique_session_id_;
  *session_id = unique_session_id_;

  auto& session_track_map = client_session_map_[client_id];
  session_track_map.emplace(*session_id, TrackInfoMap());

  sessions_state_.emplace(*session_id, SessionState::kIdle);
  QMMF_INFO("%s: Client(%u): Session(%u) created successfully", __func__,
      client_id, *session_id);

  QMMF_DEBUG("%s: Exit", __func__);
  return NO_ERROR;
}

status_t RecorderImpl::DeleteSession(const uint32_t client_id,
                                     const uint32_t session_id) {

  QMMF_DEBUG("%s: Enter", __func__);
  QMMF_KPI_DETAIL();

  int32_t ret = NO_ERROR;
  if (!IsClientValid(client_id)) {
    QMMF_ERROR("%s: Client(%u) is not connected!", __func__, client_id);
    return BAD_VALUE;
  }

  if (!IsSessionValid(client_id, session_id)) {
    QMMF_ERROR("%s: Client(%u): Session(%u) is not valid!", __func__,
        client_id, session_id);
    return BAD_VALUE;
  }

  std::lock_guard<std::mutex> lock(client_session_lock_);
  auto& session_track_map = client_session_map_[client_id];
  auto& tracks = session_track_map[session_id];

  if (!tracks.empty()) {
    QMMF_ERROR("%s: Client(%d): Session(%d) Can't be deleted until all"
        " tracks(%d) within this session are stopped & deleted!", __func__,
        client_id, session_id, tracks.size());
    return INVALID_OPERATION;
  }

  session_track_map.erase(session_id);
  sessions_state_.erase(session_id);

  QMMF_INFO("%s: Number of sessions(%d) left in client_id(%d)",
      __func__, session_track_map.size(), client_id);

  uint32_t num_sessions = 0;
  for (auto const& iter : client_session_map_) {
    QMMF_INFO("%s: Client(%d): Number of sessions(%d)", __func__, iter.first,
        iter.second.size());
    num_sessions += iter.second.size();
  }

  if (num_sessions == 0) {
    QMMF_INFO("%s: Reseting unique session id to 0!", __func__);
    unique_session_id_ = 0;
  }

  QMMF_DEBUG("%s: Exit", __func__);
  return ret;
}

status_t RecorderImpl::StartSession(const uint32_t client_id,
                                    const uint32_t session_id) {

  QMMF_DEBUG("%s: Enter client_id(%d):session_id(%d)", __func__,
      client_id, session_id);
  QMMF_KPI_DETAIL();

  uint32_t ret = NO_ERROR;
  if (!IsClientValid(client_id)) {
    QMMF_ERROR("%s: Client(%u) is not connected!", __func__, client_id);
    return BAD_VALUE;
  }

  if (!IsSessionValid(client_id, session_id)) {
    QMMF_ERROR("%s: Client(%u): Session(%u) is not valid!", __func__,
        client_id, session_id);
    return BAD_VALUE;
  }

  if (IsSessionPaused(session_id)) {
    QMMF_WARN("%s: Client(%u): Session(%u) is paused, resuming!", __func__,
        client_id, session_id);
    ResumeSession(client_id, session_id);
    return NO_ERROR;
  } else if (IsSessionActive(session_id)) {
    QMMF_WARN("%s: Client(%u): Session(%u) is already started!", __func__,
        client_id, session_id);
    return NO_ERROR;
  } else if (!IsSessionIdle(session_id)) {
    QMMF_WARN("%s: Client(%u): Session(%u) hasn't been stopped!", __func__,
        client_id, session_id);
    return NO_ERROR;
  }

  client_session_lock_.lock();
  auto& session_track_map = client_session_map_[client_id];
  auto& tracks_in_session = session_track_map[session_id];
  client_session_lock_.unlock();
#ifdef PULSE_AUDIO_ENABLE
  QMMF_INFO("%s: client_id(%d):session_id(%d) number of tracks(%d) to start",
      __func__, client_id, session_id, tracks_in_session.size());
  std::future<uint32_t> audio_tracks_result;
  std::function<uint32_t()> audio_tracks = [&]() -> uint32_t {
    uint32_t ret = NO_ERROR;

    // all of the audio tracks associated to one session starts together
    for (auto const& track : tracks_in_session) {
      uint32_t client_track_id  = track.first;
      TrackInfo track_info      = track.second;
      uint32_t service_track_id = track_info.track_id;

      if (track_info.type == TrackType::kAudio) {
        QMMF_INFO("%s: Track to Start, client_id(%u):session_id(%u), "
            "client_track_id(%u):service_track_id(%x)", __func__, client_id,
            session_id, client_track_id, service_track_id);

        assert(audio_source_ != nullptr);
        ret = audio_source_->StartTrackSource(service_track_id);
        if (ret != NO_ERROR) {
            QMMF_ERROR("%s: client_id(%d):session_id(%d), audio->"
                "StartTrackSource failed for client_track_id(%d):"
                "service_track_id(%x)", __func__, client_id, session_id,
                client_track_id, service_track_id);
          break;
        }

        if (track_info.format.audio != AudioFormat::kPCM) {
          assert(audio_encoder_core_ != nullptr);
          std::shared_ptr<IAudioTrackSource> track_source;
          ret = audio_source_->getTrackSource(service_track_id, &track_source);
          if (ret != NO_ERROR || track_source == nullptr) {
            QMMF_ERROR("%s: client_id(%d):session_id(%d), audio->getTrackSource "
                "failed for client_track_id(%d):service_track_id(%x)", __func__,
                client_id, session_id, client_track_id, service_track_id);
            break;
          }
          ret = audio_encoder_core_->StartTrackEncoder(service_track_id,
                                                       track_source);
          if (ret != NO_ERROR) {
            QMMF_ERROR("%s: client_id(%d):session_id(%d), audio->"
                "StartTrackEncoder failed for client_track_id(%d):"
                "service_track_id(%x)", __func__, client_id, session_id,
                client_track_id, service_track_id);
            break;
          }
        }
      }

      QMMF_INFO("%s: client_id(%d):session_id(%d), client_track_id(%d):"
          "service_track_id(%x) Started Successfully!", __func__, client_id,
          session_id, client_track_id, service_track_id);
    }

    return ret;
  };
  audio_tracks_result = std::async(std::launch::async, audio_tracks);
#endif
  // all of the video tracks associated to one session starts together
  for (auto const& track : tracks_in_session) {
    uint32_t client_track_id  = track.first;
    TrackInfo track_info      = track.second;
    uint32_t service_track_id = track_info.track_id;

    if (track_info.type == TrackType::kVideo) {
      QMMF_INFO("%s: Track to Start, client_id(%u):session_id(%u), "
          "client_track_id(%u):service_track_id(%x)", __func__,
          client_id, session_id, client_track_id, service_track_id);

      assert(camera_source_ != nullptr);
      ret = camera_source_->StartTrackSource(service_track_id);
      if (ret != NO_ERROR) {
        QMMF_ERROR("%s: client_id(%d):session_id(%d), StartTrackSource"
            " failed for client_track_id(%d):service_track_id(%x)",
            __func__, client_id, session_id, client_track_id, service_track_id);
        break;
      }
      if ( (track_info.format.video == VideoFormat::kHEVC) ||
           (track_info.format.video == VideoFormat::kAVC) ||
           (track_info.format.video == VideoFormat::kJPEG)) {
        assert(encoder_core_ != nullptr);
        ret = encoder_core_->StartTrackEncoder(service_track_id);
        if (ret != NO_ERROR) {
          QMMF_ERROR("%s: client_id(%d):session_id(%d), StartTrackEncoder"
              " failed for client_track_id(%d):service_track_id(%x)",
              __func__, client_id, session_id, client_track_id,
              service_track_id);
          break;
        }
      }
    }

    QMMF_INFO("%s: client_id(%d):session_id(%d), "
        "client_track_id(%d):service_track_id(%x) Started Successfully!",
        __func__, client_id, session_id, client_track_id, service_track_id);
  }
#ifdef PULSE_AUDIO_ENABLE
  if (audio_tracks_result.get() == NO_ERROR && ret == NO_ERROR) {
#else
  if (ret == NO_ERROR) {
#endif
    QMMF_INFO("%s: client_id(%d):session_id(%d) with num tracks(%d) Started"
        " Successfully!", __func__, client_id, session_id,
        tracks_in_session.size());
    ChangeSessionState(session_id, SessionState::kActive);
  }

  QMMF_DEBUG("%s: Exit client_id(%d):session_id(%d)", __func__,
      client_id, session_id);
  return ret;
}

status_t RecorderImpl::StopSession(const uint32_t client_id,
                                   const uint32_t session_id, bool do_flush,
                                   bool is_force_cleanup) {

  QMMF_DEBUG("%s: Enter client_id(%d):session_id(%d)", __func__,
      client_id, session_id);
  QMMF_KPI_DETAIL();

  uint32_t ret = NO_ERROR;
  if (!IsClientValid(client_id)) {
    QMMF_ERROR("%s: Client(%u) is not connected!", __func__, client_id);
    return BAD_VALUE;
  }

  if (!IsSessionValid(client_id, session_id)) {
    QMMF_ERROR("%s: Client(%u): Session(%u) is not valid!", __func__,
        client_id, session_id);
    return BAD_VALUE;
  }

  if (IsSessionIdle(session_id)) {
    QMMF_WARN("%s: Client(%u): Session(%u) not yet started!", __func__,
        client_id, session_id);
    return NO_ERROR;
  }

  client_session_lock_.lock();
  auto session_track_map = client_session_map_[client_id];
  auto tracks_in_session = session_track_map[session_id];
  client_session_lock_.unlock();

  QMMF_INFO("%s: client_id(%d):session_id(%d), number of tracks(%d) to stop",
      __func__, client_id, session_id, tracks_in_session.size());

  // All the tracks associated to one session are stopped together.
  auto track = tracks_in_session.rbegin();
  while (track != tracks_in_session.rend()) {
    uint32_t client_track_id  = track->first;
    TrackInfo track_info      = track->second;
    uint32_t service_track_id = track_info.track_id;

    QMMF_INFO("%s: Track to Stop, client_id(%d):session_id(%d), "
        "client_track_id(%d):service_track_id(%x)", __func__, client_id,
        session_id, client_track_id, service_track_id);

    if (track_info.type == TrackType::kVideo) {
      // Stop TrackSource
      assert(camera_source_ != nullptr);
      ret = camera_source_->StopTrackSource(service_track_id, is_force_cleanup);
      if (ret != NO_ERROR) {
        QMMF_ERROR("%s: client_id(%d):session_id(%d), StopTrackSource"
            " failed for client_track_id(%d):service_track_id(%x)",
            __func__, client_id, session_id, client_track_id,
            service_track_id);
        break;
      }
      // Stop TrackEncoder
      if ((track_info.format.video == VideoFormat::kHEVC) ||
          (track_info.format.video == VideoFormat::kAVC) ||
          (track_info.format.video == VideoFormat::kJPEG)) {
        assert(encoder_core_ != nullptr);
        ret = encoder_core_->StopTrackEncoder(service_track_id,
                                              is_force_cleanup);
        if (ret != NO_ERROR) {
          QMMF_ERROR("%s: client_id(%d):session_id(%d), StopTrackEncoder"
            " failed for client_track_id(%d):service_track_id(%x)",
            __func__, client_id, session_id, client_track_id,
            service_track_id);
          break;
        }
      }
    }
#ifdef PULSE_AUDIO_ENABLE
     else if (track_info.type == TrackType::kAudio) {

      assert(audio_source_ != nullptr);
      ret = audio_source_->StopTrackSource(service_track_id);
      if (ret != NO_ERROR) {
        QMMF_ERROR("%s: client_id(%d):session_id(%d), audio->StopTrackSource"
            " failed for client_track_id(%d):service_track_id(%x)",
            __func__, client_id, session_id, client_track_id,
            service_track_id);
        break;
      }
      if (track_info.format.audio != AudioFormat::kPCM) {
        assert(audio_encoder_core_ != nullptr);
        ret = audio_encoder_core_->StopTrackEncoder(service_track_id);
        if (ret != NO_ERROR) {
          QMMF_ERROR("%s:client_id(%d):session_id(%d), StopTrackEncoder"
            " failed for client_track_id(%d):service_track_id(%x)",
            __func__, client_id, session_id, client_track_id, service_track_id);
          break;
        }
      }
    }
#endif
    QMMF_INFO("%s: client_id(%d):session_id(%d), "
        "client_track_id(%d):service_track_id(%x) Stoped Successfully!",
        __func__, client_id, session_id, client_track_id, service_track_id);
    ++track;
  } // tracks loop ends.

   if (ret == NO_ERROR) {
    QMMF_INFO("%s: client_id(%d):session_id(%d) with num tracks(%d) Stoped"
        " Successfully!", __func__, client_id, session_id,
        tracks_in_session.size());

    ChangeSessionState(session_id, SessionState::kIdle);
  }
  QMMF_DEBUG("%s: Exit client_id(%d):session_id(%d)", __func__,
      client_id, session_id);
  return ret;
}

status_t RecorderImpl::PauseSession(const uint32_t client_id,
                                    const uint32_t session_id) {

  QMMF_DEBUG("%s: Enter client_id(%d):session_id(%d)", __func__,
      client_id, session_id);
  QMMF_KPI_DETAIL();

  uint32_t ret = NO_ERROR;
  if (!IsClientValid(client_id)) {
    QMMF_ERROR("%s: Client(%u) is not connected!", __func__, client_id);
    return BAD_VALUE;
  }

  if (!IsSessionValid(client_id, session_id)) {
    QMMF_ERROR("%s: Client(%u): Session(%u) is not valid!", __func__,
        client_id, session_id);
    return BAD_VALUE;
  }

  if (IsSessionPaused(session_id)) {
    QMMF_WARN("%s: Client(%u): Session(%u) already paused!", __func__,
        client_id, session_id);
    return NO_ERROR;
  } else if (IsSessionIdle(session_id)) {
    QMMF_WARN("%s: Client(%u): Session(%u) hasn't been started!", __func__,
        client_id, session_id);
    return NO_ERROR;
  }

  client_session_lock_.lock();
  auto session_track_map = client_session_map_[client_id];
  auto tracks_in_session = session_track_map[session_id];
  client_session_lock_.unlock();

  QMMF_INFO("%s: client_id(%d):session_id(%d),number of tracks(%d) to Pause",
      __func__, client_id, session_id, tracks_in_session.size());

  // All the tracks associated to one session are paused together.
  auto track = tracks_in_session.rbegin();
  while (track != tracks_in_session.rend()) {
    uint32_t client_track_id  = track->first;
    TrackInfo track_info      = track->second;
    uint32_t service_track_id = track_info.track_id;

    QMMF_INFO("%s: Track to Pause, client_id(%d):session_id(%d), "
        "client_track_id(%d):service_track_id(%x)", __func__, client_id,
        session_id, client_track_id, service_track_id);
    if (track_info.type == TrackType::kVideo) {
      assert(camera_source_ != nullptr);
      ret = camera_source_->PauseTrackSource(service_track_id);
      if (ret != NO_ERROR) {
        QMMF_ERROR("%s: client_id(%d):session_id(%d), PauseTrackSource"
            " failed for client_track_id(%d):service_track_id(%x)",
            __func__, client_id, session_id, client_track_id,
            service_track_id);
        break;
      }
    }
#ifdef PULSE_AUDIO_ENABLE
    else if (track_info.type == TrackType::kAudio) {

      assert(audio_source_ != nullptr);
      ret = audio_source_->PauseTrackSource(service_track_id);
      if (ret != NO_ERROR) {
        QMMF_ERROR("%s:client_id(%d):session_id(%d), audio->PauseTrackSource"
            " failed for client_track_id(%d):service_track_id(%x)",
            __func__, client_id, session_id, client_track_id,
            service_track_id);
        break;
      }
      if (track_info.format.audio != AudioFormat::kPCM) {

        assert(audio_encoder_core_ != nullptr);
        ret = audio_encoder_core_->PauseTrackEncoder(service_track_id);
        if (ret != NO_ERROR) {
          QMMF_ERROR("%s: client_id(%d):session_id(%d), "
              "audio->PauseTrackEncoder failed for client_track_id(%d):"
              "service_track_id(%x)", __func__, client_id, session_id,
              client_track_id, service_track_id);
          break;
        }
      }
    }
#endif
    ++track;
  }
  if (ret == NO_ERROR) {
    QMMF_INFO("%s: client_id(%d):session_id(%d) with num tracks(%d) Paused"
        " Successfully!", __func__, client_id, session_id,
        tracks_in_session.size());

    ChangeSessionState(session_id, SessionState::kPause);
  }
  QMMF_DEBUG("%s: Exit client_id(%d):session_id(%d)", __func__,
      client_id, session_id);
  return ret;
}

status_t RecorderImpl::ResumeSession(const uint32_t client_id,
                                     const uint32_t session_id) {

  QMMF_DEBUG("%s: Enter client_id(%d):session_id(%d)", __func__,
      client_id, session_id);
  QMMF_KPI_DETAIL();

  uint32_t ret = NO_ERROR;
  if (!IsClientValid(client_id)) {
    QMMF_ERROR("%s: Client(%u) is not connected!", __func__, client_id);
    return BAD_VALUE;
  }

  if (!IsSessionValid(client_id, session_id)) {
    QMMF_ERROR("%s: Client(%u): Session(%u) is not valid!", __func__,
        client_id, session_id);
    return BAD_VALUE;
  }

  if (!IsSessionPaused(session_id)) {
    QMMF_WARN("%s: Client(%u): Session(%u) hasn't been paused!", __func__,
        client_id, session_id);
    return NO_ERROR;
  }

  client_session_lock_.lock();
  auto session_track_map = client_session_map_[client_id];
  auto tracks_in_session = session_track_map[session_id];
  client_session_lock_.unlock();

  QMMF_INFO("%s:client_id(%d):session_id(%d),number of tracks(%d) to Resume",
      __func__, client_id, session_id, tracks_in_session.size());
  // All the tracks associated to one session starts together.
  for (auto const& track : tracks_in_session) {
    uint32_t client_track_id  = track.first;
    TrackInfo track_info      = track.second;
    uint32_t service_track_id = track_info.track_id;

    QMMF_INFO("%s: Track to Resume, client_id(%d):session_id(%d), "
        "client_track_id(%d):service_track_id(%x)", __func__, client_id,
        session_id, client_track_id, service_track_id);
    if (track_info.type == TrackType::kVideo) {
      assert(camera_source_ != nullptr);
      ret = camera_source_->ResumeTrackSource(service_track_id);
      if (ret != NO_ERROR) {
        QMMF_ERROR("%s: client_id(%d):session_id(%d), ResumeTrackSource"
            " failed for client_track_id(%d):service_track_id(%x)",
            __func__, client_id, session_id, client_track_id,
            service_track_id);
        break;
      }
    }
#ifdef PULSE_AUDIO_ENABLE
    else if (track_info.type == TrackType::kAudio) {

      assert(audio_source_ != nullptr);
      ret = audio_source_->ResumeTrackSource(service_track_id);
      if (ret != NO_ERROR) {
        QMMF_ERROR("%s:client_id(%d):session_id(%d),audio->ResumeTrackSource"
            " failed for client_track_id(%d):service_track_id(%x)",
            __func__, client_id, session_id, client_track_id,
            service_track_id);
        break;
      }
      if (track_info.format.audio != AudioFormat::kPCM) {

        assert(audio_encoder_core_ != nullptr);
        ret = audio_encoder_core_->ResumeTrackEncoder(service_track_id);
        if (ret != NO_ERROR) {
          QMMF_ERROR("%s: client_id(%d):session_id(%d), "
            "audio->ResumeTrackEncoder failed for client_track_id(%d):"
            "service_track_id(%x)", __func__, client_id, session_id,
            client_track_id, service_track_id);
          break;
        }
      }
    }
#endif
  }
  if (ret == NO_ERROR) {
    QMMF_INFO("%s: client_id(%d):session_id(%d) with num tracks(%d) Resumed"
        " Successfully!", __func__, client_id, session_id,
        tracks_in_session.size());

    ChangeSessionState(session_id, SessionState::kActive);
  }
  QMMF_DEBUG("%s: Exit client_id(%d):session_id(%d)", __func__,
      client_id, session_id);
  return ret;
}

status_t RecorderImpl::GetNumberOfCameras(const uint32_t client_id,
                                          SupportedCameras &cameras) {

  QMMF_INFO("%s: Enter client_id(%d)", __func__, client_id);

  if (!IsClientValid(client_id)) {
    QMMF_WARN("%s: Invalid client_id(%d), Not in connected client list!",
        __func__, client_id);
    return BAD_VALUE;
  }
  assert(camera_source_ != nullptr);
  auto ret = camera_source_->GetNumberOfCameras(cameras);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s: client_id(%d): GetNumberOfCameras failed!",
        __func__, client_id);
    return ret;
  }

  QMMF_INFO("%s: Exit client_id(%d)", __func__, client_id);
  return NO_ERROR;
}

bool RecorderImpl::IsAudioTrackCreateParamValid(const AudioTrackCreateParam& param) {
  bool valid = true;
  // Validate MPEGH encoding input information and shall always be same as the
  // conditions defined in ConfigureAudioEncoder() of
  // <common/codecadaptor/src/qmmf_avcodec.cc>
  if (param.format == AudioFormat::kMPEGH) {
    //Number of channels should be 4
    //Sample rate should be 48000
    //bit rate could be 300K, 384K, or 512K (K == 1024)
    if ((param.channels == 4) &&
        (param.sample_rate == 48000) &&
        ((param.codec_params.mpegh.bit_rate == 307200) ||
         (param.codec_params.mpegh.bit_rate == 393216) ||
         (param.codec_params.mpegh.bit_rate == 524288))) {
      QMMF_INFO("%s: Successfully validated MPEGH track params", __func__);
    } else {
      valid = false;
    }
  }
  return valid;
}

status_t RecorderImpl::CreateAudioTrack(const uint32_t client_id,
                                        const uint32_t session_id,
                                        const uint32_t track_id,
                                        const AudioTrackCreateParam& param) {
  QMMF_DEBUG("%s: Enter client_id(%d):session_id(%d)", __func__,
      client_id, session_id);
  QMMF_KPI_DETAIL();

  uint32_t ret = NO_ERROR;
  if (!IsClientValid(client_id)) {
    QMMF_ERROR("%s: Client(%u) is not connected!", __func__, client_id);
    return BAD_VALUE;
  }

  if (!IsSessionValid(client_id, session_id)) {
    QMMF_ERROR("%s: Client(%u): Session(%u) is not valid!", __func__,
        client_id, session_id);
    return BAD_VALUE;
  }

  if (IsTrackValid(client_id, session_id, track_id)) {
    QMMF_ERROR("%s: Client(%d):Session(%d): Track(%d) already exists!",
        __func__, client_id, session_id, track_id);
    return BAD_VALUE;
  }

  if (!IsAudioTrackCreateParamValid(param)) {
    QMMF_ERROR("%s: Track create params are not valid!", __func__);
    return BAD_VALUE;
  }

  uint32_t service_track_id = GetUniqueServiceTrackId(client_id, session_id,
                                                      track_id);
  QMMF_INFO("%s: client_id(%d):session_id(%d) client_track_id(%d):"
      "service_track_id(%x)", __func__, client_id, session_id, track_id,
      service_track_id);

  AudioTrackParams audio_params{};
  audio_params.track_id = service_track_id;
  audio_params.params   = param;
  audio_params.data_cb  = [this, client_id, session_id, track_id]
      (std::vector<BnBuffer>& buffers, std::vector<MetaData>& meta_buffers) {
          AudioTrackBufferCb(client_id, session_id, track_id,
                             buffers, meta_buffers);
      };
#ifdef PULSE_AUDIO_ENABLE
  assert(audio_source_ != nullptr);
  ret = audio_source_->CreateTrackSource(service_track_id, audio_params);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s: CreateTrackSource track_id(%d):service_track_id(%x) "
        " failed!", __func__, track_id, service_track_id);
    return BAD_VALUE;
  }
  QMMF_INFO("%s: client_id(%d):session_id(%d), TrackSource for"
      "client_track_id(%d):service_track_id(%x) Added Successfully in "
      "CameraSource", __func__, client_id, session_id, track_id,
      service_track_id);

  if (param.format != AudioFormat::kPCM) {
    assert(audio_encoder_core_ != NULL);

    ret = audio_encoder_core_->AddSource(audio_params);
    if (ret != NO_ERROR) {
      QMMF_ERROR("%s: track_id(%d):service_track_id(%x) AddSource"
        " failed!", __func__, track_id, service_track_id);
      return BAD_VALUE;
    }
  }
#endif
  // Assosiate track to session.
  TrackInfo track_info{};
  track_info.track_id     = service_track_id;
  track_info.type         = TrackType::kAudio;
  track_info.format.audio = audio_params.params.format;

  std::lock_guard<std::mutex> lock(client_session_lock_);
  auto& session_track_map = client_session_map_[client_id];
  auto& tracks_in_session = session_track_map[session_id];
  tracks_in_session.emplace(track_id, track_info);

  QMMF_DEBUG("%s: Exit client_id(%d):session_id(%d)", __func__,
      client_id, session_id);
  return NO_ERROR;
}

status_t RecorderImpl::DeleteAudioTrack(const uint32_t client_id,
                                        const uint32_t session_id,
                                        const uint32_t track_id) {
  QMMF_DEBUG("%s: Enter client_id(%d):session_id(%d)", __func__,
      client_id, session_id);
  QMMF_KPI_DETAIL();

  uint32_t ret = NO_ERROR;
  if (!IsClientValid(client_id)) {
    QMMF_ERROR("%s: Client(%u) is not connected!", __func__, client_id);
    return BAD_VALUE;
  }

  if (!IsSessionValid(client_id, session_id)) {
    QMMF_ERROR("%s: Client(%u): Session(%u) is not valid!", __func__,
        client_id, session_id);
    return BAD_VALUE;
  }

  if (!IsTrackValid(client_id, session_id, track_id)) {
    QMMF_ERROR("%s: Client(%d):Session(%d): Track(%d) does not exist!",
        __func__, client_id, session_id, track_id);
    return BAD_VALUE;
  }

  client_session_lock_.lock();
  auto& session_track_map = client_session_map_[client_id];
  auto& tracks_in_session = session_track_map[session_id];

  auto& track_info = tracks_in_session[track_id];
  uint32_t& service_track_id = track_info.track_id;
  client_session_lock_.unlock();

  QMMF_INFO("%s: client_id(%d):session_id(%d), "
      "track_id(%d):service_track_id(%x)", __func__, client_id,
      session_id, track_id, service_track_id);
#ifdef PULSE_AUDIO_ENABLE
  assert(track_info.type == TrackType::kAudio);
  assert(audio_source_ != nullptr);
  assert(service_track_id > 0);
  ret = audio_source_->DeleteTrackSource(service_track_id);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s: service_track_id(%x) DeleteTrackSource failed!",
        __func__, service_track_id);
    return ret;
  }
  if (track_info.format.audio != AudioFormat::kPCM) {
    assert(audio_encoder_core_ != NULL);

    ret = audio_encoder_core_->DeleteTrackEncoder(service_track_id);
    if (ret != NO_ERROR) {
      QMMF_ERROR("%s: DeleteTrackEncoder failed for "
          "client_track_id(%d):service_track_id(%x)", __func__,
           track_id, service_track_id);
      return ret;
    }
  }
#endif
  std::lock_guard<std::mutex> lock(client_session_lock_);
  tracks_in_session.erase(track_id);

  QMMF_INFO("%s: client_track_id(%d):service_track_id(%x) Deleted "
      "Successfully", __func__, track_id, service_track_id);
  QMMF_INFO("%s: Number of tracks(%d) left in session(%d)", __func__,
      tracks_in_session.size(), session_id);

  QMMF_DEBUG("%s: Enter client_id(%d):session_id(%d)", __func__,
      client_id, session_id);
  return NO_ERROR;
}

status_t RecorderImpl::CreateVideoTrack(const uint32_t client_id,
                                        const uint32_t session_id,
                                        const uint32_t track_id,
                                        const VideoTrackCreateParam& params) {

  QMMF_DEBUG("%s: Enter client_id(%d):session_id(%d)", __func__,
      client_id, session_id);
  QMMF_KPI_DETAIL();

  if (!IsClientValid(client_id)) {
    QMMF_ERROR("%s: Client(%u) is not connected!", __func__, client_id);
    return BAD_VALUE;
  }

  if (!IsSessionValid(client_id, session_id)) {
    QMMF_ERROR("%s: Client(%u): Session(%u) is not valid!", __func__,
        client_id, session_id);
    return BAD_VALUE;
  }

  if (IsTrackValid(client_id, session_id, track_id)) {
    QMMF_ERROR("%s: Client(%d):Session(%d): Track(%d) already exists!",
        __func__, client_id, session_id, track_id);
    return BAD_VALUE;
  }

  uint32_t service_track_id = GetUniqueServiceTrackId(client_id, session_id,
                                                      track_id);
  QMMF_INFO("%s: client_id(%d):session_id(%d) client_track_id(%d):"
      "service_track_id(%x)", __func__, client_id, session_id, track_id,
      service_track_id);
  VideoExtraParam empty_extra_params;

  VideoTrackParams video_params{};
  video_params.track_id    = service_track_id;
  video_params.params      = params;
  video_params.extra_param = empty_extra_params;
  video_params.data_cb     = [this, client_id, session_id, track_id]
      (std::vector<BnBuffer>& buffers, std::vector<MetaData>& meta_buffers) {
          VideoTrackBufferCb(client_id, session_id, track_id,
                             buffers, meta_buffers);
      };
  // Create Camera track first.
  assert(camera_source_ != nullptr);
  auto ret = camera_source_->CreateTrackSource(service_track_id, video_params);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s: CreateTrackSource track_id(%d):service_track_id(%x) "
        " failed!", __func__, track_id, service_track_id);
    return BAD_VALUE;
  }
  QMMF_INFO("%s: client_id(%d):session_id(%d), TrackSource for "
      "client_track_id(%d):service_track_id(%x) Added Successfully in "
      "CameraSource!", __func__, client_id, session_id, track_id,
      service_track_id);

  // If video codec type is set to YUV then no need to create Encoder instance.
  // direct YUV frame will go to client.
  if ((params.format_type == VideoFormat::kHEVC) ||
      (params.format_type == VideoFormat::kAVC) ||
      (params.format_type == VideoFormat::kJPEG)) {
    // Create Encoder track and add TrackSource as a source to iit.
    // Track pipeline: TrackSource <--> TrackEncoder
    assert(encoder_core_ != nullptr);
    ret = encoder_core_->AddSource(camera_source_->
        GetTrackSource(service_track_id), video_params);
    if (ret != NO_ERROR) {
      QMMF_ERROR("%s: track_id(%d):service_track_id(%x) AddSource"
        " failed!", __func__, track_id, service_track_id);
      return BAD_VALUE;
    }
  }

  {
    std::lock_guard<std::mutex> lock(camera_tracks_lock_);
    camera_tracks_map_[params.camera_id].emplace(service_track_id);
  }

  // Assosiate track to session.
  TrackInfo track_info{};
  track_info.track_id     = service_track_id;
  track_info.type         = TrackType::kVideo;
  track_info.format.video = video_params.params.format_type;

  std::lock_guard<std::mutex> lock(client_session_lock_);
  auto& session_track_map = client_session_map_[client_id];
  auto& tracks_in_session = session_track_map[session_id];
  tracks_in_session.emplace(track_id, track_info);

  QMMF_INFO("%s: client_id(%d), session_id(%d), num sessions=%d",
      __func__, client_id, session_id, session_track_map.size());
  QMMF_INFO("%s: num of tracks=%d", __func__, tracks_in_session.size());

  QMMF_INFO("%s: client_track_id(%d):service_track_id(%x):track_type(%d)",
      __func__, track_id, service_track_id, (int32_t) track_info.type);

  QMMF_DEBUG("%s: Exit client_id(%d):session_id(%d)", __func__,
      client_id, session_id);
  return NO_ERROR;
}

uint32_t RecorderImpl::FindSuitableIdForLinkedTrack(
    const VideoTrackCreateParam& params) {
  bool is_suitable_track_found = false;
  uint32_t selected_track_id = -1;
  auto client_ids = GetCameraClients(params.camera_id);
  for (auto const& id : client_ids) {
    if (IsCameraValid(id, params.camera_id)) {
      auto& main_session_track_map = client_session_map_[id];
      for (auto const& track_map : main_session_track_map) {
        // Try to find a track with same resolution
        for (auto const& track : track_map.second) {
          TrackInfo tr = track.second;
          if (tr.format.video != params.format_type) {
            continue;
          }
          std::shared_ptr<TrackSource> track_source =
              camera_source_->GetTrackSource(tr.track_id);
          VideoTrackParams tr_params = track_source->getParams();

          if (params.width == tr_params.params.width &&
              params.height == tr_params.params.height) {
            selected_track_id = tr.track_id;
            is_suitable_track_found = true;
            break;
          }
        }
        // Try to find a track with bigger resolution
        if(!is_suitable_track_found) {
          uint32_t selected_width = 0;
          uint32_t selected_height = 0;
          for (auto const& track : track_map.second) {
            TrackInfo tr = track.second;
            if (tr.format.video != params.format_type) {
              continue;
            }
            std::shared_ptr<TrackSource> track_source =
                camera_source_->GetTrackSource(tr.track_id);
            VideoTrackParams tr_params = track_source->getParams();

            if (params.width <= tr_params.params.width &&
                params.height <= tr_params.params.height) {
              // Select the lowest possible resolution from the all running
              // tracks has resolution bigger than requested track.
              if ((selected_width == 0 ||
                  tr_params.params.width < selected_width) ||
                  (selected_height == 0 ||
                  tr_params.params.height < selected_height)) {
                selected_track_id = tr.track_id;
                is_suitable_track_found = true;
                selected_width = tr_params.params.width;
                selected_height = tr_params.params.height;
              }
            }
          }
        }
      }
      break;
    }
  }
  return selected_track_id;
}

status_t RecorderImpl::CreateVideoTrack(const uint32_t client_id,
                                        const uint32_t session_id,
                                        const uint32_t track_id,
                                        const VideoTrackCreateParam& params,
                                        const VideoExtraParam& extra_param) {

  QMMF_DEBUG("%s: Enter client_id(%d):session_id(%d)", __func__,
      client_id, session_id);

  if (!IsClientValid(client_id)) {
    QMMF_ERROR("%s: Client(%u) is not connected!", __func__, client_id);
    return BAD_VALUE;
  }

  if (!IsSessionValid(client_id, session_id)) {
    QMMF_ERROR("%s: Client(%u): Session(%u) is not valid!", __func__,
        client_id, session_id);
    return BAD_VALUE;
  }

  if (IsTrackValid(client_id, session_id, track_id)) {
    QMMF_ERROR("%s: Client(%d):Session(%d): Track(%d) already exists!",
        __func__, client_id, session_id, track_id);
    return BAD_VALUE;
  }

  uint32_t service_track_id = GetUniqueServiceTrackId(client_id, session_id,
                                                      track_id);
  QMMF_INFO("%s: client_id(%d):session_id(%d) client_track_id(%d):"
      "service_track_id(%x)", __func__, client_id, session_id, track_id,
      service_track_id);

  VideoTrackParams video_params{};
  video_params.track_id    = service_track_id;
  video_params.params      = params;
  video_params.extra_param = extra_param;
  video_params.data_cb     = [this, client_id, session_id, track_id]
      (std::vector<BnBuffer>& buffers, std::vector<MetaData>& meta_buffers) {
          VideoTrackBufferCb(client_id, session_id, track_id,
                             buffers, meta_buffers);
      };

  if (video_params.extra_param.Exists(QMMF_SOURCE_VIDEO_TRACK_ID)) {
    SourceVideoTrack source_track;
    video_params.extra_param.Fetch(QMMF_SOURCE_VIDEO_TRACK_ID, source_track);

    auto source_track_id =
        GetServiceTrackId(client_id, source_track.source_track_id);
    // Overwrite client source track id with service source track id.
    source_track.source_track_id = source_track_id;

    video_params.extra_param.Update(QMMF_SOURCE_VIDEO_TRACK_ID, source_track);
  } else if (video_params.extra_param.Exists(
      QMMF_USE_LINKED_TRACK_IN_SLAVE_MODE)) {
    LinkedTrackInSlaveMode linked_track_slave_mode;
    video_params.extra_param.Fetch(QMMF_USE_LINKED_TRACK_IN_SLAVE_MODE,
        linked_track_slave_mode);
    if (linked_track_slave_mode.enable) {
      uint32_t selected_track_id = FindSuitableIdForLinkedTrack(params);
      if (selected_track_id != -1) {
        SourceVideoTrack source_track;
        source_track.source_track_id = selected_track_id;
        video_params.extra_param.Update(QMMF_SOURCE_VIDEO_TRACK_ID,
            source_track);
      } else {
        QMMF_ERROR("%s: No suitable track found for linked stream!", __func__);
        return BAD_VALUE;
      }
    }
  }

  // Create Camera track first.
  assert(camera_source_ != nullptr);
  auto ret = camera_source_->CreateTrackSource(service_track_id, video_params);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s: CreateTrackSource track_id(%d):service_track_id(%x) "
        " failed!", __func__, track_id, service_track_id);
    return BAD_VALUE;
  }
  QMMF_INFO("%s: client_id(%d):session_id(%d), TrackSource for "
      "client_track_id(%d):service_track_id(%x) Added Successfully in "
      "CameraSource!", __func__, client_id, session_id, track_id,
      service_track_id);

  // If video codec type is set to YUV then no need to create Encoder instance.
  // direct YUV frame will go to client.
  if ((params.format_type == VideoFormat::kHEVC) ||
      (params.format_type == VideoFormat::kAVC) ||
      (params.format_type == VideoFormat::kJPEG)) {
    // Create Encoder track and add TrackSource as a source to iit.
    // Track pipeline: TrackSource <--> TrackEncoder
    assert(encoder_core_ != nullptr);
    ret = encoder_core_->AddSource(camera_source_->
        GetTrackSource(service_track_id), video_params);
    if (ret != NO_ERROR) {
      QMMF_ERROR("%s: track_id(%d):service_track_id(%x) AddSource"
        " failed!", __func__, track_id, service_track_id);
      return BAD_VALUE;
    }
    if (extra_param.Exists(QMMF_VIDEO_TIMELAPSE_INTERVAL)) {
      timelapse_mode_.emplace(track_id, true);

      VideoTimeLapse timelapse;
      extra_param.Fetch(QMMF_VIDEO_TIMELAPSE_INTERVAL, timelapse);

      float fps = 1000.0 / timelapse.time_interval;
      CodecParamType type = CodecParamType::kFrameRateType;

      ret = encoder_core_->SetTrackEncoderParams(service_track_id, type,
                                                 &fps, sizeof(fps));
      if (ret != NO_ERROR) {
        QMMF_ERROR("%s: track_id(%d):service_track_id(%x) Set encoder frame "
          "rate failed!",  __func__, track_id, service_track_id);
        return ret;
      }
    }
  }

  {
    std::lock_guard<std::mutex> lock(camera_tracks_lock_);
    camera_tracks_map_[params.camera_id].emplace(service_track_id);
  }

  // Assosiate track to session.
  TrackInfo track_info{};
  track_info.track_id     = service_track_id;
  track_info.type         = TrackType::kVideo;
  track_info.format.video = video_params.params.format_type;

  std::lock_guard<std::mutex> lock(client_session_lock_);
  auto& session_track_map = client_session_map_[client_id];
  auto& tracks_in_session = session_track_map[session_id];
  tracks_in_session.emplace(track_id, track_info);

  QMMF_INFO("%s: client_id(%d), session_id(%d), num sessions=%d",
      __func__, client_id, session_id, session_track_map.size());
  QMMF_INFO("%s: num of tracks=%d", __func__, tracks_in_session.size());

  QMMF_INFO("%s: client_track_id(%d):service_track_id(%x):track_type(%d)",
      __func__, track_id, service_track_id, (int32_t) track_info.type);

  QMMF_DEBUG("%s: Exit client_id(%d):session_id(%d)", __func__,
      client_id, session_id);
  return NO_ERROR;
}

status_t RecorderImpl::DeleteVideoTrack(const uint32_t client_id,
                                        const uint32_t session_id,
                                        const uint32_t track_id) {

  QMMF_DEBUG("%s: Enter client_id(%d):session_id(%d)", __func__,
      client_id, session_id);
  QMMF_KPI_DETAIL();

  if (!IsClientValid(client_id)) {
    QMMF_ERROR("%s: Client(%u) is not connected!", __func__, client_id);
    return BAD_VALUE;
  }

  if (!IsSessionValid(client_id, session_id)) {
    QMMF_ERROR("%s: Client(%u): Session(%u) is not valid!", __func__,
        client_id, session_id);
    return BAD_VALUE;
  }

  if (!IsTrackValid(client_id, session_id, track_id)) {
    QMMF_ERROR("%s: Client(%d):Session(%d): Track(%d) does not exist!",
        __func__, client_id, session_id, track_id);
    return BAD_VALUE;
  }

  client_session_lock_.lock();
  auto& session_track_map = client_session_map_[client_id];
  auto& tracks_in_session = session_track_map[session_id];

  auto& track_info = tracks_in_session[track_id];
  uint32_t service_track_id = track_info.track_id;
  client_session_lock_.unlock();

  assert(track_info.type == TrackType::kVideo);
  assert(camera_source_ != nullptr);
  assert(service_track_id > 0);
  auto ret = camera_source_->DeleteTrackSource(service_track_id);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s: service_track_id(%x) DeleteTrackSource failed!",
        __func__, service_track_id);
    return ret;
  }
  if ((track_info.format.video == VideoFormat::kHEVC) ||
      (track_info.format.video == VideoFormat::kAVC) ||
      (track_info.format.video == VideoFormat::kJPEG)) {
    assert(encoder_core_ != nullptr);
    ret = encoder_core_->DeleteTrackEncoder(service_track_id);
    if (ret != NO_ERROR) {
      QMMF_ERROR("%s: DeleteTrackEncoder failed for client_track_id(%d):"
          "service_track_id(%x)", __func__, track_id, service_track_id);
      return ret;
    }
  }

  {
    std::lock_guard<std::mutex> lock(camera_tracks_lock_);
    // Find the camera-to-tracks mapping that this track id belongs to.
    auto it = std::find_if(
        camera_tracks_map_.begin(), camera_tracks_map_.end(),
        [service_track_id](const std::pair<uint32_t, std::set<uint32_t>>& p) {
          return p.second.count(service_track_id) != 0;
        }
    );
    // Erase the track id for the found camera-to-tracks map entry.
    if (it != camera_tracks_map_.end()) {
      auto& tracks = it->second;
      tracks.erase(track_id);
    }
  }
  {
    std::lock_guard<std::mutex> lock(client_session_lock_);
    tracks_in_session.erase(track_id);
  }

  QMMF_INFO("%s: client_track_id(%d):service_track_id(%x) Deleted "
      "Successfully", __func__, track_id, service_track_id);
  QMMF_INFO("%s: Number of tracks(%d) left in session(%d)", __func__,
      tracks_in_session.size(), session_id);

  // This method doesn't go up to client as a callback, it is just to update
  // Internal data structure used for buffer mapping.
  remote_cb_handle_(client_id)->NotifyDeleteVideoTrack(track_id);
  timelapse_mode_.erase(track_id);

  QMMF_DEBUG("%s: Exit client_id(%d):session_id(%d)", __func__,
      client_id, session_id);
  return NO_ERROR;
}

status_t RecorderImpl::ReturnTrackBuffer(const uint32_t client_id,
                                         const uint32_t session_id,
                                         const uint32_t track_id,
                                         std::vector<BnBuffer> &buffers) {

  QMMF_VERBOSE("%s: Enter client_id(%d):session_id(%d)", __func__,
      client_id, session_id);
  for (const BnBuffer& buffer : buffers)
    QMMF_VERBOSE("%s INPARAM: buffers[%s]", __func__,
                 buffer.ToString().c_str());

  uint32_t ret = NO_ERROR;
  if (!IsClientValid(client_id)) {
    QMMF_ERROR("%s: Client(%u) is not connected!", __func__, client_id);
    return BAD_VALUE;
  }

  if (!IsSessionValid(client_id, session_id)) {
    QMMF_ERROR("%s: Client(%u): Session(%u) is not valid!", __func__,
        client_id, session_id);
    return BAD_VALUE;
  }

  if (!IsTrackValid(client_id, session_id, track_id)) {
    QMMF_ERROR("%s: Client(%d):Session(%d): Track(%d) does not exist!",
        __func__, client_id, session_id, track_id);
    return BAD_VALUE;
  }

  auto track_info = GetServiceTrackInfo(client_id, session_id, track_id);

  if (track_info.type == TrackType::kVideo) {

    if ((track_info.format.video == VideoFormat::kAVC) ||
        (track_info.format.video == VideoFormat::kHEVC) ||
        (track_info.format.video == VideoFormat::kJPEG)) {
      assert(encoder_core_ != nullptr);
      ret = encoder_core_->ReturnTrackBuffer(track_info.track_id, buffers);
      assert(ret == NO_ERROR);
    } else {
      // These are the RAW frames, return them back to camera source directly.
      assert(camera_source_ != nullptr);
      ret = camera_source_->ReturnTrackBuffer(track_info.track_id, buffers);
      assert(ret == NO_ERROR);
    }

  }
#ifdef PULSE_AUDIO_ENABLE
  else {
    if (track_info.format.audio != AudioFormat::kPCM) {
      assert(audio_encoder_core_ != nullptr);
      ret = audio_encoder_core_->ReturnTrackBuffer(track_info.track_id,
                                                   buffers);
      assert(ret == NO_ERROR);
    } else {
      assert(audio_source_ != nullptr);
      ret = audio_source_->ReturnTrackBuffer(track_info.track_id, buffers);
      assert(ret == NO_ERROR);
    }
  }
#endif
  QMMF_VERBOSE("%s: Exit client_id(%d):session_id(%d)", __func__,
      client_id, session_id);
  return NO_ERROR;
}

status_t RecorderImpl::SetAudioTrackParam(const uint32_t client_id,
                                          const uint32_t session_id,
                                          const uint32_t track_id,
                                          CodecParamType type,
                                          void *param,
                                          size_t param_size) {
  QMMF_DEBUG("%s: Enter client_id(%d):session_id(%d)", __func__,
      client_id, session_id);

  if (!IsClientValid(client_id)) {
    QMMF_ERROR("%s: Client(%u) is not connected!", __func__, client_id);
    return BAD_VALUE;
  }

  if (!IsSessionValid(client_id, session_id)) {
    QMMF_ERROR("%s: Client(%u): Session(%u) is not valid!", __func__,
        client_id, session_id);
    return BAD_VALUE;
  }

  if (!IsTrackValid(client_id, session_id, track_id)) {
    QMMF_ERROR("%s: Client(%d):Session(%d): Track(%d) does not exist!",
        __func__, client_id, session_id, track_id);
    return BAD_VALUE;
  }

  QMMF_DEBUG("%s: Exit client_id(%d):session_id(%d)", __func__,
      client_id, session_id);
  return NO_ERROR;
}

status_t RecorderImpl::SetVideoTrackParam(const uint32_t client_id,
                                          const uint32_t session_id,
                                          const uint32_t track_id,
                                          CodecParamType type,
                                          void *param,
                                          size_t param_size) {
  QMMF_DEBUG("%s: Enter client_id(%d):session_id(%d)", __func__,
      client_id, session_id);

  if (!IsClientValid(client_id)) {
    QMMF_ERROR("%s: Client(%u) is not connected!", __func__, client_id);
    return BAD_VALUE;
  }

  if (!IsSessionValid(client_id, session_id)) {
    QMMF_ERROR("%s: Client(%u): Session(%u) is not valid!", __func__,
        client_id, session_id);
    return BAD_VALUE;
  }

  if (!IsTrackValid(client_id, session_id, track_id)) {
    QMMF_ERROR("%s: Client(%d):Session(%d): Track(%d) does not exist!",
        __func__, client_id, session_id, track_id);
    return BAD_VALUE;
  }

  auto track_info = GetServiceTrackInfo(client_id, session_id, track_id);

  status_t ret = 0;
  if (!(timelapse_mode_[track_id] && type == CodecParamType::kFrameRateType)) {
    // Do not override encoder track frame rate in timelapse mode.
    assert(encoder_core_ != nullptr);
    ret = encoder_core_->SetTrackEncoderParams(track_info.track_id, type,
                                               param, param_size);
    if (ret != NO_ERROR) {
      QMMF_ERROR("%s: client_id(%d) Failed to set video encode params!",
          __func__, client_id);
      return ret;
    }
  }
  if (type == CodecParamType::kFrameRateType) {
    float fps = *(static_cast<float*>(param));
    ret = camera_source_->UpdateTrackFrameRate(track_info.track_id, fps);
    if (ret != NO_ERROR) {
      QMMF_ERROR("%s: client_id(%d) Failed to set FrameRate to TrackSource",
          __func__, client_id);
      return ret;
    }
  }

  if (type == CodecParamType::kEnableFrameRepeat) {
    bool enable = *(static_cast<bool*>(param));
    ret = camera_source_->EnableFrameRepeat(track_info.track_id, enable);
    if (ret != NO_ERROR) {
      QMMF_ERROR("%s: client_id(%d) Failed to set FrameRepeat to TrackSource!",
          __func__, client_id);
      return ret;
    }
  }
  QMMF_DEBUG("%s: Exit client_id(%d):session_id(%d)", __func__,
      client_id, session_id);
  return NO_ERROR;
}

status_t RecorderImpl::CaptureImage(const uint32_t client_id,
                                    const uint32_t camera_id,
                                    const ImageParam &param,
                                    const uint32_t num_images,
                                    const std::vector<CameraMetadata> &meta) {

  QMMF_DEBUG("%s: Enter client_id(%d):camera_id(%d)", __func__,
      client_id, camera_id);

  if (!IsClientValid(client_id)) {
    QMMF_ERROR("%s: Client(%u) is not connected!", __func__, client_id);
    return BAD_VALUE;
  }

  if (!IsCameraValid(client_id, camera_id)) {
    QMMF_ERROR("%s Client(%u): Camera(%u) is not owned by this client,"
        " operation not allowed!", __func__, client_id, camera_id);
    return INVALID_OPERATION;
  }

  assert(camera_source_ != nullptr);
  SnapshotCb cb = [ this, client_id ] (uint32_t camera_id,
      uint32_t count, BnBuffer& buf, MetaData& meta_data) {
          CameraSnapshotCb(client_id, camera_id, count, buf, meta_data);
      };

  auto ret = camera_source_->CaptureImage(camera_id, param, num_images,
                                          meta, cb);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s: client_id(%d):camera_id(%d) CaptureImage failed!",
        __func__, client_id, camera_id);
    return ret;
  }
  QMMF_DEBUG("%s: Exit client_id(%d):camera_id(%d)", __func__,
      client_id, camera_id);;
  return NO_ERROR;
}

status_t RecorderImpl::ConfigImageCapture(const uint32_t client_id,
                                          const uint32_t camera_id,
                                          const ImageConfigParam &config) {

  QMMF_DEBUG("%s: Enter client_id(%d):camera_id(%d)", __func__,
      client_id, camera_id);

  if (!IsClientValid(client_id)) {
    QMMF_ERROR("%s: Client(%u) is not connected!", __func__, client_id);
    return BAD_VALUE;
  }

  if (!IsCameraValid(client_id, camera_id)) {
    QMMF_ERROR("%s Client(%u): Camera(%u) is not owned by this client,"
        " operation not allowed!", __func__, client_id, camera_id);
    return INVALID_OPERATION;
  }

  assert(camera_source_ != nullptr);
  auto ret = camera_source_->ConfigImageCapture(camera_id, config);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s: client_id(%d):camera_id(%d) ConfigImageCapture failed!",
        __func__, client_id, camera_id);
    return ret;
  }
  QMMF_DEBUG("%s: Exit client_id(%d):camera_id(%d)", __func__,
      client_id, camera_id);;
  return NO_ERROR;
}


status_t RecorderImpl::CancelCaptureImage(const uint32_t client_id,
                                          const uint32_t camera_id) {

  QMMF_DEBUG("%s: Enter client_id(%d):camera_id(%d)", __func__,
      client_id, camera_id);

  if (!IsClientValid(client_id)) {
    QMMF_ERROR("%s: Client(%u) is not connected!", __func__, client_id);
    return BAD_VALUE;
  }

  if (!IsCameraValid(client_id, camera_id)) {
    QMMF_ERROR("%s Client(%u): Camera(%u) is not owned by this client,"
        " operation not allowed!", __func__, client_id, camera_id);
    return INVALID_OPERATION;
  }

  assert(camera_source_ != nullptr);
  auto ret = camera_source_->CancelCaptureImage(camera_id);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s: CancelCaptureImage failed!", __func__);
    return ret;
  }
  QMMF_DEBUG("%s: Exit client_id(%d):camera_id(%d)", __func__,
      client_id, camera_id);
  return NO_ERROR;
}

status_t RecorderImpl::ReturnImageCaptureBuffer(const uint32_t client_id,
                                                const uint32_t camera_id,
                                                const int32_t buffer_id) {

  QMMF_DEBUG("%s: Enter client_id(%d):camera_id(%d)", __func__,
      client_id, camera_id);
  assert(camera_source_ != nullptr);
  auto ret = camera_source_->ReturnImageCaptureBuffer(camera_id, buffer_id);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s: ReturnImageCaptureBuffer failed!", __func__);
    return ret;
  }
  QMMF_DEBUG("%s: Exit client_id(%d):camera_id(%d)", __func__,
      client_id, camera_id);
  return NO_ERROR;
}


status_t RecorderImpl::SetCameraParam(const uint32_t client_id,
                                      const uint32_t camera_id,
                                      const CameraMetadata &meta) {

  QMMF_DEBUG("%s: Enter client_id(%d):camera_id(%d)", __func__,
      client_id, camera_id);

  if (!IsClientValid(client_id)) {
    QMMF_ERROR("%s: Client(%u) is not connected!", __func__, client_id);
    return BAD_VALUE;
  }

  if (!IsCameraValid(client_id, camera_id)) {
    QMMF_ERROR("%s Client(%u): Camera(%u) is not owned by this client,"
        " operation not allowed!", __func__, client_id, camera_id);
    return INVALID_OPERATION;
  }

  assert(camera_source_ != nullptr);
  auto ret = camera_source_->SetCameraParam(camera_id, meta);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s: SetCameraParam failed!", __func__);
    return ret;
  }
  QMMF_DEBUG("%s: Enter client_id(%d):camera_id(%d)", __func__,
      client_id, camera_id);
  return NO_ERROR;
}

status_t RecorderImpl::GetCameraParam(const uint32_t client_id,
                                      const uint32_t camera_id,
                                      CameraMetadata &meta) {

  QMMF_DEBUG("%s: Enter client_id(%d):camera_id(%d)", __func__,
      client_id, camera_id);

  if (!IsClientValid(client_id)) {
    QMMF_ERROR("%s: Client(%u) is not connected!", __func__, client_id);
    return BAD_VALUE;
  }

  if (!IsCameraValid(client_id, camera_id)) {
    QMMF_ERROR("%s Client(%u): Camera(%u) is not owned by this client,"
        " operation not allowed!", __func__, client_id, camera_id);
    return INVALID_OPERATION;
  }

  assert(camera_source_ != nullptr);
  auto ret = camera_source_->GetCameraParam(camera_id, meta);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s: GetCameraParam failed!", __func__);
    return ret;
  }
  QMMF_DEBUG("%s: Exit client_id(%d):camera_id(%d)", __func__,
      client_id, camera_id);
  return NO_ERROR;
}

status_t RecorderImpl::GetDefaultCaptureParam(const uint32_t client_id,
                                              const uint32_t camera_id,
                                              CameraMetadata &meta) {

  QMMF_DEBUG("%s: Enter client_id(%d):camera_id(%d)", __func__,
      client_id, camera_id);

  if (!IsClientValid(client_id)) {
    QMMF_ERROR("%s: Client(%u) is not connected!", __func__, client_id);
    return BAD_VALUE;
  }

  if (!IsCameraValid(client_id, camera_id)) {
    QMMF_ERROR("%s Client(%u): Camera(%u) is not owned by this client,"
        " operation not allowed!", __func__, client_id, camera_id);
    return INVALID_OPERATION;
  }

  assert(camera_source_ != nullptr);
  auto ret = camera_source_->GetDefaultCaptureParam(camera_id, meta);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s: GetDefaultCaptureParam failed!", __func__);
    return ret;
  }
  QMMF_DEBUG("%s: Exit client_id(%d):camera_id(%d)", __func__,
      client_id, camera_id);
  return NO_ERROR;
}

status_t RecorderImpl::GetCameraCharacteristics(const uint32_t client_id,
                                                const uint32_t camera_id,
                                                CameraMetadata &meta) {

  QMMF_DEBUG("%s: Enter client_id(%d):camera_id(%d)", __func__,
      client_id, camera_id);

  if (!IsClientValid(client_id)) {
    QMMF_ERROR("%s: Client(%u) is not connected!", __func__, client_id);
    return BAD_VALUE;
  }

  if (!IsCameraValid(client_id, camera_id)) {
    QMMF_ERROR("%s Client(%u): Camera(%u) is not owned by this client,"
        " operation not allowed!", __func__, client_id, camera_id);
    return INVALID_OPERATION;
  }

  assert(camera_source_ != nullptr);
  auto ret = camera_source_->GetCameraCharacteristics(camera_id, meta);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s: GetCameraCharacteristics failed!", __func__);
    return ret;
  }
  QMMF_DEBUG("%s: Exit client_id(%d):camera_id(%d)", __func__,
      client_id, camera_id);
  return NO_ERROR;
}

// Data callback handlers.
void RecorderImpl::VideoTrackBufferCb(uint32_t client_id, uint32_t session_id,
                                      uint32_t track_id,
                                      std::vector<BnBuffer>& buffers,
                                      std::vector<MetaData>& meta_buffers) {

  QMMF_DEBUG("%s Enter client_id(%u), session_id(%u), track_id(%u)",
      __func__, client_id, session_id, track_id);
  assert(remote_cb_handle_ != nullptr);
  assert(IsClientValid(client_id));

  if (!IsClientAlive(client_id)) {
    ReturnTrackBuffer(client_id, session_id, track_id, buffers);
  } else {
    remote_cb_handle_(client_id)->
        NotifyVideoTrackData(track_id, buffers, meta_buffers);
  }
  QMMF_DEBUG("%s Exit client_id(%u), session_id(%u), track_id(%u)",
      __func__, client_id, session_id, track_id);
}

void RecorderImpl::AudioTrackBufferCb(uint32_t client_id, uint32_t session_id,
                                      uint32_t track_id,
                                      std::vector<BnBuffer>& buffers,
                                      std::vector<MetaData>& meta_buffers) {
  QMMF_DEBUG("%s Enter client_id(%u), session_id(%u), track_id(%u)",
      __func__, client_id, session_id, track_id);
  assert(remote_cb_handle_ != nullptr);
  assert(IsClientValid(client_id));

  if (!IsClientAlive(client_id)) {
    ReturnTrackBuffer(client_id, session_id, track_id, buffers);
  } else {
    remote_cb_handle_(client_id)->
        NotifyAudioTrackData(track_id, buffers, meta_buffers);
  }
  QMMF_DEBUG("%s Exit client_id(%u), session_id(%u), track_id(%u)",
      __func__, client_id, session_id, track_id);
}

void RecorderImpl::CameraSnapshotCb(uint32_t client_id, uint32_t camera_id,
                                    uint32_t count, BnBuffer& buffer,
                                    MetaData& meta_data) {

  QMMF_DEBUG("%s Enter client_id(%u), camera_id(%u), count(%u)",
      __func__, client_id, camera_id, count);
  assert(remote_cb_handle_ != nullptr);
  assert(IsClientValid(client_id));

  remote_cb_handle_(client_id)->NotifySnapshotData(camera_id, count,
                                                   buffer, meta_data);
  QMMF_DEBUG("%s Exit client_id(%u), camera_id(%u), count(%u)",
      __func__, client_id, camera_id, count);
}

void RecorderImpl::CameraFlushCb(const uint32_t camera_id) {

  QMMF_VERBOSE("%s: Enter", __func__);
  std::lock_guard<std::mutex> lock(camera_tracks_lock_);
  for (auto track_id : camera_tracks_map_[camera_id]) {
    if (encoder_core_) {
      auto ret = encoder_core_->FlushTrack(track_id);
      assert((ret == NO_ERROR) || (ret == NAME_NOT_FOUND));
    }
    if (camera_source_) {
      auto ret = camera_source_->FlushTrack(track_id);
      assert((ret == NO_ERROR) || (ret == NAME_NOT_FOUND));
    }
  }
  QMMF_VERBOSE("%s: Exit", __func__);
}

void RecorderImpl::CameraResultCb(uint32_t camera_id,
                                  const CameraMetadata &result) {

  QMMF_DEBUG("%s Enter camera_id(%u)", __func__, camera_id);
  assert(remote_cb_handle_ != nullptr);
  auto client_ids = GetCameraClients(camera_id);

  for (auto const& client_id : client_ids) {
    assert(IsClientValid(client_id));
    remote_cb_handle_(client_id)->NotifyCameraResult(camera_id, result);
  }
  QMMF_DEBUG("%s Exit camera_id(%u)", __func__, camera_id);
}

void RecorderImpl::CameraErrorCb(RecorderErrorData &error) {

  assert(remote_cb_handle_ != nullptr);
  auto client_ids = GetCameraClients(error.camera_id);

  for (auto const& client_id : client_ids) {
    assert(IsClientValid(client_id));
    remote_cb_handle_(client_id)->NotifyRecorderEvent(
        EventType::kCameraError, reinterpret_cast<void*>(&error),
        sizeof(RecorderErrorData));
  }
}

bool RecorderImpl::IsClientValid(const uint32_t& client_id) {

  std::lock_guard<std::mutex> lock(client_session_lock_);
  return (client_session_map_.count(client_id) != 0) ? true : false;
}

bool RecorderImpl::IsClientAlive(const uint32_t& client_id) {

  std::lock_guard<std::mutex> lock(client_state_lock_);
  return (client_state_[client_id] == ClientState::kAlive) ? true : false;
}

bool RecorderImpl::IsSessionValid(const uint32_t& client_id,
                                  const uint32_t& session_id) {

  std::lock_guard<std::mutex> lock(client_session_lock_);
  auto& session_track_map = client_session_map_[client_id];
  return (session_track_map.count(session_id) != 0) ? true : false;
}

bool RecorderImpl::IsTrackValid(const uint32_t& client_id,
                                const uint32_t& session_id,
                                const uint32_t& track_id) {

  std::lock_guard<std::mutex> lock(client_session_lock_);
  auto const& tracks_in_session = client_session_map_[client_id][session_id];
  return (tracks_in_session.count(track_id) != 0) ? true : false;
}

bool RecorderImpl::IsTrackValid(const uint32_t& client_id,
                                const uint32_t& track_id) {

  std::lock_guard<std::mutex> lock(client_session_lock_);
  auto const& session_track_map = client_session_map_[client_id];

  for (auto const& session : session_track_map) {
    auto const& tracks_in_session = session.second;
    if (tracks_in_session.count(track_id) != 0) {
      return true;
    }
  }
  return false;
}

bool RecorderImpl::IsCameraValid(const uint32_t& client_id,
                                 const uint32_t& camera_id) {

  std::lock_guard<std::mutex> lock(camera_map_lock_);
  bool valid = false;

  // Check if the camera id is registered for the client id and is owned by it.
  if (client_session_map_.count(client_id) != 0) {
    auto const& cameras = client_cameraid_map_[client_id];
    valid = (cameras.count(camera_id) != 0) ? cameras.at(camera_id) : false;
  }
  return valid;
}

bool RecorderImpl::IsCameraOwned(const uint32_t& client_id,
                                 const uint32_t& camera_id) {

  std::lock_guard<std::mutex> lock(camera_map_lock_);

  for (auto const& client_cameras : client_cameraid_map_) {
    auto const& cameras = client_cameras.second;
    auto const& camera_client_id = client_cameras.first;

    // Ignore check if current client_id.
    if ((client_id != camera_client_id) && (cameras.count(camera_id) != 0) &&
        cameras.at(camera_id)) {
      return true;
    }
  }
  return false;
}

bool RecorderImpl::IsSessionActive(const uint32_t& session_id) {

  std::lock_guard<std::mutex> lock(client_session_lock_);
  return (sessions_state_[session_id] == SessionState::kActive) ? true : false;
}

bool RecorderImpl::IsSessionPaused(const uint32_t& session_id) {

  std::lock_guard<std::mutex> lock(client_session_lock_);
  return (sessions_state_[session_id] == SessionState::kPause) ? true : false;
}

bool RecorderImpl::IsSessionIdle(const uint32_t& session_id) {

  std::lock_guard<std::mutex> lock(client_session_lock_);
  return (sessions_state_[session_id] == SessionState::kIdle) ? true : false;
}

void RecorderImpl::ChangeSessionState(const uint32_t& session_id,
                                      const SessionState& state) {

  std::lock_guard<std::mutex> lock(client_session_lock_);
  QMMF_INFO("%s: Session(%u): state = %d", __func__, session_id,
    (int32_t) state);
  sessions_state_[session_id] = state;
}

uint32_t RecorderImpl::GetUniqueServiceTrackId(const uint32_t& client_id,
                                               const uint32_t& session_id,
                                               const uint32_t& track_id) {
  uint32_t service_track_id = client_id << 24;
  service_track_id |= session_id << 16;
  service_track_id |= track_id;
  return service_track_id;
}

RecorderImpl::TrackInfo
RecorderImpl::GetServiceTrackInfo(const uint32_t& client_id,
                                  const uint32_t& session_id,
                                  const uint32_t& track_id) {

  std::lock_guard<std::mutex> lock(client_session_lock_);
  auto tracks_in_session = client_session_map_[client_id][session_id];
  return tracks_in_session[track_id];
}

uint32_t RecorderImpl::GetServiceTrackId(const uint32_t& client_id,
                                         const uint32_t& track_id) {

  std::lock_guard<std::mutex> lock(client_session_lock_);
  auto const& session_track_map = client_session_map_[client_id];

  for (auto const& session : session_track_map) {
    auto tracks_in_session = session.second;
    if (tracks_in_session.count(track_id) != 0) {
      return tracks_in_session[track_id].track_id;
    }
  }
  return NO_ERROR;
}

std::vector<uint32_t> RecorderImpl::GetCameraClients(const uint32_t& camera_id) {

  std::lock_guard<std::mutex> lock(camera_map_lock_);
  std::vector<uint32_t> client_ids;

  for (auto const& client_cameras : client_cameraid_map_) {
    auto const& cameras = client_cameras.second;
    auto const& client_id = client_cameras.first;

    if (cameras.count(camera_id) != 0) {
      client_ids.push_back(client_id);
    }
  }
  return client_ids;
}

}; // namespace recorder

}; //namespace qmmf
