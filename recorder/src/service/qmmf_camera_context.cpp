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

#define TAG "RecorderCameraContext"
#include <algorithm>

#include "qmmf_camera_context.h"
#include "qmmf_recorder_utils.h"

namespace qmmf {

namespace recorder {

CameraContext::CameraContext()
    : camera_id_(-1),
      streaming_request_id_(-1),
      request_created_(false) {

}

CameraContext::~CameraContext() {

  QMMF_INFO("%s:%s: Enter", TAG, __func__);
  if(camera_device_.get()) {
    camera_device_.clear();
    camera_device_ = NULL;
  }
  //TODO: check all active ports
  QMMF_INFO("%s:%s: Exit", TAG, __func__);
}

status_t CameraContext::OpenCamera(uint32_t camera_id,
                                   CameraStartParam &param) {

  uint32_t ret = NO_ERROR;
  bool match_camera_id = false;
  uint32_t num_camera = 0;

  DebugCameraStartParams(__func__, &param);

  //Setup Camera3DeviceClient callbacks.
  memset(&camera_callbacks_, 0x0, sizeof camera_callbacks_);
  camera_callbacks_.errorCb = [&] (CameraErrorCode error_code,
      const CaptureResultExtras &extras) { CameraErrorCb(error_code, extras);};

  camera_callbacks_.idleCb = [&] () { CameraIdleCb(); };

  camera_callbacks_.peparedCb = [&] (int32_t id) { CameraPreparedCb(id); };

  camera_callbacks_.shutterCb = [&] (const CaptureResultExtras &extras,
      int64_t ts) { CameraShutterCb(extras, ts); };

  camera_callbacks_.resultCb = [&] (const CaptureResult &result)
      { CameraResultCb(result); };

  camera_device_ = new Camera3DeviceClient(camera_callbacks_);
  if(!camera_device_.get()) {
    QMMF_ERROR("%s:%s: Can't Instantiate Camera3DeviceClient", TAG, __func__);
    return NO_MEMORY;
  }

  ret = camera_device_->Initialize(CAMERA_HAL_MODULE_PATH, GRALLOC_MODULE_PATH);
  if(ret != NO_ERROR) {
    QMMF_ERROR("%s:%s Unable to Initialize Camera3DeviceClient", TAG, __func__,
               ret);
    goto FAIL;
  }

  num_camera = camera_device_->GetNumberOfCameras();
  for(uint32_t i = 0; i < num_camera; i++) {
    if(i == camera_id) {
      match_camera_id = true;
      break;
    }
  }
  if(!match_camera_id) {
    QMMF_ERROR("%s:%s: Invalid Camera Id (%d)", TAG, __func__, camera_id);
    ret = BAD_VALUE;
    goto FAIL;
  }

  ret = camera_device_->OpenCamera(camera_id);
  assert(ret == NO_ERROR);
  return ret;

FAIL:
  camera_device_.clear();
  camera_device_= NULL;
  return ret;
}

status_t CameraContext::CloseCamera(uint32_t camera_id) {

  int32_t ret = NO_ERROR;
  int64_t last_frame_number;

  assert(camera_id_ != camera_id);
  assert(camera_device_.get() != NULL);

  if (streaming_request_id_ > 0) {
    QMMF_ERROR("%s:%s: Streaming Request still running! delete all tracks "
    "before closing camera", TAG, __func__);
    return INVALID_OPERATION;
  }

  ret = camera_device_->WaitUntilIdle();
  assert(ret == NO_ERROR);

  camera_device_.clear();
  camera_device_ = NULL;

  QMMF_INFO("%s:%s: Camera Closed Succussfully!", TAG, __func__);
  return ret;
}

status_t CameraContext::CreateStream(CameraStreamParam& param) {

  QMMF_LEVEL2("%s:%s: Enter", TAG, __func__);
  // 1. Check if streaming request already is going on, if yes then cancel it
  //    and reconfigure it with adding new request.
  // 2. Check for available port where consumer can be attached, if not then
  //    Create new one.
  // 3. Create camera adaptor stream.
  // 4. Create port and link it with adaptor stream.
  // 5. Create producer interface in port and link consumer.

  assert(camera_device_.get() != NULL);
  assert(param.id != 0);

  sp<CameraPort> port;
  port = new CameraPort(param, CameraPortType::kVideo, this);
  assert(port.get() != NULL);

  auto ret = port->Init();
  if(ret != NO_ERROR) {
    QMMF_ERROR("%s:%s: CameraPort Can't be Created!", TAG, __func__);
    return BAD_VALUE;
  }

  // Create global streaming capture request, this capture request would be
  // Common to all video/preview and zsl snapshot stream. non zsl snapshot
  // will have separate capture request.
  if (!request_created_) {
    ret = CreateCaptureRequest(streaming_request_,
                               CAMERA3_TEMPLATE_VIDEO_RECORD);
    assert(ret == NO_ERROR);
    QMMF_INFO("%s:%s: Global Streaming Capture request created successfully!",
        TAG, __func__);
  }

  // Add port to list of active ports.
  active_ports_.add(param.id, port);

  QMMF_INFO("%s:%s: Number of Active ports=%d", TAG, __func__,
      active_ports_.size());

  QMMF_LEVEL2("%s:%s: Exit", TAG, __func__);
  return ret;
}

status_t CameraContext::DeleteStream(const uint32_t track_id) {

  if(active_ports_.indexOfKey(track_id) < 0) {
    QMMF_ERROR("%s:%s: Invalid track_id = %d", TAG, __func__, track_id);
    return BAD_VALUE;
  }
  sp<CameraPort> port = active_ports_.valueFor(track_id);
  assert(port.get() != NULL);
  if (port->GetNumConsumers() > 0) {
    // Port still being used by another consumer, eventually this port would be
    // deleted once consumers count would become zero.
    return NO_ERROR;
  }

  auto ret = port->DeInit();
  assert(ret == NO_ERROR);

  // Delete the port.
  active_ports_.removeItem(track_id);
  QMMF_INFO("%s:%s: Camera Port for track_id(%d) deleted", TAG, __func__,
      track_id);

  return ret;
}

status_t CameraContext::StartStream(const uint32_t track_id,
                                    sp<IBufferConsumer>& consumer) {

  if(active_ports_.indexOfKey(track_id) < 0) {
    QMMF_ERROR("%s:%s: Invalid track_id = %d", TAG, __func__, track_id);
    return BAD_VALUE;
  }

  sp<CameraPort> port = active_ports_.valueFor(track_id);
  assert(port.get() != NULL);

  auto ret = port->Start(track_id, consumer);
  assert(ret == NO_ERROR);
  QMMF_INFO("%s:%s: track_id(%d) started on port(0x%x)", TAG, __func__,
      track_id, port.get());
  return ret;
}

status_t CameraContext::StopStream(const uint32_t track_id) {

  if(active_ports_.indexOfKey(track_id) < 0) {
    QMMF_ERROR("%s:%s: Invalid track_id = %d", TAG, __func__, track_id);
    return BAD_VALUE;
  }

  sp<CameraPort> port = active_ports_.valueFor(track_id);
  assert(port.get() != NULL);

  auto ret = port->Stop(track_id);
  assert(ret == NO_ERROR);
  return ret;
}

status_t CameraContext::CreateDeviceStream(CameraStreamParameters& params,
                                           int32_t* stream_id) {

  Mutex::Autolock lock(device_access_lock_);
  QMMF_LEVEL2("%s:%s: Enter", TAG, __func__);

  int32_t ret = NO_ERROR;
  assert(camera_device_.get() != NULL);

  /*
  * Configure is required only once, if streaming request is already submitted
  * then BeginConfigure is not required to be called, stream can be created
  * without calling it.
  */
  if (streaming_request_id_ < 0) {
    ret = camera_device_->BeginConfigure();
    assert(ret == NO_ERROR);
  }

  int32_t id;
  id = camera_device_->CreateStream(params);
  if (id < 0) {
    QMMF_INFO("%s:%s: createStream failed!!", TAG, __func__);
    return BAD_VALUE;
  }
  *stream_id = id;
  /*
  * At this point stream is created but it is not added to request, it will be
  * added once corresponding port will get the start cmd from it's consumer.
  */
  if (streaming_request_id_ < 0) {
    ret = camera_device_->EndConfigure();
    assert(ret == NO_ERROR);
  }
  QMMF_LEVEL2("%s:%s: Exit", TAG, __func__);
}


status_t CameraContext::DeleteDeviceStream(int32_t stream_id) {

  Mutex::Autolock lock(device_access_lock_);

  QMMF_LEVEL2("%s:%s: Enter", TAG, __func__);
  assert(camera_device_.get() != NULL);
  auto ret = camera_device_->DeleteStream(stream_id);
  assert(ret == NO_ERROR);
  QMMF_INFO("%s:%s: Camera Device Stream(%d) deleted successfully!", TAG,
      __func__, stream_id);

  QMMF_LEVEL2("%s:%s: Exit", TAG, __func__);
  return ret;
}

status_t CameraContext::CreateCaptureRequest(Camera3Request& request,
                                             camera3_request_template_t
                                             template_type) {

  Mutex::Autolock lock(device_access_lock_);

  auto ret = camera_device_->CreateDefaultRequest(template_type,
      &request.metadata);
  assert(ret == NO_ERROR);
  request_created_ = true;
  return ret;
}

status_t CameraContext::UpdateRequest(bool is_streaming) {

  int32_t ret = NO_ERROR;

  // Get all camera stream ids from all active ports which are ready to start.
  size_t size = active_ports_.size();
  assert(size > 0);

  for (size_t i = 0; i < size; i++) {
    sp<CameraPort> port = active_ports_.valueAt(i);
    assert(port != NULL);

    int32_t cam_stream_id = port->GetCameraStreamId();
    if (port->getPortState() == PortState::PORT_READYTOSTART) {

      QMMF_INFO("%s:%s: CameraPort(0x%x):camera_stream_id(%d) is ready to"
          " start!", TAG, __func__, port.get(), cam_stream_id);
      streaming_request_.streamIds.add(cam_stream_id);
    } else if (port->getPortState() == PortState::PORT_READYTOSTOP) {

      QMMF_INFO("%s:%s: CameraPort(0x%x):camera_stream_id(%d) is stopped ",
          TAG, __func__, port.get(), cam_stream_id);
      // Check if camera stream is already part of request, if yes then remove
      // it from request list. if not then it means stream is created but its
      // correspnding port is not started yet.
      bool match = false;
      size_t idx = -1;
      for (size_t i = 0; i < streaming_request_.streamIds.size(); i++) {
        if (cam_stream_id == streaming_request_.streamIds[i]) {
          match = true;
          idx = i;
          break;
        }
      }
      if(match == true) {
          QMMF_INFO("%s:%s: cam_stream_id(%d) removed from Request!", TAG,
              __func__, cam_stream_id);
          streaming_request_.streamIds.removeAt(idx);
      }
    }
  }
  size = streaming_request_.streamIds.size();
  QMMF_INFO("%s:%s: Number of streams(%d) to start", TAG, __func__, size);
  if (size == 0) {

    QMMF_INFO("%s:%s:Cancelling the request, no pending stream!", TAG, __func__);
    ret = CancelRequest();
    assert (ret == NO_ERROR);
    return NO_ERROR;
  }

  {
    Mutex::Autolock lock(device_access_lock_);
    int64_t last_frame_mumber;
    auto ret = camera_device_->SubmitRequest(streaming_request_, is_streaming,
        &last_frame_mumber);
    assert(ret >= 0);
    streaming_request_id_ = ret;
  }
  QMMF_INFO("%s:%s: SubmitRequest for Num streams(%d) is successfull"
      " request_id(%d)", TAG, __func__, size, streaming_request_id_);
  return ret;
}

status_t CameraContext::CancelRequest() {

  Mutex::Autolock lock(device_access_lock_);

  int64_t last_frame_mumber;
  assert(streaming_request_id_ >= 0);

  QMMF_INFO("%s:%s: Issuing CancelRequest!", TAG, __func__);
  auto ret = camera_device_->CancelRequest(streaming_request_id_,
                                           &last_frame_mumber);
  assert(ret == NO_ERROR);
  QMMF_INFO("%s:%s: last_frame_mumber(%lld) after CancelRequest", TAG, __func__,
      last_frame_mumber);

  //ret = camera_device_->WaitUntilIdle();
  //assert(ret == NO_ERROR);

  streaming_request_id_ = -1;
  QMMF_INFO("%s:%s: Request cancelled last frame number: %lld\n", TAG,
      __func__, last_frame_mumber);
  return ret;
}

status_t CameraContext::ReturnStreamBuffer(int32_t stream_id,
                                           StreamBuffer buffer) {
  Mutex::Autolock lock(device_access_lock_);
  QMMF_LEVEL2("%s:%s: camera_stream_id: %d, buffer: 0x%x ts: %lld\n", TAG,
      __func__, stream_id, buffer.handle, buffer.timestamp);

  auto ret = camera_device_->ReturnStreamBuffer(stream_id, buffer);
  assert(ret == NO_ERROR);
  return ret;
}

//Camera device callbacks
void CameraContext::CameraErrorCb(CameraErrorCode error_code,
                                  const CaptureResultExtras &result) {

  QMMF_WARN("%s:%s: Camera Client: error_code: %d\n", TAG, __func__,
            error_code);
}

void CameraContext::CameraIdleCb() {
  QMMF_WARN("%s:%s: Camera is in Idle State!!", TAG, __func__);
}

void CameraContext::CameraShutterCb(const CaptureResultExtras &result,
                                    int64_t time_stamp) {

}

void CameraContext::CameraPreparedCb(int32_t) {

}

void CameraContext::CameraResultCb(const CaptureResult &result) {

  QMMF_LEVEL1("%s:%s: frame number=%lld", TAG, __func__,
      result.resultExtras.frameNumber);
}

CameraPort::CameraPort(CameraStreamParam& param, CameraPortType port_type,
    CameraContext* context)
    : params_(param),
      port_type_(port_type),
      context_(context),
      camera_stream_id_(-1),
      ready_to_start_(false) {

  QMMF_INFO("%s:%s: Enter", TAG, __func__);

  BufferProducerImpl<CameraPort> *producer_impl;
  producer_impl = new BufferProducerImpl<CameraPort>(this);
  buffer_producer_impl_ = producer_impl;
  QMMF_INFO("%s:%s: Exit (0x%x)", TAG, __func__, this);
}

CameraPort::~CameraPort() {

  QMMF_INFO("%s:%s: Enter ", TAG, __func__);
  buffer_producer_impl_.clear();
  buffer_producer_impl_ = NULL;
  QMMF_INFO("%s:%s: Exit (0x%x)", TAG, __func__, this);
}

status_t CameraPort::Init() {

  memset(&cam_stream_params_, 0, sizeof(cam_stream_params_));
  CameraStreamType stream_type = params_.cam_stream_type;

  cam_stream_params_.format       = HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED;
  cam_stream_params_.width        = params_.cam_stream_dim.width;
  cam_stream_params_.height       = params_.cam_stream_dim.height;
  cam_stream_params_.grallocFlags = GRALLOC_USAGE_HW_FB;

  if (stream_type == CameraStreamType::kVideo) {
    cam_stream_params_.grallocFlags |= private_handle_t::
        PRIV_FLAGS_VIDEO_ENCODER;
    cam_stream_params_.bufferCount   = VIDEO_STREAM_BUFFER_COUNT;
  } else {
    cam_stream_params_.bufferCount  = PREVIEW_STREAM_BUFFER_COUNT;
  }

  cam_stream_params_.cb = [&] (int32_t stream_id, StreamBuffer buffer)
      { StreamCallback (stream_id, buffer); };

  assert(context_ != NULL);
  int32_t stream_id;
  auto ret = context_->CreateDeviceStream(cam_stream_params_, &stream_id);
  if (ret != NO_ERROR && stream_id < 0) {
    QMMF_ERROR("%s:%s: CreateDeviceStream failed!!", TAG, __func__);
    return BAD_VALUE;
  }
  camera_stream_id_ = stream_id;

  port_state_ = PortState::PORT_CREATED;

  QMMF_INFO("%s:%s: Camera Device Stream(%d) is created Succussfully!", TAG,
      __func__, camera_stream_id_);
  QMMF_INFO("%s:%s: track_id(%d) is mapped to camera stream_id(%d)", TAG,
      __func__, params_.id, camera_stream_id_);

  return NO_ERROR;
}

status_t CameraPort::DeInit() {

  assert(ready_to_start_ == false);
  assert(context_ != NULL);

  auto ret = context_->DeleteDeviceStream(camera_stream_id_);
  if(ret != NO_ERROR) {
    QMMF_ERROR("%s:%s: DeleteDeviceStream failed!!", TAG, __func__);
    return BAD_VALUE;
  }
  consumer_map_.clear();
  QMMF_LEVEL1("%s:%s: CameraPort(0x%x) deinitialized successfully! ", TAG,
      __func__, this);
  return ret;
}

status_t CameraPort::Start(const uint32_t consumer_id,
                           const sp<IBufferConsumer>& consumer) {

  assert(consumer.get() != NULL);
  // Establish buffer communication link between consumer (TrackSource) and
  // Buffer Producer interface of camera port.
  AddConsumer(consumer_id, consumer);

  //TODO: protect it with lock.
  ready_to_start_ = true;
  port_state_ = PortState::PORT_READYTOSTART;

  QMMF_INFO("%s:%s: track_id(%d):camera stream(%d) to start!", TAG, __func__,
      consumer_id, camera_stream_id_);

  auto ret = context_->UpdateRequest(true);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s:%s: CameraPort:Start:UpdateRequest failed! for track_id = %d"
        , TAG, __func__, consumer_id);
  }
  port_state_ = PortState::PORT_STARTED;
  return ret;
}

status_t CameraPort::Stop(const uint32_t consumer_id) {

  if (!IsConsumerIdValid(consumer_id)) {
    QMMF_ERROR("%s:%s: consumer_id(%d) is not valid!", TAG, __func__,
        consumer_id);
    return BAD_VALUE;
  }
  /*
  * Break buffer communication link between consumer and camera port.
  */
  RemoveConsumer(consumer_id);

  size_t size = consumer_map_.size();
  QMMF_INFO("%s:%s: Number of Consumer left = %d", TAG, __func__, size);
  if (size > 0) {
    /*
    * Camera port is still getting used by some other consumer, don't delete
    * camera device stream, eventaully it would be deleted when number
    * of consumer becomes zero.
    */
    return NO_ERROR;
  }
  //TODO: protect it with lock.
  ready_to_start_ = false;
  port_state_ = PortState::PORT_READYTOSTOP;

  /*
  * Stop basically removes the stream from current running capture request,
  * it doen't delete the stream.
  */
  auto ret = context_->UpdateRequest(true);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s:%s: CameraPort:Start:UpdateRequest failed! for track_id = %d"
        , TAG, __func__, consumer_id);
  }
  QMMF_INFO("%s:%s: track_id(%d):Port(0x%x) Stopped Succussfully!", TAG,
      __func__, consumer_id, this);

  port_state_ = PortState::PORT_STOPPED;

  return ret;
}

status_t CameraPort::AddConsumer(const uint32_t consumer_id,
                                 const sp<IBufferConsumer>& consumer) {

  Mutex::Autolock lock(consumer_lock_);

  consumer_map_.add(consumer_id, consumer);
  // Add consumer to port's producer interface.
  assert(buffer_producer_impl_.get() != NULL);
  buffer_producer_impl_->AddConsumer(consumer);
  consumer->SetProducerHandle(buffer_producer_impl_);
  QMMF_LEVEL1("%s:%s: ConsumerId(%d):(0x%x) has been added to CameraPort(0x%x)."
      "Total number of consumer =%d", TAG, __func__, consumer_id, consumer.get()
      , this, consumer_map_.size());
}

status_t CameraPort::RemoveConsumer(const uint32_t consumer_id) {

  Mutex::Autolock lock(consumer_lock_);

  sp<IBufferConsumer> consumer = consumer_map_.valueFor(consumer_id);
  assert(consumer.get() != NULL);
  // Remove consumer from port's producer interface.
  assert(buffer_producer_impl_.get() != NULL);
  buffer_producer_impl_->RemoveConsumer(consumer);

  consumer_map_.removeItem(consumer_id);
  QMMF_LEVEL1("%s:%s: ConsumerId(%d):(0x%x) has been Remved CameraPort(0x%x)."
      "Total number of consumer =%d", TAG, __func__,consumer_id, consumer.get(),
      this, consumer_map_.size());
}

void CameraPort::NotifyBufferReturned(Buffer& buffer) {

  QMMF_LEVEL2("%s:%s: Buffer(0x%x) Came back to CameraPort", TAG, __func__,
      buffer.stream_buffer.handle);
  //TODO: protect this with lock, would be required once multiple camera ports
  // are enabled.
  context_->ReturnStreamBuffer(camera_stream_id_, buffer.stream_buffer);
}

int32_t CameraPort::GetNumConsumers() {
  Mutex::Autolock lock(consumer_lock_);
  return consumer_map_.size();
}

bool CameraPort::IsReadyToStart() {
  //TODO: protect it with lock.
  return ready_to_start_;
}

PortState& CameraPort::getPortState() {
  return port_state_;
}

bool CameraPort::IsConsumerIdValid(const uint32_t id) {

  bool valid = false;
  size_t size = consumer_map_.size();
  for(size_t i = 0; i < size; i++) {
    if (id == consumer_map_.keyAt(i)) {
        valid = true;
        break;
    }
  }
  return valid;
}

void CameraPort::StreamCallback(int32_t stream_id, StreamBuffer stream_buffer) {

  QMMF_LEVEL2("%s:%s: Enter stream_id(%d)", TAG, __func__, stream_id);
  assert(stream_id == camera_stream_id_);
  assert(buffer_producer_impl_.get() != NULL);

  QMMF_LEVEL2("%s:%s: camera stream_id: %d, buffer: 0x%x ts: %lld\n", TAG,
      __func__, stream_id, stream_buffer.handle, stream_buffer.timestamp);

  if(buffer_producer_impl_->GetNumConsumer() > 0) {
    Buffer buffer;
    memset(&buffer, 0x0, sizeof buffer);
    buffer.stream_param  = params_;
    buffer.stream_buffer = stream_buffer;
    buffer_producer_impl_->NotifyBuffer(buffer);
  } else {
    /*
    * Return the buffer back to camera.
    */
    QMMF_LEVEL2("%s:%s: No consumer, simply return buffer back to camera!",
        TAG, __func__);
    context_->ReturnStreamBuffer(stream_id, stream_buffer);
  }
  QMMF_LEVEL2("%s:%s: Exit ", TAG, __func__);
}

}; // namespace recoder

}; // namespace qmmf