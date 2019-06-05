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

#define LOG_TAG "RecorderMultiCameraManager"

#include <algorithm>
#include <functional>
#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <cinttypes>
#include <fcntl.h>
#include <dlfcn.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "recorder/src/service/qmmf_multicamera_manager.h"
#include "recorder/src/service/qmmf_camera_context.h"
#include "recorder/src/service/qmmf_recorder_utils.h"

namespace qmmf {

namespace recorder {

MultiCameraManager::MultiCameraManager()
  : virtual_camera_id_(kVirtualCameraIdOffset),
    start_params_{},
    multicam_type_(MultiCameraConfigType::k360Stitch),
    result_cb_(nullptr),
    error_cb_(nullptr),
    snapshot_param_{0, 0, 0, BufferFormat::kBLOB},
    sequence_cnt_(0),
    snapshot_configured_(false),
    client_snapshot_cb_(nullptr) {}

MultiCameraManager::~MultiCameraManager() {

  QMMF_INFO("%s: Enter", __func__);

  stream_stitch_algos_.clear();
  snapshot_stitch_algo_ = nullptr;

  // Close cameras backwards since first camera is master camera.
  while (!camera_contexts_.empty()) {
    uint32_t cam_id = camera_contexts_.rbegin()->first;
    std::shared_ptr<CameraContext> camera_context = camera_contexts_.rbegin()->second;

    auto ret = camera_context->CloseCamera(cam_id);
    if (ret != NO_ERROR) {
      QMMF_ERROR("%s: CloseCamera(%d) failed!", __func__, cam_id);
    } else {
      QMMF_INFO("%s Camera %d closed successfully!", __func__, cam_id);
    }
    camera_contexts_.erase(cam_id);
  }

  QMMF_INFO("%s: Exit", __func__);
}

status_t
MultiCameraManager::CreateMultiCamera(const std::vector<uint32_t> camera_ids,
                                      uint32_t* virtual_camera_id) {

  QMMF_INFO("%s: Enter", __func__);
  if (camera_ids.size() < 2) {
    return BAD_VALUE;
  }
  QMMF_INFO("%s Number of camera to be used(%d)", __func__,
      camera_ids.size());

  if (virtual_camera_id_ + 1 < kVirtualCameraIdOffset) {
    virtual_camera_id_ = kVirtualCameraIdOffset;
  }
  virtual_camera_map_.emplace(++virtual_camera_id_, camera_ids);
  *virtual_camera_id = virtual_camera_id_;

  QMMF_INFO("%s: Exit", __func__);
  return NO_ERROR;
}

void MultiCameraManager::SetFlushCb(FlushCb &cb) {}

status_t MultiCameraManager::ConfigureMultiCamera(
    const uint32_t virtual_camera_id, const MultiCameraConfigType type,
    const void *param, const size_t param_size) {

  multicam_type_ = type;
  return NO_ERROR;
}

status_t MultiCameraManager::OpenCamera(const uint32_t virtual_camera_id,
                                        const CameraStartParam &param,
                                        const ResultCb &cb,
                                        const ErrorCb &errcb) {
  QMMF_INFO("%s: Enter", __func__);
  status_t ret = NO_ERROR;

  if (virtual_camera_map_.count(virtual_camera_id) == 0) {
    QMMF_ERROR("%s: Invalid virtual camera ID!", __func__);
    return BAD_VALUE;
  }
  auto camera_ids = virtual_camera_map_[virtual_camera_id];

  QMMF_INFO("%s: Total Number of cameras to be open(%d)", __func__,
      camera_ids.size());

  // Status(std::future) from asynchronous tasks for each camera(uint32_t).
  std::vector<std::tuple<uint32_t, std::future<status_t>>> results;

  // Open cameras in separate asynchronous tasks.
  for (auto const& cam_id : camera_ids) {
    std::shared_ptr<CameraContext> context = std::make_shared<CameraContext>();
    camera_contexts_.emplace(cam_id, context);

    ResultCb result_cb = [this] (uint32_t camera_id,
      const CameraMetadata &meta) { ResultCallback(camera_id, meta); };

    auto future = std::async(std::launch::async, &CameraContext::OpenCamera,
                             context.get(), cam_id, param, result_cb, nullptr);
    results.push_back(std::make_tuple(cam_id, std::move(future)));
  }

  start_params_ = param;
  result_cb_    = cb;
  error_cb_     = errcb;

  StitchingBase::InitParams algo_param {};
  algo_param.multicam_id = virtual_camera_id_;
  algo_param.camera_ids  = virtual_camera_map_[virtual_camera_id_];
  algo_param.stitch_mode = multicam_type_;
  algo_param.frame_rate  = 1;

  snapshot_stitch_algo_ =
      std::make_shared<SnapshotStitching>(algo_param, this);

  StreamSnapshotCb snapshot_cb = [&] (uint32_t count, StreamBuffer& buffer) {
     OnStitchedFrameAvailable(buffer);
  };
  snapshot_stitch_algo_->SetClientCallback(snapshot_cb);

  // Wait for all asynchronous tasks to complete and return status.
  for (auto& result : results) {
    auto camera_id = std::get<0>(result);
    auto future = std::move(std::get<1>(result));

    if (future.get() != NO_ERROR) {
      QMMF_ERROR("%s: Failed to open camera(%d)!", __func__, camera_id);
      ret |= NO_INIT;
    }
  }

  QMMF_INFO("%s: Exit", __func__);
  return ret;
}

status_t MultiCameraManager::CloseCamera(const uint32_t virtual_camera_id) {

  QMMF_INFO("%s: Enter", __func__);
  status_t ret = NO_ERROR;
  bool closing_failed = false;

  if (virtual_camera_map_.count(virtual_camera_id) == 0) {
    QMMF_ERROR("%s: Invalid virtual camera ID!", __func__);
    return BAD_VALUE;
  }
  QMMF_INFO("%s: Total Number of cameras to be closed(%d)", __func__,
      camera_contexts_.size());

  snapshot_stitch_algo_->RequestExitAndWait();
  snapshot_stitch_algo_ = nullptr;

  // Close cameras backwards since first camera is master camera.
  while (!camera_contexts_.empty()) {
    uint32_t cam_id = camera_contexts_.rbegin()->first;
    std::shared_ptr<CameraContext> camera_context = camera_contexts_.rbegin()->second;

    ret = camera_context->CloseCamera(cam_id);
    if (ret != NO_ERROR) {
      QMMF_ERROR("%s: CloseCamera(%d) failed!", __func__, cam_id);
      closing_failed = true;
      camera_context = nullptr;
    }
    camera_contexts_.erase(cam_id);
  }

  virtual_camera_map_.erase(virtual_camera_id);

  QMMF_INFO("%s: Exit", __func__);
  return closing_failed ? UNKNOWN_ERROR : NO_ERROR;
}

status_t MultiCameraManager::WaitAecToConverge(const uint32_t timeout) {

  // Since both cameras are in sync we need to wait Aec
  // to converge only on main camera
  status_t ret = camera_contexts_.at(0)->WaitAecToConverge(timeout);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s: WaitAecToConverge Failed!", __func__);
    return ret;
  }
  return NO_ERROR;
}

status_t MultiCameraManager::SetUpCapture(const SnapshotParam& param,
                                          const uint32_t num_images) {

  std::unique_lock<std::mutex> lock(lock_);
  status_t ret = NO_ERROR;

  if (sequence_cnt_ != 0) {
    QMMF_WARN("%s: Wait for pending captures, count = %u!", __func__,
        sequence_cnt_);
    capture_done_.Wait(lock);
  }
  sequence_cnt_ = num_images * 2;

  bool reconfigure_needed = (snapshot_param_.width != param.width) ||
                            (snapshot_param_.height != param.height) ||
                            !snapshot_configured_;

  SnapshotParam sparam = snapshot_param_ = param;

  if (reconfigure_needed) {
    snapshot_stitch_algo_->RequestExitAndWait();
  }

  auto streams = active_streams_;
  if (reconfigure_needed) {
    // Stop all active streams.
    for (auto const& track_id : streams) {
      StopStream(track_id);
    }
  }
  SetDefaultSurfaceDim(sparam.width, sparam.height);

  for (auto& camera : camera_contexts_) {
    uint32_t camera_id = camera.first;
    std::shared_ptr<CameraContext> context = camera.second;

    ret = context->SetUpCapture(sparam, num_images);
    if (ret != NO_ERROR) {
      QMMF_ERROR("%s: Camera %d: SetUpCapture Failed!", __func__,
          camera_id);
      return ret;
    }
  }

  if (reconfigure_needed) {
    // Resume all previously active streams.
    for (auto const& track_id : streams) {
      StartStream(track_id);
    }
    // Wait avoid capturing black frames
    ret = WaitAecToConverge(kAecConvergeTimeout);
    if (ret != NO_ERROR) {
      QMMF_WARN("%s: AE failed to converge!", __func__);
    }
  }
  snapshot_configured_ = true;

  return NO_ERROR;
}

status_t MultiCameraManager::CaptureImage(const std::vector<CameraMetadata>
                                          &meta, const StreamSnapshotCb& cb) {

  std::lock_guard<std::mutex> lock(lock_);
  status_t ret = NO_ERROR;

  StreamSnapshotCb stream_cb = [&] (uint32_t count, StreamBuffer& buf) {
    snapshot_stitch_algo_->FrameAvailableCb(count, buf);
  };
  client_snapshot_cb_ = cb;

  snapshot_stitch_algo_->Run();

  // Always use synchronized request for capture.
  std::vector<CameraMetadata> capture_meta = meta;

  const uint8_t sync_req = 1;
  capture_meta[0].update(QCAMERA3_DUALCAM_SYNCHRONIZED_REQUEST,
                         &sync_req, 1);

  auto camera_ids = virtual_camera_map_[virtual_camera_id_];
  for (size_t idx = 0; idx < camera_ids.size(); ++idx) {
    auto camera_id = camera_ids[idx];

    // Dual camera meta should only be sent only on first capture in burst.
    ret = FillDualCamMetadata(capture_meta.at(0), idx);
    if (ret != NO_ERROR) {
      QMMF_ERROR("%s: Camera %d: FillDualCamMetadata failed!",
          __func__, camera_id);
      return ret;
    }
    ret = camera_contexts_[camera_id]->CaptureImage(capture_meta, stream_cb);
    if (ret != NO_ERROR) {
      QMMF_ERROR("%s: Camera %d: CaptureImage failed!", __func__,
          camera_id);
      return ret;
    }
  }
  return NO_ERROR;
}

status_t MultiCameraManager::ConfigImageCapture(const ImageConfigParam &config) {

  return NO_ERROR;
}

status_t MultiCameraManager::CancelCaptureImage() {

  snapshot_stitch_algo_->RequestExitAndWait();

  if (snapshot_configured_) {
    auto streams = active_streams_;
    status_t ret = NO_ERROR;

    for (auto const& track_id : streams) {
      ret = StopStream(track_id);
      if (ret != NO_ERROR) {
        QMMF_ERROR("%s: StopStream %d Failed!", __func__, track_id);
        return ret;
      }
    }
  auto camera_ids = virtual_camera_map_[virtual_camera_id_];
  for (auto const& camera_id : camera_ids) {
    auto ret = camera_contexts_[camera_id]->CancelCaptureImage();
    if (ret != NO_ERROR) {
      QMMF_ERROR("%s: Camera %d: CancelCaptureImage failed!", __func__,
          camera_id);
      return ret;
    }
  }
    for (auto const& track_id : streams) {
      ret = StartStream(track_id);
      if (ret != NO_ERROR) {
        QMMF_ERROR("%s: StartStream %d Failed!", __func__, track_id);
        return ret;
      }
    }
    snapshot_configured_ = false;
  }
  return NO_ERROR;
}

status_t MultiCameraManager::CreateStream(const StreamParam& param,
                                          const VideoExtraParam& extra_param) {

  SourceSurfaceDesc surface;
  source_surface_.clear();
  surface_crop_.clear();

  auto camera_ids = virtual_camera_map_[virtual_camera_id_];

  if (extra_param.Exists(QMMF_SOURCE_SURFACE_DESCRIPTOR)) {
    // Source surface entry count should be equal to the number of cameras.
    size_t entry_count = extra_param.EntryCount(QMMF_SOURCE_SURFACE_DESCRIPTOR);
    if (entry_count < camera_contexts_.size()) {
      QMMF_ERROR("%s: Not enough QMMF_SOURCE_SURFACE_PARAM entries! "
          "Required entries: %d!",  __func__, camera_contexts_.size());
      return NOT_ENOUGH_DATA;
    } else if (entry_count > camera_contexts_.size()) {
      QMMF_ERROR("%s: QMMF_SOURCE_SURFACE_PARAM entries count exceeds "
          "camera count (%d)!",  __func__, camera_contexts_.size());
      return BAD_INDEX;
    }
    // Fetch source surface dimensions data from the container.
    for (size_t i = 0; i < entry_count; ++i) {
      extra_param.Fetch(QMMF_SOURCE_SURFACE_DESCRIPTOR, surface, i);
      if (source_surface_.find(surface.camera_id) != source_surface_.end()) {
        QMMF_ERROR("%s: Found more than one QMMF_SOURCE_SURFACE_PARAM "
            "entry for camera %d!",  __func__, surface.camera_id);
        return ALREADY_EXISTS;
      }
      source_surface_.emplace(surface.camera_id, surface);
    }
    // Verify that surface dimensions are set for every camera.
    auto camera_ids = virtual_camera_map_[virtual_camera_id_];
    for (auto const& cam_id : camera_ids) {
      if (source_surface_.find(cam_id) == source_surface_.end()) {
        QMMF_ERROR("%s: QMMF_SOURCE_SURFACE_PARAM for camera %d missing!",
            __func__, cam_id);
        return NAME_NOT_FOUND;
      }
    }
  } else {
    surface.width  = param.width;
    surface.height = param.height;
    // Fill the source camera surfaces with default values.
    SetDefaultSurfaceDim(surface.width, surface.height);
    for (auto const& cam_id : camera_ids) {
      surface.camera_id = cam_id;
      source_surface_.emplace(cam_id, surface);
    }
  }

  if (extra_param.Exists(QMMF_SURFACE_CROP)) {
    SurfaceCrop crop;
    // Fetch crop rectangle data from the container.
    for (size_t i = 0; i < extra_param.EntryCount(QMMF_SURFACE_CROP); ++i) {
      extra_param.Fetch(QMMF_SURFACE_CROP, crop, i);
      if (surface_crop_.find(crop.camera_id) != surface_crop_.end()) {
        QMMF_ERROR("%s: Found more than one QMMF_SURFACE_CROP entry "
            "for camera %d!",  __func__, crop.camera_id);
        return ALREADY_EXISTS;
      }
      // Verify the camera ID.
      if (camera_contexts_.count(crop.camera_id) == 0) {
        QMMF_ERROR("%s: Camera ID %d for QMMF_SURFACE_CROP entry %d "
            "does not exist!",  __func__, crop.camera_id, i);
        return NAME_NOT_FOUND;
      }
      auto surface = source_surface_.at(crop.camera_id);
      if (crop.width == 0 && crop.height == 0) {
        auto camera_ids = virtual_camera_map_[virtual_camera_id_];
        for (auto const& cam_id : camera_ids) {
          source_surface_.at(cam_id).width  = param.width;
          source_surface_.at(cam_id).height = param.height;
        }
      } else if (surface.width < crop.width || surface.height < crop.height) {
        QMMF_ERROR("%s: Invalid QMMF_SURFACE_CROP entry dimensions for "
            "camera %d!",  __func__, crop.camera_id);
        return BAD_VALUE;
      }
      surface_crop_.emplace(crop.camera_id, crop);
    }
  }


  // Stop all active streams.
  auto streams = active_streams_;
  for (auto const& track_id : streams) {
    StopStream(track_id);
  }

  auto ret = CreateStreamStitching(param);
  if (NO_ERROR != ret) {
    QMMF_ERROR("%s: CreateStreamStitching Failed!", __func__);
    return ret;
  }

  // Start streams in reverse order. This is needed because camera
  // context is caching our streams and streams will be destroyed only
  // when new stream is created, and not on delete stream as expected.
  for (ssize_t ctx_idx = camera_contexts_.size() - 1; ctx_idx >= 0; --ctx_idx) {
    StreamParam stream_param(param);
    auto &camera_surface = source_surface_.at(camera_ids[ctx_idx]);
    stream_param.width  = camera_surface.width;
    stream_param.height = camera_surface.height;

    stream_param.wait_aec_mode &= (ctx_idx == 0) ? true : false;

    ret = CreateCameraStream(ctx_idx, stream_param, extra_param);
    if (ret != NO_ERROR) {
      QMMF_ERROR("%s: Camera %d: CreateCameraStream failed!", __func__,
          camera_ids[ctx_idx]);
      for (size_t idx = ctx_idx + 1; idx < camera_contexts_.size(); ++idx) {
        DeleteCameraStream(idx, param.id);
      }
      DeleteStreamStitching(param.id);
      return ret;
    }
  }

  std::shared_ptr<StreamStitching> stitching_algo = stream_stitch_algos_[param.id];
  assert(stitching_algo.get() != nullptr);

  for (auto& camera_id : camera_ids) {
    sp<IBufferConsumer> consumer = stitching_algo->GetConsumerIntf(camera_id);
    assert(consumer.get() != nullptr);

    ret = camera_contexts_[camera_id]->AddConsumer(param.id, consumer);
    if (ret != NO_ERROR) {
      QMMF_ERROR("%s: AddConsumer Failed!", __func__);
      return ret;
    }
  }

  // Resume all previously active streams.
  for (auto const& track_id : streams) {
    StartStream(track_id);
  }
  return NO_ERROR;
}

status_t MultiCameraManager::DeleteStream(const uint32_t track_id) {

  status_t ret = NO_ERROR;

  std::shared_ptr<StreamStitching> stitching_algo = stream_stitch_algos_.at(track_id);
  assert(stitching_algo.get() != nullptr);

  for (auto& camera : camera_contexts_) {
    uint32_t camera_id = camera.first;
    std::shared_ptr<CameraContext> context = camera.second;

    sp<IBufferConsumer> consumer = stitching_algo->GetConsumerIntf(camera_id);
    assert(consumer.get() != nullptr);

    ret = context->RemoveConsumer(track_id, consumer);
    if (ret != NO_ERROR) {
      QMMF_ERROR("%s: RemoveConsumer Failed!", __func__);
      return ret;
    }
  }

  // Delete the streams backwards since first camera is master camera
  // and need to be stopped last.
  for (ssize_t idx = camera_contexts_.size() - 1; idx >= 0; --idx) {
    ret = DeleteCameraStream(idx, track_id);
    if (ret != NO_ERROR) {
      QMMF_ERROR("%s: DeleteCameraStream Failed!", __func__);
      return ret;
    }
  }

  ret = DeleteStreamStitching(track_id);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s: DeleteStreamStitching failed %d!", __func__, ret);
  }
  return NO_ERROR;
}

status_t MultiCameraManager::AddConsumer(const uint32_t& track_id,
                                         sp<IBufferConsumer>& consumer) {

  std::shared_ptr<StreamStitching> stitching_algo = stream_stitch_algos_.at(track_id);
  assert(stitching_algo.get() != nullptr);

  auto ret = stitching_algo->AddConsumer(consumer);
  assert(ret == NO_ERROR);
  QMMF_INFO("%s: Consumer(%p) added to track_id(%d)", __func__,
      consumer.get(), track_id);

  return NO_ERROR;
}

status_t MultiCameraManager::RemoveConsumer(const uint32_t& track_id,
                                            sp<IBufferConsumer>& consumer) {

  std::shared_ptr<StreamStitching> stitching_algo = stream_stitch_algos_.at(track_id);
  assert(stitching_algo.get() != nullptr);

  auto ret = stitching_algo->RemoveConsumer(consumer);
  assert(ret == NO_ERROR);

  return NO_ERROR;
}

status_t MultiCameraManager::StartStream(const uint32_t track_id) {

  status_t ret = NO_ERROR;

  std::shared_ptr<StreamStitching> stitching_algo = stream_stitch_algos_.at(track_id);
  assert(stitching_algo.get() != nullptr);

  stitching_algo->Run();

  for (auto& camera : camera_contexts_) {
    std::shared_ptr<CameraContext> context = camera.second;

    ret = context->StartStream(track_id);
    if (ret != NO_ERROR) {
      QMMF_ERROR("%s: StartStream Failed!", __func__);
      return ret;
    }
  }
  if (active_streams_.count(track_id) == 0) {
    active_streams_.emplace(track_id);
  }
  return ret;
}

status_t MultiCameraManager::StopStream(const uint32_t track_id) {

  status_t ret = NO_ERROR;
  std::shared_ptr<StreamStitching> stitching_algo = stream_stitch_algos_.at(track_id);
  assert(stitching_algo.get() != nullptr);

  stitching_algo->RequestExitAndWait();

  // Stop the streams backwards since first camera is master camera
  // and need to be stopped last.
  for (auto i = camera_contexts_.rbegin(); i != camera_contexts_.rend(); ++i) {
    std::shared_ptr<CameraContext> camera_context = i->second;
    assert(camera_context.get() != nullptr);

    ret = camera_context->StopStream(track_id);
    if (ret != NO_ERROR) {
      QMMF_ERROR("%s: StopStream Failed!", __func__);
      return ret;
    }
  }
  if (active_streams_.count(track_id) != 0) {
    active_streams_.erase(track_id);
  }
  return ret;
}

status_t MultiCameraManager::SetCameraParam(const CameraMetadata &meta) {

  size_t ctx_idx = 0;
  for (auto ctx_it = camera_contexts_.begin();
    ctx_it != camera_contexts_.end(); ++ctx_it, ++ctx_idx) {
    std::shared_ptr<CameraContext> camera_context = ctx_it->second;
    int32_t camera_id = ctx_it->first;

    auto ret = FillDualCamMetadata(const_cast<CameraMetadata&>(meta), ctx_idx);
    if (ret != NO_ERROR) {
      QMMF_ERROR("%s: Camera %d: FillDualCamMetadata Failed!",
          __func__, camera_id);
      return ret;
    }

    ret = camera_context->SetCameraParam(meta);
    if (ret != NO_ERROR) {
      QMMF_ERROR("%s: Camera %d: SetCameraParam Failed!", __func__,
          camera_id);
      return ret;
    }
  }
  return NO_ERROR;
}

status_t MultiCameraManager::GetCameraParam(CameraMetadata &meta) {

  std::shared_ptr<CameraContext> camera_context = camera_contexts_.at(0);
  assert(camera_context.get() != nullptr);
  status_t ret = camera_context->GetCameraParam(meta);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s: GetCameraParam Failed!", __func__);
  }
  return ret;
}

status_t MultiCameraManager::GetDefaultCaptureParam(CameraMetadata &meta) {

  std::shared_ptr<CameraContext> camera_context = camera_contexts_.at(0);
  assert(camera_context.get() != nullptr);
  status_t ret = camera_context->GetDefaultCaptureParam(meta);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s: GetDefaultCaptureParam Failed!", __func__);
  }
  return ret;
}

status_t MultiCameraManager::GetCameraCharacteristics(CameraMetadata &meta) {

  std::shared_ptr<CameraContext> camera_context = camera_contexts_.at(0);
  assert(camera_context.get() != nullptr);
  status_t ret = camera_context->GetCameraCharacteristics(meta);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s: GetCameraCharacteristics Failed!", __func__);
  }
  return ret;
}

status_t MultiCameraManager::ReturnImageCaptureBuffer(const uint32_t camera_id,
                                                      const int32_t buffer_id) {

  status_t ret = snapshot_stitch_algo_->ImageBufferReturned(buffer_id);
  if (NO_ERROR != ret) {
   QMMF_ERROR("%s: Unable to return stitched buffer!", __func__);
    return ret;
  }

  return NO_ERROR;
}

std::vector<int32_t>& MultiCameraManager::GetSupportedFps() {

  return camera_contexts_.at(0)->GetSupportedFps();
}

void MultiCameraManager::ResultCallback(uint32_t camera_id,
                                        const CameraMetadata &meta) {

  if (camera_id == 0) {
    if (nullptr != result_cb_) {
      result_cb_(virtual_camera_id_, meta);
    }
  }
}

status_t MultiCameraManager::SetDefaultSurfaceDim(uint32_t& w, uint32_t& h) {

  switch (multicam_type_) {
    case MultiCameraConfigType::k360Stitch:
    case MultiCameraConfigType::kSideBySide:
      // Divide the width of the stitched output on the number of cameras.
      w /= camera_contexts_.size();
      break;
    default:
      QMMF_ERROR("%s: Unsupported MultiCamera mode: 0x%x", __func__,
          multicam_type_);
      return NAME_NOT_FOUND;
  }
  return NO_ERROR;
}

void MultiCameraManager::OnStitchedFrameAvailable(StreamBuffer buffer) {

  client_snapshot_cb_(1, buffer);

  {
    std::lock_guard<std::mutex> lock(lock_);
    --sequence_cnt_;

    if (sequence_cnt_ == 0) {
      snapshot_stitch_algo_->RequestExitAndWait();
      capture_done_.Signal();
    }
  }
}

status_t MultiCameraManager::CreateStreamStitching(const StreamParam& param) {

  StitchingBase::InitParams algo_param {};
  algo_param.multicam_id  = virtual_camera_id_;
  algo_param.camera_ids   = virtual_camera_map_.at(virtual_camera_id_);
  algo_param.stitch_mode  = multicam_type_;
  algo_param.surface_crop = surface_crop_;
  algo_param.frame_rate   = param.framerate;

  std::shared_ptr<StreamStitching> stitching_algo =
      std::make_shared<StreamStitching>(algo_param, this);

  stream_stitch_algos_.emplace(param.id, stitching_algo);

  return NO_ERROR;
}

status_t MultiCameraManager::DeleteStreamStitching(const uint32_t id) {

  if (stream_stitch_algos_.count(id) == 0) {
    QMMF_ERROR("%s: Stitching algo not present for track id %d",
               __func__, id);
    return BAD_VALUE ;
  }
  stream_stitch_algos_.erase(id);

  return NO_ERROR;
}

status_t MultiCameraManager::CreateCameraStream(const uint32_t& cam_idx, const
                                                StreamParam& param, const
                                                VideoExtraParam& extra_param) {

  auto camera_id = virtual_camera_map_[virtual_camera_id_].at(cam_idx);
  std::shared_ptr<CameraContext> camera_context = camera_contexts_[camera_id];

  // On CreateStream, camera context most probably will
  // reconfigure the camera. So make sure that every time.
  // after reconfiguration we are linking the related cameras.
  status_t ret = camera_context->CreateStream(param, extra_param);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s: Camera %d: CreateStream Failed!", __func__,
        camera_id);
    return ret;
  }

  CameraMetadata meta;
  ret = camera_context->GetCameraParam(meta);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s: Camera %d: GetCameraParam Failed!", __func__,
        camera_id);
    return ret;
  }
  ret = FillDualCamMetadata(meta, cam_idx);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s: Camera %d: FillDualCamMetadata Failed!",
        __func__, camera_id);
    return ret;
  }

  ret = camera_context->SetCameraParam(meta);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s: Camera %d: SetCameraParam Failed!", __func__,
        camera_id);
    return ret;
  }

  return NO_ERROR;
}

status_t MultiCameraManager::DeleteCameraStream(const uint32_t& cam_idx,
                                                const uint32_t& track_id) {

  auto camera_id = virtual_camera_map_[virtual_camera_id_].at(cam_idx);
  status_t ret = camera_contexts_[camera_id]->DeleteStream(track_id);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s: Camera %d: DeleteStream Failed!", __func__,
        camera_id);
    return ret;
  }
  return NO_ERROR;
}

status_t MultiCameraManager::FillDualCamMetadata(CameraMetadata& meta,
                                                 const uint32_t& cam_idx) {

  if (cam_idx >= camera_contexts_.size()) {
    QMMF_ERROR("%s: Invalid camera index %d number of cameras %d!",
        __func__, cam_idx, camera_contexts_.size());
    return BAD_VALUE;
  }

  if (camera_contexts_.size() < 2) {
      QMMF_INFO("%s: No need to link one camera skip!", __func__);
      return NO_ERROR;
  }

  // If we don't have even cameras to link don't link the last camera
  if ((cam_idx == camera_contexts_.size() - 1) &&
      (camera_contexts_.size() & 1)) {
    QMMF_WARN("%s: Last camera id %d will not be linked, No pair!",
        __func__, camera_contexts_.end()->first);
    return NO_ERROR;
  }

  int32_t related_id;
  uint8_t is_main;

  // Link First with Second, Second with first etc...
  if (cam_idx & 1) {
    related_id = virtual_camera_map_[virtual_camera_id_].at(cam_idx - 1);
    is_main = 0;
  } else {
    related_id = virtual_camera_map_[virtual_camera_id_].at(cam_idx + 1);
    is_main = 1;
  }

  meta.update(QCAMERA3_DUALCAM_LINK_IS_MAIN, &is_main, 1);
  meta.update(QCAMERA3_DUALCAM_LINK_RELATED_CAMERA_ID, &related_id, 1);

  uint8_t sync = 1;
  meta.update(QCAMERA3_DUALCAM_LINK_ENABLE, &sync, 1);

  uint8_t role = QCAMERA3_DUALCAM_LINK_CAMERA_ROLE_BAYER;
  meta.update(QCAMERA3_DUALCAM_LINK_CAMERA_ROLE, &role, 1);

  uint8_t sync_mode = QCAMERA3_DUALCAM_LINK_3A_360_CAMERA;
  meta.update(QCAMERA3_DUALCAM_LINK_3A_SYNC_MODE, &sync_mode, 1);

  return NO_ERROR;
}

SnapshotStitching::SnapshotStitching(InitParams &param, MultiCameraManager *mgr)
    : StitchingBase(param, mgr),
      snapshot_cb_(nullptr) {

  QMMF_INFO("%s: Enter", __func__);
  work_thread_name_ = "SnapshotStitching";
  QMMF_INFO("%s: Exit (0x%p)", __func__, this);
}

SnapshotStitching::~SnapshotStitching() {

  QMMF_INFO("%s: Enter", __func__);
  QMMF_INFO("%s: Exit (0x%p)", __func__, this);
}

void SnapshotStitching::FrameAvailableCb(uint32_t count,
                                         StreamBuffer &buffer) {

  std::lock_guard<std::mutex> lock(frame_lock_);
  QMMF_DEBUG("%s: Camera %u: Snapshot Frame %d is available",
      __func__, buffer.camera_id, buffer.frame_number);

  // Handling input buffers from camera contexts.
  if (stop_frame_sync_) {
    ReturnBufferToCamera(buffer);
  } else {
    FrameSync(buffer);
  }
}

status_t SnapshotStitching::ImageBufferReturned(const int32_t buffer_id) {

  std::lock_guard<std::mutex> lock(snapshot_lock_);
  if (snapshot_buffer_list_.count(buffer_id) == 0) {
    QMMF_ERROR("%s: buffer_id(%u) is not valid!", __func__, buffer_id);
    return BAD_VALUE;
  }

  StreamBuffer buffer = snapshot_buffer_list_[buffer_id];
  QMMF_DEBUG("%s: Image capture buffer(handle %p, fd %d) returned",
      __func__, buffer.handle, buffer.fd);

  auto ret = ReturnBufferToCamera(buffer);
  if (NO_ERROR != ret) {
    QMMF_ERROR("%s: Failed to return buffer %p for camera %d!", __func__,
        buffer.handle, buffer.camera_id);
  }
  snapshot_buffer_list_.erase(buffer_id);
  return NO_ERROR;
}

status_t SnapshotStitching::NotifyBufferToClient(StreamBuffer &buffer) {

  status_t ret = NO_ERROR;
  if (nullptr != snapshot_cb_) {
    {
      std::lock_guard<std::mutex> lock(snapshot_lock_);
      snapshot_buffer_list_.emplace(buffer.fd, buffer);
    }
    snapshot_cb_(1, buffer);
  }

  return ret;
}

status_t SnapshotStitching::ReturnBufferToCamera(StreamBuffer &buffer) {

  status_t ret = NO_ERROR;
  if (manager_->camera_contexts_.count(buffer.camera_id) == 0) {
    QMMF_ERROR("%s: Invalid camera ID(%d)", __func__, buffer.camera_id);
    return BAD_VALUE;
  }

  auto& camera = manager_->camera_contexts_[buffer.camera_id];

  ret = camera->ReturnImageCaptureBuffer(buffer.camera_id, buffer.fd);
  if(NO_ERROR != ret) {
    QMMF_ERROR("%s: Failed to return buffer to camera(%d)",
        __func__, buffer.camera_id);
  }
  return ret;
}

StreamStitching::StreamStitching(InitParams &param, MultiCameraManager *mgr)
    : StitchingBase(param, mgr) {

  QMMF_INFO("%s: Enter", __func__);

  work_thread_name_ = "StreamStitching";

  // Create consumers for the physical cameras.
  for (auto const& camera_id : params_.camera_ids) {
    BufferConsumerImpl<StreamStitching> *impl;
    impl = new BufferConsumerImpl<StreamStitching>(this);
    camera_consumers_map_.emplace(camera_id, impl);
  }

  BufferProducerImpl<StreamStitching> *producer_impl;
  producer_impl = new BufferProducerImpl<StreamStitching>(this);
  buffer_producer_impl_ = producer_impl;

  QMMF_INFO("%s: Exit (0x%p)", __func__, this);
}

StreamStitching::~StreamStitching() {

  QMMF_INFO("%s: Enter", __func__);
  buffer_producer_impl_.clear();
  camera_consumers_map_.clear();
  QMMF_INFO("%s: Exit (0x%p)", __func__, this);
}

status_t StreamStitching::AddConsumer(const sp<IBufferConsumer>& consumer) {

  std::lock_guard<std::mutex> lock(consumer_lock_);
  if (consumer.get() == nullptr) {
    QMMF_ERROR("%s: Input consumer is NULL", __func__);
    return BAD_VALUE;
  }

  if (IsConnected(consumer)) {
    QMMF_ERROR("%s: consumer(%p) already added to the producer!",
        __func__, consumer.get());
    return ALREADY_EXISTS;
  }

  buffer_producer_impl_->AddConsumer(consumer);
  consumer->SetProducerHandle(buffer_producer_impl_);
  QMMF_DEBUG("%s: Consumer(%p) has been added."
      " Total number of consumers = %d",  __func__, consumer.get(),
      buffer_producer_impl_->GetNumConsumer());

  stitching_consumers_.emplace(reinterpret_cast<uintptr_t>(consumer.get()),
                               consumer);
  return NO_ERROR;
}

status_t StreamStitching::RemoveConsumer(sp<IBufferConsumer>& consumer) {

  std::lock_guard<std::mutex> lock(consumer_lock_);
  if (consumer.get() == nullptr) {
    QMMF_ERROR("%s: Input consumer is NULL", __func__);
    return BAD_VALUE;
  }

  if(buffer_producer_impl_->GetNumConsumer() == 0) {
    QMMF_ERROR("%s: There are no connected consumers!", __func__);
    return INVALID_OPERATION;
  }

  if (!IsConnected(consumer)) {
    QMMF_ERROR("%s: consumer(%p) is not connected to this port(%p)!",
        __func__, consumer.get(), this);
    return BAD_VALUE;
  }

  buffer_producer_impl_->RemoveConsumer(consumer);
  QMMF_DEBUG("%s: Consumer(%p) has been removed."
      "Total number of consumer = %d",  __func__, consumer.get(),
      buffer_producer_impl_->GetNumConsumer());

  stitching_consumers_.erase(reinterpret_cast<uintptr_t>(consumer.get()));
  return NO_ERROR;
}

sp<IBufferConsumer>& StreamStitching::GetConsumerIntf(uint32_t camera_id) {

  return camera_consumers_map_.at(camera_id);
}

void StreamStitching::OnFrameAvailable(StreamBuffer& buffer) {

  std::lock_guard<std::mutex> lock(frame_lock_);
  QMMF_DEBUG("%s: camera_id: %d, stream_id: %d, buffer: %p ts: %lld "
      "frame_number: %d", __func__, buffer.camera_id, buffer.stream_id,
      buffer.handle, buffer.timestamp, buffer.frame_number);

  if (stop_frame_sync_ || (single_camera_mode_ &&
      buffer.camera_id == skip_camera_id_)) {
    ReturnBufferToCamera(buffer);
  } else if (single_camera_mode_ && (buffer.camera_id != skip_camera_id_)) {
    NotifyBufferToClient(buffer);
  } else {
    FrameSync(buffer);
  }
}

void StreamStitching::NotifyBufferReturned(const StreamBuffer& buffer) {

  QMMF_VERBOSE("%s: Stream buffer(handle %p) returned", __func__,
      buffer.handle);
  if (buffer.camera_id != params_.multicam_id) {
    ReturnBufferToCamera(const_cast<StreamBuffer&>(buffer));
  }
}

status_t StreamStitching::NotifyBufferToClient(StreamBuffer &buffer) {

  status_t ret = NO_ERROR;
  if(buffer_producer_impl_->GetNumConsumer() > 0) {
    buffer_producer_impl_->NotifyBuffer(buffer);
  }

  return ret;
}

status_t StreamStitching::ReturnBufferToCamera(StreamBuffer &buffer) {

  const sp<IBufferConsumer> consumer = GetConsumerIntf(buffer.camera_id);
  if (consumer.get() == nullptr) {
    QMMF_ERROR("%s: Failed to retrieve buffer consumer for camera(%d)!",
               __func__, buffer.camera_id);
    return BAD_VALUE;
  }
  consumer->GetProducerHandle()->NotifyBufferReturned(buffer);
  return NO_ERROR;
}

bool StreamStitching::IsConnected(const sp<IBufferConsumer>& consumer) {

  uintptr_t key = reinterpret_cast<uintptr_t>(consumer.get());
  if (stitching_consumers_.count(key) != 0) {
    return true;
  }
  return false;
}

StitchingBase::StitchingBase(InitParams &param, MultiCameraManager *mgr)
    : manager_(mgr),
      params_(param),
      stop_frame_sync_(false),
      skip_camera_id_ (0),
      single_camera_mode_(false) {

  QMMF_INFO("%s: Enter", __func__);

  // Initialize the buffer map with unsynchronized buffers.
  for (auto const& camera_id : params_.camera_ids) {
    std::vector<StreamBuffer> empty_buffers;
    unsynced_buffer_map_.emplace(camera_id, empty_buffers);
  }

  for (auto const& cam_id : params_.camera_ids) {
    if (params_.surface_crop.find(cam_id) != params_.surface_crop.end()) {
      auto crop = params_.surface_crop.at(cam_id);
      if (crop.width == 0 && crop.height == 0) {
        skip_camera_id_ = cam_id;
        single_camera_mode_ = true;
      }
    }
  }

  QMMF_INFO("%s: Exit (0x%p)", __func__, this);
}

StitchingBase::~StitchingBase() {

  QMMF_INFO("%s: Enter", __func__);

  RequestExitAndWait();

  unsynced_buffer_map_.clear();
  process_buffers_map_.clear();
  registered_buffers_.clear();

  QMMF_INFO("%s: Exit (0x%p)", __func__, this);
}

int32_t StitchingBase::Run() {

  std::lock_guard<std::mutex> lock(frame_lock_);
  stop_frame_sync_ = false;
  return ThreadHelper::Run(work_thread_name_);
}

void StitchingBase::RequestExitAndWait() {

  std::lock_guard<std::mutex> lock(frame_lock_);
  status_t ret = StopFrameSync();
  assert(NO_ERROR == ret);
}

bool StitchingBase::ThreadLoop() {

  std::vector<StreamBuffer> input_buffers;
  {
    // If there aren't any pending synchronized buffers waiting to go through
    // stitch processing, wait until such buffer becomes available.
    std::unique_lock<std::mutex> lock(sync_lock_);

    while (synced_buffer_queue_.empty() && !stop_frame_sync_) {
      if (work_thread_name_ == "StreamStitching") {
        std::chrono::nanoseconds wait_time(kVideoFrameSyncTimeout);
        auto ret = wait_for_sync_frames_.WaitFor(lock, wait_time);

        if (ret != 0) {
          QMMF_DEBUG("%s: Wait for frame available timed out", __func__);
        }
      } else {
        std::chrono::nanoseconds wait_time(kImageFrameSyncTimeout);
        auto ret = wait_for_sync_frames_.WaitFor(lock, wait_time);

        if (ret != 0) {
          QMMF_WARN("%s: Wait for frame available timed out, cleaning"
              " unsynched buffers!", __func__);

          // Return all unsynced buffers back to the camera contexts.
          for (auto const& camera_id : params_.camera_ids) {
            ReturnUnsyncedBuffers(camera_id);
          }
          std::lock_guard<std::mutex> lock(manager_->lock_);
          stop_frame_sync_ = true;
          ThreadHelper::RequestExit();

          --manager_->sequence_cnt_;
          manager_->capture_done_.Signal();
        }
      }
    }
    // Exit from thread loop if frame sync is stopped
    if (stop_frame_sync_) return false;

    for (auto const& id : params_.camera_ids) {
      input_buffers.push_back(synced_buffer_queue_.front().at(id));
    }
    synced_buffer_queue_.pop();
  }


  QMMF_VERBOSE("%s: Matched: frame_number(%d), camera(%u), stream(%d), ts(%lld)"
      " with frame_number(%d), camera(%u), stream(%d), ts(%lld)", __func__,
      input_buffers.at(0).frame_number, input_buffers.at(0).camera_id,
      input_buffers.at(0).stream_id, input_buffers.at(0).timestamp,
      input_buffers.at(1).frame_number, input_buffers.at(1).camera_id,
      input_buffers.at(1).stream_id, input_buffers.at(1).timestamp);

  NotifyBufferToClient(input_buffers[0]);
  NotifyBufferToClient(input_buffers[1]);
  return true;
}

status_t StitchingBase::FrameSync(StreamBuffer& buffer) {

  bool match_found;
  int32_t timestamp_delta;
  uint32_t num_matched_frames = 1;
  std::vector<StreamBuffer> *unsynced_buffers;
  // Map of camera id and index of the matched buffer from
  // the unsynced_buffers queue for that camera id.
  std::map<uint32_t, uint32_t> matched_buffers;

  // Each matched buffer for given camera will be added to the
  // synced_frames vector and identified by it's camera id.
  std::map<uint32_t, StreamBuffer> synced_frames;
  synced_frames.emplace(buffer.camera_id, buffer);

  // Iterate through the unsynced buffers for each camera, except current one.
  for (auto const& camera_id : params_.camera_ids) {
    if (camera_id == buffer.camera_id) {
      continue;
    }
    match_found = false;

    // Retrieve a list with unsynced buffers for each of the other cameras.
    unsynced_buffers = &unsynced_buffer_map_.at(camera_id);

    if (work_thread_name_ == "SnapshotStitching") {
      if (!unsynced_buffers->empty()) {
        const StreamBuffer &unsynced_frame = unsynced_buffers->at(0);
        synced_frames.emplace(camera_id, unsynced_frame);
        matched_buffers.emplace(camera_id, 0);
        ++num_matched_frames;
        match_found = true;
        break;
      }
    } else {
      // Backward search, as the latest buffers are at the back.
      for (int32_t idx = (unsynced_buffers->size() - 1); idx >= 0; --idx) {
        const StreamBuffer &unsynced_frame = unsynced_buffers->at(idx);
        timestamp_delta = buffer.timestamp - unsynced_frame.timestamp;

        if (std::abs(timestamp_delta) < kMaxTimestampDelta) {
          synced_frames.emplace(camera_id, unsynced_frame);
          matched_buffers.emplace(camera_id, idx);
          ++num_matched_frames;
          match_found = true;
          break;
        } else if (timestamp_delta > 0) {
          // No need to check the rest of the buffers in the queue for
          // this camera_id, as they will be with a lower timestamp.
          break;
        }
      }
    }
    // If a matched frame wasn't found there is no need to check
    // all other remaining cameras (if any).
    if (!match_found) {
      break;
    }
  }

  // Matched number of frames is not the same as the number of cameras.
  if (num_matched_frames != params_.camera_ids.size()) {
    QMMF_DEBUG("%s: Camera %u: No matching buffers found", __func__,
        buffer.camera_id);

    // Push the buffer in the unsynced buffer queue for its camera id.
    unsynced_buffer_map_.at(buffer.camera_id).push_back(buffer);

    // Check if the queue of current buffer camera_id has reached max size.
    unsynced_buffers = &unsynced_buffer_map_.at(buffer.camera_id);
    int32_t excess_buffers = unsynced_buffers->size() - kUnsyncedQueueMaxSize;

    if (excess_buffers > 0) {
      QMMF_DEBUG("%s: Camera %u: Unsynced buffer queue reached max "
          "size: %d",  __func__, buffer.camera_id, kUnsyncedQueueMaxSize);

      // Remove older excess buffers from the queue.
      for (int32_t i = 0; i < excess_buffers; ++i) {
        StreamBuffer &buf = unsynced_buffers->at(i);
        ReturnBufferToCamera(buf);
      }
      unsynced_buffers->erase(unsynced_buffers->begin(),
          unsynced_buffers->begin() + excess_buffers);
    }
    return FAILED_TRANSACTION;
  }

  // A matched frame(s) have been found, return all unsynced buffers and clear
  // the queue of the camera_id from which the synchronization buffer came.
  ReturnUnsyncedBuffers(buffer.camera_id);

  // Clear the obsolete unsynced buffers from queue of the matched cameras,
  // starting from beginning to the latest matched buffer and return them
  // back to their corresponding producers.
  for (auto& it : matched_buffers) {
    uint32_t camera_id = it.first;
    uint32_t match_idx = it.second;

    unsynced_buffers = &unsynced_buffer_map_.at(camera_id);
    unsynced_buffers->erase(unsynced_buffers->begin() + match_idx);
    for (uint32_t i = 0; i < match_idx; ++i) {
      StreamBuffer &buf = unsynced_buffers->at(i);
      ReturnBufferToCamera(buf);
    }
    unsynced_buffers->erase(unsynced_buffers->begin(),
        unsynced_buffers->begin() + match_idx);
  }

  std::lock_guard<std::mutex> lock(sync_lock_);
  synced_buffer_queue_.push(synced_frames);
  wait_for_sync_frames_.Signal();

  return NO_ERROR;
}

status_t StitchingBase::StopFrameSync() {

  status_t ret = NO_ERROR;
  {
    //First signal the thread to not wait on frames
    std::lock_guard<std::mutex> lock(sync_lock_);
    if (stop_frame_sync_ == true) {
      QMMF_DEBUG("%s: Worker thread is already in stopped state",
          __func__);
      return ret;
    }
    stop_frame_sync_ = true;
    wait_for_sync_frames_.Signal();
  }
  // We need to wait thread to exit to avoid ace between
  // flush and ongoing processing in the thread
  ThreadHelper::RequestExitAndWait();

  // Return all unsynced buffers back to the camera contexts.
  for (auto const& camera_id : params_.camera_ids) {
    ret = ReturnUnsyncedBuffers(camera_id);
    if (ret != NO_ERROR) {
      QMMF_ERROR("%s: Failed to return some of the unsynchronized buffers"
          " for camera %d!",  __func__, camera_id);
    }
  }
  {
    // Return all synced but unconsumed buffers back to the camera contexts.
    std::lock_guard<std::mutex> lock(sync_lock_);

    while (!synced_buffer_queue_.empty()) {
      for (auto const& id : params_.camera_ids) {
        StreamBuffer &buffer = synced_buffer_queue_.front().at(id);
        if (ReturnBufferToCamera(buffer) != NO_ERROR) {
          QMMF_ERROR("%s: Failed to return buffer %p for camera %d",
              __func__, buffer.handle, id);
        }
      }
      synced_buffer_queue_.pop();
    }
  }

  // Wait for all currently processed buffers to return.
  std::unique_lock<std::mutex> lock(buffers_lock_);

  return NO_ERROR;
}

status_t StitchingBase::ReturnUnsyncedBuffers(uint32_t camera_id) {

  std::vector<StreamBuffer> &buffers =
      unsynced_buffer_map_.at(camera_id);

  status_t ret = NO_ERROR;
  while (!buffers.empty()) {
    StreamBuffer &buf = buffers.back();
    ret = ReturnBufferToCamera(buf);
    if (NO_ERROR != ret) {
      QMMF_ERROR("%s: Failed to return buffer %p for camera %d",
          __func__, buf.handle, camera_id);
      return ret;
    }
    buffers.pop_back();
  }
  return ret;
}

}; //namespace recorder.

}; //namespace qmmf.
