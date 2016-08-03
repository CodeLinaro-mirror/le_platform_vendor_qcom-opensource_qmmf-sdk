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

#define TAG "RecorderCameraSource"

#include <sys/time.h>
#include <math.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/mman.h>

#include "recorder/src/service/qmmf_camera_source.h"
#include "recorder/src/service/qmmf_recorder_common.h"
#include "recorder/src/service/qmmf_recorder_utils.h"

namespace qmmf {

namespace recorder {

CameraSource* CameraSource::instance_ = NULL;

CameraSource* CameraSource::CreateCameraSource() {

  if(!instance_) {
    instance_ = new CameraSource;
    if(!instance_) {
      QMMF_ERROR("%s:%s: Can't Create CameraSource Instance", TAG, __func__);
      //return NULL;
    }
  }
  QMMF_INFO("%s:%s: CameraSource Instance Created Successfully(0x%x)", TAG,
      __func__, instance_);
  return instance_;
}

CameraSource::CameraSource() {

  QMMF_INFO("%s:%s: Enter", TAG, __func__);
  QMMF_INFO("%s:%s: Exit", TAG, __func__);
}

CameraSource::~CameraSource() {

  QMMF_INFO("%s:%s: Enter", TAG, __func__);
  if(!camera_contexts_.isEmpty()) {
    camera_contexts_.clear();
  }
  instance_ = nullptr;
  QMMF_INFO("%s:%s: Exit (0x%x)", TAG, __func__, this);
}

status_t CameraSource::StartCamera(std::vector<uint32_t> camera_ids,
                                   CameraStartParam &param) {

  assert(camera_ids.size() != 0);
  uint32_t ret = NO_ERROR;

  QMMF_INFO("%s:%s: Number of Cameras to open = %d", TAG, __func__,
                                                 camera_ids.size());

  sp<CameraContext> camera_context;
  for(uint8_t i = 0; i < camera_ids.size(); i++) {
    camera_context = new CameraContext();
    if(!camera_context.get()) {
      QMMF_ERROR("%s:%s: Can't Instantiate CameraDevice(%d)!!", TAG,__func__,i);
      return NO_MEMORY;
    }
    ret = camera_context->OpenCamera(i, param);
    if(ret != NO_ERROR) {
      QMMF_ERROR("%s:%s: CameraDevice:OpenCamera(%d)failed!", TAG, __func__, i);
      camera_context.clear();
      camera_context = NULL;
      ret = NO_INIT;
      goto FAIL;
    }
    QMMF_INFO("%s:%s: Camera(%d) Open is Successfull!", TAG, __func__, i);
    camera_contexts_.add(i, camera_context);
  }
  return ret;
FAIL:
  camera_contexts_.clear();
  return ret;
}

status_t CameraSource::StopCamera(std::vector<uint32_t> camera_ids) {

  int32_t ret = NO_ERROR;
  QMMF_INFO("%s:%s: Number of Cameras to Close=%d", TAG, __func__,
                                                 camera_ids.size());

  //TODO: check if streams are still active, flush them before closing camera.

  for(uint8_t i = 0; i < camera_ids.size(); i++) {
    bool match = false;
    for(uint8_t j = 0; i < camera_contexts_.size(); j++) {
      if(i == camera_contexts_.keyAt(j)) {
        match = true;
        sp<CameraContext> context = camera_contexts_.valueAt(j);
        ret = context->CloseCamera(i);
        assert(ret == NO_ERROR);
        camera_contexts_.removeItem(i);
        QMMF_INFO("%s:%s: Camera(%d) is Closed Successfull!", TAG, __func__, i);
      }
    }
    if(!match) {
      QMMF_ERROR("%s:%s: Invalid Camera Id(%d)", TAG, __func__, i);
      return BAD_VALUE;
    }
  }
  return ret;
}

status_t CameraSource::CaptureImage(std::vector<uint32_t> camera_id,
                                    ImageParam &param,
                                    const CaptureImageCb& cb) {

  QMMF_DEBUG("%s:%s: Enter", TAG, __func__);

  bool match = false;
  sp<CameraContext> camera_context;
  for (uint8_t i = 0; i < camera_contexts_.size(); i++) {
    if (camera_id[0] == camera_contexts_.keyAt(i)) {
        match = true;
        camera_context = camera_contexts_.valueAt(i);
    }
  }
  if (!match) {
    QMMF_ERROR("%s:%s: Invalid Camera Id, It is different then camera is open"
        "with", TAG, __func__);
    return BAD_VALUE;
  }
  assert(camera_context.get() != nullptr);
  auto ret = camera_context->CaptureImage(param, cb);
  // Initial debug purpose.
  assert(ret == NO_ERROR);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s:%s: CaptureImage Failed!", TAG, __func__);
    return ret;
  }
  QMMF_DEBUG("%s:%s: Exit", TAG, __func__);
  return ret;
}

status_t CameraSource::CancelCaptureImage() {
  //Not Implemented
}

status_t CameraSource::CreateTrackSource(const uint32_t track_id,
                                         VideoTrackParams& param) {

  QMMF_DEBUG("%s:%s: Enter", TAG, __func__);

  assert(track_id != 0);
  if(param.camera_ids.size() > 1) {
    QMMF_WARN("%s:%s: Dual Camera is not supported yet!!", TAG, __func__);
    return BAD_VALUE;
  }
  DebugVideoTrackParams(__func__, &param);

  // Find out the camera context corresponding to camera id where track has to
  // be created.
  bool match = false;
  sp<CameraContext> camera_context;
  for (uint8_t i = 0; i < camera_contexts_.size(); i++) {
    if (param.camera_ids[0] == camera_contexts_.keyAt(i)) {
        match = true;
        camera_context = camera_contexts_.valueAt(i);
    }
  }
  if (!match) {
    QMMF_ERROR("%s:%s: Invalid Camera Id, It is different then camera is open"
        "with", TAG, __func__);
    return BAD_VALUE;
  }

  // Create TrackSource and give it to CameraContext, CameraConext in turn would
  // Map it to its one of port.
  sp<TrackSource> track_source;
  track_source = new TrackSource(param, camera_context);
  if (!track_source.get()) {
    QMMF_ERROR("%s:%s: Can't create TrackSource Instance", TAG, __func__);
    return NO_MEMORY;
  }

  assert(camera_context.get() != NULL);
  CameraStreamParam stream_param;
  memset(&stream_param, 0x0, sizeof stream_param);
  stream_param.cam_stream_dim.width  = param.width;
  stream_param.cam_stream_dim.height = param.height;
  stream_param.cam_stream_format     = CameraStreamFormat::kNV21; //don't care
  stream_param.cam_stream_type       = param.camera_stream_type;
  stream_param.frame_rate            = param.frame_rate;
  stream_param.id                    = param.track_id;

  auto ret = camera_context->CreateStream(stream_param);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s:%s: CreateStream failed!!", TAG, __func__);
    goto FAIL;
  }

  QMMF_INFO("%s:%s: TrackSource(0x%x)(%dx%d) and Camera Device Stream "
      " Created Succesffuly for track_id(%d)", TAG, __func__, track_source.get()
      , param.width, param.height, track_id);

  track_sources_.add(track_id, track_source);

  QMMF_DEBUG("%s:%s: Exit", TAG, __func__);
  return ret;
FAIL:
  track_source.clear();
  return ret;
}

status_t CameraSource::DeleteTrackSource(const uint32_t track_id) {

  if (!IsTrackIdValid(track_id)) {
    QMMF_ERROR("%s:%s: track_id is not valid !!", TAG, __func__);
    return BAD_VALUE;
  }

  sp<TrackSource> track = track_sources_.valueFor(track_id);
  assert(track.get() != NULL);
  /*
  * Find out the CameraContext to which this track belongs to.
  */
  uint32_t camera_id = track->getParams().camera_ids[0];
  sp<CameraContext> camera_context = camera_contexts_.valueFor(camera_id);
  assert(camera_context.get() != NULL);

  auto ret = camera_context->DeleteStream(track_id);
  assert(ret == NO_ERROR);

  track_sources_.removeItem(track_id);

  QMMF_INFO("%s:%s: track_id(%d) Deleted Successfully!", TAG, __func__);
  return ret;
}

status_t CameraSource::StartTrackSource(const uint32_t track_id) {

  if (!IsTrackIdValid(track_id)) {
    QMMF_ERROR("%s:%s: track_id is not valid !!", TAG, __func__);
    return BAD_VALUE;
  }

  sp<TrackSource> track = track_sources_.valueFor(track_id);
  assert(track.get() != NULL);

  //Find out the CameraContext to which this track belongs to.
  // TODO: enhance it for 360 camera usecase where one track can be associated
  // with two different streams from two cameras.
  uint32_t camera_id = track->getParams().camera_ids[0];
  sp<CameraContext> camera_context = camera_contexts_.valueFor(camera_id);
  assert(camera_context.get() != NULL);

  sp<IBufferConsumer> consumer;
  consumer = track->GetConsumerIntf();
  assert(consumer.get() != NULL);

  track->StartTrack();

  auto ret = camera_context->StartStream(track_id, consumer);
  assert(ret == NO_ERROR);

  QMMF_VERBOSE("%s:%s: TrackSource id(%d) Started Succesffuly!", TAG, __func__,
      track_id);

  return ret;
}

status_t CameraSource::StopTrackSource(const uint32_t track_id) {

  int32_t ret = NO_ERROR;

  if (!IsTrackIdValid(track_id)) {
    QMMF_ERROR("%s:%s: track_id is not valid !!", TAG, __func__);
    return BAD_VALUE;
  }
  sp<TrackSource> track = track_sources_.valueFor(track_id);
  assert(track.get() != NULL);

  VideoTrackParams params = track->getParams();
  if (params.format_type == VideoFormat::kYUV ||
      params.format_type == VideoFormat::kBayerRDI ||
      params.format_type == VideoFormat::kBayerIdeal) {

      //Encoder is not involved in this case.
      uint32_t camera_id = track->getParams().camera_ids[0];
      sp<CameraContext> camera_context = camera_contexts_.valueFor(camera_id);
      assert(camera_context.get() != NULL);

      auto ret = camera_context->StopStream(track_id);
      assert(ret == NO_ERROR);

  } else {
    // Stop sequence:
    // 1. Send EOS to encoder with last valid buffer. If read thread is waiting
    //    then wait for buffer and send EOS to encoder then stop feeding the
    //    buffers to encoder.
    // 2. Once Encoder acknowledges EOS and gives stop callback then call
    //    stop camera port and break its connection with TrackSource.
    // 3. Return all the buffers back to camera port from frames_received_ queue
    //    if any. there are very less chances frames_received_ list wil have
    //    buffers after we send EOS to encoder and before we break the connection
    //    between CameraPort and TrackSource. but it is very important to check
    //    otherwise camera adaptor will not go in idle state or will fail to delete
    //    camera stream.
    // 4. Once Encoder receives EOS it will return all pending buffer held in
    //    being encoded list.
    track->StopTrack();
  }
  QMMF_VERBOSE("%s:%s: TrackSource id(%d) Stopped Succesffuly!", TAG, __func__,
      track_id);
  return ret;
}

status_t CameraSource::PauseTrackSource(const uint32_t track_id) {
  // Not Implemented
}

status_t CameraSource::ResumeTrackSource(const uint32_t track_id) {
  // Not Implemented
}

status_t CameraSource::ReturnTrackBuffer(const uint32_t track_id,
                                         std::vector<BnTrackBuffer> &buffers) {

  if (!IsTrackIdValid(track_id)) {
    QMMF_ERROR("%s:%s: track_id is not valid !!", TAG, __func__);
    return BAD_VALUE;
  }

  sp<TrackSource> track = track_sources_.valueFor(track_id);
  assert(track.get() != NULL);
  auto ret = track->ReturnTrackBuffer(buffers);
  assert(ret == NO_ERROR);
  return ret;
}

status_t CameraSource::SetCameraParam(uint32_t camera_id,
                                      CameraMetadata &meta) {
  sp<CameraContext> camera_context = camera_contexts_.valueFor(camera_id);
  assert(camera_context.get() != NULL);

  return camera_context->SetCameraParam(meta);
}

status_t CameraSource::GetCameraParam(uint32_t camera_id,
                                      CameraMetadata &meta) {
  sp<CameraContext> camera_context = camera_contexts_.valueFor(camera_id);
  assert(camera_context.get() != NULL);

  return camera_context->GetCameraParam(meta);
}

status_t CameraSource::CreateOverlayObject(OverlayParam &param,
                                           uint32_t *overlay_id) {
  // Not Implemented
}

status_t CameraSource::DeleteOverlayObject(const uint32_t overlay_id) {

  // Not Implemented
}

status_t CameraSource::GetOverlayObjectParams(const uint32_t overlay_id,
                                              OverlayParam &param) {
  // Not Implemented
}

status_t CameraSource::UpdateOverlayObjectParams(const uint32_t overlay_id,
                                                 OverlayParam &param) {
  // Not Implemented
}

status_t CameraSource::SetOverlayObject(const uint32_t track_id,
                                        const uint32_t overlay_id) {
  // Not Implemented
}

status_t CameraSource::RemoveOverlayObject(const uint32_t track_id,
                                           const uint32_t overlay_id) {
  // Not Implemented
}

const sp<TrackSource>& CameraSource::getTrackSource(uint32_t track_id) {

  int32_t idx = track_sources_.indexOfKey(track_id);
  assert(idx >= 0);
  return track_sources_.valueFor(track_id);
}

bool CameraSource::IsTrackIdValid(const uint32_t track_id) {

  bool valid = false;
  size_t size = track_sources_.size();
  QMMF_INFO("%s: Number of Tracks exist = %d",__func__, size);
  for(size_t i = 0; i < size; i++) {
    if (track_id == track_sources_.keyAt(i)) {
        valid = true;
        break;
    }
  }
  return valid;
}

TrackSource::TrackSource(VideoTrackParams& params, sp<CameraContext>& context)
    : track_params_(params), is_stop_(false) {

  BufferConsumerImpl<TrackSource> *impl;
  impl = new BufferConsumerImpl<TrackSource>(this);
  buffer_consumer_impl_ = impl;
  assert(context.get() != nullptr);
  context_ = context;

#ifdef DEBUG_TRACK_FPS
  timeval prevtv_ = {0x0, 0x0};
  count_ = 0;
#endif
  QMMF_INFO("%s:%s: TrackSource (0x%x)", TAG, __func__, this);
}

TrackSource::~TrackSource() {

  QMMF_INFO("%s:%s: Enter ", TAG, __func__);

  QMMF_INFO("%s:%s: Exit(0x%x) ", TAG, __func__, this);
}

status_t TrackSource::Stop() {

  QMMF_DEBUG("%s:%s Enter track_id(%d)", TAG, __func__, TrackId());
  // Encoder Received the EOS successfully, stop the camera stream and clear
  // the queue.
  QMMF_INFO("%s:%s: EOS acknowledged by Endor!!", TAG, __func__);
  assert(context_.get() != nullptr);
  auto ret = context_->StopStream(TrackId());
  assert(ret == NO_ERROR);
  ClearInputQueue();

  QMMF_DEBUG("%s:%s Exit track_id(%d)", TAG, __func__, TrackId());
  return NO_ERROR;
}

status_t TrackSource::Read(StreamBuffer& buffer) {

  QMMF_DEBUG("%s:%s Enter track_id(%d)", TAG, __func__, TrackId());
  bool timeout = false;

  if (frames_received_.Size() == 0) {
    QMMF_DEBUG("%s:%s: track_id(%d) Wait for bufferr!!", TAG, __func__,
        TrackId());
    auto ret = wait_for_frame_.waitRelative(lock_, kWaitDuration);
    if (ret == TIMED_OUT) {
        QMMF_ERROR("%s:%s: track_id(%d) Buffer Timed out happend! No buffers"
            "from Camera", TAG, __func__, TrackId());
        timeout = true;
    }
  }
  assert(timeout == false);

  QMMF_VERBOSE("%s:%s: track_id(%d) frames_received_.size(%d)", TAG, __func__,
      TrackId(), frames_received_.Size());

  StreamBuffer stream_buffer = *frames_received_.Begin();
  buffer = stream_buffer;
  frames_being_encoded_.PushBack(stream_buffer);
  frames_received_.Erase(frames_received_.Begin());

  if (IsStop()) {
    QMMF_DEBUG("%s:%s: track_id(%d) Send EOS to Encoder!", TAG, __func__,
        TrackId());
    // TODO defile EOS flag in AVCodec to delete EOS.
    return -1;
  }
  QMMF_DEBUG("%s:%s Exit track_id(%d)", TAG, __func__, TrackId());
  return NO_ERROR;
}

status_t TrackSource::SignalBufferReturned(StreamBuffer& buffer) {

  QMMF_DEBUG("%s:%s: Enter track_id(%d)", TAG, __func__, TrackId());

  QMMF_VERBOSE("%s:%s: track_id(%d) frames_being_encoded_.size(%d)", TAG,
      __func__, TrackId(), frames_being_encoded_.Size());

  bool found = false;
  auto iter = frames_being_encoded_.Begin();
  for (; iter != frames_being_encoded_.End(); ++iter) {
    if ((*iter).handle ==  buffer.handle) {
      QMMF_VERBOSE("%s:%s: Buffer found in frames_being_encoded_ list!", TAG,
          __func__);
      buffer_consumer_impl_->GetProducerHandle()->NotifyBufferReturned((*iter));
      frames_being_encoded_.Erase(iter);
      found = true;
      break;
    }
  }
  assert(found == true);
  QMMF_VERBOSE("%s:%s: frames_being_encoded_.Size(%d)", TAG, __func__,
      frames_being_encoded_.Size());

  QMMF_DEBUG("%s:%s Exit track_id(%d)", TAG, __func__, TrackId());
  return NO_ERROR;
}

void TrackSource::OnFrameAvailable(StreamBuffer& buffer) {

  QMMF_VERBOSE("%s:%s: Enter track_id = %d", TAG, __func__,
      TrackId());

#ifdef NO_FRAME_PROCESS
  buffer_consumer_impl_->GetProducerHandle()->NotifyBufferReturned(buffer);
  return;
#endif

#ifdef DEBUG_TRACK_FPS
  struct timeval tv;
  gettimeofday(&tv,NULL);
  uint64_t time_diff = (uint64_t)((tv.tv_sec * 1000000 + tv.tv_usec) -
                    (prevtv_.tv_sec * 1000000 + prevtv_.tv_usec));
  count_++;
  if(time_diff >= FPS_TIME_INTERVAL) {
    float framerate = (count_ * 1000000)/(float)time_diff;
    QMMF_INFO("%s:%s: Track_id(%d):fps: = %0.2f", TAG, __func__,
        TrackId(), framerate);
    prevtv_ = tv;
    count_ = 0;
  }
#endif

  buffer_handle_t native_handle = buffer.handle;
  // native_handle->data[0] = Ion fd.
  // native_handle->data[4] = frame length.
  // native_handle->data[14] = stride.
  // native_handle->data[15] = scanline.
  uint32_t ion_fd       = native_handle->data[0];
  uint32_t frame_length = native_handle->data[4];

  QMMF_VERBOSE("%s:%s: numInts = %d", TAG, __func__, native_handle->numInts);
  for (uint32_t i = 0; i < native_handle->numInts; i++) {
    QMMF_VERBOSE("%s:%s: data[%d] =%d", TAG, __func__, i, native_handle->data[i]);
  }
  QMMF_DEBUG("%s:%s: ion_fd = %d", TAG, __func__, ion_fd);

#ifdef ENABLE_FRAME_DUMP
  static uint32_t id;
  ++id;
  // Dump every 100th frame.
  if (id == 100) {
    void *buf_vaaddr = mmap(NULL, frame_length, PROT_READ  | PROT_WRITE,
                            MAP_SHARED, ion_fd, 0);
    assert(buf_vaaddr != NULL);

    String8 file_path;
    size_t written_len;
    file_path.appendFormat(FRAME_DUMP_PATH"/track_%d_%lld.yuv",
        TrackId(), buffer.stream_buffer.timestamp);

    FILE *file = fopen(file_path.string(), "w+");
    if (!file) {
      QMMF_ERROR("%s:%s: Unable to open file(%s)", TAG, __func__,
          file_path.string());
      goto FAIL;
    }
    written_len = fwrite(buf_vaaddr, sizeof(uint8_t), frame_length,
        file);
    QMMF_INFO("%s:%s: written_len =%d", TAG, __func__, written_len);

    if (frame_length != written_len) {
      QMMF_ERROR("%s:%s: Bad Write error (%d):(%s)\n", TAG, __func__, errno,
          strerror(errno));
        goto FAIL;
    }
    QMMF_INFO("%s:%s: Buffer(0x%x) Size(%u) Stored(%s)\n",__func__,
        buf_vaaddr, written_len, file_path.string());

FAIL:
    if (file != NULL) {
      fclose(file);
    }
    if(buf_vaaddr != NULL) {
      munmap(buf_vaaddr, frame_length);
      buf_vaaddr = NULL;
    }
    id = 0;
  }
#endif

  // if format type is YUV or BAYER then give callback from this point, do not
  // feed buffer to Encoder.
  if (track_params_.format_type == VideoFormat::kYUV ||
      track_params_.format_type == VideoFormat::kBayerRDI ||
      track_params_.format_type == VideoFormat::kBayerIdeal) {

    BnTrackBuffer bn_buffer;
    memset(&bn_buffer, 0x0, sizeof bn_buffer);
    bn_buffer.ion_fd         = ion_fd;
    bn_buffer.size           = frame_length;
    bn_buffer.timestamp      = buffer.timestamp;
    bn_buffer.width          = buffer.info.plane_info[0].width;
    bn_buffer.height         = buffer.info.plane_info[0].height;
    bn_buffer.buffer_id      = ion_fd;
    bn_buffer.flag           = 0x10;
    bn_buffer.capacity       = frame_length;

    // Buffers from this list used for YUV callback.
    buffer_list_.add(ion_fd, buffer);

    std::vector<BnTrackBuffer> bn_buffers;
    bn_buffers.push_back(bn_buffer);
    track_params_.data_cb(TrackId(), bn_buffers,
                          static_cast<void*>(&buffer.info),
                          TrackMetaParamType::kCamBufMetaData,
                          sizeof (MetaInfo));
  } else {
    // Push buffers into encoder queue.
    PushFrameToQueue(buffer);
  }
}

status_t TrackSource::ReturnTrackBuffer(std::vector<BnTrackBuffer>&
                                        bn_buffers) {

  QMMF_VERBOSE("%s:%s: Enter", TAG, __func__);
  assert(bn_buffers.size() > 0);
  assert(buffer_consumer_impl_ != NULL);

  for (size_t i = 0; i < bn_buffers.size(); ++i) {
    QMMF_VERBOSE("%s:%s: bn_buffers[%d].ion_fd=%d", TAG, __func__, i,
        bn_buffers[i].ion_fd);
    int32_t idx = buffer_list_.indexOfKey(bn_buffers[i].ion_fd);
    assert(idx >= 0);
    QMMF_VERBOSE("%s:%s: Buffer fd(%d) found in list", TAG, __func__,
        bn_buffers[i].ion_fd);
    StreamBuffer buffer = buffer_list_.valueFor(bn_buffers[i].ion_fd);
    buffer_consumer_impl_->GetProducerHandle()->NotifyBufferReturned(buffer);
  }
  QMMF_VERBOSE("%s:%s: Exit", TAG, __func__);
  return NO_ERROR;
}

void TrackSource::PushFrameToQueue(StreamBuffer& buffer) {

  QMMF_VERBOSE("%s:%s: Enter track_id(%d)", TAG, __func__, TrackId());

  Mutex::Autolock lock(lock_);
  frames_received_.PushBack(buffer);
  QMMF_DEBUG("%s:%s: frames_received.size(%d)", TAG, __func__,
      frames_received_.Size());
  wait_for_frame_.signal();

  QMMF_VERBOSE("%s:%s: Exit track_id(%d)", TAG, __func__, TrackId());
}

status_t TrackSource::StartTrack() {

  QMMF_DEBUG("%s:%s: Enter track_id(%d)", TAG, __func__, TrackId());
  Mutex::Autolock lock(stop_lock_);
  is_stop_ = false;
  QMMF_DEBUG("%s:%s: Exit track_id(%d)", TAG, __func__, TrackId());
  return NO_ERROR;
}

status_t TrackSource::StopTrack() {

  QMMF_DEBUG("%s:%s: Enter track_id(%d)", TAG, __func__, TrackId());
  Mutex::Autolock lock(stop_lock_);
  is_stop_ = true;
  QMMF_DEBUG("%s:%s: Exit track_id(%d)", TAG, __func__, TrackId());
  return NO_ERROR;
}

bool TrackSource::IsStop() {

  QMMF_VERBOSE("%s:%s: Enter track_id(%d)", TAG, __func__, TrackId());
  Mutex::Autolock lock(stop_lock_);
  QMMF_VERBOSE("%s:%s: Exit track_id(%d)", TAG, __func__, TrackId());
  return is_stop_;
}

void TrackSource::ClearInputQueue() {

  QMMF_DEBUG("%s:%s: Enter track_id(%d)", TAG, __func__, TrackId());
  // Once connection is broken b/w port and trackSoure there is no chance to
  // get new buffers in frames_received_ queue.
  uint32_t size = frames_received_.Size();
  QMMF_INFO("%s:%s: track_id(%d): (%d) buffers to return from frames_received_",
      TAG, __func__, TrackId(), size);
  assert(buffer_consumer_impl_->GetProducerHandle().get() != nullptr);
  auto iter = frames_received_.Begin();
  for (; iter != frames_received_.End(); ++iter) {
    buffer_consumer_impl_->GetProducerHandle()->NotifyBufferReturned((*iter));
  }
  frames_received_.Clear();
  QMMF_DEBUG("%s:%s: Exit track_id(%d)", TAG, __func__, TrackId());
}

}; //namespace recorder

}; //namespace qmmf
