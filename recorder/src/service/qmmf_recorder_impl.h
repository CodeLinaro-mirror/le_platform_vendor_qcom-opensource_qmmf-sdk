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

/*! @file qmmf_recorder_impl.h
*/

#pragma once

#include <algorithm>
#include <atomic>
#include <vector>
#include <mutex>
#include <tuple>
#include <set>

#include <camera/CameraMetadata.h>

#include "recorder/src/client/qmmf_recorder_service_intf.h"
#include "recorder/src/service/qmmf_recorder_common.h"
#include "recorder/src/service/qmmf_audio_source.h"
#include "recorder/src/service/qmmf_audio_encoder_core.h"
#include "recorder/src/service/qmmf_camera_source.h"
#include "recorder/src/service/qmmf_encoder_core.h"
#include "recorder/src/service/qmmf_remote_cb.h"

/// @namespace qmmf
namespace qmmf {

/// @namespace recorder
namespace recorder {


using namespace android;

/// @brief RecorderImpl interface
///
/// Handles all Server operations
class RecorderImpl {
 public:

  /// Create Recorder Instance
  static RecorderImpl* CreateRecorder();

  /// RecorderImpl Destructor
  ~RecorderImpl();

  /// Create CameraSource, EncoderCore Instance, AudioSource
  /// and AudioEncoderCore instances
  status_t Init(const RemoteCallbackHandle& remote_cb_handler);

  /// Destroys all instances created in Init
  status_t DeInit();

  /// Connect and register client to the session
  status_t RegisterClient(const uint32_t client_id);

  /// Cleans up and closes the cameras owned by previous "dead" client.
  /// Register new client.
  status_t DeRegisterClient(const uint32_t client_id,
                            bool force_cleanup = false);

  /// Start(open) the Camera
  status_t StartCamera(const uint32_t client_id, const uint32_t camera_id,
                       const CameraStartParam& param,
                       bool enable_result_cb = false);

  /// Stop(close) the Camera
  status_t StopCamera(const uint32_t client_id, const uint32_t camera_id);

  /// Create session
  status_t CreateSession(const uint32_t client_id, uint32_t *session_id);

  /// Delete session
  status_t DeleteSession(const uint32_t client_id, const uint32_t session_id);

  /// Start the session
  status_t StartSession(const uint32_t client_id, const uint32_t session_id);

  /// Stop the session
  status_t StopSession(const uint32_t client_id, const uint32_t session_id,
                       bool do_flush, bool force_cleanup = false);

  /// Pause the session
  status_t PauseSession(const uint32_t client_id, const uint32_t session_id);

  /// Resume the session
  status_t ResumeSession(const uint32_t client_id, const uint32_t session_id);

  /// Return number of available Cameras to the Camera Source
  status_t GetNumberOfCameras(const uint32_t client_id,
                              SupportedCameras &cameras);

  /// Return all supported plugins to the Camera Source
  status_t GetSupportedPlugins(const uint32_t client_id,
                               SupportedPlugins *plugins);

  /// Create plugin
  status_t CreatePlugin(const uint32_t client_id, uint32_t *uid,
                        const PluginInfo &plugin);

  /// Delete plugin
  status_t DeletePlugin(const uint32_t client_id, const uint32_t &uid);

  /// Configures plugin
  status_t ConfigPlugin(const uint32_t client_id, const uint32_t &uid,
                        const std::string &json_config);

  /// Create Audio Track and associates it to the session.
  status_t CreateAudioTrack(const uint32_t client_id,
                            const uint32_t session_id,
                            const uint32_t track_id,
                            const AudioTrackCreateParam& param);

  /// Create Video Track and associates it to the session.
  status_t CreateVideoTrack(const uint32_t client_id,
                            const uint32_t session_id,
                            const uint32_t track_id,
                            const VideoTrackCreateParam& param);

  /// Create Video Track and associates it to the session with
  /// additional configure parameters
  status_t CreateVideoTrack(const uint32_t client_id,
                            const uint32_t session_id,
                            const uint32_t track_id,
                            const VideoTrackCreateParam& param,
                            const VideoExtraParam& extra_param);

  /// Delete Audio Track from the session.
  status_t DeleteAudioTrack(const uint32_t client_id,
                            const uint32_t session_id,
                            const uint32_t track_id);

  /// Delete Video Track from the session.
  status_t DeleteVideoTrack(const uint32_t client_id,
                            const uint32_t session_id,
                            const uint32_t track_id);

  /// Return Track buffers to Encoder, Camera Source, Audio Encoder
  /// or Audio Source
  status_t ReturnTrackBuffer(const uint32_t client_id,
                             const uint32_t session_id,
                             const uint32_t track_id,
                             std::vector<BnBuffer> &buffers);

  /// Set Audio Track parameters
  status_t SetAudioTrackParam(const uint32_t client_id,
                              const uint32_t session_id,
                              const uint32_t track_id,
                              CodecParamType type,
                              void *param,
                              size_t param_size);

  /// Set Video Track parameters
  status_t SetVideoTrackParam(const uint32_t client_id,
                              const uint32_t session_id,
                              const uint32_t track_id,
                              CodecParamType type,
                              void *param,
                              size_t param_size);

  /// Image Capture
  status_t CaptureImage(const uint32_t client_id,
                        const uint32_t camera_id,
                        const ImageParam &param,
                        const uint32_t num_images,
                        const std::vector<CameraMetadata> &meta);

  /// Configuration for Image Capture
  status_t ConfigImageCapture(const uint32_t client_id,
                              const uint32_t camera_id,
                              const ImageConfigParam &config);

  /// Cancel Image Capture
  status_t CancelCaptureImage(const uint32_t client_id,
                              const uint32_t camera_id);

  /// Return Image Capture buffer
  status_t ReturnImageCaptureBuffer(const uint32_t client_id,
                                    const uint32_t camera_id,
                                    const int32_t buffer_id);

  /// Set Camera parameters
  status_t SetCameraParam(const uint32_t client_id,
                          const uint32_t camera_id, const CameraMetadata &meta);

  /// Get Camera parameters
  status_t GetCameraParam(const uint32_t client_id,
                          const uint32_t camera_id, CameraMetadata &meta);

  /// Get default Capture parameters
  status_t GetDefaultCaptureParam(const uint32_t client_id,
                                  const uint32_t camera_id,
                                  CameraMetadata &meta);

  /// Create Overlay object
  status_t CreateOverlayObject(const uint32_t client_id,
                               const uint32_t track_id,
                               OverlayParam *param,
                               uint32_t *overlay_id);

  /// Delete Overlay object
  status_t DeleteOverlayObject(const uint32_t client_id,
                               const uint32_t track_id,
                               const uint32_t overlay_id);

  /// Get Overlay object parameters
  status_t GetOverlayObjectParams(const uint32_t client_id,
                                  const uint32_t track_id,
                                  const uint32_t overlay_id,
                                  OverlayParam &param);

  /// Update Overlay object parameters
  status_t UpdateOverlayObjectParams(const uint32_t client_id,
                                     const uint32_t track_id,
                                     const uint32_t overlay_id,
                                     OverlayParam *param);

  /// Set Overlay object parameters
  status_t SetOverlayObject(const uint32_t client_id,
                            const uint32_t track_id,
                            const uint32_t overlay_id);

  /// Remove Overlay object
  status_t RemoveOverlayObject(const uint32_t client_id,
                               const uint32_t track_id,
                               const uint32_t overlay_id);

  /// Create Multiple-camera instance
  status_t CreateMultiCamera(const uint32_t client_id,
                             const std::vector<uint32_t> camera_ids,
                             uint32_t *virtual_camera_id);

  /// Configure Multiple-camera
  status_t ConfigureMultiCamera(const uint32_t client_id,
                                const uint32_t virtual_camera_id,
                                const MultiCameraConfigType type,
                                const void *param,
                                const uint32_t param_size);

  // Data callback handlers.
  /// Video Track buffer callback handler
  void VideoTrackBufferCb(uint32_t client_id, uint32_t session_id,
                          uint32_t track_id, std::vector<BnBuffer>& buffers,
                          std::vector<MetaData>& meta_buffers);

  /// Audio Track buffer callback handler
  void AudioTrackBufferCb(uint32_t client_id, uint32_t session_id,
                          uint32_t track_id, std::vector<BnBuffer>& buffers,
                          std::vector<MetaData>& meta_buffers);

  /// Camera Snapshot callback handler
  void CameraSnapshotCb(uint32_t client_id, uint32_t camera_id, uint32_t count,
                        BnBuffer& buffer, MetaData& meta_data);

  /// Camera Result callback handler
  void CameraResultCb(uint32_t camera_id, const CameraMetadata &result);

  /// Camera Error callback handler
  void CameraErrorCb(RecorderErrorData &error);

  // Camera Flush Callback Handler
  void CameraFlushCb(const uint32_t camera_id);

/// @cond PRIVATE
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

  // <client id, map<camera id, owned?> >
  typedef std::map<uint32_t, std::map<uint32_t, bool> > ClientCameraIdMap;

  // <client id, ClientState>
  typedef std::map<uint32_t, ClientState> ClientStateMap;

  // <session_id, SessionState>
  typedef std::map<uint32_t, SessionState> SessionStateMap;
  // <client_id, SessionStateMap>
  typedef std::map<uint32_t, SessionStateMap> ClientSessionStateMap;

  // <camera id, set <track id> >
  typedef std::map<uint32_t, std::set<uint32_t>> CameraTrackIdsMap;

  bool IsClientValid(const uint32_t& client_id);
  bool IsClientAlive(const uint32_t& client_id);
  bool IsSessionValid(const uint32_t& client_id, const uint32_t& session_id);
  bool IsTrackValid(const uint32_t& client_id, const uint32_t& session_id,
                    const uint32_t& track_id);
  bool IsTrackValid(const uint32_t& client_id, const uint32_t& track_id);
  bool IsCameraValid(const uint32_t& client_id, const uint32_t& camera_id);
  bool IsCameraOwned(const uint32_t& client_id, const uint32_t& camera_id);

  bool IsSessionActive(const uint32_t& client_id, const uint32_t& session_id);
  bool IsSessionPaused(const uint32_t& client_id, const uint32_t& session_id);
  bool IsSessionIdle(const uint32_t& client_id, const uint32_t& session_id);
  void ChangeSessionState(const uint32_t& client_id,
                          const uint32_t& session_id,
                          const SessionState& state);

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

  std::vector<uint32_t> GetCameraClients(const uint32_t& camera_id);

  status_t GetUniqueSessionID(const uint32_t& client_id, uint32_t* session_id);

  CameraSource*                 camera_source_;
  EncoderCore*                  encoder_core_;
  AudioSource*                  audio_source_;
  AudioEncoderCore*             audio_encoder_core_;

  RemoteCallbackHandle          remote_cb_handle_;

  ClientSessionMap              client_session_map_;
  std::mutex                    client_session_lock_;

  ClientCameraIdMap             client_cameraid_map_;
  QCondition                    slave_camera_closed_;
  std::mutex                    camera_map_lock_;

  CameraTrackIdsMap             camera_tracks_map_;
  std::mutex                    camera_tracks_lock_;

  ClientStateMap                client_state_;
  std::mutex                    client_state_lock_;

  ClientSessionStateMap         client_sessions_state_;

  std::mutex                    stop_camera_lock_;

  // Not allowed
  RecorderImpl();
  RecorderImpl(const RecorderImpl&);
  RecorderImpl& operator=(const RecorderImpl&);
  static RecorderImpl* instance_;
  /// @endcond

};

}; // namespace recorder

}; //namespace qmmf
