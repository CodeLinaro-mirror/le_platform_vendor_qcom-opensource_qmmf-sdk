/*
* Copyright (c) 2016-2019, The Linux Foundation. All rights reserved.
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

/*! @file qmmf_camera_source.h
*/

#pragma once

#include <memory>
#include <mutex>

#include <camera/CameraMetadata.h>
#include <qmmf-sdk/qmmf_recorder_extra_param_tags.h>

#include "common/utils/qmmf_condition.h"
#include "common/cameraadaptor/qmmf_camera3_device_client.h"
#include "common/codecadaptor/src/qmmf_avcodec.h"
#include "recorder/src/service/qmmf_recorder_common.h"
#include "recorder/src/service/qmmf_camera_interface.h"
#include "recorder/src/service/qmmf_camera_context.h"
#include "recorder/src/service/qmmf_camera_rescaler.h"
#include "recorder/src/service/qmmf_camera_frc.h"

namespace qmmf {

using namespace cameraadaptor;
using namespace android;
using namespace overlay;
using namespace avcodec;

namespace recorder {

#define FPS_CHANGE_THRESHOLD  (0.5)
#define FRAME_SKIP_THRESHOLD_PERCENT (0.05)

class TrackSource;


/// @brief CameraSource class
/// Operates the Camera
class CameraSource {
 public:

  /// Create CameraSource Instance
  static CameraSource* CreateCameraSource();

  /// CameraSource Destructor
  ~CameraSource();

  /// Open Camera.
  status_t StartCamera(const uint32_t camera_id,
                       const float frame_rate,
                       const CameraExtraParam& extra_param,
                       const ResultCb &cb = nullptr,
                       const ErrorCb &errcb = nullptr);

  /// Close Camera.
  status_t StopCamera(const uint32_t camera_id);

  /// Add multiple Cameras
  status_t CreateMultiCamera(const std::vector<uint32_t> camera_ids,
                             uint32_t *virtual_camera_id);

  /// Configure multiple Cameras
  status_t ConfigureMultiCamera(const uint32_t virtual_camera_id,
                                const MultiCameraConfigType type,
                                const void *param,
                                const uint32_t param_size);

  /// Get number of supported Cameras.
  status_t GetNumberOfCameras(SupportedCameras &cameras);

  /// Get all supported plugins.
  status_t GetSupportedPlugins(SupportedPlugins *plugins);

  /// Create plugin
  status_t CreatePlugin(uint32_t *uid, const PluginInfo &plugin);

  /// Delete plugin
  status_t DeletePlugin(const uint32_t &uid);

  /// Configure plugin
  status_t ConfigPlugin(const uint32_t &uid, const std::string &json_config);

  /// Image Capture
  status_t CaptureImage(const uint32_t camera_id,
                        const ImageParam &param,
                        const uint32_t num_images,
                        const std::vector<CameraMetadata> &meta,
                        const SnapshotCb &cb);

  /// Configure Image Capture
  status_t ConfigImageCapture(const uint32_t camera_id,
                              const ImageConfigParam &config);

  /// Cancel Image Capture
  status_t CancelCaptureImage(const uint32_t camera_id);

  /// Return Image Capture buffer
  status_t ReturnImageCaptureBuffer(const uint32_t camera_id,
                           const int32_t buffer_id);

  /// Create Track Source
  status_t CreateTrackSource(const uint32_t track_id,
                             const VideoTrackParams& param);
  /// Delete Track Source
  status_t DeleteTrackSource(const uint32_t track_id);

  /// Start Track Source
  status_t StartTrackSource(const uint32_t track_id);

  /// Stop Track Source
  status_t StopTrackSource(const uint32_t track_id,
                           bool is_force_cleanup = false);

  /// Pause Track Source
  status_t PauseTrackSource(const uint32_t track_id);

  /// Resume Track Source
  status_t ResumeTrackSource(const uint32_t track_id);

  /// Return Track buffer
  status_t ReturnTrackBuffer(const uint32_t track_id,
                             std::vector<BnBuffer> &buffers);

  /// Set Camera configuration to Camera Interface
  status_t SetCameraParam(const uint32_t camera_id, const CameraMetadata &meta);

  /// Get Camera configuration to Camera Interface
  status_t GetCameraParam(const uint32_t camera_id, CameraMetadata &meta);

  /// Return default settings for Image Capture
  status_t GetDefaultCaptureParam(const uint32_t camera_id,
                                  CameraMetadata &meta);

  /// Return static metadata
  status_t GetCameraCharacteristics(const uint32_t camera_id,
                                    CameraMetadata &meta);

  /// UpdateTrackFrameRate
  status_t UpdateTrackFrameRate(const uint32_t track_id,
                                const float frame_rate);

  /// Enable repeating of frames to ensure target frame rate
  status_t EnableFrameRepeat(const uint32_t track_id,
                             const bool enable_frame_repeat);

  /// Create Overlay object
  status_t CreateOverlayObject(const uint32_t track_id,
                               OverlayParam *param,
                               uint32_t *overlay_id);

  /// Delete Overlay object parameters
  status_t DeleteOverlayObject(const uint32_t track_id,
                               const uint32_t overlay_id);

  /// Get Overlay object parameters
  status_t GetOverlayObjectParams(const uint32_t track_id,
                                  const uint32_t overlay_id,
                                  OverlayParam &param);

  /// Update Overlay object parameters
  status_t UpdateOverlayObjectParams(const uint32_t track_id,
                                     const uint32_t overlay_id,
                                     OverlayParam *param);

  /// Set Overlay object parameters
  status_t SetOverlayObject(const uint32_t track_id,
                            const uint32_t overlay_id);

  /// Remove Overlay object
  status_t RemoveOverlayObject(const uint32_t track_id,
                               const uint32_t overlay_id);

  /// Register Flush Callback
  status_t SetFlushCb(const uint32_t camera_id, FlushCb &cb);

  /// Clear Track input queue
  status_t FlushTrack(const uint32_t track_id);

  /// Return instance for track source for given ID
  const ::std::shared_ptr<TrackSource>& GetTrackSource(uint32_t track_id);

  /// Get Rescaller configuration parameters
  std::string GetRescalerConfig(const VideoTrackParams& track_params);

  /// @cond PRIVATE
 private:
  bool IsTrackIdValid(const uint32_t track_id);
  void SnapshotCallback(uint32_t count, StreamBuffer& buffer);
  uint32_t GetJpegSize(uint8_t *blobBuffer, uint32_t width);

  bool ValidateSlaveTrackParam(
    const VideoTrackParams& slave_track,
    const VideoTrackParams& master_track);

  bool CheckLinkedStream(
    const VideoTrackParams& slave_track,
    const VideoTrackParams& master_track);

  int32_t GetSourceTrackId(const VideoExtraParam& extra_param);

  status_t ParseThumb(uint8_t* vaddr, uint32_t size, StreamBuffer& buffer);

  status_t DetectCameras();

  VideoFormat GetYUVFormatType(VideoFormat format_tpye);

  bool IsFormatChanged(VideoFormat src_format_type,
                       VideoFormat dst_format_type);

  // Map of camera id and CameraContext.
  std::map<uint32_t, std::shared_ptr<CameraInterface>> camera_map_;

  // Map of track id and TrackSources.
  std::map<uint32_t, std::shared_ptr<TrackSource>> track_sources_;

  SnapshotCb client_snapshot_cb_;

  std::shared_ptr<PostProcFactory> factory_;

  SupportedCameras supported_cameras_;

  // Not allowed
  CameraSource();
  CameraSource(const CameraSource&);
  CameraSource& operator=(const CameraSource&);
  static CameraSource* instance_;
  std::map<int32_t, std::shared_ptr<CameraRescaler> > rescalers_;
  /// @endcond

};

/// @brief This class behaves as producer and consumer both, at one end
/// it takes YUV buffers from camera stream and another end it provides buffers
/// to Encoder, and manages buffer circulation, frame skip etc.
class TrackSource : public ICodecSource {
 public:

  /// TrackSource Constructor
  TrackSource(const VideoTrackParams& params,
              const std::shared_ptr<CameraInterface>& camera_intf);

  /// TrackSource Destructor
  ~TrackSource();

  /// Create stream and initialize additional processing
  status_t Init();

  /// Destroy stream and de-initialize additional processing
  status_t DeInit();

  /// Link track source with consumer and start additional processing
  status_t StartTrack();

  /// Unlink track source with consumer and stops additional processing
  status_t StopTrack(bool is_force_cleanup = false);

  // Methods of IInputCodecSource
  // This method to provide input buffer to Encoder.
  /// Provide input buffer to Encoder
  status_t GetBuffer(BufferDescriptor& buffer, void* client_data) override;

  // This method is used by Encoder to provide buffer back after encoding.
  /// Return buffer from Encoder
  status_t ReturnBuffer(BufferDescriptor& buffer, void* client_data) override;

  // This method is used by Encoder to notify stop.
  /// Used by Encoder to notify stop.
  status_t NotifyPortEvent(PortEventType event_type,
                           void* event_data) override;

  // Global track specific params can be query from TrackSource during its life
  // cycle.
  /// Get Track parameters
  VideoTrackParams& getParams() { return track_params_; }

  // This method to handle incoming buffers from producer, producer can be
  // anyone, Camera context's port or rescaler.
  /// Handle incoming buffers from producer
  void OnFrameAvailable(StreamBuffer& buffer);

  /// Return track buffers to producer
  status_t ReturnTrackBuffer(std::vector<BnBuffer>& buffers);

  /// Return true if current state is different then running
  bool IsStop();

  /// Return buffers to producer
  void ClearInputQueue();

  // Overlay Apis. TrackSource has instance of Overlay to deal with static
  // and dynamic types of overlay.

  /// Create Overlay object
  status_t CreateOverlayObject(OverlayParam *param, uint32_t *overlay_id);

  /// Delete Overlay object
  status_t DeleteOverlayObject(const uint32_t overlay_id);

  /// Get Overlay object parameters
  status_t GetOverlayObjectParams(const uint32_t overlay_id,
                                  OverlayParam &param);


  /// Update Overlay object parameters
  status_t UpdateOverlayObjectParams(const uint32_t overlay_id,
                                     OverlayParam *param);

  /// Set Overlay object parameters
  status_t SetOverlayObject(const uint32_t overlay_id);

  /// Remove Overlay objects
  status_t RemoveOverlayObject(const uint32_t overlay_id);

  /// Change frame rate
  void UpdateFrameRate(const float frame_rate);

  /// Enable frame repeat
  void EnableFrameRepeat(const bool enable_frame_repeat);

  /// Callback to handle returned buffers
  void NotifyBufferReturned(StreamBuffer& buffer);

  //status_t GetStreamParam(CameraStreamParam& stream_param);

  /// Sets source track and enable track duplication
  status_t InitCopy(std::shared_ptr<TrackSource> track_source,
                    const std::shared_ptr<CameraRescaler>& rescaler,
                    int32_t port_track_id,
                    int32_t track_id_master);

  /// Return connected Camera port
  bool IsConnectedToCameraPort() { return connected_tocamera_port_;};

  /// Return if the source of this track is another track
  bool IsSlaveTrack() {return slave_track_source_; };

  /// Return master Track ID
  int32_t GetMasterTrackId();

  /// Return port Track ID
  int32_t GetCameraPortId();

  /// Add track source Consumer
  status_t AddConsumer(const sp<IBufferConsumer>& consumer);

  /// Remove track source Consumer
  status_t RemoveConsumer(sp<IBufferConsumer>& consumer);

  /// @cond PRIVATE
 private:

  // Method to provide consumer interface, it would be used by producer to
  // post buffers.
  sp<IBufferConsumer>& GetConsumerIntf() { return buffer_consumer_impl_; }

  void PushFrameToQueue(StreamBuffer& buffer);

  uint32_t TrackId() { return track_params_.track_id; }

  bool IsEnableFrameSkip();

  bool IsFrameSkip();

  uint32_t CalculateEncodesPerFrame();

#ifdef ENABLE_FRAME_DUMP
  status_t DumpYUV(StreamBuffer& buffer);
#endif

  void ReturnBufferToProducer(StreamBuffer& buffer);

  bool IsNeedScaler(const VideoTrackParams& slave_track,
                    const VideoTrackParams& master_track);

  uint64_t GetWaitTime();

  VideoTrackParams         track_params_;
  sp<IBufferConsumer>      buffer_consumer_impl_;
  bool                     is_stop_;
  std::mutex               stop_lock_;
  bool                     eos_acked_;
  std::mutex               eos_lock_;

  std::mutex               frame_lock_;
  QCondition               wait_for_frame_;

  std::mutex               lock_;

  // will be used till we make stop api as async.
  bool                     is_idle_;
  std::mutex               idle_lock_;
  QCondition               wait_for_idle_;

  // Maps of Unique buffer Id and Buffer.
  std::map<uint32_t, StreamBuffer> buffer_list_;

  // Maps AVCodec and TrackSource description of image buffers
  std::unordered_map <void*, IBufferHandle> buffers_map_;

  std::mutex buffer_list_lock_;

  // Input buffer list, to feed buffers to encoder.
  TSQueue<StreamBuffer> frames_received_;

  // List of buffers held by encoder.
  TSQueue<StreamBuffer> frames_being_encoded_;

  std::shared_ptr<CameraInterface>   camera_interface_;

  Overlay  overlay_;
  uint32_t active_overlays_;

  float   input_frame_rate_;
  double  input_frame_interval_;
  double  output_frame_interval_;
  double  remaining_frame_skip_time_;
  std::mutex frame_skip_lock_;

  uint32_t debug_fps_;
  struct timeval input_prevtv_;
  uint32_t input_count_;
  struct timeval prevtv_;
  uint32_t count_;

  float      pending_encodes_per_frame_ratio_;
  uint64_t   frame_repeat_ts_prev_;
  uint64_t   frame_repeat_ts_curr_;
  bool       enable_frame_repeat_;
  std::mutex frame_repeat_lock_;
  std::shared_ptr<FrameRateController> fsc_;
  std::shared_ptr<FrameRateController> frc_;
  std::shared_ptr<CameraRescaler>  rescaler_;

  bool  connected_tocamera_port_;
  bool  slave_track_source_;

  int32_t track_id_master_;
  int32_t port_track_id_;

  sp<IBufferProducer>    buffer_producer_impl_;
  std::mutex             consumer_lock_;
  std::shared_ptr<TrackSource> master_track_;

  std::map<IBufferHandle, uint32_t >  buffer_map_;
  std::map<IBufferHandle, StreamBuffer > stream_buffer_map_;

  bool time_lapse_mode_;
  uint32_t time_lapse_interval_;
  uint64_t time_stamp_;

  uint32_t num_consumers_;

  int32_t rotation_;
  uint64_t wait_duration_;
  static const uint32_t kWaitNumFrames_;
  /// @endcond
};

}; //namespace recorder

}; //namespace qmmf
