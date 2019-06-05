/*
* Copyright (c) 2016, 2019, The Linux Foundation. All rights reserved.
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

#include <vector>
#include <set>
#include <map>
#include <mutex>

#include <camera/CameraMetadata.h>
#include <qmmf-sdk/qmmf_recorder_params.h>
#include <qmmf-sdk/qmmf_recorder_extra_param.h>

#include <ion/ion.h>
#include <linux/dma-buf.h>

#include "common/utils/qmmf_log.h"
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
                       const CameraStartParam &param,
                       const CameraResultCb &result_cb = nullptr);

  status_t StopCamera(const uint32_t camera_id);

  status_t CreateSession(const SessionCb& cb, uint32_t* session_id);

  status_t DeleteSession(const uint32_t session_id);

  status_t StartSession(const uint32_t session_id);

  status_t StopSession(const uint32_t session_id, bool do_flush);

  status_t PauseSession(const uint32_t session_id);

  status_t ResumeSession(const uint32_t session_id);

  status_t GetNumberOfCameras(SupportedCameras &cameras);

  status_t GetSupportedPlugins(SupportedPlugins *plugins);

  status_t CreatePlugin(uint32_t *uid, const PluginInfo &plugin);

  status_t DeletePlugin(const uint32_t &uid);

  status_t ConfigPlugin(const uint32_t &uid, const std::string &json_config);

  status_t CreateAudioTrack(const uint32_t session_id, const uint32_t track_id,
                            const AudioTrackCreateParam& param,
                            const TrackCb& cb);

  status_t CreateVideoTrack(const uint32_t session_id, const uint32_t track_id,
                            const VideoTrackCreateParam& param,
                            const TrackCb& cb);

  status_t CreateVideoTrack(const uint32_t session_id, const uint32_t track_id,
                            const VideoTrackCreateParam& param,
                            const VideoExtraParam& extra_param,
                            const TrackCb& cb);

  status_t ReturnTrackBuffer(const uint32_t session_id,
                             const uint32_t track_id,
                             std::vector<BufferDescriptor> &buffers);

  status_t SetAudioTrackParam(const uint32_t session_id,
                              const uint32_t track_id,
                              CodecParamType type, const void *param,
                              size_t param_size);

  status_t SetVideoTrackParam(const uint32_t session_id,
                              const uint32_t track_id,
                              CodecParamType type, const void *param,
                              size_t param_size);

  status_t DeleteAudioTrack(const uint32_t session_id,
                            const uint32_t track_id);

  status_t DeleteVideoTrack(const uint32_t session_id,
                            const uint32_t track_id);

  status_t CaptureImage(const uint32_t camera_id,
                        const ImageParam &param,
                        const uint32_t num_images,
                        const std::vector<CameraMetadata> &meta,
                        const ImageCaptureCb &cb);

  status_t ConfigImageCapture(const uint32_t camera_id,
                              const ImageConfigParam &config);

  status_t CancelCaptureImage(const uint32_t camera_id);

  status_t ReturnImageCaptureBuffer(const uint32_t camera_id,
                                    const BufferDescriptor &buffer);

  status_t SetCameraParam(const uint32_t camera_id, const CameraMetadata &meta);

  status_t GetCameraParam(const uint32_t camera_id, CameraMetadata &meta);

  status_t GetDefaultCaptureParam(const uint32_t camera_id,
                                  CameraMetadata &meta);

  status_t GetCameraCharacteristics(const uint32_t camera_id,
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

  status_t CreateMultiCamera(const std::vector<uint32_t> camera_ids,
                             uint32_t *virtual_camera_id);

  status_t ConfigureMultiCamera(const uint32_t virtual_camera_id,
                                const MultiCameraConfigType type,
                                const void *param,
                                const uint32_t param_size);

  status_t GetVendorTagDescriptor(sp<VendorTagDescriptor> &desc);

  // Callback handlers from service.ap
  void NotifyRecorderEvent(EventType event_type, void *event_data,
                           size_t event_data_size);

  void NotifySessionEvent(EventType event_type, void *event_data,
                          size_t event_data_size);

  void NotifySnapshotData(uint32_t camera_id, uint32_t image_sequence_count,
                          BnBuffer& buffer, MetaData& meta_data);

  void NotifyVideoTrackData(uint32_t track_id,
                            std::vector<BnBuffer>& bn_buffers,
                            std::vector<MetaData>& meta_buffers);

  void NotifyVideoTrackEvent(uint32_t track_id,
                             EventType event_type,
                             void *event_data,
                             size_t event_data_size);

  void NotifyAudioTrackData(uint32_t track_id,
                            const std::vector<BnBuffer>& buffers,
                            const std::vector<MetaData>& meta_buffers);

  void NotifyAudioTrackEvent(uint32_t track_id,
                             EventType event_type,
                             void *event_data,
                             size_t event_data_size);

  void NotifyCameraResult(uint32_t camera_id,
                          const CameraMetadata &result);

 private:
  typedef std::function <void(void)> NotifyServerDeathCB;

  class DeathNotifier : public IBinder::DeathRecipient {
   public:
    DeathNotifier(NotifyServerDeathCB& cb) : notify_server_death_(cb) {}

    void binderDied(const wp<IBinder>&) override {
      QMMF_DEBUG("RecorderClient:%s: Recorder service died", __func__);
      notify_server_death_();
    }
    NotifyServerDeathCB notify_server_death_;
  };

  struct BufferInfo {
    uint32_t          ion_fd;     // Transferred ION Id.
    size_t            size;       // Buffer length/size.
    void*             vaddr;      // Memory mapped buffer.
  };

  // Map <buffer index, buffer info>
  typedef std::map<uint32_t, BufferInfo> BufferInfoMap;

  status_t MapBuffer(BufferInfo& info);
  status_t UnmapBuffer(BufferInfo& info);

  void UpdateSessionTopology(const uint32_t& session_id,
                             const uint32_t& track_id,
                             bool /*Add or Delete*/);

  bool CheckServiceStatus();

  void ServiceDeathHandler();

  sp<IRecorderService>              recorder_service_;
  sp<DeathNotifier>                 death_notifier_;

  RecorderClientIon                 buffer_ion_;
  int32_t                           ion_device_;
  uint32_t                          client_id_;

  // List track IDs in a session.
  std::map<uint32_t, std::set<uint32_t> > sessions_;

  // List of session callbacks.
  std::map<uint32_t, SessionCb >    session_cb_list_;

  // List of track callbacks.
  std::map<uint32_t, TrackCb>       track_cb_list_;
  std::mutex                        track_cb_lock_;

  RecorderCb                        recorder_cb_;
  ImageCaptureCb                    image_capture_cb_;
  CameraResultCb                    metadata_cb_;

  // List of information regarding the buffers in a track.
  std::map<uint32_t, BufferInfoMap> track_buffers_map_;
  std::mutex                        track_buffers_lock_;

  // List of information regarding the buffers for image capture.
  BufferInfoMap                     snapshot_buffers_;
  std::mutex                        snapshot_buffers_lock_;

  // VendorTagDescriptor
  sp<VendorTagDescriptor>           vendor_tag_desc_;

  // Global mutex.
  std::mutex                        lock_;
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
                          BnBuffer& buffer, MetaData& meta_data) override;

  void NotifyVideoTrackData(uint32_t track_id,
                            std::vector<BnBuffer>& buffers,
                            std::vector<MetaData>& meta_buffers) override;

  void NotifyVideoTrackEvent(uint32_t track_id, EventType event_type,
                             void *event_data,
                             size_t event_data_size) override;

  void NotifyAudioTrackData(uint32_t track_id,
                            const std::vector<BnBuffer>& buffers,
                            const std::vector<MetaData>& meta_buffers) override;

  void NotifyAudioTrackEvent(uint32_t track_id, EventType event_type,
                             void *event_data,
                             size_t event_data_size) override;

  void NotifyCameraResult(uint32_t camera_id,
                          const CameraMetadata &result) override;

  RecorderClient *client_;
};


}; // namespace qmmf

}; // namespace recorder.
