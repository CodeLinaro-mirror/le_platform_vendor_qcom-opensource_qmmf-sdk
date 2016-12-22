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

#pragma once

#include <utils/KeyedVector.h>
#include <utils/Log.h>
#include <libgralloc/gralloc_priv.h>

#include "recorder/src/service/qmmf_camera_context.h"
#include "recorder/src/service/qmmf_recorder_utils.h"
#include "recorder/src/service/qmmf_recorder_common.h"

namespace qmmf {

namespace recorder {

static const uint32_t kVirtualCameraIdOffset = 1000;

class MultiCameraManager : public CameraInterface {
 public:
  MultiCameraManager();

  ~MultiCameraManager();

  // This Api will map actaul camera Ids to virtual camera id.
  status_t CreateMultiCamera(const std::vector<uint32_t> camera_ids,
                             uint32_t* virtual_camera_id);

  status_t ConfigureMultiCamera(uint32_t virtual_camera_id,
                                /*MultiCameraConfigTypes*/ uint32_t type,
                                void *param, size_t param_size);

  status_t OpenCamera(const uint32_t camera_id, const CameraStartParam &param,
                      const ResultCb &cb = nullptr) override;

  status_t CloseCamera(const uint32_t camera_id) override;

  status_t CaptureImage(const ImageParam &param, const uint32_t num_images,
                        const std::vector<CameraMetadata> &meta,
                        const SnapshotCb& cb) override;

  status_t CreateStream(const CameraStreamParam& param) override;

  status_t DeleteStream(const uint32_t track_id) override;

  status_t StartStream(const uint32_t track_id,
                       sp<IBufferConsumer>& consumer) override;

  status_t StopStream(const uint32_t track_id) override;

  status_t SetCameraParam(const CameraMetadata &meta) override;

  status_t GetCameraParam(CameraMetadata &meta) override;

  status_t GetDefaultCaptureParam(CameraMetadata &meta) override;

  status_t ReturnImageCaptureBuffer(const uint32_t camera_id,
                                    const int32_t buffer_id) override;

  CameraStartParam& GetCameraStartParam() override;

  Vector<int32_t>& GetSupportedFps() override;

 private:
  void SnapshotCbCam(uint32_t camera_id, uint32_t count, BnBuffer& buffer,
                     MetaData& meta_data);

  uint32_t                 virtual_camera_id_;
  CameraStartParam         multicam_start_params_;
  Vector<int32_t>          supported_fps_;

  // map of virtual camera id and its corresponding actual camera Ids.
  // <virtual camera id, Vector of actual camera id >
  KeyedVector<uint32_t, Vector<uint32_t> > virtual_camera_map_;

  // Map of camera id and CameraContext.
  KeyedVector<uint32_t, sp<CameraContext>> camera_contexts_;

  SnapshotCb               source_snapshot_cb_;
  Mutex                    lock_;
};

}; // recorder.

}; // qmmf.
