/*
* Copyright (c) 2016-2021, The Linux Foundation. All rights reserved.
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

#define LOG_TAG "RecorderCameraSource"

#include <cmath>
#include <fcntl.h>
#include <dirent.h>
#include <sys/mman.h>
#include <sys/time.h>
#ifndef CAMERA_HAL1_SUPPORT
#include <hardware/camera3.h>
#endif

#include "recorder/src/service/qmmf_camera_source.h"
#include "recorder/src/service/qmmf_recorder_common.h"
#include "recorder/src/service/qmmf_recorder_utils.h"

#ifndef JPEG_BLOB_OFFSET
#define JPEG_BLOB_OFFSET (1)
#endif

namespace qmmf {

namespace recorder {

using ::std::make_shared;
using ::std::shared_ptr;

const uint32_t TrackSource::kWaitNumFrames_ = 5; //frames
static const nsecs_t kWaitDuration = 5000000000; // 5 s.
static const uint64_t kTsFactor = 10000000; // 10 ms.

CameraSource* CameraSource::instance_ = nullptr;

CameraSource* CameraSource::CreateCameraSource() {

  if (!instance_) {
    instance_ = new CameraSource;
    if (!instance_) {
      QMMF_ERROR("%s: Can't Create CameraSource Instance", __func__);
      //return nullptr;
    }
  }
  QMMF_INFO("%s: CameraSource Instance Created Successfully(0x%p)",
      __func__, instance_);
  return instance_;
}

CameraSource::CameraSource() {
  QMMF_GET_LOG_LEVEL();
  QMMF_KPI_GET_MASK();
  QMMF_KPI_DETAIL();
  QMMF_INFO("%s: Enter", __func__);
  QMMF_INFO("%s: Exit", __func__);
}

CameraSource::~CameraSource() {

  QMMF_KPI_DETAIL();
  QMMF_INFO("%s: Enter", __func__);
  camera_map_.clear();
  instance_ = nullptr;
  QMMF_INFO("%s: Exit (0x%p)", __func__, this);
}

status_t CameraSource::SetFlushCb(const uint32_t camera_id, FlushCb &cb) {

  if (camera_map_.count(camera_id) == 0) {
    QMMF_ERROR("%s: Camera Id(%d) is not opened!", __func__, camera_id);
    return BAD_VALUE;
  }

  // Register Encoder Flush Cb
  auto camera = camera_map_[camera_id];
  camera->SetFlushCb(cb);

  return NO_ERROR;
}

status_t CameraSource::FlushTrack(const uint32_t track_id) {

  QMMF_DEBUG("%s: Enter", __func__);

  if (!IsTrackIdValid(track_id)) {
    QMMF_ERROR("%s: Track(%x) does not exist !!", __func__, track_id);
    return NAME_NOT_FOUND;
  }
  auto const& track = track_sources_[track_id];
  track->ClearInputQueue();

  QMMF_DEBUG("%s: Exit", __func__);
  return NO_ERROR;
}

status_t CameraSource::StartCamera(const uint32_t camera_id,
                                   const float frame_rate,
                                   const CameraExtraParam& extra_param,
                                   const ResultCb &cb,
                                   const ErrorCb &errcb) {

  QMMF_INFO("%s: Camera Id(%u) to open!", __func__, camera_id);
  QMMF_KPI_DETAIL();
  std::shared_ptr<CameraInterface> camera;

  if (camera_map_.count(camera_id) != 0) {
    QMMF_ERROR("%s: Camera Id(%u) is already open!", __func__, camera_id);
    return BAD_VALUE;
  }
  camera = std::make_shared<CameraContext>();
  if (!camera.get()) {
    QMMF_ERROR("%s: Can't Instantiate CameraDevice(%d)!!",
        __func__, camera_id);
    return NO_MEMORY;
  }

  // Add contexts to map when in regular camera case.
  camera_map_.emplace(camera_id, camera);

  // This is required to send it to rescaler to take decision on UBWC.
  start_cam_param_ = extra_param;

  auto ret = camera->OpenCamera(camera_id, frame_rate, extra_param, cb, errcb);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s: OpenCamera(%d) Failed!", __func__, camera_id);
    camera = nullptr;
    if (camera_map_.count(camera_id) != 0) {
      camera_map_.erase(camera_id);
    }
    return ret;
  }
  QMMF_INFO("%s: Camera(%d) opened successfully!", __func__, camera_id);
  return NO_ERROR;
}

status_t CameraSource::StopCamera(const uint32_t camera_id) {

  QMMF_INFO("%s: CameraId(%u) to close!", __func__, camera_id);
  QMMF_KPI_DETAIL();

  //TODO: check if streams are still active, flush them before closing camera.

  if (camera_map_.count(camera_id) == 0) {
    QMMF_ERROR("%s: Invalid Camera Id(%d)", __func__, camera_id);
    return BAD_VALUE;
  }

  auto ret = camera_map_[camera_id]->CloseCamera(camera_id);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s: Failed to close camera(%d)!", __func__, camera_id);
    return FAILED_TRANSACTION;
  }
  camera_map_.erase(camera_id);
  QMMF_INFO("%s: Camera(%d) successfully closed!", __func__, camera_id);

  return NO_ERROR;
}

status_t CameraSource::CaptureImage(const uint32_t camera_id,
                                    const uint32_t num_images,
                                    const std::vector<CameraMetadata> &meta,
                                    const SnapshotCb& cb) {

  QMMF_DEBUG("%s: Enter", __func__);
  QMMF_KPI_DETAIL();

  if (camera_map_.count(camera_id) == 0) {
    QMMF_ERROR("%s: Invalid Camera Id(%d)", __func__, camera_id);
    return BAD_VALUE;
  }
  auto const& camera = camera_map_[camera_id];

  client_snapshot_cb_ = cb;
  StreamSnapshotCb stream_cb = [&] (uint32_t count, StreamBuffer& buf) {
    SnapshotCallback(count, buf);
  };
  auto ret = camera->CaptureImage(num_images, meta, stream_cb);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s: CaptureImage Failed!", __func__);
    return ret;
  }

  QMMF_DEBUG("%s: Exit", __func__);
  return NO_ERROR;
}

status_t CameraSource::ConfigImageCapture(const uint32_t camera_id,
                                          const ImageParam &param,
                                          const ImageConfigParam &config) {

  QMMF_DEBUG("%s: Enter", __func__);

  if (camera_map_.count(camera_id) == 0) {
    QMMF_ERROR("%s: Invalid Camera Id(%d)", __func__, camera_id);
    return BAD_VALUE;
  }
  auto const& camera = camera_map_[camera_id];

  auto ret = camera->ConfigImageCapture(config);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s: ConfigImageCapture Failed!", __func__);
    return ret;
  }

  SnapshotParam sparam {};
  sparam.width   = param.width;
  sparam.height  = param.height;
  sparam.format  = Common::FromImageToQmmfFormat(param.image_format);
  sparam.quality = param.image_quality;

  ret = camera->SetUpCapture(sparam);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s: SetUpCapture Failed!", __func__);
    return ret;
  }

  QMMF_DEBUG("%s: Exit", __func__);
  return NO_ERROR;
}

status_t CameraSource::CancelCaptureImage(const uint32_t camera_id) {

  QMMF_DEBUG("%s: Enter", __func__);
  QMMF_KPI_DETAIL();

  if (camera_map_.count(camera_id) == 0) {
    QMMF_ERROR("%s: Invalid Camera Id(%d)", __func__, camera_id);
    return BAD_VALUE;
  }
  auto const& camera = camera_map_[camera_id];

  auto ret = camera->CancelCaptureImage();
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s: CancelCaptureImage Failed!", __func__);
    return ret;
  }
  QMMF_DEBUG("%s: Exit", __func__);
  return NO_ERROR;
}

status_t CameraSource::ReturnAllImageCaptureBuffers(const uint32_t camera_id) {
  QMMF_DEBUG("%s: Enter", __func__);

  if (camera_map_.count(camera_id) == 0) {
    QMMF_ERROR("%s: Invalid Camera Id(%d)", __func__, camera_id);
    return BAD_VALUE;
  }
  auto const& camera = camera_map_[camera_id];

  auto ret = camera->ReturnAllImageCaptureBuffers();
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s: ReturnAllImageCaptureBuffers Failed!", __func__);
    return ret;
  }

  QMMF_DEBUG("%s: Exit", __func__);
  return ret;
}

status_t CameraSource::ReturnImageCaptureBuffer(const uint32_t camera_id,
                                                const int32_t buffer_id) {
  QMMF_DEBUG("%s: Enter", __func__);

  if (camera_map_.count(camera_id) == 0) {
    QMMF_ERROR("%s: Invalid Camera Id(%d)", __func__, camera_id);
    return BAD_VALUE;
  }
  auto const& camera = camera_map_[camera_id];

  auto ret = camera->ReturnImageCaptureBuffer(camera_id, buffer_id);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s: ReturnImageCaptureBuffer Failed!", __func__);
    return ret;
  }

  QMMF_DEBUG("%s: Exit", __func__);
  return ret;
}

int32_t CameraSource::GetSourceTrackId(const VideoExtraParam& extra_param) {

  if (extra_param.Exists(QMMF_SOURCE_VIDEO_TRACK_ID)) {
    SourceVideoTrack source_track;
    extra_param.Fetch(QMMF_SOURCE_VIDEO_TRACK_ID, source_track);
    return source_track.source_track_id;
  }
  return NAME_NOT_FOUND;
}

bool CameraSource::ValidateSlaveTrackParam(
    const VideoTrackParams& slave_track,
    const VideoTrackParams& master_track) {

  QMMF_DEBUG("%s %d x %d -> %d x %d fmt 0x%x -> 0x%x", __func__,
      master_track.params.width,
      master_track.params.height,
      slave_track.params.width,
      slave_track.params.height,
      (int32_t) master_track.params.format_type,
      (int32_t) slave_track.params.format_type);

  if ((slave_track.params.format_type != VideoFormat::kHEVC) &&
      (slave_track.params.format_type != VideoFormat::kAVC) &&
      (slave_track.params.format_type != VideoFormat::kNV12) &&
      (slave_track.params.format_type != VideoFormat::kNV12UBWC) &&
      (slave_track.params.format_type != VideoFormat::kRGB) &&
      (master_track.params.format_type != VideoFormat::kHEVC) &&
      (master_track.params.format_type != VideoFormat::kAVC) &&
      (master_track.params.format_type != VideoFormat::kNV12) &&
      (master_track.params.format_type != VideoFormat::kNV12UBWC) &&
      (master_track.params.format_type != VideoFormat::kRGB)) {
    QMMF_ERROR("%s Invalid format:", __func__);
    return false;
  }

  if((slave_track.params.width >  master_track.params.width) ||
      (slave_track.params.height > master_track.params.height)) {
    QMMF_ERROR("%s Invalid size:", __func__);
    return false;
  }
  return true;
}

VideoFormat CameraSource::GetYUVFormatType(VideoFormat format_type) {

  switch (format_type) {
    case VideoFormat::kHEVC:
    case VideoFormat::kAVC:
      format_type = VideoFormat::kNV12UBWC;
      break;
    case VideoFormat::kNV12:
    case VideoFormat::kNV12UBWC:
    case VideoFormat::kRGB:
    case VideoFormat::kJPEG:
    case VideoFormat::kBayerRDI8BIT:
    case VideoFormat::kBayerRDI10BIT:
    case VideoFormat::kBayerRDI12BIT:
    case VideoFormat::kBayerRDI16BIT:
    case VideoFormat::kBayerIdeal:
      break;
  }
  return format_type;
}

bool CameraSource::IsFormatChanged(VideoFormat src_format_type,
                     VideoFormat dst_format_type) {

  if (GetYUVFormatType(src_format_type) ==
      GetYUVFormatType(dst_format_type)) {
    return true;
  } else {
    return false;
  }
}

bool CameraSource::CheckLinkedStream(
    const VideoTrackParams& slave_track,
    const VideoTrackParams& master_track) {

  QMMF_DEBUG("%s %d x %d -> %d x %d fmt 0x%x -> 0x%x", __func__,
    master_track.params.width,
    master_track.params.height,
    slave_track.params.width,
    slave_track.params.height,
    (int32_t) master_track.params.format_type,
    (int32_t) slave_track.params.format_type);

  if ((slave_track.params.format_type != VideoFormat::kHEVC) &&
      (slave_track.params.format_type != VideoFormat::kAVC) &&
      (slave_track.params.format_type != VideoFormat::kNV12) &&
      (slave_track.params.format_type != VideoFormat::kNV12UBWC) &&
      (slave_track.params.format_type != VideoFormat::kRGB) &&
      (master_track.params.format_type != VideoFormat::kHEVC) &&
      (master_track.params.format_type != VideoFormat::kAVC) &&
      (master_track.params.format_type != VideoFormat::kNV12) &&
      (master_track.params.format_type != VideoFormat::kNV12UBWC) &&
      (master_track.params.format_type != VideoFormat::kRGB)) {
    QMMF_ERROR("%s Invalid format:", __func__);
    return false;
  }

  if((slave_track.params.width ==  master_track.params.width) &&
      (slave_track.params.height == master_track.params.height) &&
       IsFormatChanged(master_track.params.format_type,
                       slave_track.params.format_type)) {
    QMMF_ERROR("%s Same size and format.", __func__);
    return true;
  }
  return false;
}

ResizerCrop CameraSource::GetRescalerConfig(const VideoTrackParams& track_params) {

  ResizerCrop resizer_crop;
  if (track_params.extra_param.Exists(QMMF_TRACK_CROP)) {
    TrackCrop crop;
    track_params.extra_param.Fetch(QMMF_TRACK_CROP, crop);

    resizer_crop.x = crop.x;
    resizer_crop.y = crop.y;
    resizer_crop.width = crop.width;
    resizer_crop.height = crop.height;
    resizer_crop.valid = true;
    QMMF_INFO("%s Crop applied successfully!", __func__);
  } else {
    resizer_crop.valid = false;
    QMMF_INFO("%s Crop param doesn't exist so it's not applied!", __func__);
  }

  return resizer_crop;
}

status_t CameraSource::CreateTrackSource(const uint32_t track_id,
                                         const VideoTrackParams& track_params) {

  QMMF_DEBUG("%s: Enter", __func__);
  QMMF_KPI_DETAIL();

  auto camera_id = track_params.params.camera_id;
  if (camera_map_.count(camera_id) == 0) {
    QMMF_ERROR("%s: Invalid Camera Id(%d)", __func__, camera_id);
    return BAD_VALUE;
  }
  auto const& camera = camera_map_[camera_id];

  status_t ret = NO_ERROR;
  bool copy_stream_mode = false;
  bool linked_mode = false;
  VideoTrackParams source_track_params {};

  int32_t source_track_id = GetSourceTrackId(track_params.extra_param);
  if (source_track_id != NAME_NOT_FOUND) {
    QMMF_INFO("%s: Master->slave 0x%x->0x%x", __func__, source_track_id,
        track_id);

    assert(track_sources_.count(source_track_id) != 0);
    auto track = track_sources_[source_track_id];

    if (ValidateSlaveTrackParam(track_params, track->getParams())) {
      linked_mode = CheckLinkedStream(track_params, track->getParams());
      copy_stream_mode = true;
      source_track_params = track->getParams();
      QMMF_INFO("%s: Copy stream should be create.", __func__);
    } else {
      QMMF_ERROR("%s: Copy stream validation failed.", __func__);
    }
  } else {
    QMMF_INFO("%s: Normal stream should be create.", __func__);
  }

  // Create TrackSource and give it to CameraInterface, CameraConext in turn
  // would map it to its one of port.
  auto track_source = make_shared<TrackSource>(track_params, camera);
  if (!track_source.get()) {
    QMMF_ERROR("%s: Can't create TrackSource Instance", __func__);
    return NO_MEMORY;
  }

  shared_ptr<TrackSource> master_track;
  if (copy_stream_mode) {
    std::shared_ptr<CameraRescaler> rescaler;
    if (linked_mode == false) {
      rescaler = std::make_shared<CameraRescaler>();
      auto format =
          Common::FromVideoToQmmfFormat(track_params.params.format_type);
      ret = rescaler->Init(track_params.params.width,
                           track_params.params.height,
                           format,
                           source_track_params.params.frame_rate,
                           track_params.params.frame_rate,
                           start_cam_param_);
      if (ret != NO_ERROR) {
        rescaler = nullptr;
        QMMF_ERROR("%s: Rescaler Init Failed", __func__);
        return BAD_VALUE;
      }
      rescaler->Configure(GetRescalerConfig(track_params));
      rescalers_.emplace(track_id, rescaler);
    }

    assert(track_sources_.count(source_track_id) != 0);
    master_track = track_sources_[source_track_id];
    assert(master_track.get() != nullptr);

    ret = track_source->InitCopy(master_track, rescaler);
    if (ret != NO_ERROR) {
      QMMF_ERROR("%s: track_id(%x) TrackSource InitCopy failed!", __func__,
         track_id);
      rescalers_.erase(track_id);
      goto FAIL;
    }
  } else {
    if (track_params.params.format_type == VideoFormat::kRGB) {
      QMMF_ERROR("%s Unsupported format: RGB", __func__);
      goto FAIL;
    }
    ret = track_source->Init();
    if (ret != NO_ERROR) {
      QMMF_ERROR("%s: track_id(%x) TrackSource Init failed!", __func__,
          track_id);
      goto FAIL;
    }
  }

  track_sources_.emplace(track_id, track_source);

  QMMF_DEBUG("%s: Exit", __func__);
  return ret;
FAIL:
  track_source = nullptr;
  return ret;
}

status_t CameraSource::DeleteTrackSource(const uint32_t track_id) {

  if (!IsTrackIdValid(track_id)) {
    QMMF_ERROR("%s: Track(%x) does not exist !!", __func__, track_id);
    return BAD_VALUE;
  }
  auto const& track = track_sources_[track_id];

  auto ret = track->DeInit();
  assert(ret == NO_ERROR);

  track_sources_.erase(track_id);
  rescalers_.erase(track_id);

  QMMF_INFO("%s: track_id(%x) Deleted Successfully!", __func__,
      track_id);
  return ret;
}

status_t CameraSource::StartTrackSource(const uint32_t track_id) {

  QMMF_KPI_DETAIL();
  if (!IsTrackIdValid(track_id)) {
    QMMF_ERROR("%s: Track(%x) does not exist !!", __func__, track_id);
    return BAD_VALUE;
  }
  auto const& track = track_sources_[track_id];

  auto ret = track->StartTrack();
  assert(ret == NO_ERROR);

  QMMF_VERBOSE("%s: TrackSource id(%x) Started Successfully!", __func__,
      track_id);
  return ret;
}

status_t CameraSource::FlushTrackSource(const uint32_t track_id) {

  QMMF_KPI_DETAIL();
  if (!IsTrackIdValid(track_id)) {
    QMMF_ERROR("%s: Track(%x) does not exist !!", __func__, track_id);
    return BAD_VALUE;
  }
  auto const& track = track_sources_[track_id];

  auto ret = track->Flush();
  assert(ret == NO_ERROR);

  QMMF_VERBOSE("%s: TrackSource id(%x) Flush Buffers Successfully!", __func__,
      track_id);
  return ret;
}

status_t CameraSource::StopTrackSource(const uint32_t track_id) {

  QMMF_KPI_DETAIL();
  if (!IsTrackIdValid(track_id)) {
    QMMF_ERROR("%s: Track(%x) does not exist !!", __func__, track_id);
    return BAD_VALUE;
  }
  auto const& track = track_sources_[track_id];

  auto ret = track->StopTrack();
  assert(ret == NO_ERROR);

  QMMF_VERBOSE("%s: TrackSource id(%x) Stopped Successfully!", __func__,
      track_id);
  return ret;
}

status_t CameraSource::PauseTrackSource(const uint32_t track_id) {

  QMMF_KPI_DETAIL();
  if (!IsTrackIdValid(track_id)) {
    QMMF_ERROR("%s: Track(%x) does not exist !!", __func__, track_id);
    return BAD_VALUE;
  }
  auto const& track = track_sources_[track_id];

  auto ret = track->PauseTrack();
  assert(ret == NO_ERROR);

  QMMF_VERBOSE("%s: TrackSource id(%x) Paused Successfully!", __func__,
      track_id);
  return ret;
}

status_t CameraSource::ResumeTrackSource(const uint32_t track_id) {

  QMMF_KPI_DETAIL();
  if (!IsTrackIdValid(track_id)) {
    QMMF_ERROR("%s: Track(%x) does not exist !!", __func__, track_id);
    return BAD_VALUE;
  }
  auto const& track = track_sources_[track_id];

  auto ret = track->ResumeTrack();
  assert(ret == NO_ERROR);

  QMMF_VERBOSE("%s: TrackSource id(%x) Resumed Successfully!", __func__,
      track_id);
  return ret;
}

status_t CameraSource::ReturnTrackBuffer(const uint32_t track_id,
                                         std::vector<BnBuffer> &buffers) {

  if (!IsTrackIdValid(track_id)) {
    QMMF_ERROR("%s: Track(%x) does not exist !!", __func__, track_id);
    return BAD_VALUE;
  }
  auto const& track = track_sources_[track_id];

  auto ret = track->ReturnTrackBuffer(buffers);
  assert(ret == NO_ERROR);
  return ret;
}

status_t CameraSource::SetCameraParam(const uint32_t camera_id,
                                      const CameraMetadata &meta) {

  if (camera_map_.count(camera_id) == 0) {
    QMMF_ERROR("%s: Invalid Camera Id(%d)", __func__, camera_id);
    return BAD_VALUE;
  }
  return camera_map_[camera_id]->SetCameraParam(meta);
}

status_t CameraSource::GetCameraParam(const uint32_t camera_id,
                                      CameraMetadata &meta) {

  if (camera_map_.count(camera_id) == 0) {
    QMMF_ERROR("%s: Invalid Camera Id(%d)", __func__, camera_id);
    return BAD_VALUE;
  }
  return camera_map_[camera_id]->GetCameraParam(meta);
}

status_t CameraSource::GetDefaultCaptureParam(const uint32_t camera_id,
                                              CameraMetadata &meta) {

  if (camera_map_.count(camera_id) == 0) {
    QMMF_ERROR("%s: Invalid Camera Id(%d)", __func__, camera_id);
    return BAD_VALUE;
  }
  return camera_map_[camera_id]->GetDefaultCaptureParam(meta);
}

status_t CameraSource::GetCameraCharacteristics(const uint32_t camera_id,
                                                CameraMetadata &meta) {

  if (camera_map_.count(camera_id) == 0) {
    QMMF_ERROR("%s: Invalid Camera Id(%d)", __func__, camera_id);
    return BAD_VALUE;
  }
  return camera_map_[camera_id]->GetCameraCharacteristics(meta);
}

status_t CameraSource::UpdateTrackFrameRate(const uint32_t track_id,
                                            const float frame_rate) {

  if (!IsTrackIdValid(track_id)) {
    QMMF_ERROR("%s: Track(%x) does not exist !!", __func__, track_id);
    return BAD_VALUE;
  }
  auto const& track = track_sources_[track_id];

  track->UpdateFrameRate(frame_rate);
  return NO_ERROR;
}

status_t CameraSource::EnableFrameRepeat(const uint32_t track_id,
                                         const bool enable_frame_repeat) {

  if (!IsTrackIdValid(track_id)) {
    QMMF_ERROR("%s: Track(%x) does not exist !!", __func__, track_id);
    return BAD_VALUE;
  }
  auto const& track = track_sources_[track_id];

  track->EnableFrameRepeat(enable_frame_repeat);
  return NO_ERROR;
}

const shared_ptr<TrackSource>& CameraSource::GetTrackSource(uint32_t track_id) {

  assert(track_sources_.count(track_id) != 0);
  return track_sources_[track_id];
}

bool CameraSource::IsTrackIdValid(const uint32_t track_id) {

  QMMF_DEBUG("%s: Number of Tracks exist: %d",__func__, track_sources_.size());
  return (track_sources_.count(track_id) != 0) ? true : false;
}

uint32_t CameraSource::GetJpegSize(uint8_t *blobBuffer, uint32_t size) {

  uint32_t ret = size;
#ifndef CAMERA_HAL1_SUPPORT
  uint32_t blob_size = sizeof(struct camera3_jpeg_blob);

  if (size > blob_size) {
    size_t offset = size - blob_size - JPEG_BLOB_OFFSET;
    uint8_t *footer = blobBuffer + offset;
    struct camera3_jpeg_blob *jpegBlob = (struct camera3_jpeg_blob *)footer;

    if (CAMERA3_JPEG_BLOB_ID == jpegBlob->jpeg_blob_id) {
      ret = jpegBlob->jpeg_size;
    } else {
      QMMF_ERROR("%s Jpeg Blob structure missing!\n", __func__);
    }
  } else {
    QMMF_ERROR("%s Buffer size: %u equal or smaller than Blob size: %u\n",
        __func__, size, blob_size);
  }
#endif
  return ret;
}

status_t CameraSource::ParseThumb(uint8_t* vaddr, uint32_t size,
                                  StreamBuffer& buffer) {
  enum Tags { TAG = 0xFF, SOI = 0xD8, EOI = 0xD9, APP1 = 0xE1, APP2 = 0xE2 };
  enum TagSizeByte { MARKER_TAG_SIZE = 2, MARKER_LENGTH_SIZE = 2 };

  uint8_t thumb_num = 0;
  uint8_t *in_img = vaddr;
  uint32_t block_size = 0;
  uint32_t block_start = 0;
  uint32_t block_end = 0;
  CameraBufferMetaData info = buffer.info;

  // reset planes num
  info.num_planes = 0;
  info.plane_info[info.num_planes].offset = 0;
  info.plane_info[info.num_planes].size = size;
  info.num_planes++;

  QMMF_INFO("%s: Parse Thumbnail", __func__);

  for (uint32_t i = 0; i < size - 1; i++) {
    // search for marker
    if (in_img[i] == TAG) {
      // search for App1 and App2 marker
      if ((in_img[i + 1] == APP1) || (in_img[i + 1] == APP2)) {
        if (i >= size - 4) { // prevent bad access
          break;
        }

        block_size  = (256UL * in_img[i + 2]) + in_img[i + 3];
        block_start = i + MARKER_TAG_SIZE; // AppN marker is not part of block
        block_end   = block_start + block_size;

        // Skip App marker and size
        i += (1 + MARKER_LENGTH_SIZE);

      // Search for start of thumbnail or continue with multy segment thumbnail
      } else if (in_img[i + 1] == SOI && block_size) {

        uint32_t w_size = block_end - i;
        if (i + w_size > size) {
          QMMF_ERROR("%s: Unable to write. Overflow thumb file. %d > %d",
              __func__, i + w_size, size);
          break;
        }

        for (;;) {

          if (info.num_planes == MAX_PLANE) {
            QMMF_ERROR("%s: Fail to parse thumbnail num_plane: %d!!!", __func__,
                info.num_planes);
            return BAD_VALUE;
          }

          info.plane_info[info.num_planes].offset = i;
          info.plane_info[info.num_planes].size = w_size;
          info.num_planes++;

          // Move to end of block
          i += w_size;

          // Check for end of thumbnail
          if (in_img[i - 2] == TAG && in_img[i - 1] == EOI) {
            break;
          } else if (i + 4 < size && // prevent bad access
                     in_img[i] == TAG && in_img[i + 1] == APP2) {
            block_size  = (256UL * in_img[i + 2]) + in_img[i + 3];
            block_start = i + MARKER_TAG_SIZE;//AppN marker is not part of block
            block_end   = block_start + block_size;

            i = block_start + MARKER_LENGTH_SIZE; // Skip length
            w_size = block_end - i;
          } else {
            return BAD_VALUE;
          }
        }

        i--; // because of increment in main loop
        block_size = 0;
        block_end = 0;
        thumb_num++;
        // max supported thumbnails is 2
        if (thumb_num > 1) {
          break;
        }
      }
    }
  }

  // main image
  if (info.num_planes > 1) {
    info.plane_info[0].offset =
        info.plane_info[info.num_planes - 1].offset +
        info.plane_info[info.num_planes - 1].size;
    info.plane_info[0].size = size - info.plane_info[0].offset;
  }

  // restore plane info
  buffer.info = info;
  return NO_ERROR;
}

void CameraSource::SnapshotCallback(uint32_t count, StreamBuffer& buffer) {

  uint32_t content_size = 0;
  int32_t width = -1, height = -1;
  void* vaddr = nullptr;
  switch (buffer.info.format) {
    case BufferFormat::kNV12:
    case BufferFormat::kNV21:
    case BufferFormat::kNV16:
    case BufferFormat::kRAW8:
    case BufferFormat::kRAW10:
    case BufferFormat::kRAW12:
    case BufferFormat::kRAW16:
      width  = buffer.info.plane_info[0].width;
      height = buffer.info.plane_info[0].height;
      content_size = buffer.size;
      break;
    case BufferFormat::kBLOB:
      vaddr = mmap(nullptr, buffer.size, PROT_READ | PROT_WRITE, MAP_SHARED,
          buffer.fd, 0);
      assert(vaddr != nullptr);
      assert(0 < buffer.info.num_planes);
      content_size = GetJpegSize((uint8_t*) vaddr,
                                 buffer.info.plane_info[0].size);
      QMMF_INFO("%s: jpeg buffer size(%d)", __func__, content_size);
      assert(0 < content_size);
      if (buffer.second_thumb) {
        auto ret = ParseThumb(static_cast<uint8_t*>(vaddr),
                              content_size, buffer);
        if (ret != NO_ERROR) {
          QMMF_ERROR("%s: Warning: ParseThumb failed!!", __func__);
        }
      }

      if (vaddr) {
        munmap(vaddr, buffer.size);
        vaddr = nullptr;
      }
      width  = -1;
      height = -1;
    break;
    default:
      QMMF_ERROR("%s format(%d) not supported", __func__,
        (int32_t) buffer.info.format);
      assert(0);
    break;
  }

  BnBuffer bn_buffer{};
  bn_buffer.ion_fd      = buffer.fd;
  bn_buffer.ion_meta_fd = buffer.metafd;
  bn_buffer.size        = content_size;
  bn_buffer.timestamp   = buffer.timestamp;
  bn_buffer.width       = width;
  bn_buffer.height      = height;
  bn_buffer.buffer_id   = buffer.fd;
  bn_buffer.capacity    = buffer.size;

  MetaData meta_data{};
  meta_data.meta_flag = static_cast<uint32_t>(MetaParamType::kCamBufMetaData);
  meta_data.cam_buffer_meta_data = buffer.info;
  client_snapshot_cb_(buffer.camera_id, count, bn_buffer, meta_data);
}

TrackSource::TrackSource(const VideoTrackParams& params,
                         const std::shared_ptr<CameraInterface>& camera_intf)
    : track_params_(params),
      is_stop_(false),
      is_paused_(false),
      eos_acked_(false),
      is_idle_(true),
      input_count_(0),
      count_(0),
      pending_encodes_per_frame_ratio_(0.0),
      frame_repeat_ts_prev_(0),
      frame_repeat_ts_curr_(0),
      enable_frame_repeat_(0),
      fsc_(nullptr),
      frc_(nullptr),
      rescaler_(nullptr),
      slave_track_source_(false),
      time_lapse_mode_(false),
      time_stamp_(0),
      num_consumers_(0) {
  QMMF_GET_LOG_LEVEL();
  char prop[PROPERTY_VALUE_MAX];
  memset(prop, 0, sizeof(prop));
  property_get("persist.qmmf.yuv.dump.freq", prop, "0");
  yuv_dump_freq_ = atoi(prop);

  BufferConsumerImpl<TrackSource> *impl;
  impl = new BufferConsumerImpl<TrackSource>(this);
  buffer_consumer_impl_ = impl;

  BufferProducerImpl<TrackSource> *producer_impl;
  producer_impl = new BufferProducerImpl<TrackSource>(this);
  buffer_producer_impl_ = producer_impl;

  assert(camera_intf.get() != nullptr);
  camera_interface_ = camera_intf;

  output_frame_interval_ = 1000000.0 / track_params_.params.frame_rate;
  auto wait = output_frame_interval_ * 1000 * kWaitNumFrames_;
  wait_duration_ = wait < kWaitDuration ? kWaitDuration : wait;
  QMMF_INFO("%s: track_id(%x) wait_duration_:(%lld) ns",
      __func__, TrackId(), wait_duration_);
  std::stringstream track_name;
  track_name << "Track(" << std::hex << track_params_.track_id << ")";
  fsc_ = std::make_shared<FrameRateController>(
      "FrameSkip: " + track_name.str());
  assert(fsc_.get() != nullptr);
  fsc_->SetFrameRate(track_params_.params.frame_rate);

  frc_ = std::make_shared<FrameRateController>(
      "FrameRepeat: " + track_name.str());
  assert(frc_.get() != nullptr);
  frc_->SetFrameRate(track_params_.params.frame_rate);

  // TODO: There are issues related to how recorder service
  // treats the adb properties at runtime. Once it gets resolved,
  // the following lines for prop querying may be moved to
  // OnFrameAvailable.
  char prop_val[PROPERTY_VALUE_MAX];
  property_get(PROP_DEBUG_FPS, prop_val, "1");
  debug_fps_ = atoi(prop_val);

  if (track_params_.extra_param.Exists(QMMF_VIDEO_TIMELAPSE_INTERVAL) ) {
     VideoTimeLapse timestamp;
     track_params_.extra_param.Fetch(QMMF_VIDEO_TIMELAPSE_INTERVAL, timestamp);
     time_lapse_interval_ = timestamp.time_interval;
     time_lapse_mode_ = true;
     QMMF_INFO("%s: track_id(%x) TimeLapseMode enabled! interval(%u)",
        __func__, TrackId(), time_lapse_interval_);
  }
  QMMF_INFO("%s: TrackSource (0x%p)", __func__, this);
}

TrackSource::~TrackSource() {

  QMMF_INFO("%s: Enter ", __func__);

  QMMF_INFO("%s: Exit(0x%p) ", __func__, this);
}

status_t TrackSource::AddConsumer(const sp<IBufferConsumer>& consumer) {
  std::lock_guard<std::mutex> lock(consumer_lock_);
  if (consumer.get() == nullptr) {
    QMMF_ERROR("%s: Input consumer is nullptr", __func__);
    return BAD_VALUE;
  }

  buffer_producer_impl_->AddConsumer(consumer);
  consumer->SetProducerHandle(buffer_producer_impl_);

  num_consumers_ = buffer_producer_impl_->GetNumConsumer();

  QMMF_VERBOSE("%s: Consumer(%p) has been added.", __func__,
      consumer.get());
  return NO_ERROR;
}

status_t TrackSource::RemoveConsumer(sp<IBufferConsumer>& consumer) {
  std::lock_guard<std::mutex> lock(consumer_lock_);

  if(buffer_producer_impl_->GetNumConsumer() == 0) {
    QMMF_ERROR("%s: There are no connected consumers!", __func__);
    return INVALID_OPERATION;
  }

  buffer_producer_impl_->RemoveConsumer(consumer);

  num_consumers_ = buffer_producer_impl_->GetNumConsumer();

  return NO_ERROR;
}

status_t TrackSource::InitCopy(shared_ptr<TrackSource> master_track_source,
                               const std::shared_ptr<CameraRescaler>& rescaler) {

  QMMF_DEBUG("%s: Enter track_id(%x)", __func__, TrackId());
  status_t result = 0;
  assert(master_track_source.get() != nullptr);
  master_track_ = master_track_source;

  slave_track_source_ = true;

  if (rescaler.get() == nullptr) {
    QMMF_INFO("%s Linked stream", __func__);
  } else {
    rescaler_ = rescaler;
  }

  QMMF_DEBUG("%s: Exit track_id(%x)", __func__, TrackId());
  return result;
}

status_t TrackSource::Init() {

  QMMF_DEBUG("%s Enter track_id(%x)", __func__, TrackId());

  slave_track_source_ = false;
  rescaler_ = nullptr;
  master_track_ = nullptr;

  StreamParam param{};
  param.id = track_params_.track_id;
  param.width = track_params_.params.width;
  param.height = track_params_.params.height;
  param.framerate = track_params_.params.frame_rate;

  param.format =
      Common::FromVideoToQmmfFormat(track_params_.params.format_type);

  param.flags = ((track_params_.params.format_type == VideoFormat::kAVC) ||
      (track_params_.params.format_type == VideoFormat::kHEVC)) ?
          StreamFlags::kEncoded : StreamFlags::kNone;

  if (track_params_.extra_param.Exists(QMMF_CPU_CACHE)) {
    SystemCache mode;
    track_params_.extra_param.Fetch(QMMF_CPU_CACHE, mode, 0);
    param.flags |= mode.enable ? StreamFlags::kNone : StreamFlags::kUncashed;
  }

  if (track_params_.extra_param.Exists(QMMF_VIDEO_WAIT_AEC_MODE)) {
    VideoWaitAECMode wait_aec;
    track_params_.extra_param.Fetch(QMMF_VIDEO_WAIT_AEC_MODE, wait_aec);
    param.flags |= wait_aec.enable ? StreamFlags::kIAEC : StreamFlags::kNone;
  }

  if (track_params_.extra_param.Exists(QMMF_VIDEO_ROTATE)) {
    VideoRotate rotate;
    track_params_.extra_param.Fetch(QMMF_VIDEO_ROTATE, rotate);
    param.rotation = static_cast<int32_t> (rotate.flags);
  }

  assert(camera_interface_.get() != nullptr);
  auto ret = camera_interface_->CreateStream(param, track_params_.extra_param);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s: CreateStream failed!!", __func__);
    return BAD_VALUE;
  }

  QMMF_INFO("%s: TrackSource(0x%p)(%dx%d) and Camera Device Stream "
      " Created Successfully for track_id(%x)",  __func__, this,
      track_params_.params.width, track_params_.params.height, TrackId());

  QMMF_DEBUG("%s Exit track_id(%x)", __func__, TrackId());
  return ret;
}

status_t TrackSource::DeInit() {

  QMMF_DEBUG("%s Enter track_id(%x)", __func__, TrackId());
  assert(camera_interface_.get() != nullptr);
  status_t ret = NO_ERROR;

  if (slave_track_source_ == false) {
    ret = camera_interface_->DeleteStream(TrackId());
  }
  assert(ret == NO_ERROR);

  rescaler_ = nullptr;

  QMMF_DEBUG("%s Exit track_id(%x)", __func__, TrackId());
  return ret;
}

status_t TrackSource::StartTrack() {

  QMMF_DEBUG("%s: Enter track_id(%x)", __func__, TrackId());
  std::lock_guard<std::mutex> lock(lock_);

  assert(camera_interface_.get() != nullptr);

  std::lock_guard<std::mutex> stop_lock(stop_lock_);
  is_stop_ = false;

  std::lock_guard<std::mutex> eos_lock(eos_lock_);
  eos_acked_ = false;

  sp<IBufferConsumer> consumer;
  consumer = GetConsumerIntf();
  assert(consumer.get() != nullptr);

  status_t ret = NO_ERROR;
  ret = frc_->AddConsumer(consumer);
  assert(ret == NO_ERROR);
  consumer = frc_->GetConsumerIntf();
  assert(consumer.get() != nullptr);

  if (rescaler_.get() != nullptr) {
    ret = master_track_->AddConsumer(fsc_->GetConsumerIntf());
    assert(ret == NO_ERROR);
    ret = fsc_->AddConsumer(rescaler_->GetCopyConsumerIntf());
    assert(ret == NO_ERROR);
    ret = rescaler_->AddConsumer(consumer);
    assert(ret == NO_ERROR);
  } else if (slave_track_source_ == true) {
    assert(nullptr != fsc_);
    ret = master_track_->AddConsumer(fsc_->GetConsumerIntf());
    assert(ret == NO_ERROR);
    ret = fsc_->AddConsumer(consumer);
    assert(ret == NO_ERROR);
  }

  if (slave_track_source_ == false) {
    ret = camera_interface_->AddConsumer(TrackId(), fsc_->GetConsumerIntf());
    assert(ret == NO_ERROR);
    ret = fsc_->AddConsumer(consumer);
    assert(ret == NO_ERROR);
    ret = camera_interface_->StartStream(TrackId());
    assert(ret == NO_ERROR);
  }

  if (rescaler_.get() != nullptr) {
    ret = rescaler_->Start();
    assert(ret == NO_ERROR);
  }

  ret = fsc_->Start();
  assert(ret == NO_ERROR);

  ret = frc_->Start();
  assert(ret == NO_ERROR);

  QMMF_DEBUG("%s: Exit track_id(%x)", __func__, TrackId());
  return NO_ERROR;
}

status_t TrackSource::Flush() {
  status_t ret;

  QMMF_DEBUG("%s: Enter track_id(%x)", __func__, TrackId());

  if (track_params_.params.format_type == VideoFormat::kRGB ||
      track_params_.params.format_type == VideoFormat::kNV12 ||
      track_params_.params.format_type == VideoFormat::kNV12UBWC ||
      track_params_.params.format_type == VideoFormat::kBayerRDI8BIT ||
      track_params_.params.format_type == VideoFormat::kBayerRDI10BIT ||
      track_params_.params.format_type == VideoFormat::kBayerRDI12BIT ||
      track_params_.params.format_type == VideoFormat::kBayerIdeal) {

      QMMF_INFO("%s: track_id(%x) Force return buffers!", __func__, TrackId());
      std::lock_guard<std::mutex> lk(buffer_list_lock_);
      for (auto it = buffer_list_.begin(); it != buffer_list_.end(); it++) {
        StreamBuffer buffer = it->second;
        ReturnBufferToProducer(buffer);
      }
      buffer_list_.clear();

      std::lock_guard<std::mutex> idle_lock(idle_lock_);
      is_idle_ = true;
  }
  // else video encoder should return buffers

  QMMF_DEBUG("%s: Exit track_id(%x)", __func__, TrackId());
  return NO_ERROR;
}

status_t TrackSource::StopTrack() {
  status_t ret;

  QMMF_DEBUG("%s: Enter track_id(%x)", __func__, TrackId());
  is_paused_ = false;

  std::lock_guard<std::mutex> lock(lock_);
  {
    std::lock_guard<std::mutex> lock(stop_lock_);
    if (is_stop_ == true) {
      QMMF_WARN("%s: Track(%x) already stopped!", __func__, TrackId());
      return NO_ERROR;
    }
    is_stop_ = true;
  }
  // Stop sequence when encoder is involved.
  // 1. Send EOS to encoder with last valid buffer. If frames_received_ queue is
  //    empty and read thread is waiting for buffers then wait till next buffer
  //    is available then send EOS to encoder. once EOS is notified encoder will
  //    stop calling read method.
  // 2. Once EOS is acknowledged by encoder stop the camera port, which in turn
  //    will break port's connection with TrackSource.
  // 3. Return all the buffers back to camera port from frames_received_ queue
  //    if any. there are very less chances frames_received_ list wil have
  //    buffers after we send EOS to encoder and before we break the connection
  //    between CameraPort and TrackSource, it is very important to check
  //    otherwise camera adaptor will never go in idle state and as a side
  //    effect delete camera stream would fail.
  // 4. Once all buffers are returned at input port of encoder it will notify
  //    the status:kPortIdle, and at this point client's stop method can be
  //    returned.

  if (track_params_.params.format_type == VideoFormat::kRGB ||
      track_params_.params.format_type == VideoFormat::kNV12 ||
      track_params_.params.format_type == VideoFormat::kNV12UBWC ||
      track_params_.params.format_type == VideoFormat::kBayerRDI8BIT ||
      track_params_.params.format_type == VideoFormat::kBayerRDI10BIT ||
      track_params_.params.format_type == VideoFormat::kBayerRDI12BIT ||
      track_params_.params.format_type == VideoFormat::kBayerRDI16BIT ||
      track_params_.params.format_type == VideoFormat::kBayerIdeal) {

    // Encoder is not involved in this case.
    assert(camera_interface_.get() != nullptr);

    ret = frc_->Stop();
    assert(ret == NO_ERROR);

    ret = fsc_->Stop();
    assert(ret == NO_ERROR);

    if (rescaler_.get() != nullptr) {
      ret = rescaler_->Stop();
      assert(ret == NO_ERROR);
    }

    sp<IBufferConsumer> consumer = frc_->GetConsumerIntf();
    assert(consumer.get() != nullptr);

    if (slave_track_source_ == false) {
      ret = camera_interface_->StopStream(TrackId());
      assert(ret == NO_ERROR);
      ret = fsc_->RemoveConsumer(consumer);
      assert(ret == NO_ERROR);
      ret = camera_interface_->RemoveConsumer(TrackId(),
          fsc_->GetConsumerIntf());
      assert(ret == NO_ERROR);
    }

    if (rescaler_.get() != nullptr) {
      ret = rescaler_->RemoveConsumer(consumer);
      assert(ret == NO_ERROR);
      ret = fsc_->RemoveConsumer(rescaler_->GetCopyConsumerIntf());
      assert(ret == NO_ERROR);
      ret = master_track_->RemoveConsumer(fsc_->GetConsumerIntf());
      assert(ret == NO_ERROR);
    } else if (slave_track_source_ == true) {
      ret = fsc_->RemoveConsumer(consumer);
      assert(ret == NO_ERROR);
      ret = master_track_->RemoveConsumer(fsc_->GetConsumerIntf());
      assert(ret == NO_ERROR);
    }

    consumer = GetConsumerIntf();
    assert(consumer.get() != nullptr);
    ret = frc_->RemoveConsumer(consumer);
    assert(ret == NO_ERROR);

    QMMF_INFO("%s: Pipe stop done(%x)", __func__, TrackId());
    {
      std::lock_guard<std::mutex> lk(buffer_list_lock_);
      QMMF_DEBUG("%s: track_id(%x) buffer_list_.size(%d)", __func__,
          TrackId(), buffer_list_.size());
    }
  } else {
      QMMF_DEBUG("%s: track_id(%x), Wait for Encoder to return being encoded"
          " buffers!",  __func__, TrackId());
  }
  std::unique_lock<std::mutex> idle_lock(idle_lock_);
  std::chrono::nanoseconds wait_time(kWaitDuration);

  while (!is_idle_) {
    auto ret = wait_for_idle_.WaitFor(idle_lock, wait_time);
    if (ret != 0) {
      QMMF_ERROR("%s: track_id(%x) StopTrack Timed out happened! Encoder"
          " failed to go in Idle state!",  __func__, TrackId());
      return TIMED_OUT;
    }
  }
  QMMF_DEBUG("%s: Exit track_id(%x)", __func__, TrackId());
  return NO_ERROR;
}

status_t TrackSource::PauseTrack() {

  QMMF_DEBUG("%s: Enter track_id(%x)", __func__, TrackId());

  std::lock_guard<std::mutex> lock(lock_);
  is_paused_ = true;

  assert(camera_interface_.get() != nullptr);
  status_t ret = NO_ERROR;
  if (slave_track_source_ == false) {
    ret = camera_interface_->PauseStream(TrackId());
    assert(ret == NO_ERROR);
  }

  std::lock_guard<std::mutex> idle_lock(idle_lock_);
  is_idle_ = true;

  return NO_ERROR;
}

status_t TrackSource::ResumeTrack() {

  QMMF_DEBUG("%s: Enter track_id(%x)", __func__, TrackId());

  std::lock_guard<std::mutex> lock(lock_);
  is_paused_ = false;

  assert(camera_interface_.get() != nullptr);
  status_t ret = NO_ERROR;
  ret = camera_interface_->ResumeStream(TrackId());
  assert(ret == NO_ERROR);

  std::lock_guard<std::mutex> idle_lock(idle_lock_);
  is_idle_ = false;

  return NO_ERROR;
}

status_t TrackSource::NotifyPortEvent(PortEventType event_type,
                                      void* event_data) {

  QMMF_DEBUG("%s Enter track_id(%x)", __func__, TrackId());
  if (event_type == PortEventType::kPortStatus) {
    CodecPortStatus status = *(static_cast<CodecPortStatus*>(event_data));
    if (status == CodecPortStatus::kPortStop) {
      // Encoder Received the EOS with valid last buffer successfully, stop the
      // camera stream and clear the received buffer queue.
      QMMF_INFO("%s: track_id(%x) EOS acknowledged by Encoder!!",
          __func__, TrackId());
      ClearInputQueue();
      std::lock_guard<std::mutex> lock(eos_lock_);
      eos_acked_ = true;
    } else if (status == CodecPortStatus::kPortIdle) {
      QMMF_INFO("%s: track_id(%x) PortIdle acknowledged by Encoder!!",
          __func__, TrackId());
      ClearInputQueue();
      assert(camera_interface_.get() != nullptr);
      {
        std::unique_lock<std::mutex> lock(frame_lock_);
        for(auto it : buffer_map_) {
          if (stream_buffer_map_.find(it.first) !=  stream_buffer_map_.end()) {
            QMMF_INFO("%s: track_id(%x) fd: %d stream_id: %x", __func__,
                TrackId(), stream_buffer_map_[it.first].fd,
                stream_buffer_map_[it.first].stream_id);
            buffer_consumer_impl_->GetProducerHandle()->NotifyBufferReturned(
                stream_buffer_map_[it.first]);
          }
        }
      }
      status_t ret = NO_ERROR;
      ret = frc_->Stop();
      assert(ret == NO_ERROR);

      ret = fsc_->Stop();
      assert(ret == NO_ERROR);

      if (rescaler_.get() != nullptr) {
        ret = rescaler_->Stop();
        assert(ret == NO_ERROR);
      }

      sp<IBufferConsumer> consumer = frc_->GetConsumerIntf();
      assert(consumer.get() != nullptr);

      if (slave_track_source_ == false) {
        ret = camera_interface_->StopStream(TrackId());
        assert(ret == NO_ERROR);
        ret = fsc_->RemoveConsumer(consumer);
        assert(ret == NO_ERROR);
        ret = camera_interface_->RemoveConsumer(TrackId(),
            fsc_->GetConsumerIntf());
        assert(ret == NO_ERROR);
      }

      if (rescaler_.get() != nullptr) {
        ret = rescaler_->RemoveConsumer(consumer);
        assert(ret == NO_ERROR);
        ret = fsc_->RemoveConsumer(rescaler_->GetCopyConsumerIntf());
        assert(ret == NO_ERROR);
        ret = master_track_->RemoveConsumer(fsc_->GetConsumerIntf());
        assert(ret == NO_ERROR);
      } else if (slave_track_source_ == true) {
        ret = fsc_->RemoveConsumer(consumer);
        assert(ret == NO_ERROR);
        ret = master_track_->RemoveConsumer(fsc_->GetConsumerIntf());
        assert(ret == NO_ERROR);
      }

      consumer = GetConsumerIntf();
      assert(consumer.get() != nullptr);
      ret = frc_->RemoveConsumer(consumer);
      assert(ret == NO_ERROR);
      // All input port buffers from encoder are returned, Being encoded queue
      // should be zero at this point.
      assert(frames_being_encoded_.Size() == 0);
      QMMF_INFO("%s: track_id(%x) All queued buffers are returned from"
          " encoder!!",  __func__, TrackId());
      // wait_for_idle_ will not be needed once we make stop api as async.
      std::lock_guard<std::mutex> lock(idle_lock_);
      is_idle_ = true;
      wait_for_idle_.Signal();
    } else if (status == CodecPortStatus::kPortStart) {
      std::lock_guard<std::mutex> lock(idle_lock_);
      is_idle_ = false;
      QMMF_INFO("%s: Track(%x) encoder started",  __func__, TrackId());
    }
  }

  QMMF_DEBUG("%s Exit track_id(%x)", __func__, TrackId());
  return NO_ERROR;
}

status_t TrackSource::GetBuffer(BufferDescriptor& buffer,
                                void* client_data) {

  QMMF_DEBUG("%s Enter track_id(%x)", __func__, TrackId());
  bool timeout = false;

  {
    std::unique_lock<std::mutex> lock(frame_lock_);
    std::chrono::nanoseconds wait_time(GetWaitTime());
    while (frames_received_.Size() == 0) {
      QMMF_DEBUG("%s: track_id(%x) Wait for bufferr!!", __func__,
          TrackId());
      auto ret = wait_for_frame_.WaitFor(lock, wait_time);
      if (ret != 0 && IsPaused()) {
        continue;
      }
      if (ret != 0) {
          QMMF_ERROR("%s: track_id(%x) Buffer Timed out happend! No buffers"
              "from Camera",  __func__, TrackId());
        timeout = true;
        break;
      }
    }
    assert(timeout == false);

    QMMF_VERBOSE("%s: track_id(%x) frames_received_.size(%d)", __func__,
        TrackId(), frames_received_.Size());

    auto stream_buffer = frames_received_.Begin();
    buffer.data = const_cast<void*>(reinterpret_cast<const void*>(
                     GetAllocBufferHandle(stream_buffer->handle)));
    buffers_map_[buffer.data] = stream_buffer->handle;
    buffer.fd = stream_buffer->fd;
    buffer.capacity = stream_buffer->frame_length;
    buffer.size = stream_buffer->size;
    buffer.timestamp = stream_buffer->timestamp;
    buffer.flag = stream_buffer->flags;

    if (time_lapse_mode_) {
      stream_buffer->needs_return = true;
      frames_being_encoded_.PushBack((*stream_buffer));
      frames_received_.Erase(frames_received_.Begin());
      time_stamp_ += time_lapse_interval_ * 1000000;
      buffer.timestamp = time_stamp_;
    } else {
      buffer.timestamp = stream_buffer->timestamp;
      uint64_t ts_factor = (stream_buffer->timestamp - frame_repeat_ts_prev_) /
          stream_buffer->encodes_per_frame_count - kTsFactor;
      if (stream_buffer->encodes_per_frame_count !=
          stream_buffer->pending_encodes_per_frame) {
        buffer.timestamp = frame_repeat_ts_curr_ + ts_factor;
      }
      frame_repeat_ts_curr_ = buffer.timestamp;

      stream_buffer->pending_encodes_per_frame--;
      if (!stream_buffer->pending_encodes_per_frame) {
        stream_buffer->needs_return = true;
        frame_repeat_ts_prev_ = stream_buffer->timestamp;
      }
      frames_being_encoded_.PushBack((*stream_buffer));

      if (!stream_buffer->pending_encodes_per_frame) {
        frames_received_.Erase(frames_received_.Begin());
      }
    }
  }

  if (IsStop()) {
    QMMF_DEBUG("%s: track_id(%x) Send EOS to Encoder!", __func__,
        TrackId());
    // TODO defile EOS flag in AVCodec to delete EOS.
    return -1;
  }
  QMMF_DEBUG("%s Exit track_id(%x)", __func__, TrackId());
  return NO_ERROR;
}

status_t TrackSource::ReturnBuffer(BufferDescriptor& buffer,
                                   void* client_data) {

  QMMF_DEBUG("%s: Enter track_id(%x)", __func__, TrackId());

  QMMF_VERBOSE("%s: track_id(%x) frames_being_encoded_.size(%d)",
      __func__, TrackId(), frames_being_encoded_.Size());

  bool found = false;

  std::unique_lock<std::mutex> lock(frame_lock_);
  auto iter = frames_being_encoded_.Begin();
  for (; iter != frames_being_encoded_.End(); ++iter) {
    auto it = buffers_map_.find(buffer.data);
    if (it != buffers_map_.end() &&
       (*iter).handle ==  buffers_map_[buffer.data]) {
      QMMF_VERBOSE("%s: Buffer found in frames_being_encoded_ list!",
          __func__);

      if (iter->needs_return) {
        ReturnBufferToProducer((*iter));
      }

      frames_being_encoded_.Erase(iter);
      buffers_map_.erase(buffer.data);
      found = true;
      break;
    }
  }
  assert(found == true);
  QMMF_VERBOSE("%s: frames_being_encoded_.Size(%d)", __func__,
      frames_being_encoded_.Size());

  QMMF_DEBUG("%s Exit track_id(%x)", __func__, TrackId());
  return NO_ERROR;
}

void TrackSource::OnFrameAvailable(StreamBuffer& buffer) {

  QMMF_VERBOSE("%s: Enter track_id(%x)", __func__, TrackId());
  {
    std::unique_lock<std::mutex> lock(frame_lock_);
    buffer_map_.insert(std::make_pair(buffer.handle, 1));
    stream_buffer_map_.emplace(buffer.handle, buffer);
  }

#ifdef NO_FRAME_PROCESS
  ReturnBufferToProducer(buffer);
  return;
#endif

  {
    std::lock_guard<std::mutex> lock(eos_lock_);
    if (eos_acked_ && IsStop()) {
      auto const& track_format = track_params_.params.format_type;
      if (track_format == VideoFormat::kAVC ||
          track_format == VideoFormat::kHEVC ||
          track_format == VideoFormat::kJPEG) {
        // Return buffer if track is stoped and EOS is acknowledged by AVCodec.
        QMMF_INFO("%s: Track(%x) Stoped and eos is acked!", __func__,
          TrackId());
        std::unique_lock<std::mutex> lock(frame_lock_);
        ReturnBufferToProducer(buffer);
        return;
      }
    }
  }

  if (!time_lapse_mode_) {
    buffer.needs_return = false;
    buffer.pending_encodes_per_frame = CalculateEncodesPerFrame();
    buffer.encodes_per_frame_count = buffer.pending_encodes_per_frame;
  }

  QMMF_VERBOSE("%s: track_id(%x) ion_fd = %d", __func__, TrackId(),
      buffer.fd);
  QMMF_VERBOSE("%s: track_id(%x) size = %d", __func__, TrackId(),
      buffer.size);
  if (yuv_dump_freq_ > 0) {
    DumpYUV(buffer);
  }

  {
    std::lock_guard<std::mutex> lk(consumer_lock_);
    if (num_consumers_ > 0) {
      {
        std::unique_lock<std::mutex> lock(frame_lock_);
        auto val = buffer_map_.at(buffer.handle);
        buffer_map_[buffer.handle] = ++val;
      }
      buffer_producer_impl_->NotifyBuffer(buffer);
    }
  }

  if(IsPaused()) {
    std::lock_guard<std::mutex> lock(frame_lock_);
    ReturnBufferToProducer(buffer);
    return;
  }

  // If format type is YUV or BAYER then give callback from this point, do not
  // feed buffer to Encoder.
  if (track_params_.params.format_type == VideoFormat::kNV12 ||
      track_params_.params.format_type == VideoFormat::kNV12UBWC ||
      track_params_.params.format_type == VideoFormat::kRGB ||
      track_params_.params.format_type == VideoFormat::kBayerRDI8BIT ||
      track_params_.params.format_type == VideoFormat::kBayerRDI10BIT ||
      track_params_.params.format_type == VideoFormat::kBayerRDI12BIT ||
      track_params_.params.format_type == VideoFormat::kBayerRDI16BIT ||
      track_params_.params.format_type == VideoFormat::kBayerIdeal) {

    if (IsStop()) {
      QMMF_DEBUG("%s: track_id(%x) Stop is triggred, Stop giving raw buffer"
          " to client!",  __func__, TrackId());
      std::unique_lock<std::mutex> lock(frame_lock_);
      ReturnBufferToProducer(buffer);
      return;
    }

    BnBuffer bn_buffer{};
    bn_buffer.ion_fd            = buffer.fd;
    bn_buffer.ion_meta_fd       = buffer.metafd;
    bn_buffer.size              = buffer.size;
    bn_buffer.timestamp         = buffer.timestamp;
    bn_buffer.width             = buffer.info.plane_info[0].width;
    bn_buffer.height            = buffer.info.plane_info[0].height;
    bn_buffer.buffer_id         = buffer.fd;
    bn_buffer.flag              = 0x10;
    bn_buffer.capacity          = buffer.size;

    // Buffers from this list used for YUV callback.
    {
      std::lock_guard<std::mutex> autoLock(buffer_list_lock_);
      buffer_list_.insert(std::make_pair(buffer.fd, buffer));
    }
    {
      std::lock_guard<std::mutex> idle_lock(idle_lock_);
      is_idle_ = false;
    }
    std::vector<BnBuffer> bn_buffers;
    bn_buffers.push_back(bn_buffer);

    MetaData meta_data {};
    meta_data.meta_flag = static_cast<uint32_t>(MetaParamType::kCamBufMetaData);
    meta_data.meta_flag |= static_cast<uint32_t>
        (MetaParamType::kCamMetaFrameNumber);
    meta_data.cam_buffer_meta_data  = buffer.info;
    meta_data.cam_meta_frame_number = buffer.frame_number;

    std::vector<MetaData> meta_buffers;
    meta_buffers.push_back(meta_data);

    track_params_.data_cb(bn_buffers, meta_buffers);
  } else {
    // Push buffers into encoder queue.
    PushFrameToQueue(buffer);
  }
}

status_t TrackSource::ReturnTrackBuffer(std::vector<BnBuffer>& bn_buffers) {

  QMMF_DEBUG("%s: Enter track_id(%x)", __func__, TrackId());
  assert(bn_buffers.size() > 0);
  assert(buffer_consumer_impl_ != nullptr);

  std::unique_lock<std::mutex> lock(frame_lock_);
  for (size_t i = 0; i < bn_buffers.size(); ++i) {
    QMMF_VERBOSE("%s: track_id(%x) bn_buffers[%d].ion_fd=%d", __func__,
        TrackId(), i, bn_buffers[i].ion_fd);
    {
      std::lock_guard<std::mutex> autoLock(buffer_list_lock_);
      auto it = buffer_list_.find(bn_buffers[i].ion_fd);
      QMMF_DEBUG("%s: track_id(%x) Buffer fd(%d) found in list",
          __func__, TrackId(), bn_buffers[i].ion_fd);
      assert(it != buffer_list_.end());
      StreamBuffer buffer = it->second;
      ReturnBufferToProducer(buffer);
      buffer_list_.erase(it);
    }
  }
  QMMF_DEBUG("%s: Track(%x) buffer count still with client = %d", __func__,
      TrackId(), buffer_list_.size());

  if (buffer_list_.size() == 0) {
    std::lock_guard<std::mutex> lock(idle_lock_);
    // wait_for_idle_ will not be needed once we make stop api as async.
    QMMF_DEBUG("%s: Track(%x) All buffers have been returned from client!",
        __func__, TrackId());

    is_idle_ = true;
    wait_for_idle_.Signal();
  }

  QMMF_DEBUG("%s: Exit track_id(%x)", __func__, TrackId());
  return NO_ERROR;
}

void TrackSource::PushFrameToQueue(StreamBuffer& buffer) {

  QMMF_VERBOSE("%s: Enter track_id(%x)", __func__, TrackId());

  frames_received_.PushBack(buffer);
  QMMF_DEBUG("%s: track_id(%x) frames_received.size(%d)", __func__,
      TrackId(), frames_received_.Size());
  wait_for_frame_.Signal();

  QMMF_VERBOSE("%s: Exit track_id(%x)", __func__, TrackId());
}

bool TrackSource::IsStop() {

  QMMF_VERBOSE("%s: Enter track_id(%x)", __func__, TrackId());
  std::lock_guard<std::mutex> lock(stop_lock_);
  QMMF_VERBOSE("%s: Exit track_id(%x)", __func__, TrackId());
  return is_stop_;
}

bool TrackSource::IsPaused() {

  return is_paused_;
}

void TrackSource::ClearInputQueue() {

  QMMF_DEBUG("%s: Enter track_id(%x)", __func__, TrackId());
  std::unique_lock<std::mutex> lock(frame_lock_);
  // Once connection is broken b/w port and trackSoure there is no chance to
  // get new buffers in frames_received_ queue.
  uint32_t size = frames_received_.Size();
  QMMF_INFO("%s: track_id(%x):(%d) buffers to return from frames_received_",
      __func__, TrackId(), size);
  assert(buffer_consumer_impl_->GetProducerHandle().get() != nullptr);
  auto iter = frames_received_.Begin();
  for (; iter != frames_received_.End(); ++iter) {
    ReturnBufferToProducer((*iter));
  }
  frames_received_.Clear();
  QMMF_DEBUG("%s: Exit track_id(%x)", __func__, TrackId());
}

void TrackSource::UpdateFrameRate(const float frame_rate) {

  std::lock_guard<std::mutex> autoLock(frame_skip_lock_);
  assert(frame_rate > 0.0f);

  if (fabs(track_params_.params.frame_rate - frame_rate) > 0.1f) {
    fsc_->SetFrameRate(frame_rate);
    frc_->SetFrameRate(frame_rate);
    track_params_.params.frame_rate = frame_rate;
  }
}

uint64_t TrackSource::GetWaitTime(){
  std::lock_guard<std::mutex> autoLock(frame_skip_lock_);
  return wait_duration_;
}

void TrackSource::EnableFrameRepeat(const bool enable_frame_repeat) {

  std::lock_guard<std::mutex> lock(frame_repeat_lock_);
  enable_frame_repeat_ = enable_frame_repeat;
  frc_->EnableFrameRepeat(enable_frame_repeat);
}

uint32_t TrackSource::CalculateEncodesPerFrame() {

  std::lock_guard<std::mutex> lock(frame_repeat_lock_);
  uint32_t encodes_per_frame;

  if ((!enable_frame_repeat_) ||
      (input_frame_rate_ >= track_params_.params.frame_rate)) {
    return 1;
  }

  encodes_per_frame = static_cast<uint32_t>(input_frame_interval_) /
      output_frame_interval_;
  pending_encodes_per_frame_ratio_ +=
      (input_frame_interval_ / output_frame_interval_) - encodes_per_frame;

  if (pending_encodes_per_frame_ratio_ >= 1.0) {
    pending_encodes_per_frame_ratio_ -= 1.0;
    ++encodes_per_frame;
  }

  return encodes_per_frame;
}

void TrackSource::ReturnBufferToProducer(StreamBuffer& buffer) {
  QMMF_DEBUG("%s: Enter track_id(%x) fd: %d ts: %lld", __func__,
      TrackId(), buffer.fd, buffer.timestamp);

  if (buffer_map_.find(buffer.handle) == buffer_map_.end()) {
    QMMF_INFO("%s: Error track_id(%x) fd: %d ts: %lld", __func__,
         TrackId(), buffer.fd, buffer.timestamp);

  } else {
    QMMF_DEBUG("%s: Buffer is back to Producer Intf,buffer(0x%p) RefCount=%d",
        __func__, buffer.handle, buffer_map_.at(buffer.handle));
    if(buffer_map_.at(buffer.handle) == 1) {
      buffer_map_.erase(buffer.handle);
      // Return buffer back to actual owner.
      if (stream_buffer_map_.find(buffer.handle) != stream_buffer_map_.end()) {
        stream_buffer_map_.erase(buffer.handle);
      }
      buffer_consumer_impl_->GetProducerHandle()->NotifyBufferReturned(buffer);
    } else {
      // Hold this buffer, do not return until its ref count is 1.
      uint32_t value = buffer_map_.at(buffer.handle);
      buffer_map_[buffer.handle] = --value;
      if (num_consumers_ == 0 && rescaler_.get() != nullptr) {
        stream_buffer_map_.erase(buffer.handle);
        buffer_consumer_impl_->GetProducerHandle()->NotifyBufferReturned(buffer);
        buffer_map_.erase(buffer.handle);
      }
    }
  }
}

void TrackSource::NotifyBufferReturned(StreamBuffer& buffer) {
  QMMF_DEBUG("%s: Enter track_id(%x) fd: %d ts: %lld", __func__,
      TrackId(), buffer.fd, buffer.timestamp);
  std::unique_lock<std::mutex> lock(frame_lock_);
  ReturnBufferToProducer(buffer);
}

void TrackSource::DumpYUV(StreamBuffer& buffer) {

  static uint32_t id;
  ++id;

  if (id == yuv_dump_freq_) {

    void *buf_vaaddr = mmap(nullptr, buffer.size, PROT_READ  | PROT_WRITE,
                            MAP_SHARED, buffer.fd, 0);
    assert(buf_vaaddr != nullptr);

    std::string file_path(FRAME_DUMP_PATH);
    size_t written_len;
    file_path += "/track_";
    file_path += std::to_string(TrackId()) + "_";
    file_path += std::to_string(buffer.timestamp);
    file_path += ".yuv";

    FILE *file = fopen(file_path.c_str(), "w+");
    if (!file) {
      QMMF_ERROR("%s: Unable to open file(%s)", __func__,
          file_path.c_str());
      goto FAIL;
    }
    written_len = fwrite(buf_vaaddr, sizeof(uint8_t), buffer.size,
        file);
    QMMF_INFO("%s: written_len =%d", __func__, written_len);

    if (buffer.size != written_len) {
      QMMF_ERROR("%s: Bad Write error (%d):(%s)\n", __func__, errno,
          strerror(errno));
        goto FAIL;
    }
    QMMF_INFO("%s: Buffer(0x%p) Size(%u) Stored(%s)\n",__func__,
        buf_vaaddr, written_len, file_path.c_str());

FAIL:
    if (file != nullptr) {
      fclose(file);
    }
    if (buf_vaaddr != nullptr) {
      munmap(buf_vaaddr, buffer.size);
      buf_vaaddr = nullptr;
    }
    id = 0;
  }
}

}; //namespace recorder

}; //namespace qmmf
