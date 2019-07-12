/*
* Copyright (c) 2018, 2019, The Linux Foundation. All rights reserved.
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
#include <mutex>

#include <qmmf-sdk/qmmf_recorder_params.h>
#include <qmmf-sdk/qmmf_recorder_extra_param_tags.h>

#include "common/utils/qmmf_condition.h"

#include "recorder/src/service/qmmf_camera_interface.h"
#include "recorder/src/service/post-process/memory/qmmf_postproc_memory_pool.h"

namespace qmmf {

namespace recorder {

class FakeCamera : public CameraInterface {
 public:
  FakeCamera();

  ~FakeCamera();

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

  void NotifyBufferReturned(StreamBuffer& buffer);

  void SetFlushCb(FlushCb &cb) override;

private:

  static void *StreamThreadLoop(void *userdata);

  status_t GetCameraBuffer(StreamBuffer& buffer);

  status_t FillBuffer(StreamBuffer& buffer);

  status_t MapBuf(StreamBuffer& buffer);

  void UnMapBufs();

  struct map_data_t {
    void* addr;
    size_t size;
  };

  uint32_t                 camera_id_;
  int32_t                  stream_id_;
  int32_t                  stream_id_count_;
  CameraStartParam         camera_start_params_;
  StreamParam              stream_param_;
  ResultCb                 result_cb_;
  ErrorCb                  error_cb_;

  std::atomic_bool         streamon_;
  std::atomic_bool         abort_;

  sp<IBufferProducer>      buffer_producer_impl_;
  CameraMetadata           metadata_;
  std::thread              *stream_thread_;
  int32_t                  frame_number_;

  MemPool                  mem_pool_;
  MemPoolParams            mem_pool_params_;

  char                     *buffer_data_;
  std::map<uint32_t, map_data_t>    mapped_buffs_;
  int32_t                  buffer_count_;

  std::mutex               buffer_lock_;
  QCondition               wait_for_buffer_;

  static std::vector<int32_t> supported_fps_;

  static const std::string kCameraBufferPath;
  static const uint32_t    kBufferWaitTimeout;
};

}; //namespace recorder

}; //namespace qmmf
