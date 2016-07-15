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

#include <cstddef>
#include <cstdlib>
#include <vector>
#include <string>

#include "qmmf_recorder_params.h"

namespace qmmf {
namespace recorder {

class RecorderClient;

// Client interface for audio, video recording and image capture.
//
// For audio and video recording, a session must be created which
// controls the state machine of recording. A session can contain
// multiple audio and video tracks and multiple sessions can be
// created and run at the same time.
// Do note that A/V muxer is not part of the Recorder class.
// Recorder provides elementary stream callback to clients and clients
// manage the muxer.
//
// Recorder API provides both single, burst and timed image capture
// Do note that Image capture is not associated with session and can
// be triggered independent of session.
//
// Some of the APIs in the class are sync and others async.
// The API documentation explicitly says whether the API is async.
//
// Callbacks used for async communication from recorder class are expected
// to be lambda functions. The client is expected to capture the context
// while setting the callback.
class Recorder {
 public:
  Recorder();

  ~Recorder();

  // Connect to recoder service and set callback.
  status_t Connect(RecorderCb& cb);

  // Disconnect from recorder service.
  // All the session & tracks should be deleted by application before calling
  // Disconnet Api.
  status_t Disconnect();

  // Initializes camera and prepares camera for image capture and/or video
  // record. This API must be called before calling create video track or
  // calling CaptureImage API
  // For 360 degree capture, camera_id vector should contain the id of
  // multiple cameras involved in 360 capture
  status_t StartCamera(std::vector<uint32_t> &camera_id,
                       const CameraStartParam &param);

  // Stops camera. This API should be called to free up all resources
  // associated with camera. StopCamera cannot be called when there is an
  // active video record session or image capture session.
  status_t StopCamera(std::vector<uint32_t> &camera_id);

  // Creates session and returns session id.
  // session id is used to idenfity the session in subsequent API calls.
  // A session can contain multiple audio and video tracks. All tracks within
  // a session changes states (like start, stop, pause) together.
  //
  // A callback(cb) must be registered along with session creation. This cb
  // is used by the recorder to notify state change event notification and
  // session specific errors back to clients asynchronously.
  //
  // Clients may create and run multiple sessions concurrently. All tracks
  // within a session changes state (start, pause, stop etc) together.
  status_t CreateSession(SessionCb& cb, uint32_t* session_id);

  // Deletes session corresponding to id. Session must be stopped before
  // calling delete.
  // All tracks within the sesion must be deleted before calling DeleteSession
  status_t DeleteSession(const uint32_t session_id);

  // Starts session corresponding to id. Session shall contain at least one
  // track before calling start.
  // This is an async API. When start is completed, session specific event cb
  // is called by recorder
  status_t StartSession(const uint32_t session_id);

  // Stops session corresponding to id. If the do_flush flag is set to true,
  // the pending buffers in the pipeline is discarded else stop waits till
  // buffers in its pipeline is encoded.
  // EOS flag is set along with last buffer cb of each track within the
  // session. This is an async API. When stop is completed, session specific
  // event cb is called by recorder
  status_t StopSession(const uint32_t session_id, bool do_flush);

  // Pause session corresponding to id. When the pause is called the camera
  // device or audio device is not paused, but only encoding of the tracks
  // is paused. This is an async API. When pause is completed, session
  // specific event cb is called by recoder
  status_t PauseSession(const uint32_t session_id);

  // Resumes session corresponding to id. When the session is resumed,
  // the timestamp for all tracks in the session are resumed ignoring the
  // duration of pause. This is an async API. When resume is completed,
  // session specific event cb is called by recorder
  status_t ResumeSession(const uint32_t session_id);

  // Creates an audio track and associates it to the session id provided.
  // User must specify the unqiue track_id for the session.
  // params must specify the audio track characteristics such as codec,
  // bitrate etc. cb is used by the recorder to inform clients about track
  // data availability and track specific async errors.
  status_t CreateAudioTrack(const uint32_t session_id, uint32_t track_id,
                            const AudioTrackCreateParam& param, TrackCb& cb);

  // Creates an video track and associates it to the session uuid provided.
  // User must specify the unqiue track_id for the session. params must
  // specify the video track characteristics such as codec, bitrate etc.
  // cb is used by the recorder to inform clients about track data
  // availability and track specific async errors
  status_t CreateVideoTrack(const uint32_t session_id, uint32_t track_id,
                            const VideoTrackCreateParam& param, TrackCb& cb);

  // Returns the track buffer back to recoder
  status_t ReturnTrackBuffer(const uint32_t session_id,
                             const uint32_t track_id,
                             std::vector<TrackBuffer> &buffers);

  // Changes runtime audio track parameters such as audio source device.
  // The type of *param* data depends on the enum value of *type*
  // param_size should be set to sizeof the param data structure
  status_t SetAudioTrackParam(const uint32_t session_id,
                              const uint32_t track_id,
                              AudioTrackParamType type, const void *param,
                              size_t param_size);

  // Changes runtime video track params such as video encoder
  // bitrate, framerate, IDR insertion etc
  // The type of *param* data depends on the enum value of *type*
  // param_size should be set to sizeof the param data structure
  status_t SetVideoTrackParam(const uint32_t session_id,
                              const uint32_t track_id,
                              VideoTrackParamType type, const void *param,
                              size_t param_size);

  // Deletes Audio track. The track can be deleted only when the session
  // the track is associated with is in stopped state
  status_t DeleteAudioTrack(const uint32_t session_id,
                            const uint32_t track_id);

  // Deletes video track. The track can be deleted only when the session
  // the track is associated with is in stopped state
  status_t DeleteVideoTrack(const uint32_t session_id,
                            const uint32_t track_id);

  // API to capture single, burst or timed image capture from camera.
  //
  // @param camera_id: A vector with device IDs of the camera. The vector
  //         could contain more than a single camera for 360 capture usecase
  // @param param: Detail specification of image to be captured
  // @param cb: Callbacks for data and error notifications
  //
  // This is an async API. When the image is ready, data callback specified
  // through cb is called which enables clients to process the image data.
  // If multiple images are captured, data cb is called for every image.
  // If CancelCaptureImage() is called before all images are returned to
  // clients, the Recorder stops the burst or timed image capture and returns
  // an event indicating CANCEL is complete
  status_t CaptureImage(std::vector<uint32_t> &camera_id,
                        const ImageParam &param,
                        CaptureImageCb& cb);

  status_t CancelCaptureImage();

  // Sets camera parameters. param would be interpreted
  // based on param_type.
  status_t SetCameraParam(uint32_t camera_id, CameraParamType param_type,
                          const void *param, size_t param_size);

  status_t GetCameraParam(uint32_t camera_id, CameraParamType param_type,
                          void *param, size_t param_size);

  // Create overlay object of type static image, date/time, bounding box,
  // simple text, and privacy mask.This Api returns the object id which
  // can be use for configuration change at runtime.
  status_t CreateOverlayObject(const OverlayParam &param,
                               uint32_t *overlay_id);

  // Overlay object can be deleted at any point after creation.
  status_t DeleteOverlayObject(const uint32_t overlay_id);

  // Overlay object's parameters can be queried after creation, it is
  // recommended to call get parameters first before setting any new
  // parameters using Api updateOverlayObject.
  status_t GetOverlayObjectParams(const uint32_t overlay_id,
                                  OverlayParam &param);

  // Overlay object's configuration can be updated at run time using this Api.
  // Client has to provide overlay Id and updated parameters. It is
  // recommended to call getOverlayObjectParams first to get current
  // parameters then update them using this Api.
  status_t UpdateOverlayObjectParams(const uint32_t overlay_id,
                                     const OverlayParam &param);

  // Overlay Object can be set and removed per track at runtime
  status_t SetOverlay(const uint32_t session_id, const uint32_t track_id,
                      const uint32_t overlay_id);

  status_t RemoveOverlay(const uint32_t session_uuid, const uint32_t track_id,
                         const uint32_t overlay_id);

 private:
  RecorderClient* recorder_client_;

};

};  // namespace recorder

};  // namespace qmmf
