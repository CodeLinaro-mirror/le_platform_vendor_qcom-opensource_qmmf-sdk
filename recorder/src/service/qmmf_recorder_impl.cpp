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

#define TAG "RecorderImpl"

#include "qmmf_recorder_impl.h"

namespace qmmf {

namespace recorder {

RecorderImpl* RecorderImpl::instance_ = nullptr;

RecorderImpl* RecorderImpl::CreateRecorder() {

  if(!instance_) {
    instance_ = new RecorderImpl;
    if(!instance_) {
      QMMF_ERROR("%s:%s: Can't Create Recorder Instance!", TAG, __func__);
      return NULL;
    }
  }
  QMMF_INFO("%s:%s: Recorder Instance Created Successfully(0x%x)", TAG,
      __func__, instance_);
  return instance_;
}

RecorderImpl::RecorderImpl()
    : unique_id_(0), camera_source_(nullptr) {

  QMMF_INFO("%s:%s: Enter", TAG, __func__);
  QMMF_INFO("%s:%s: Exit", TAG, __func__);
}

RecorderImpl::~RecorderImpl() {

  QMMF_INFO("%s:%s: Enter", TAG, __func__);

  if (camera_source_) {
    delete camera_source_;
    camera_source_ = nullptr;
  }
  if (encoder_core_) {
    delete encoder_core_;
    encoder_core_ = nullptr;
  }
  instance_ = nullptr;
  QMMF_INFO("%s:%s: Exit (0x%x)", TAG, __func__, this);
}

status_t RecorderImpl::Connect(sp<RemoteCallBack>& remote_cb) {

  QMMF_INFO("%s:%s: Enter", TAG, __func__);

  assert(remote_cb.get() != nullptr);
  remote_cb_ = remote_cb;

  camera_source_ = CameraSource::CreateCameraSource();
  if (!camera_source_) {
    QMMF_ERROR("%s:%s: Can't Create CameraSource Instance!", TAG, __func__);
    return NO_MEMORY;
  }
  QMMF_INFO("%s:%s: CameraSource Instance Created Successfully!", TAG,
      __func__);

  encoder_core_ = EncoderCore::CreateEncoderCore();
  if (!encoder_core_) {
    QMMF_ERROR("%s:%s: Can't Create EncoderCore Instance!", TAG, __func__);
    return NO_MEMORY;
  }
  QMMF_INFO("%s:%s: EncoderCore Instance Created Successfully!", TAG,
      __func__);

  QMMF_INFO("%s:%s: Exit", TAG, __func__);
  return NO_ERROR;
}

status_t RecorderImpl::Disconnect() {

  QMMF_INFO("%s:%s: Enter", TAG, __func__);
  if (camera_source_) {
    delete camera_source_;
    camera_source_ = nullptr;
  }
  if (encoder_core_) {
    delete encoder_core_;
    encoder_core_ = nullptr;
  }
  return NO_ERROR;
  QMMF_INFO("%s:%s: Exit", TAG, __func__);
}

status_t RecorderImpl::StartCamera(std::vector<uint32_t> camera_id,
                                   CameraStartParam &param) {

  assert(camera_source_ != NULL);

  auto ret = camera_source_->StartCamera(camera_id, param);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s:%s: StartCamera Failed!!", TAG, __func__);
    return BAD_VALUE;
  }
  return ret;
}

status_t RecorderImpl::StopCamera(std::vector<uint32_t> camera_id) {

  assert(camera_source_ != NULL);

  QMMF_INFO("%s:%s: Enter", TAG, __func__);
  auto ret = camera_source_->StopCamera(camera_id);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s:%s: StopCamera Failed!!", TAG, __func__);
    return BAD_VALUE;
  }
  return ret;
}

status_t RecorderImpl::CreateSession(uint32_t *session_id) {

  if(!camera_source_) {
    QMMF_ERROR("%s:%s: Can't Create Session! Connect Should be called before"
       " Calling CreateSession", TAG, __func__);
    return NO_INIT;
  }
  ++unique_id_;
  *session_id = unique_id_;
  session_ids_.push_back(*session_id);
  QMMF_INFO("%s:%s: session_id = %d ", TAG, __func__, *session_id);
}

status_t RecorderImpl::DeleteSession(const uint32_t session_id) {

  int32_t ret = NO_ERROR;

  if(!IsSessionValid(session_id)) {
    QMMF_ERROR("%s:%s: session_id is not valid!", TAG, __func__);
    return BAD_VALUE;
  }

  Vector<TrackInfo> tracks = sessions_.valueFor(session_id);
  if (tracks.size() > 0) {
    QMMF_ERROR("%s:%s: Session(%d) Can't be deleted until all tracks(%d) within"
        "this session are stopped & deleted(%d)!", TAG, __func__, session_id,
        tracks.size());
    return INVALID_OPERATION;
  }
  sessions_.removeItem(session_id);
  return ret;
}

status_t RecorderImpl::StartSession(const uint32_t session_id) {

  uint32_t ret = NO_ERROR;

  if(!IsSessionValid(session_id)) {
    QMMF_ERROR("%s:%s: Session Id is not valid Or No track is associated"
        " with this sesssion(%d)", TAG, __func__, session_id);
    return BAD_VALUE;
  }

  Vector<TrackInfo> tracks = sessions_.valueFor(session_id);
  size_t num_tracks = tracks.size();
  QMMF_INFO("%s:%s: Number of tracks(%d) to start in session(%d)!", TAG,
      __func__, num_tracks, session_id);

  // All the tracks associated to one session starts together.
  for(uint8_t i = 0; i < num_tracks; i++) {

    if (tracks[i].type == TrackType::kVideo) {
      assert(camera_source_ != NULL);
      ret = camera_source_->StartTrackSource(tracks[i].track_id);

      if (ret != NO_ERROR) {
        ret = BAD_VALUE;
        QMMF_ERROR("%s:%s: StartTrackSource failed for track_id(%d) and"
            "session_id(%d)", TAG, __func__, tracks[i].track_id, session_id);
        break;
      }
      QMMF_INFO("%s:%s: track_id(%d) Started Successfully :session_id(%d)",
          TAG, __func__, tracks[i].track_id, session_id);

      if ( (tracks[i].params.codec_type == VideoCodecType::kHEVC) ||
           (tracks[i].params.codec_type == VideoCodecType::kAVC) ) {
        //TODO: Add logic to start TrackEncoder
      }

    } else if (tracks[i].type == TrackType::kAudio) {
      //TODO: Add support for Audio.
    }
  }

  if(ret == NO_ERROR) {
    QMMF_INFO("%s:%s: session_id(%d) with num tracks(%d) Started"
    " Successfully!", TAG, __func__, session_id, num_tracks);
  }
  return ret;
  //TODO: Send session status callback to application.
  //propably not from here??
}

status_t RecorderImpl::StopSession(const uint32_t session_id, bool do_flush) {

  uint32_t ret = NO_ERROR;

  if(!IsSessionValid(session_id)) {
    QMMF_ERROR("%s:%s: Session Id is not valid Or No track is associated "
        "with this sesssion(%d)", TAG, __func__, session_id);
    return BAD_VALUE;
  }

  Vector<TrackInfo> tracks = sessions_.valueFor(session_id);
  QMMF_INFO("%s:%s: Number of tracks(%d) to stop in session(%d)!", TAG,
      __func__, tracks.size(), session_id);

  // All the tracks associated to one session stops together.
  for(uint8_t i = 0; i < tracks.size(); i++) {

    if (tracks[i].type == TrackType::kVideo) {
      assert(camera_source_ != NULL);
      ret = camera_source_->StopTrackSource(tracks[i].track_id);

      if (ret != NO_ERROR) {
        ret = BAD_VALUE;
        QMMF_ERROR("%s:%s: StopTrackSource failed for track_id(%d) and"
            "session_id(%d)", TAG, __func__, tracks[i].track_id, session_id);
        break;
      }
      QMMF_INFO("%s:%s: track_id(%d) Stopped Successfully :session_id(%d)",
          TAG, __func__, tracks[i].track_id, session_id);

      if ( (tracks[i].params.codec_type == VideoCodecType::kHEVC) ||
           (tracks[i].params.codec_type == VideoCodecType::kAVC) ) {
        //TODO: Add logic to stop TrackEncoder
      }

    } else if (tracks[i].type == TrackType::kAudio) {
      //TODO: Add support for Audio.
    }
  }

  if(ret == NO_ERROR) {
      QMMF_INFO("%s:%s: session_id(%d) Stopped Successfully!", TAG, __func__,
      session_id);
  }
  return ret;
  //TODO: Send session status callback to application.
  //propably not from here??
}

status_t RecorderImpl::PauseSession(const uint32_t session_id) {

  uint32_t ret = NO_ERROR;

  if(!IsSessionValid(session_id)) {
    QMMF_ERROR("%s:%s: Session Id is not valid Or No track is associated"
        "with this sesssion(%d)", TAG, __func__, session_id);
    return BAD_VALUE;
  }

  Vector<TrackInfo> tracks = sessions_.valueFor(session_id);
  QMMF_INFO("%s:%s: Number of tracks(%d) to Pause in session(%d)!", TAG,
      __func__, tracks.size(), session_id);

  // All the tracks associated to one session stops together.
  for(uint8_t i = 0; i < tracks.size(); i++) {

    if (tracks[i].type == TrackType::kVideo) {
      assert(camera_source_ != NULL);
      ret = camera_source_->PauseTrackSource(tracks[i].track_id);

      if (ret != NO_ERROR) {
        ret = BAD_VALUE;
        QMMF_ERROR("%s:%s: PauseTrackSource failed for track_id(%d) and"
            "session_id(%d)", TAG, __func__, tracks[i].track_id, session_id);
        break;
      }
      QMMF_INFO("%s:%s: track_id(%d) Paused Successfully :session_id(%d)",
          TAG, __func__, tracks[i].track_id, session_id);

      if ( (tracks[i].params.codec_type == VideoCodecType::kHEVC) ||
           (tracks[i].params.codec_type == VideoCodecType::kAVC) ) {
        //TODO: Add logic to stop TrackEncoder
      }

    } else if (tracks[i].type == TrackType::kAudio) {
      //TODO: Add support for Audio.
    }
  }

  if(ret == NO_ERROR) {
      QMMF_INFO("%s:%s: session_id(%d) Paused Successfully!", TAG, __func__,
      session_id);
  }
  return ret;
  //TODO: Send session status callback to application.
  //propably not from here??
}

status_t RecorderImpl::ResumeSession(const uint32_t session_id) {

  uint32_t ret = NO_ERROR;

  if(!IsSessionValid(session_id)) {
    QMMF_ERROR("%s:%s: Session Id is not valid Or No track is associated"
        "with this sesssion(%d)", TAG, __func__, session_id);
    return BAD_VALUE;
  }

  Vector<TrackInfo> tracks = sessions_.valueFor(session_id);
  QMMF_INFO("%s:%s: Number of tracks(%d) to Resume in session(%d)!", TAG,
      __func__, tracks.size(), session_id);

  // All the tracks associated to one session stops together.
  for(uint8_t i = 0; i < tracks.size(); i++) {

    if (tracks[i].type == TrackType::kVideo) {
      assert(camera_source_ != NULL);
      ret = camera_source_->ResumeTrackSource(tracks[i].track_id);

      if (ret != NO_ERROR) {
        ret = BAD_VALUE;
        QMMF_ERROR("%s:%s: ResumeTrackSource failed for track_id(%d) and"
            "session_id(%d)", TAG, __func__, tracks[i].track_id, session_id);
        break;
      }
      QMMF_INFO("%s:%s: track_id(%d) Resumed Successfully :session_id(%d)",
          TAG, __func__, tracks[i].track_id, session_id);

      if ( (tracks[i].params.codec_type == VideoCodecType::kHEVC) ||
           (tracks[i].params.codec_type == VideoCodecType::kAVC) ) {
        //TODO: Add logic to stop TrackEncoder
      }

    } else if (tracks[i].type == TrackType::kAudio) {
      //TODO: Add support for Audio.
    }
  }

  if(ret == NO_ERROR) {
      QMMF_INFO("%s:%s: session_id(%d) Rsumed Successfully!", TAG, __func__,
      session_id);
  }
  return ret;
  //TODO: Send session status callback to application.
  //propably not from here??
}

status_t RecorderImpl::CreateAudioTrack(const uint32_t session_id,
                                        uint32_t track_id,
                                        AudioTrackCreateParam& param) {
  if(!IsSessionIdValid(session_id)) {
    QMMF_ERROR("%s:%s: session_id is not valid!", TAG, __func__);
    return BAD_VALUE;
  }
}

status_t RecorderImpl::DeleteAudioTrack(const uint32_t session_id,
                                        const uint32_t track_id) {
  if(!IsSessionValid(session_id)) {
    QMMF_ERROR("%s:%s: session_id is not valid!", TAG, __func__);
    return BAD_VALUE;
  }
}

status_t RecorderImpl::CreateVideoTrack(const uint32_t session_id,
                                        uint32_t track_id,
                                        VideoTrackCreateParam& param) {

  QMMF_LEVEL1("%s:%s: Enter", TAG, __func__);

  if(!IsSessionIdValid(session_id)) {
    QMMF_ERROR("%s:%s: session_id is not valid!", TAG, __func__);
    return BAD_VALUE;
  }

  if(param.num_cameras > 1) {
    QMMF_ERROR("%s:%s: Multi Camera not supported!", TAG, __func__);
    return BAD_VALUE;
  }
  DebugVideoTrackCreateParam(__func__, &param);

  VideoTrackParams video_track_params;
  memset(&video_track_params, 0x0, sizeof video_track_params);
  video_track_params.track_id    = track_id;
  video_track_params.width       = param.width;
  video_track_params.height      = param.height;
  video_track_params.frame_rate  = param.frame_rate;
  video_track_params.codec_type  = param.codec_type;
  video_track_params.codec_param = param.codec_param;
  video_track_params.data_cb     = [&] (uint32_t track_id,
      std::vector<BnTrackBuffer> buffers, void *meta_param,
      TrackMetaParamType meta_type, size_t meta_size)
      { VideoTrackBufferCallback(track_id, buffers, meta_param, meta_type,
        meta_size);
      };
  //std::copy(param.camera_ids, param.camera_ids + param.num_cameras,
  //    video_track_params.camera_ids.begin());
  for(uint32_t i = 0; i < param.num_cameras; i++) {
    video_track_params.camera_ids.push_back(param.camera_ids[i]);
  }
  // TODO: define VideoOutDevices, and have switch case, for now assuming
  // 1 is encode, 2 is preview
  if(param.out_device == 1) {
    video_track_params.camera_stream_type = CameraStreamType::kVideo;
  } else if (param.out_device == 2) {
    video_track_params.camera_stream_type = CameraStreamType::kPreview;
  } else {
    QMMF_ERROR("%s:%s: out_device(%d) is not supported!", TAG, __func__,
               param.out_device);
    return BAD_VALUE;
  }

  // Create Camera track first.
  assert(camera_source_ != NULL);
  auto ret = camera_source_->CreateTrackSource(track_id, video_track_params);
  if(ret != NO_ERROR) {
    QMMF_ERROR("%s:%s: CreateTrackSource id(%d) failed!", TAG, __func__,
        track_id);
    return BAD_VALUE;
  }
  QMMF_INFO("%s:%s: TrackSource for track_id(%d) Added Successfully in "
      "CameraSource", TAG, __func__, track_id);

  // If video codec type is set to YUV then no need to create Encoder instance.
  // direct YUV frame will go to client.
  if ( (video_track_params.codec_type == VideoCodecType::kHEVC)
      || (video_track_params.codec_type == VideoCodecType::kAVC) ){

    // Create Encoder track and add TrackSource as a source to iit.
    // Track pipeline: TrackSource <--> TrackEncoder
    assert(encoder_core_ != NULL);
    ret = encoder_core_->AddSource(camera_source_->getTrackSource(track_id),
                                   video_track_params);
    if (ret != NO_ERROR) {
      QMMF_ERROR("%s:%s: track_id(%d) AddSource failed!", TAG, __func__,
          track_id);
      return BAD_VALUE;
    }
  }

  // Assosiate track to session.
  TrackInfo track_info;
  memset(&track_info, 0x0, sizeof track_info);
  track_info.track_id     = track_id;
  track_info.type         = TrackType::kVideo;
  track_info.params = video_track_params;

  Vector<TrackInfo> tracks;
  if (!sessions_.isEmpty()) {
    tracks = sessions_.valueFor(session_id);
  }
  tracks.add(track_info);
  // Update existing entry or add new one.
  // replaceValueFor() performs as add if entry doesn't exist.
  sessions_.add(session_id, tracks);

  QMMF_LEVEL1("%s:%s: Exit", TAG, __func__);
  return ret;
}

status_t RecorderImpl::DeleteVideoTrack(const uint32_t session_id,
                                        const uint32_t track_id) {

  QMMF_LEVEL2("%s:%s: Enter", TAG, __func__);

  if (!IsTrackValid(session_id, track_id)) {
    QMMF_ERROR("%s:%s: Session_id(%d):Track id(%d) is not valid!", TAG,
        __func__, session_id, track_id);
    return BAD_VALUE;
  }

  Vector<TrackInfo> tracks;
  tracks = sessions_.valueFor(session_id);

  TrackInfo track_info;
  memset(&track_info, 0x0, sizeof track_info);
  size_t size = tracks.size();
  size_t idx;
  for(size_t i = 0; i < size; i++) {
    if (track_id == tracks[i].track_id) {
      track_info = tracks[i];
      idx = i;
      break;
    }
  }
  assert(track_info.type == TrackType::kVideo);
  assert(camera_source_ != NULL);
  auto ret = camera_source_->DeleteTrackSource(track_id);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s:%s: track_id(%d) DeleteTrackSource failed!", TAG, __func__);
    return ret;
  }
  tracks.removeAt(idx);
  sessions_.replaceValueFor(session_id, tracks);

  QMMF_INFO("%s:%s: track_id(%d) Deleted Successfully :session_id(%d)",
      TAG, __func__, track_id, session_id);
  QMMF_INFO("%s:%s: Number of tracks(%d) left in session(%d)", TAG, __func__,
    tracks.size(), session_id);

  // This method doesn't go up to client as a callback, it is just to update
  // Internal data structure used for buffer mapping.
  remote_cb_->NotifyDeleteVideoTrack(track_id);

  QMMF_LEVEL2("%s:%s: Exit", TAG, __func__);
  return ret;
}

status_t RecorderImpl::ReturnTrackBuffer(const uint32_t session_id,
                                         const uint32_t track_id,
                                         std::vector<BnTrackBuffer> &buffers) {
  QMMF_LEVEL2("%s:%s: Enter", TAG, __func__);
  uint32_t ret = NO_ERROR;

  if (!IsTrackValid(session_id, track_id)) {
    QMMF_ERROR("%s:%s: Session_id(%d):Track id(%d) is not valid!", TAG,
        __func__, session_id, track_id);
    return BAD_VALUE;
  }

  Vector<TrackInfo> tracks;
  tracks = sessions_.valueFor(session_id);

  TrackInfo track_info;
  memset(&track_info, 0x0, sizeof track_info);
  size_t size = tracks.size();
  for(size_t i = 0; i < size; i++) {
    if (track_id == tracks[i].track_id) {
      track_info = tracks[i];
      break;
    }
  }

  QMMF_LEVEL1("%s:%s: buffers(%d) returned for session_id(%d), track_id(%d)",
      TAG, __func__, buffers.size(), session_id, track_id);
  for (size_t i = 0; i < buffers.size(); i++) {
    QMMF_LEVEL2("%s:%s: buffer[%d].ionfd=%d", TAG, __func__, i,
        buffers[i].ion_fd);
    QMMF_LEVEL2("%s:%s: buffer[%d].timestamp=%lld", TAG, __func__, i,
        buffers[i].timestamp);
  }

  if (track_info.type == TrackType::kVideo) {

    if ( (track_info.params.codec_type == VideoCodecType::kYUV) ||
         (track_info.params.codec_type == VideoCodecType::kBayerRDI) ||
         (track_info.params.codec_type == VideoCodecType::kBayerRDI) ) {
      // Return buffers back to camera source.
      assert(camera_source_ != NULL);
      ret = camera_source_->ReturnTrackBuffer(track_id, buffers);

    } else if ( (track_info.params.codec_type == VideoCodecType::kHEVC)
              || (track_info.params.codec_type == VideoCodecType::kAVC) ) {
      //Return buffers back to Encoder.
    }

  } else {
    //TODO:
    //handle Audio case.
  }
  QMMF_LEVEL2("%s:%s: Exit", TAG, __func__);
  return ret;
}

status_t RecorderImpl::SetAudioTrackParam(const uint32_t session_id,
                                          const uint32_t track_id,
                                          AudioTrackParamType type,
                                          void *param,
                                          size_t param_size) {

}

status_t RecorderImpl::SetVideoTrackParam(const uint32_t session_id,
                                          const uint32_t track_id,
                                          VideoTrackParamType type,
                                          void *param,
                                          size_t param_size) {

}

status_t RecorderImpl::CaptureImage(std::vector<uint32_t> camera_id,
                                    ImageParam &param) {

}

status_t RecorderImpl::CancelCaptureImage() {

}

status_t RecorderImpl::SetCameraParam(uint32_t camera_id,
                                      CameraParamType param_type,
                                      void *param,
                                      size_t param_size) {

}

status_t RecorderImpl::GetCameraParam(uint32_t camera_id,
                                      CameraParamType param_type,
                                      void *param,
                                      size_t param_size) {

}

status_t RecorderImpl::CreateOverlayObject(OverlayParam &param,
                                           uint32_t *overlay_id) {

}

status_t RecorderImpl::DeleteOverlayObject(const uint32_t overlay_id) {

}

status_t RecorderImpl::GetOverlayObjectParams(const uint32_t overlay_id,
                                              OverlayParam &param)
{

}

status_t RecorderImpl::UpdateOverlayObjectParams(const uint32_t overlay_id,
                                                 OverlayParam &param) {

}

status_t RecorderImpl::SetOverlayObject(const uint32_t session_id,
                                        const uint32_t track_id,
                                        const uint32_t overlay_id) {

}

status_t RecorderImpl::RemoveOverlayObject(const uint32_t session_id,
                                           const uint32_t track_id,
                                           const uint32_t overlay_id) {

}


void RecorderImpl::VideoTrackBufferCallback(uint32_t track_id,
                                            std::vector<BnTrackBuffer> buffers,
                                            void *meta_param,
                                            TrackMetaParamType meta_type,
                                            size_t meta_size) {

  assert(remote_cb_.get() != nullptr);
  remote_cb_->NotifyVideoTrackData(track_id, buffers, meta_param,
                                          meta_type, meta_size);
}

bool RecorderImpl::IsSessionIdValid(const uint32_t session_id) {

  bool valid = false;
  size_t size = session_ids_.size();
  for(size_t i = 0; i < size; i++) {
    if (session_id == session_ids_[i]) {
      valid = true;
      break;
    }
  }
  return valid;
}

bool RecorderImpl::IsSessionValid(const uint32_t session_id) {

  // there could be a case where application can try to start session
  // without adding valid track init, this function is to restrict this case.
  bool valid = false;
  if (IsSessionIdValid(session_id)) {
    size_t size = sessions_.size();
    QMMF_INFO("%s: Number of Session exist = %d",__func__, size);
    for(size_t i = 0; i < size; i++) {
      if (session_id == sessions_.keyAt(i)) {
          valid = true;
          break;
      }
    }
  }
  return valid;
}

bool RecorderImpl::IsTrackValid(const uint32_t session_id,
                                const uint32_t track_id) {

  bool valid = false;
  if(!IsSessionValid(session_id)) {
    return false;
  }

  Vector<TrackInfo> tracks;
  tracks = sessions_.valueFor(session_id);
  for(size_t i = 0; i < tracks.size(); i++) {
    if (track_id == tracks[i].track_id) {
      valid = true;
      break;
    }
  }
  return valid;
}

}; // namespace recorder

}; //namespace qmmf