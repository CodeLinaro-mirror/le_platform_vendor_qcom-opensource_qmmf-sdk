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
#include <utils/Condition.h>

#include "recorder/src/service/qmmf_recorder_common.h"
#include "recorder/src/service/qmmf_camera_context.h"
#include "common/cameraadaptor/qmmf_camera3_device_client.h"
#include "common/codecadaptor/src/qmmf_avcodec.h"

namespace qmmf {

using namespace cameraadaptor;
using namespace android;

namespace recorder {

class TrackSource;

class CameraSource {
 public:

  static CameraSource* CreateCameraSource();

  ~CameraSource();

  status_t StartCamera(std::vector<uint32_t> camera_ids,
                       CameraStartParam &param);

  status_t StopCamera(std::vector<uint32_t> camera_ids);

  status_t CaptureImage(std::vector<uint32_t> camera_id,
                        ImageParam &param, const CaptureImageCb& cb);

  status_t CancelCaptureImage();

  status_t CreateTrackSource(const uint32_t track_id, VideoTrackParams& param);

  status_t DeleteTrackSource(const uint32_t track_id);

  status_t StartTrackSource(const uint32_t track_id);

  status_t StopTrackSource(const uint32_t track_id);

  status_t PauseTrackSource(const uint32_t track_id);

  status_t ResumeTrackSource(const uint32_t track_id);

  status_t ReturnTrackBuffer(const uint32_t track_id,
                             std::vector<BnTrackBuffer> &buffers);

  status_t SetCameraParam(uint32_t camera_id, CameraMetadata &meta);

  status_t GetCameraParam(uint32_t camera_id, CameraMetadata &meta);

  status_t CreateOverlayObject(OverlayParam &param, uint32_t *overlay_id);

  status_t DeleteOverlayObject(const uint32_t overlay_id);

  status_t GetOverlayObjectParams(const uint32_t overlay_id,
                                  OverlayParam &param);

  status_t UpdateOverlayObjectParams(const uint32_t overlay_id,
                                     OverlayParam &param);

  status_t SetOverlayObject(const uint32_t track_id, const uint32_t overlay_id);

  status_t RemoveOverlayObject(const uint32_t track_id,
                               const uint32_t overlay_id);

  const sp<TrackSource>& getTrackSource(uint32_t track_id);

 private:

  bool IsTrackIdValid(const uint32_t track_id);

  // Map of camera id and CameraContext.
  DefaultKeyedVector<uint32_t, sp<CameraContext> > camera_contexts_;

  // Map of track it and TrackSources.
  DefaultKeyedVector<uint32_t, sp<TrackSource> > track_sources_;

  // Not allowed
  CameraSource();
  CameraSource(const CameraSource&);
  CameraSource& operator=(const CameraSource&);
  static CameraSource* instance_;
};

// This class is behaves as producer and consumer both, at one end it takes
// YUV buffers from camera stream and another end it provides buffers to
// Encoder, and manages buffer circulation, skip etc.
class TrackSource : public IInputCodecSource {
 public:
  TrackSource(VideoTrackParams& params, sp<CameraContext>& context);

  ~TrackSource();

  // Methods of IInputCodecSource
  // This method to provide input buffer to Encoder.
  status_t Read(StreamBuffer& buffer) override;

  // This method is used by Encoder to provide buffer back after encoding.
  status_t SignalBufferReturned(StreamBuffer& buffer) override;

  // This method is used by Encoder to notify stop.
  status_t Stop() override;

  // Global track specific params can be query from TrackSource during its life
  // cycle.
  VideoTrackParams& getParams() { return track_params_; }

  // This method to handle incoming buffers from producer, producer can be
  // anyone, Camera context's port or rescaler.
  void OnFrameAvailable(StreamBuffer& buffer);

  status_t ReturnTrackBuffer(std::vector<BnTrackBuffer>& buffers);

  // Method to provide consumer interface, it would be used by producer to
  // post buffers.
  sp<IBufferConsumer>& GetConsumerIntf() { return buffer_consumer_impl_; }

  status_t StartTrack();

  status_t StopTrack();

  bool IsStop();

  void ClearInputQueue();

 private:

  void PushFrameToQueue(StreamBuffer& buffer);

  uint32_t TrackId() { return track_params_.track_id; }

  static const nsecs_t kWaitDuration = 5e9; // 5 sec.

  VideoTrackParams    track_params_;
  sp<IBufferConsumer> buffer_consumer_impl_;
  Condition           wait_for_frame_;
  Mutex               lock_;
  bool                is_stop_;
  Mutex               stop_lock_;

  // Maps of Unique buffer Id and Buffer.
  DefaultKeyedVector<uint32_t, StreamBuffer> buffer_list_;

  // Input buffer list, to feed buffers to encoder.
  TSQueue<StreamBuffer> frames_received_;

  // List of buffers held by encoder.
  TSQueue<StreamBuffer> frames_being_encoded_;

  sp<CameraContext>     context_;

#ifdef DEBUG_TRACK_FPS
  struct timeval prevtv_;;
  uint32_t count_;;
#endif
};

}; //namespace recorder

}; //namespace qmmf
