/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
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

#define TAG "RecorderMultiCameraManager"

#include <algorithm>
#include <fcntl.h>
#include <sys/mman.h>
#include <QCamera3VendorTags.h>

#include "recorder/src/service/qmmf_multicamera_manager.h"
#include "recorder/src/service/qmmf_camera_context.h"
#include "recorder/src/service/qmmf_recorder_utils.h"

namespace qmmf {

namespace recorder {

MultiCameraManager::MultiCameraManager()
  : virtual_camera_id_(kVirtualCameraIdOffset),
    source_snapshot_cb_(nullptr) {}

MultiCameraManager::~MultiCameraManager() {}

status_t MultiCameraManager::CreateMultiCamera(const std::vector<uint32_t>
                                               camera_ids,
                                               uint32_t* virtual_camera_id) {

  QMMF_INFO("%s:%s: Enter", TAG, __func__);
  if (camera_ids.size() < 2) {
    return BAD_VALUE;
  }
  QMMF_INFO("%s:%s: Number of camera to be used(%d)", TAG, __func__,
    camera_ids.size());

  Vector<uint32_t> ids;
  for (uint32_t i = 0; i < camera_ids.size(); ++i) {
    QMMF_INFO("%s:%s camera id=%d", TAG, __func__, camera_ids[i]);
    ids.push_back(camera_ids[i]);
  }
  ++virtual_camera_id_;
  virtual_camera_map_.add(virtual_camera_id_, ids);
  *virtual_camera_id = virtual_camera_id_;

  QMMF_INFO("%s:%s: Exit", TAG, __func__);
  return NO_ERROR;
}

// TODO: define MultiCameraConfigTypes in qmmf_recorder_param.h
status_t MultiCameraManager::ConfigureMultiCamera(uint32_t virtual_camera_id,
                                                  /*MultiCameraConfigTypes*/
                                                  uint32_t type,
                                                  void *param,
                                                  size_t param_size) {
  //TODO:
  return NO_ERROR;
}

status_t MultiCameraManager::OpenCamera(const uint32_t virtual_camera_id,
                                        const CameraStartParam &param,
                                        const ResultCb &cb) {

  QMMF_INFO("%s:%s: Enter", TAG, __func__);
  status_t ret = NO_ERROR;

  ssize_t idx = virtual_camera_map_.indexOfKey(virtual_camera_id);
  if (NAME_NOT_FOUND == idx) {
    QMMF_ERROR("%s:%s: Invalid virtual camera ID!", TAG, __func__);
    return BAD_VALUE;
  }
  Vector<uint32_t> camera_ids = virtual_camera_map_.valueFor(virtual_camera_id);
  QMMF_INFO("%s:%s: Total Number of cameras to be open(%d)", TAG, __func__,
      camera_ids.size());

  for (auto const& cam_id : camera_ids) {
    if (camera_contexts_.indexOfKey(cam_id) >= 0) {
      QMMF_WARN("%s:%s: Camera Id(%u) is already open, skipping!", TAG,
                 __func__, cam_id);
      continue;
    }
    QMMF_INFO("%s:%s camera id(%d) to be open", TAG, __func__, cam_id);

    sp<CameraContext> camera_context = new CameraContext();
    ret = camera_context->OpenCamera(cam_id, param);
    if (ret != NO_ERROR) {
      QMMF_ERROR("%s:%s: OpenCamera(%d) failed!", TAG, __func__, cam_id);
      camera_context.clear();
      return NO_INIT;
    }
    camera_contexts_.add(cam_id, camera_context);
  }

  multicam_start_params_ = param;
  supported_fps_ = camera_contexts_.valueAt(0)->GetSupportedFps();

  QMMF_INFO("%s:%s: Exit", TAG, __func__);
  return ret;
}

status_t MultiCameraManager::CloseCamera(const uint32_t virtual_camera_id) {

  QMMF_INFO("%s:%s: Enter", TAG, __func__);
  status_t ret = NO_ERROR;
  bool closing_failed = false;

  ssize_t idx = virtual_camera_map_.indexOfKey(virtual_camera_id);
  if (NAME_NOT_FOUND == idx) {
    QMMF_ERROR("%s:%s: Invalid virtual camera ID!", TAG, __func__);
    return BAD_VALUE;
  }
  Vector<uint32_t> camera_ids = virtual_camera_map_.valueFor(virtual_camera_id);
  QMMF_INFO("%s:%s: Total Number of cameras to be closed(%d)", TAG, __func__,
      camera_ids.size());

  for (auto const& cam_id : camera_ids) {
    QMMF_INFO("%s:%s camera id(%d) to be closed", TAG, __func__, cam_id);

    sp<CameraContext> camera_context = camera_contexts_.valueFor(cam_id);
    ret = camera_context->CloseCamera(cam_id);
    if (ret != NO_ERROR) {
      QMMF_ERROR("%s:%s: CloseCamera(%d) failed!", TAG, __func__, cam_id);
      closing_failed = true;
      camera_context.clear();
    }
    camera_contexts_.removeItem(cam_id);
  }

  QMMF_INFO("%s:%s: Exit", TAG, __func__);
  return closing_failed ? UNKNOWN_ERROR : NO_ERROR;
}

status_t MultiCameraManager::CaptureImage(const ImageParam &param,
                                          const uint32_t num_images,
                                          const std::vector<CameraMetadata>
                                          &meta, const SnapshotCb& cb) {
  Mutex::Autolock lock(lock_);
  status_t ret = NO_ERROR;
  source_snapshot_cb_ = cb;
  //Create callback for multi camera.
  SnapshotCb cb_multi_cam = [&] (uint32_t camera_id, uint32_t count,
      BnBuffer& buf, MetaData& meta_data) {
    SnapshotCbCam(camera_id, count, buf, meta_data);
  };

  for (size_t i = 0; i < camera_contexts_.size(); i++) {
    sp<CameraContext> camera_context = camera_contexts_.valueAt(i);
    assert(camera_context.get() != nullptr);
    ret = camera_context->CaptureImage(param, num_images, meta, cb_multi_cam);
    if (ret != NO_ERROR) {
      QMMF_ERROR("%s:%s: CaptureImage Failed!", TAG, __func__);
      return ret;
    }
  }
  return NO_ERROR;
}

status_t MultiCameraManager::CreateStream(const CameraStreamParam& param) {
  status_t ret = NO_ERROR;
  for (size_t i = 0; i < camera_contexts_.size(); i++) {
    sp<CameraContext> camera_context = camera_contexts_.valueAt(i);
    assert(camera_context.get() != nullptr);
    ret = camera_context->CreateStream(param);
    if (ret != NO_ERROR) {
      QMMF_ERROR("%s:%s: CreateStream Failed!", TAG, __func__);
      return ret;
    }
  }
  return ret;
}

status_t MultiCameraManager::DeleteStream(const uint32_t track_id) {
  status_t ret = NO_ERROR;
  for (size_t i = 0; i < camera_contexts_.size(); i++) {
    sp<CameraContext> camera_context = camera_contexts_.valueAt(i);
    assert(camera_context.get() != nullptr);
    ret = camera_context->DeleteStream(track_id);
    if (ret != NO_ERROR) {
      QMMF_ERROR("%s:%s: DeleteStream Failed!", TAG, __func__);
      return ret;
    }
  }
  return ret;
}

status_t MultiCameraManager::StartStream(const uint32_t track_id,
                                         sp<IBufferConsumer>& consumer) {
  status_t ret = NO_ERROR;
  for (size_t i = 0; i < camera_contexts_.size(); i++) {
    sp<CameraContext> camera_context = camera_contexts_.valueAt(i);
    assert(camera_context.get() != nullptr);
    ret = camera_context->StartStream(track_id, consumer);
    if (ret != NO_ERROR) {
      QMMF_ERROR("%s:%s: StartStream Failed!", TAG, __func__);
      return ret;
    }
  }
  return ret;
}

status_t MultiCameraManager::StopStream(const uint32_t track_id) {
  status_t ret = NO_ERROR;
  for (size_t i = 0; i < camera_contexts_.size(); i++) {
    sp<CameraContext> camera_context = camera_contexts_.valueAt(i);
    assert(camera_context.get() != nullptr);
    ret = camera_context->StopStream(track_id);
    if (ret != NO_ERROR) {
      QMMF_ERROR("%s:%s: StopStream Failed!", TAG, __func__);
      return ret;
    }
  }
  return ret;
}

status_t MultiCameraManager::SetCameraParam(const CameraMetadata &meta) {

  //One of the cameras will be master cam that's why we need
  //to set the params only for one camera.
  sp<CameraContext> camera_context = camera_contexts_.valueAt(0);
  assert(camera_context.get() != nullptr);
  status_t ret = camera_context->SetCameraParam(meta);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s:%s: SetCameraParam Failed!", TAG, __func__);
  }
  return ret;
}

status_t MultiCameraManager::GetCameraParam(CameraMetadata &meta) {

  sp<CameraContext> camera_context = camera_contexts_.valueAt(0);
  assert(camera_context.get() != nullptr);
  status_t ret = camera_context->GetCameraParam(meta);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s:%s: GetCameraParam Failed!", TAG, __func__);
  }
  return ret;
}

status_t MultiCameraManager::GetDefaultCaptureParam(CameraMetadata &meta) {

  sp<CameraContext> camera_context = camera_contexts_.valueAt(0);
  assert(camera_context.get() != nullptr);
  status_t ret = camera_context->GetDefaultCaptureParam(meta);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s:%s: GetDefaultCaptureParam Failed!", TAG, __func__);
  }
  return ret;
}

status_t MultiCameraManager::ReturnImageCaptureBuffer(const uint32_t virtual_camera_id,
                                                      const int32_t buffer_id) {
  status_t ret = NO_ERROR;
  ssize_t idx = virtual_camera_map_.indexOfKey(virtual_camera_id);
  assert(idx >= 0);
  if (NAME_NOT_FOUND == idx) {
    // virtual camera id is not correct.
    return BAD_VALUE;
  }
  Vector<uint32_t> camera_ids;
  camera_ids = virtual_camera_map_.valueFor(virtual_camera_id);
  for (uint32_t i = 0; i < camera_ids.size(); ++i) {
    sp<CameraContext> camera_context = camera_contexts_.valueAt(i);
    assert(camera_context.get() != nullptr);
    ret = camera_context->ReturnImageCaptureBuffer(camera_ids[i], buffer_id);
    if (ret != NO_ERROR) {
      QMMF_ERROR("%s:%s: ReturnImageCaptureBuffer Failed!", TAG, __func__);
      return ret;
    }
  }
  return ret;
}

CameraStartParam& MultiCameraManager::GetCameraStartParam() {

  return multicam_start_params_;
}

Vector<int32_t>& MultiCameraManager::GetSupportedFps() {

  return supported_fps_;
}

// TODO: The callback for camera source should be called only after the frames
//       from the two cameras have been synced. After that the Stitching algo
//       will be called & the output buffer will be passed to camera source cb.
void MultiCameraManager::SnapshotCbCam(uint32_t camera_id, uint32_t count,
                                       BnBuffer& buffer, MetaData& meta_data) {
  Mutex::Autolock lock(lock_);
  QMMF_INFO("%s:%s: SnapshotCbCam camera_id: %d", TAG, __func__, camera_id);
  source_snapshot_cb_(camera_id, count, buffer, meta_data);
}

}; //namespace recorder.

}; //namespace qmmf.
