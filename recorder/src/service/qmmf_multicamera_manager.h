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

#pragma once

#include <queue>
#include <map>
#include <set>
#include <future>
#include <mutex>

#include <qmmf-sdk/qmmf_recorder_extra_param.h>
#include <qmmf-sdk/qmmf_recorder_extra_param_tags.h>

#include "common/utils/qmmf_condition.h"
#include "recorder/src/service/qmmf_camera_context.h"
#include "recorder/src/service/qmmf_recorder_utils.h"
#include "recorder/src/service/qmmf_recorder_common.h"
#include "recorder/src/service/qmmf_camera_reprocess.h"

namespace qmmf {

namespace recorder {

static const uint32_t kVirtualCameraIdOffset = 1000;

class StreamStitching;
class SnapshotStitching;

class MultiCameraManager : public CameraInterface {
 public:
  MultiCameraManager();

  ~MultiCameraManager();

  // This Api will map actaul camera Ids to virtual camera id.
  status_t CreateMultiCamera(const std::vector<uint32_t> camera_ids,
                             uint32_t* virtual_camera_id);

  status_t ConfigureMultiCamera(const uint32_t virtual_camera_id,
                                const MultiCameraConfigType type,
                                const void *param, const size_t param_size);

  status_t OpenCamera(const uint32_t camera_id, const CameraStartParam &param,
                      const ResultCb &cb = nullptr,
                      const ErrorCb &errcb = nullptr) override;

  status_t CloseCamera(const uint32_t camera_id) override;

  status_t WaitAecToConverge(const uint32_t timeout) override;

  status_t SetUpCapture(const SnapshotParam& param,
                        const uint32_t num_images) override;

  status_t CaptureImage(const std::vector<CameraMetadata> &meta,
                        const StreamSnapshotCb& cb) override;

  status_t ConfigImageCapture(const ImageConfigParam &config) override;

  status_t CancelCaptureImage() override;

  status_t CreateStream(const StreamParam& param,
                        const VideoExtraParam& extra_param) override;

  status_t DeleteStream(const uint32_t track_id) override;

  status_t AddConsumer(const uint32_t& track_id,
                       sp<IBufferConsumer>& consumer) override;

  status_t RemoveConsumer(const uint32_t& track_id,
                          sp<IBufferConsumer>& consumer) override;

  status_t StartStream(const uint32_t track_id) override;

  status_t StopStream(const uint32_t track_id) override;

  status_t SetCameraParam(const CameraMetadata &meta) override;

  status_t GetCameraParam(CameraMetadata &meta) override;

  status_t GetDefaultCaptureParam(CameraMetadata &meta) override;

  status_t ReturnImageCaptureBuffer(const uint32_t camera_id,
                                    const int32_t buffer_id) override;

  std::vector<int32_t>& GetSupportedFps() override;

  void SetFlushCb(FlushCb &cb) override;

 private:
  friend class StitchingBase;
  friend class StreamStitching;
  friend class SnapshotStitching;

  void ResultCallback(uint32_t camera_id, const CameraMetadata &meta);

  status_t SetDefaultSurfaceDim(uint32_t& w, uint32_t& h);

  void OnStitchedFrameAvailable(StreamBuffer buffer);

  // Create Stitching stream is identified with param.id, make sure
  // that same id is passed on DeleteStreamStitching
  status_t CreateStreamStitching(const StreamParam& param);
  status_t DeleteStreamStitching(const uint32_t id);

  status_t CreateCameraStream(const uint32_t& cam_idx,
                              const StreamParam& param,
                              const VideoExtraParam& extra_param);
  status_t DeleteCameraStream(const uint32_t& cam_idx,
                              const uint32_t& track_id);

  status_t FillDualCamMetadata(CameraMetadata& meta, const uint32_t& cam_idx);
  status_t FillCropMetadata(CameraMetadata& meta, const uint32_t& cam_idx);

  uint32_t                 virtual_camera_id_;
  CameraStartParam         start_params_;
  MultiCameraConfigType    multicam_type_;
  std::vector<int32_t>     supported_fps_;
  ResultCb                 result_cb_;
  ErrorCb                  error_cb_;

  //Non zsl capture request.
  SnapshotParam            snapshot_param_;
  uint32_t                 sequence_cnt_;
  bool                     snapshot_configured_;

  std::shared_ptr<SnapshotStitching>    snapshot_stitch_algo_;
  StreamSnapshotCb         client_snapshot_cb_;

  std::map<int32_t, SourceSurfaceDesc> source_surface_;
  std::map<int32_t, SurfaceCrop> surface_crop_;

  std::set<uint32_t>    active_streams_;

  // map of virtual camera id and its corresponding actual camera Ids.
  // <virtual camera id, Vector of actual camera id >
  std::map<uint32_t, std::vector<uint32_t> > virtual_camera_map_;

  // Map of camera id and CameraContext.
  std::map<uint32_t, std::shared_ptr<CameraContext>> camera_contexts_;

  // Map of track id and StreamStitching class
  std::map<uint32_t, std::shared_ptr<StreamStitching> > stream_stitch_algos_;

  std::mutex               lock_;
  QCondition               capture_done_;

  static const uint32_t kAecConvergeTimeout = 500000000; // 500 ms

  static const uint32_t kWidth4K  = 3840;
  static const uint32_t kHeight4K = 1920;
};

class StitchingBase : public ThreadHelper {
 public:
  struct InitParams {
    uint32_t                       multicam_id;
    std::vector<uint32_t>          camera_ids;
    MultiCameraConfigType          stitch_mode;
    std::map<int32_t, SurfaceCrop> surface_crop;
    uint32_t                       frame_rate;
  };

  StitchingBase(InitParams &param, MultiCameraManager *mgr);
  virtual ~StitchingBase();


  int32_t Run();
  void RequestExitAndWait() override;

 protected:
  // Thread for preparing synced and output buffers for processing by the
  // stitch library and passing them to the same library for stitching.
  bool ThreadLoop() override;

  // Pure virtual methods for handling the return of stream buffers
  // to their corresponding point of origin.
  virtual status_t NotifyBufferToClient(StreamBuffer &buffer) = 0;
  virtual status_t ReturnBufferToCamera(StreamBuffer &buffer) = 0;

  // Method for handling the synchronization between frames.
  status_t FrameSync(StreamBuffer& buffer);


  MultiCameraManager       *manager_;

  InitParams               params_;
  bool                     stop_frame_sync_;
  std::string              work_thread_name_;

  uint32_t                 skip_camera_id_;
  bool                     single_camera_mode_;

  std::mutex               frame_lock_;

 private:

  status_t StopFrameSync();

  status_t ReturnUnsyncedBuffers(uint32_t camera_id);

  // Map of incoming filled buffers for each of the actual cameras
  // that have not yet been synchronized.
  std::map<uint32_t, std::vector<StreamBuffer> > unsynced_buffer_map_;

  // List with buffers ready to go through stitch processing.
  // The uint32_t is the camera id to which this buffer belongs to.
  std::queue<std::map<uint32_t, StreamBuffer> > synced_buffer_queue_;

  // Map of the stream buffers that are given to the library for processing.
  std::map<IBufferHandle, StreamBuffer> process_buffers_map_;

  // List containing all buffers file descriptors that have been
  // registered by the library.
  std::set<int32_t> registered_buffers_;

  std::future<status_t>    init_library_status_;

  std::mutex               register_buffer_lock_;

  std::mutex               buffers_lock_;
  QCondition               wait_for_buffers_;

  std::mutex               sync_lock_;
  QCondition               wait_for_sync_frames_;

  static const uint32_t kWaitBuffersTimeout = 100000000; // 100 ms
  static const uint32_t kVideoFrameSyncTimeout = 50000000;  // 50 ms
  static const uint32_t kImageFrameSyncTimeout = 400000000;  // 400 ms

  static const uint8_t kUnsyncedQueueMaxSize = 3;

  // The maximum interval in which two frames are thought of as syncable.
  static const int32_t kMaxTimestampDelta = 2000000; // 2 ms
};

class StreamStitching : public StitchingBase {
 public:
  StreamStitching(InitParams &param, MultiCameraManager *mgr);
  ~StreamStitching();

  // Methods for establishing buffer communication link between the
  // consumer of the client and buffer producer of the stitching pipeline.
  status_t AddConsumer(const sp<IBufferConsumer>& consumer);
  status_t RemoveConsumer(sp<IBufferConsumer>& consumer);

  // Method to provide consumer interface, it would be used by a CameraContext
  // port producer to post buffers.
  sp<IBufferConsumer>& GetConsumerIntf(uint32_t camera_id);

  // Method for handling incoming buffers from CameraPort buffer producer.
  void OnFrameAvailable(StreamBuffer& buffer);

  // Method for handling a buffer returned back from the CameraSource.
  void NotifyBufferReturned(const StreamBuffer& buffer);

 protected:
  status_t NotifyBufferToClient(StreamBuffer &buffer) override;
  status_t ReturnBufferToCamera(StreamBuffer &buffer) override;

 private:
  bool IsConnected(const sp<IBufferConsumer>& consumer);

  sp<IBufferProducer>      buffer_producer_impl_;

  std::mutex               consumer_lock_;

  std::map<uintptr_t, sp<IBufferConsumer> > stitching_consumers_;

  // Map of camera id and it's corresponding buffer consumer.
  std::map<uint32_t, sp<IBufferConsumer> > camera_consumers_map_;
};

class SnapshotStitching : public StitchingBase {
 public:
  SnapshotStitching(InitParams &param, MultiCameraManager *mgr);
  ~SnapshotStitching();

  void SetClientCallback(const StreamSnapshotCb& cb) { snapshot_cb_ = cb; }

  // A callback method for handling incoming buffers from CameraContexts.
  void FrameAvailableCb(uint32_t count, StreamBuffer &buffer);

  // Method for handling a buffer returned back from the CameraSource.
  status_t ImageBufferReturned(const int32_t buffer_id);

 protected:
  status_t NotifyBufferToClient(StreamBuffer &buffer) override;
  status_t ReturnBufferToCamera(StreamBuffer &buffer) override;

 private:
  // Maps of buffer Id and Buffer.
  std::map<uint32_t, StreamBuffer> snapshot_buffer_list_;

  StreamSnapshotCb         snapshot_cb_;
  std::mutex               snapshot_lock_;
};

}; // recorder.

}; // qmmf.
