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

#include <camera/CameraMetadata.h>
#include <utils/KeyedVector.h>

#include "recorder/src/client/qmmf_recorder_service_intf.h"
#include "recorder/src/service/qmmf_recorder_common.h"
#include "recorder/src/service/qmmf_audio_source.h"
#include "recorder/src/service/qmmf_camera_source.h"
#include "recorder/src/service/qmmf_encoder_core.h"
#include "recorder/src/service/qmmf_remote_cb.h"

namespace qmmf {

namespace recorder {

using namespace android;
class RecorderImpl
{
 public:

  static RecorderImpl* CreateRecorder();

  ~RecorderImpl();

  status_t Connect(sp<RemoteCallBack>& remote_cb);

  status_t Disconnect();

  status_t StartCamera(std::vector<uint32_t> camera_ids,
                       CameraStartParam &param);

  status_t StopCamera(std::vector<uint32_t> camera_ids);

  status_t CreateSession(uint32_t *session_id);

  status_t DeleteSession(const uint32_t session_id);

  status_t StartSession(const uint32_t session_id);

  status_t StopSession(const uint32_t session_id, bool do_flush);

  status_t PauseSession(const uint32_t session_id);

  status_t ResumeSession(const uint32_t session_id);

  status_t CreateAudioTrack(const uint32_t session_id,
                            uint32_t track_id,
                            const AudioTrackCreateParam& param);

  status_t CreateVideoTrack(const uint32_t session_id,
                            uint32_t track_id,
                            VideoTrackCreateParam& param);

  status_t DeleteAudioTrack(const uint32_t session_id,
                            const uint32_t track_id);

  status_t DeleteVideoTrack(const uint32_t session_id,
                            const uint32_t track_id);

  status_t ReturnTrackBuffer(const uint32_t session_id,
                             const uint32_t track_id,
                             std::vector<BnTrackBuffer> &buffers);

  status_t SetAudioTrackParam(const uint32_t session_id,
                              const uint32_t track_id,
                              AudioTrackParamType type,
                              void *param,
                              size_t param_size);

  status_t SetVideoTrackParam(const uint32_t session_id,
                              const uint32_t track_id,
                              VideoTrackParamType type,
                              void *param,
                              size_t param_size);

  status_t CaptureImage(std::vector<uint32_t> camera_id,
                        ImageParam &param);

  status_t CancelCaptureImage();

  status_t SetCameraParam(uint32_t camera_id, CameraMetadata &meta);

  status_t GetCameraParam(uint32_t camera_id, CameraMetadata &meta);

  status_t CreateOverlayObject(OverlayParam &param,
                               uint32_t *overlay_id);

  status_t DeleteOverlayObject(const uint32_t overlay_id);

  status_t GetOverlayObjectParams(const uint32_t overlay_id,
                                  OverlayParam &param);

  status_t UpdateOverlayObjectParams(const uint32_t overlay_id,
                                     OverlayParam &param);

  status_t SetOverlayObject(const uint32_t session_id,
                            const uint32_t track_id,
                            const uint32_t overlay_id);

  status_t RemoveOverlayObject(const uint32_t session_id,
                               const uint32_t track_id,
                               const uint32_t overlay_id);

  void VideoTrackBufferCallback(uint32_t track_id,
                                std::vector<BnTrackBuffer> buffers,
                                void *meta_param,
                                TrackMetaParamType meta_type,
                                size_t meta_size);

  void AudioTrackBufferCallback(uint32_t track_id,
                                std::vector<BnTrackBuffer> buffers,
                                void *meta_param,
                                TrackMetaParamType meta_type,
                                size_t meta_size);

  void CaptureImageCallback(void* buffer, uint32_t buffer_size);

 private:

  bool IsSessionIdValid(const uint32_t session_id);

  bool IsSessionValid(const uint32_t session_id);

  bool IsTrackValid(const uint32_t session_id, const uint32_t track_id);

  typedef struct TrackInfo {
    uint32_t         track_id;
    TrackType        type;
    VideoTrackParams params;
    AudioTrackParams audio_params;
    //TODO: Add union and pack AudioTrack params.
  } TrackInfo;

  uint32_t            unique_id_;
  AudioSource*        audio_source_;
  CameraSource*       camera_source_;
  EncoderCore*        encoder_core_;
  Vector<uint32_t>    session_ids_;
  sp<RemoteCallBack>  remote_cb_;

  DefaultKeyedVector<uint32_t, Vector<TrackInfo> > sessions_;
  /**Not allowed */
  RecorderImpl();
  RecorderImpl(const RecorderImpl&);
  RecorderImpl& operator=(const RecorderImpl&);
  static RecorderImpl* instance_;

};

}; // namespace recorder

}; //namespace qmmf
