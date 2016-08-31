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

#include <utils/Errors.h>
#include <utils/Mutex.h>
#include <utils/RefBase.h>
#include <utils/KeyedVector.h>
#include <camera/CameraMetadata.h>
#include <map>

#include "qmmf-sdk/qmmf_recorder_params.h"
#include "recorder/src/client/qmmf_recorder_client_ion.h"
#include "recorder/src/client/qmmf_recorder_service_intf.h"

namespace qmmf {

namespace recorder {

using namespace android;

class RecorderClient {
 public:
  RecorderClient();

  ~RecorderClient();

  status_t Connect(const RecorderCb& cb);

  status_t Disconnect();

  status_t StartCamera(const uint32_t camera_id,
                       const CameraStartParam &param);

  status_t StopCamera(const uint32_t camera_id);

  status_t CreateSession(const SessionCb& cb, uint32_t* session_id);

  status_t DeleteSession(const uint32_t session_id);

  status_t StartSession(const uint32_t session_id);

  status_t StopSession(const uint32_t session_id, bool do_flush);

  status_t PauseSession(const uint32_t session_id);

  status_t ResumeSession(const uint32_t session_id);

  status_t CreateAudioTrack(const uint32_t session_id, const uint32_t track_id,
                            const AudioTrackCreateParam& param,
                            const TrackCb& cb);

  status_t CreateVideoTrack(const uint32_t session_id, const uint32_t track_id,
                            const VideoTrackCreateParam& param,
                            const TrackCb& cb);

  status_t ReturnTrackBuffer(const uint32_t session_id,
                             const uint32_t track_id,
                             std::vector<BufferDescriptor> &buffers);

  status_t SetAudioTrackParam(const uint32_t session_id,
                              const uint32_t track_id,
                              AudioTrackParamType type, const void *param,
                              size_t param_size);

  status_t SetVideoTrackParam(const uint32_t session_id,
                              const uint32_t track_id,
                              VideoTrackParamType type, const void *param,
                              size_t param_size);

  status_t DeleteAudioTrack(const uint32_t session_id,
                            const uint32_t track_id);

  status_t DeleteVideoTrack(const uint32_t session_id,
                            const uint32_t track_id);

  status_t CaptureImage(const uint32_t camera_id,
                        const ImageParam &param,
                        const uint32_t num_images,
                        const std::vector<CameraMetadata> &meta,
                        const ImageCaptureCb& cb);

  status_t ConfigImageCapture(const uint32_t camera_id,
                              const ImageCaptureConfig &config);

  status_t CancelCaptureImage();

  status_t ReturnImageCaptureBuffer(const uint32_t camera_id,
                                    const BufferDescriptor &buffer);

  status_t SetCameraParam(const uint32_t camera_id, const CameraMetadata &meta);

  status_t GetCameraParam(const uint32_t camera_id, CameraMetadata &meta);

  status_t GetDefaultCaptureParam(const uint32_t camera_id,
                                  CameraMetadata &meta);

  status_t CreateOverlayObject(const uint32_t track_id,
                               const OverlayParam &param,
                               uint32_t *overlay_id);

  status_t DeleteOverlayObject(const uint32_t track_id,
                               const uint32_t overlay_id);

  status_t GetOverlayObjectParams(const uint32_t track_id,
                                  const uint32_t overlay_id,
                                  OverlayParam &param);

  status_t UpdateOverlayObjectParams(const uint32_t track_id,
                                     const uint32_t overlay_id,
                                     const OverlayParam &param);

  status_t SetOverlay(const uint32_t track_id, const uint32_t overlay_id);

  status_t RemoveOverlay(const uint32_t track_id, const uint32_t overlay_id);

  // Callback handlers from service.ap
  void NotifyRecorderEvent(EventType event_type, void *event_data,
                           size_t event_data_size);

  void NotifySessionEvent(EventType event_type, void *event_data,
                          size_t event_data_size);

  void NotifySnapshotData(uint32_t camera_id, uint32_t image_sequence_count,
                          BnBuffer& buffer);

  void NotifyVideoTrackData(uint32_t track_id,
                            std::vector<BnBuffer> &bn_buffers,
                            void *meta_param,
                            TrackMetaParamType meta_type,
                            size_t meta_size);

  void NotifyVideoTrackEvent(uint32_t track_id,
                             EventType event_type,
                             void *event_data,
                             size_t event_data_size);

  void NotifyAudioTrackData(uint32_t track_id,
                            const std::vector<BnBuffer> &buffers,
                            void *meta_param,
                            TrackMetaParamType meta_type,
                            size_t meta_size);

  void NotifyAudioTrackEvent(uint32_t track_id,
                             EventType event_type,
                             void *event_data,
                             size_t event_data_size);
 private:

  void UpdateSessionTopology(const uint32_t session_id, const uint32_t track_id,
                             bool /*Add or Delete*/);

  bool CheckServiceStatus();

  class DeathNotifier : public IBinder::DeathRecipient {
   public:
    DeathNotifier(RecorderClient* parent) : parent_(parent) {}

    void binderDied(const wp<IBinder>&) override {
          ALOGD("RecorderClient:%s: Recorder service died", __func__);

          Mutex::Autolock l(parent_->lock_);
          parent_->recorder_service_.clear();
          parent_->recorder_service_ = NULL;
    }
    RecorderClient* parent_;
  };
  friend class DeathNotifier;

  Mutex                lock_;
  sp<IRecorderService> recorder_service_;
  sp<DeathNotifier>    death_notifier_;
  RecorderCb           recorder_cb_;
  int32_t              ion_device_;
  RecorderClientIon    buffer_ion_;

  // List of session callbacks.
  DefaultKeyedVector<uint32_t, SessionCb > session_cb_list_;
  // List of Track callbacks.
  DefaultKeyedVector<uint32_t, TrackCb >   track_cb_list_;
  // Capture callback.
  ImageCaptureCb                           image_capture_cb_;
  //CaptureImageCb                           image_capture_cb_;

  typedef struct BufInfo {
    // Transferred ION Id.
    uint32_t ion_fd;
    // Memory mapped buffer.
    void    *pointer;
    // Size
    size_t  frame_len;
  } BufInfo;

  // map <session id, vector<track id> >
  DefaultKeyedVector<uint32_t, Vector<uint32_t> >  sessions_;
  // map <buffer index, buffer_info>
  typedef DefaultKeyedVector<uint32_t, BufInfo> buf_info_map;
  // map <track_id, map <buffer index, buffer_info> >
  DefaultKeyedVector<uint32_t,  buf_info_map> track_buf_map_;

  // This map keeps track of incoming BnBuffers from service.
  // and used to return buffers back to service.
  // <track_id, vector<BnBuffer>>
  DefaultKeyedVector<uint32_t, Vector<BnBuffer> > track_in_buffers_;
  // Lock to protect track_in_buffers_.
  Mutex list_lock_;
};

class ServiceCallbackHandler : public BnRecorderServiceCallback {
 public:

  ServiceCallbackHandler(RecorderClient* client);

  ~ServiceCallbackHandler();

 private:
  //Methods of BnRecorderServiceCallback.
  void NotifyRecorderEvent(EventType event_type, void *event_data,
                           size_t event_data_size) override;

  void NotifySessionEvent(EventType event_type, void *event_data,
                          size_t event_data_size) override;

  void NotifySnapshotData(uint32_t camera_id, uint32_t image_sequence_count,
                          BnBuffer& buffer) override;

  void NotifyVideoTrackData(uint32_t track_id,
                            std::vector<BnBuffer> &buffers,
                            void *meta_param,
                            TrackMetaParamType meta_type,
                            size_t meta_size) override;

  void NotifyVideoTrackEvent(uint32_t track_id, EventType event_type,
                             void *event_data,
                             size_t event_data_size) override;

  void NotifyAudioTrackData(uint32_t track_id,
                            const std::vector<BnBuffer> &buffers,
                            void *meta_param,
                            TrackMetaParamType meta_type,
                            size_t meta_size) override;

  void NotifyAudioTrackEvent(uint32_t track_id, EventType event_type,
                             void *event_data,
                             size_t event_data_size) override;

  RecorderClient *client_;
};


}; // namespace qmmf

}; // namespace recorder.
