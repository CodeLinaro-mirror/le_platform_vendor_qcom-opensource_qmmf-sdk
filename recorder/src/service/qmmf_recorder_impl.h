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

#include <camera/CameraMetadata.h>
#include <algorithm>
#include <vector>
#include <mutex>
#include <tuple>

#include "recorder/src/client/qmmf_recorder_service_intf.h"
#include "recorder/src/service/qmmf_recorder_common.h"
#include "recorder/src/service/qmmf_audio_source.h"
#include "recorder/src/service/qmmf_audio_encoder_core.h"
#include "recorder/src/service/qmmf_camera_source.h"
#include "recorder/src/service/qmmf_encoder_core.h"
#include "recorder/src/service/qmmf_remote_cb.h"

namespace qmmf {

namespace recorder {

using namespace android;

class RecorderImpl {
 public:

  static RecorderImpl* CreateRecorder();

  ~RecorderImpl();

  status_t Init(const RemoteCallbackHandle& remote_cb_handler);

  status_t DeInit();

  status_t RegisterClient(const uint32_t client_id);

  status_t DeRegisterClient(const uint32_t client_id,
                            bool force_cleanup = false);

  status_t StartCamera(const uint32_t client_id, const uint32_t camera_id,
                       const CameraStartParam& param,
                       bool enable_result_cb = false);

  status_t StopCamera(const uint32_t client_id, const uint32_t camera_id);

  status_t CreateSession(const uint32_t client_id, uint32_t *session_id);

  status_t DeleteSession(const uint32_t client_id, const uint32_t session_id);

  status_t StartSession(const uint32_t client_id, const uint32_t session_id);

  status_t StopSession(const uint32_t client_id, const uint32_t session_id,
                       bool do_flush, bool force_cleanup = false);

  status_t PauseSession(const uint32_t client_id, const uint32_t session_id);

  status_t ResumeSession(const uint32_t client_id, const uint32_t session_id);

  status_t GetNumberOfCameras(const uint32_t client_id,
                              SupportedCameras &cameras);

  status_t GetSupportedPlugins(const uint32_t client_id,
                               SupportedPlugins *plugins);

  status_t CreatePlugin(const uint32_t client_id, uint32_t *uid,
                        const PluginInfo &plugin);

  status_t DeletePlugin(const uint32_t client_id, const uint32_t &uid);

  status_t ConfigPlugin(const uint32_t client_id, const uint32_t &uid,
                        const std::string &json_config);

  status_t CreateAudioTrack(const uint32_t client_id,
                            const uint32_t session_id,
                            const uint32_t track_id,
                            const AudioTrackCreateParam& param);

  status_t CreateVideoTrack(const uint32_t client_id,
                            const uint32_t session_id,
                            const uint32_t track_id,
                            const VideoTrackCreateParam& param);

  status_t CreateVideoTrack(const uint32_t client_id,
                            const uint32_t session_id,
                            const uint32_t track_id,
                            const VideoTrackCreateParam& param,
                            const VideoExtraParam& extra_param);

  status_t DeleteAudioTrack(const uint32_t client_id,
                            const uint32_t session_id,
                            const uint32_t track_id);

  status_t DeleteVideoTrack(const uint32_t client_id,
                            const uint32_t session_id,
                            const uint32_t track_id);

  status_t ReturnTrackBuffer(const uint32_t client_id,
                             const uint32_t session_id,
                             const uint32_t track_id,
                             std::vector<BnBuffer> &buffers);

  status_t SetAudioTrackParam(const uint32_t client_id,
                              const uint32_t session_id,
                              const uint32_t track_id,
                              CodecParamType type,
                              void *param,
                              size_t param_size);

  status_t SetVideoTrackParam(const uint32_t client_id,
                              const uint32_t session_id,
                              const uint32_t track_id,
                              CodecParamType type,
                              void *param,
                              size_t param_size);

  status_t CaptureImage(const uint32_t client_id,
                        const uint32_t camera_id,
                        const ImageParam &param,
                        const uint32_t num_images,
                        const std::vector<CameraMetadata> &meta);

  status_t ConfigImageCapture(const uint32_t client_id,
                              const uint32_t camera_id,
                              const ImageConfigParam &config);

  status_t CancelCaptureImage(const uint32_t client_id,
                              const uint32_t camera_id);

  status_t ReturnImageCaptureBuffer(const uint32_t client_id,
                                    const uint32_t camera_id,
                                    const int32_t buffer_id);

  status_t SetCameraParam(const uint32_t client_id,
                          const uint32_t camera_id, const CameraMetadata &meta);

  status_t GetCameraParam(const uint32_t client_id,
                          const uint32_t camera_id, CameraMetadata &meta);

  status_t GetDefaultCaptureParam(const uint32_t client_id,
                                  const uint32_t camera_id,
                                  CameraMetadata &meta);

  status_t CreateOverlayObject(const uint32_t client_id,
                               const uint32_t track_id,
                               OverlayParam *param,
                               uint32_t *overlay_id);

  status_t DeleteOverlayObject(const uint32_t client_id,
                               const uint32_t track_id,
                               const uint32_t overlay_id);

  status_t GetOverlayObjectParams(const uint32_t client_id,
                                  const uint32_t track_id,
                                  const uint32_t overlay_id,
                                  OverlayParam &param);

  status_t UpdateOverlayObjectParams(const uint32_t client_id,
                                     const uint32_t track_id,
                                     const uint32_t overlay_id,
                                     OverlayParam *param);

  status_t SetOverlayObject(const uint32_t client_id,
                            const uint32_t track_id,
                            const uint32_t overlay_id);

  status_t RemoveOverlayObject(const uint32_t client_id,
                               const uint32_t track_id,
                               const uint32_t overlay_id);

  status_t CreateMultiCamera(const uint32_t client_id,
                             const std::vector<uint32_t> camera_ids,
                             uint32_t *virtual_camera_id);

  status_t ConfigureMultiCamera(const uint32_t client_id,
                                const uint32_t virtual_camera_id,
                                const MultiCameraConfigType type,
                                const void *param,
                                const uint32_t param_size);

  // Data callback handlers.
  void VideoTrackBufferCb(uint32_t client_id, uint32_t session_id,
                          uint32_t track_id, std::vector<BnBuffer>& buffers,
                          std::vector<MetaData>& meta_buffers);

  void AudioTrackBufferCb(uint32_t client_id, uint32_t session_id,
                          uint32_t track_id, std::vector<BnBuffer>& buffers,
                          std::vector<MetaData>& meta_buffers);

  void CameraSnapshotCb(uint32_t client_id, uint32_t camera_id, uint32_t count,
                        BnBuffer& buffer, MetaData& meta_data);

  void CameraResultCb(uint32_t client_id, uint32_t camera_id,
                      const CameraMetadata &result);

  void CameraErrorCb(uint32_t client_id, RecorderErrorData &error);

 private:
  enum class ClientState {
    kAlive,
    kDead,
  };

  enum class SessionState {
    kActive,
    kPause,
    kIdle,
  };

  struct TrackInfo {
    uint32_t         track_id;
    TrackType        type;
    union {
      VideoFormat video;
      AudioFormat audio;
    } format;
  };

  // <client track id, TrackInfo>
  typedef std::map<uint32_t, TrackInfo> TrackInfoMap;
  // <session id, map <tracks> >
  typedef std::map<uint32_t, TrackInfoMap> SessionTrackMap;
  // <client id, <session_id, vector<client track id, service track id> > >
  typedef std::map<uint32_t, SessionTrackMap> ClientSessionMap;

  // <client id, set<camera ids> >
  typedef std::map<uint32_t, std::set<uint32_t> > ClientCameraIdMap;

  // <client id, ClientState>
  typedef std::map<uint32_t, ClientState> ClientStateMap;

  // <session_id, SessionState>
  typedef std::map<uint32_t, SessionState> ClientSessionStateMap;

  bool IsClientValid(const uint32_t& client_id);
  bool IsClientAlive(const uint32_t& client_id);
  bool IsSessionValid(const uint32_t& client_id, const uint32_t& session_id);
  bool IsTrackValid(const uint32_t& client_id, const uint32_t& session_id,
                    const uint32_t& track_id);
  bool IsTrackValid(const uint32_t& client_id, const uint32_t& track_id);
  bool IsCameraValid(const uint32_t& client_id, const uint32_t& camera_id);
  bool IsCameraOwned(const uint32_t& client_id, const uint32_t& camera_id);

  bool IsSessionActive(const uint32_t& session_id);
  bool IsSessionPaused(const uint32_t& session_id);
  bool IsSessionIdle(const uint32_t& session_id);
  void ChangeSessionState(const uint32_t& session_id, const SessionState& state);

  //Validate the input params during CreateAudioTrack requests.
  bool IsAudioTrackCreateParamValid(const AudioTrackCreateParam& param);

  uint32_t GetUniqueServiceTrackId(const uint32_t& client_id,
                                   const uint32_t& session_id,
                                   const uint32_t& track_id);

  TrackInfo GetServiceTrackInfo(const uint32_t& client_id,
                                const uint32_t& session_id,
                                const uint32_t& track_id);

  uint32_t GetServiceTrackId(const uint32_t& client_id,
                             const uint32_t& track_id);

  uint32_t                      unique_session_id_;

  CameraSource*                 camera_source_;
  EncoderCore*                  encoder_core_;
  AudioSource*                  audio_source_;
  AudioEncoderCore*             audio_encoder_core_;

  RemoteCallbackHandle          remote_cb_handle_;

  ClientSessionMap              client_session_map_;
  std::mutex                    client_session_lock_;

  ClientCameraIdMap             client_cameraid_map_;
  std::mutex                    camera_map_lock_;

  ClientStateMap                client_state_;
  std::mutex                    client_state_lock_;

  ClientSessionStateMap         sessions_state_;

  // Not allowed
  RecorderImpl();
  RecorderImpl(const RecorderImpl&);
  RecorderImpl& operator=(const RecorderImpl&);
  static RecorderImpl* instance_;

};

}; // namespace recorder

}; //namespace qmmf
