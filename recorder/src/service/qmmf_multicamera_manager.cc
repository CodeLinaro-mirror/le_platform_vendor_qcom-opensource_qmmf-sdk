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
  : virtual_camera_id_(0x5DC) {}

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
    // virtual camera id is not correct.
    return BAD_VALUE;
  }
  Vector<uint32_t> camera_ids;
  camera_ids = virtual_camera_map_.valueFor(virtual_camera_id);
  QMMF_INFO("%s:%s: Total Number of cameras to be open(%d)", TAG, __func__,
      camera_ids.size());

  for (uint32_t i = 0; i < camera_ids.size(); ++i) {
    QMMF_INFO("%s:%s camera id(%d) to be open", TAG, __func__, camera_ids[i]);
    sp<CameraContext> camera_context;
    camera_context = new CameraContext();
    auto ret = camera_context->OpenCamera(camera_ids[i], param);
    if(ret != NO_ERROR) {
      QMMF_ERROR("%s:%s: OpenCamera(%d) failed!", TAG, __func__, camera_ids[i]);
      camera_context.clear();
      camera_context = nullptr;
      return NO_INIT;
    }
    camera_contexts_.add(camera_ids[i], camera_context);
  }

  QMMF_INFO("%s:%s: Exit", TAG, __func__);
  return ret;
}

status_t MultiCameraManager::CloseCamera(const uint32_t camera_id) {

  //TODO
  return NO_ERROR;
}

status_t MultiCameraManager::CaptureImage(const ImageParam &param,
                                          const uint32_t num_images,
                                          const std::vector<CameraMetadata>
                                          &meta, const SnapshotCb& cb) {
  //TODO
  return NO_ERROR;
}

status_t MultiCameraManager::CreateStream(const CameraStreamParam& param) {

  //TODO
  return NO_ERROR;
}

status_t MultiCameraManager::DeleteStream(const uint32_t track_id) {

  //TODO
  return NO_ERROR;
}

status_t MultiCameraManager::StartStream(const uint32_t track_id,
                                         sp<IBufferConsumer>& consumer) {

  //TODO
  return NO_ERROR;
}

status_t MultiCameraManager::StopStream(const uint32_t track_id) {

  //TODO
  return NO_ERROR;
}

status_t MultiCameraManager::SetCameraParam(const CameraMetadata &meta) {

  //TODO
  return NO_ERROR;
}

status_t MultiCameraManager::GetCameraParam(CameraMetadata &meta) {

  //TODO
  return NO_ERROR;
}

status_t MultiCameraManager::GetDefaultCaptureParam(CameraMetadata &meta) {

  //TODO
  return NO_ERROR;
}

status_t MultiCameraManager::ReturnImageCaptureBuffer(const uint32_t camera_id,
                                                      const int32_t buffer_id) {
  //TODO
  return NO_ERROR;
}

CameraStartParam& MultiCameraManager::GetCameraStartParam() {

  //TODO
  return multicam_start_params_;
}

Vector<int32_t>& MultiCameraManager::GetSupportedFps() {

  //TODO
  return supported_fps_;
}

}; //namespace recorder.

}; //namespace qmmf.
