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

#pragma once

#include <atomic>
#include <camera/CameraMetadata.h>
#include <camera/VendorTagDescriptor.h>

#include "recorder/src/client/qmmf_recorder_service_intf.h"
#include "recorder/src/service/qmmf_recorder_impl.h"

namespace qmmf {

namespace recorder {

using namespace android;

class RecorderService : public BnInterface<IRecorderService> {
 public:
  RecorderService();

  ~RecorderService();

 private:

  typedef std::function <void(void)> NotifyClientDeath;
  class DeathNotifier : public IBinder::DeathRecipient {
   public:
    DeathNotifier() {}

    void SetDeathNotifyCB(NotifyClientDeath& cb) {
      notify_client_death_ = cb;
    }
    void binderDied(const wp<IBinder>&) override {
      QMMF_WARN("RecorderSerive:%s: Client Exited or Died!", __func__);
      notify_client_death_();
    }
    NotifyClientDeath notify_client_death_;
  };

  friend class DeathNotifier;

  // Method of BnInterface<IRecorderService>.
  // This method would get call to handle incoming messages from clients.
  status_t onTransact(uint32_t code, const Parcel& data,
                               Parcel* reply, uint32_t flags = 0) override;

  status_t Connect(const sp<IRecorderServiceCallback>& service_cb,
                   uint32_t* client_id) override;

  status_t Disconnect(const uint32_t client_id) override;

  status_t StartCamera(const uint32_t client_id, const uint32_t camera_id,
                       const float frame_rate,
                       const CameraExtraParam& extra_param,
                       bool enable_result_cb = false) override;

  status_t StopCamera(const uint32_t client_id,
                      const uint32_t camera_id) override;

  status_t CreateSession(const uint32_t client_id,
                         uint32_t *session_id) override;

  status_t DeleteSession(const uint32_t client_id,
                         const uint32_t session_id) override;

  status_t StartSession(const uint32_t client_id,
                        const uint32_t session_id) override;

  status_t StopSession(const uint32_t client_id,
                       const uint32_t session_id, bool do_flush) override;

  status_t PauseSession(const uint32_t client_id,
                        const uint32_t session_id) override;

  status_t ResumeSession(const uint32_t client_id,
                         const uint32_t session_id) override;

  status_t GetNumberOfCameras(const uint32_t client_id,
                              SupportedCameras &cameras) override;

  status_t CreateAudioTrack(const uint32_t client_id,
                            const uint32_t session_id,
                            const uint32_t track_id,
                            const AudioTrackCreateParam& param) override;

  status_t CreateVideoTrack(const uint32_t client_id,
                            const uint32_t session_id,
                            const uint32_t track_id,
                            const VideoTrackCreateParam& param) override;

  status_t CreateVideoTrack(const uint32_t client_id,
                            const uint32_t session_id,
                            const uint32_t track_id,
                            const VideoTrackCreateParam& param,
                            const VideoExtraParam& extra_param) override;

  status_t DeleteAudioTrack(const uint32_t client_id,
                            const uint32_t session_id,
                            const uint32_t track_id) override;

  status_t DeleteVideoTrack(const uint32_t client_id,
                            const uint32_t session_id,
                            const uint32_t track_id) override;

  status_t ReturnTrackBuffer(const uint32_t client_id,
                             const uint32_t session_id,
                             const uint32_t track_id,
                             std::vector<BnBuffer> &buffers) override;

  status_t SetAudioTrackParam(const uint32_t client_id,
                              const uint32_t session_id,
                              const uint32_t track_id,
                              CodecParamType type,
                              void *param,
                              size_t param_size) override;

  status_t SetVideoTrackParam(const uint32_t client_id,
                              const uint32_t session_id,
                              const uint32_t track_id,
                              CodecParamType type,
                              void *param,
                              size_t param_size) override;

  status_t CaptureImage(const uint32_t client_id,
                        const uint32_t camera_id,
                        const uint32_t num_images,
                        const std::vector<CameraMetadata> &meta) override;

  status_t ConfigImageCapture(const uint32_t client_id,
                              const uint32_t camera_id,
                              const ImageParam &param,
                              const ImageConfigParam &config) override;

  status_t CancelCaptureImage(const uint32_t client_id,
                              const uint32_t camera_id) override;

  status_t ReturnImageCaptureBuffer(const uint32_t client_id,
                                    const uint32_t camera_id,
                                    const int32_t  buffer_id) override;

  status_t SetCameraParam(const uint32_t client_id,
                          const uint32_t camera_id,
                          const CameraMetadata &meta) override;

  status_t GetCameraParam(const uint32_t client_id,
                          const uint32_t camera_id,
                          CameraMetadata &meta) override;

  status_t GetDefaultCaptureParam(const uint32_t client_id,
                                  const uint32_t camera_id,
                                  CameraMetadata &meta) override;

  status_t GetCameraCharacteristics(const uint32_t client_id,
                                    const uint32_t camera_id,
                                    CameraMetadata &meta) override;

  void ClientDeathHandler(const uint32_t client_id);

  bool IsRecorderInitialized();

  status_t DisconnectInternal(const uint32_t client_id);

  status_t GetVendorTagDescriptor(sp<VendorTagDescriptor> &desc) override;

  std::unique_ptr<RecorderImpl>           recorder_;

  // Map of client ids and their death notifiers.
  std::map<uint32_t, sp<DeathNotifier> >  death_notifier_list_;
  // Map of client ids and their callback handlers.
  std::map<uint32_t, sp<RemoteCallBack> > remote_cb_list_;

  uint32_t                     unique_client_id_;
  std::mutex                   lock_;
};

}; //namespace qmmf

}; //namespace recorder
