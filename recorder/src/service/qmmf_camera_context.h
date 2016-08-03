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

#pragma once

#include <utils/KeyedVector.h>
#include <utils/Log.h>
#include <libgralloc/gralloc_priv.h>

#include "qmmf-sdk/qmmf_recorder_params.h"
#include "recorder/src/service/qmmf_recorder_common.h"
#include "common/cameraadaptor/qmmf_camera3_device_client.h"

namespace qmmf {

using namespace cameraadaptor;

#define VIDEO_STREAM_BUFFER_COUNT   11
#define PREVIEW_STREAM_BUFFER_COUNT 10

namespace recorder {

class CameraPort;
class IBufferConsumer;
class IBufferProducer;

// This class deals with Camera3DeviceClient, and exposes simple Apis to create
// Different types of streams (preview, video, and snashot). this class has a
// Concept of ports, maintains vector of ports, each port is mapped one-to-one
// to camera device stream.
class CameraContext : public RefBase {
 public:
  CameraContext();

  ~CameraContext();

  status_t OpenCamera(uint32_t camera_id, CameraStartParam &param);

  status_t CloseCamera(uint32_t camera_id);

  status_t CaptureImage(ImageParam &param, const CaptureImageCb& cb);

  status_t CreateStream(CameraStreamParam& param);

  status_t DeleteStream(const uint32_t track_id);

  status_t StartStream(const uint32_t track_id, sp<IBufferConsumer>& consumer);

  status_t StopStream(const uint32_t track_id);

  status_t SetCameraParam(CameraMetadata &meta);

  status_t GetCameraParam(CameraMetadata &meta);

 private:

  friend class CameraPort;

  status_t CreateDeviceStream(CameraStreamParameters& params,
                              int32_t* stream_id);

  status_t DeleteDeviceStream(int32_t stream_id);

  status_t CreateCaptureRequest(Camera3Request& request,
                                camera3_request_template_t template_type);

  status_t UpdateRequest(bool is_streaming);

  status_t CancelRequest();

  status_t ReturnStreamBuffer(int32_t stream_id, StreamBuffer buffer);

  uint32_t GetJpegSize(uint8_t *blobBuffer, uint32_t width);

  //Camera client callbacks.
  void NonZslCaptureCallback(int32_t stream_id, StreamBuffer buffer);

  void CameraErrorCb(CameraErrorCode errorCode, const CaptureResultExtras &);

  void CameraIdleCb();

  void CameraShutterCb(const CaptureResultExtras &, int64_t time_stamp);

  void CameraPreparedCb(int32_t);

  void CameraResultCb(const CaptureResult &result);

  sp<Camera3DeviceClient>  camera_device_;
  CameraClientCallbacks    camera_callbacks_;
  int32_t                  camera_id_;
  Mutex                    device_access_lock_;
  CameraStartParam         camera_start_params_;

  // Global Capture request.
  Camera3Request           streaming_request_;
  int32_t                  streaming_request_id_;

  //Non zsl capture request.
  Camera3Request           snapshot_request_;
  int32_t                  snapshot_request_id_;
  ImageInfo                snapshot_info_;
  CaptureImageCb           client_capture_cb_;

  // Map of <consumer id and CameraPort>
  DefaultKeyedVector<uint32_t, sp<CameraPort> > active_ports_;
};

enum class CameraPortType {
  kVideo,
  kPreview,
  kZSL,
};

enum class PortState {
  PORT_CREATED,
  PORT_READYTOSTART,
  PORT_STARTED,
  PORT_READYTOSTOP,
  PORT_STOPPED,
};

// CameraPort is one to one mapped to Camera device stream. It takes buffers
// from camera stream and passes to its consumers, for optimization reason
// single port can serve multiple consumers if their characterstics are exactly
// same.
class CameraPort : public RefBase {
 public:
  CameraPort(CameraStreamParam& param, CameraPortType port_type,
             CameraContext *context);

  ~CameraPort();

  status_t Init();

  status_t DeInit();

  status_t Start(const uint32_t consumer_id,
                 const sp<IBufferConsumer>& consumer);

  status_t Stop(const uint32_t consumer_id);

  // Apis to Add/Remove consumer at run time.
  status_t AddConsumer(const uint32_t consumer_id,
                       const sp<IBufferConsumer>& consumer);

  status_t RemoveConsumer(const uint32_t consumer_id);

  void NotifyBufferReturned(const StreamBuffer& buffer);

  int32_t GetNumConsumers();

  bool IsReadyToStart();

  PortState& getPortState();

  int32_t GetCameraStreamId() { return camera_stream_id_; }

 private:

  bool IsConsumerIdValid(const uint32_t id);

  void StreamCallback(int32_t stream_id, StreamBuffer Buffer);

  sp<IBufferProducer>    buffer_producer_impl_;
  CameraPortType         port_type_;
  CameraStreamParam      params_;
  CameraContext*         context_;
  int32_t                camera_stream_id_;
  CameraStreamParameters cam_stream_params_;
  Mutex                  consumer_lock_;
  bool                   ready_to_start_;
  PortState              port_state_;

  // map of <consumer id, IBufferConsumer>
  DefaultKeyedVector<uint32_t , sp<IBufferConsumer> > consumer_map_;


};

}; //namespace recorder

}; //namespace qmmf
