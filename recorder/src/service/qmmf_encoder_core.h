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

#include "recorder/src/service/qmmf_recorder_common.h"
#include "recorder/src/service/qmmf_camera_source.h"
#include "common/codecadaptor/src/qmmf_avcodec.h"

namespace qmmf {

namespace recorder {

class TrackEncoder;

class EncoderCore {
 public:

  static EncoderCore* CreateEncoderCore();

  ~EncoderCore();

  status_t AddSource(const sp<TrackSource>& track_source,
                     VideoTrackParams& params);

  status_t StartTrackEncoder(uint32_t track_id);

  status_t StopTrackEncoder(uint32_t track_id);

  status_t SetTrackEncoderParams(uint32_t track_id,
                                 VideoTrackParamType param_type, void* param,
                                 uint32_t param_size);

  status_t DeleteTrackEncoder(uint32_t track_id);

  status_t ReturnTrackBuffer(const uint32_t track_id,
                             std::vector<BnBuffer> &buffers);
 private:

  bool isTrackValid(uint32_t track_id);

  // vector <track_id, sp<TrackEncoder> >
  DefaultKeyedVector<uint32_t, sp<TrackEncoder> > track_encoders_;

  int32_t ion_device_;

  // Not allowed
  EncoderCore();
  EncoderCore(const EncoderCore&);
  EncoderCore& operator=(const EncoderCore&);
  static EncoderCore* instance_;
};

class TrackEncoder : public IOutputCodecSource {
 public:

  TrackEncoder(int32_t ion_device);

  ~TrackEncoder();

  status_t Init(const sp<TrackSource>& track_source,
                VideoTrackParams& params);

  status_t Start();

  status_t Stop();

  status_t SetParams(VideoTrackParamType param_type, void* param,
                     uint32_t param_size);

  status_t ReleaseHeaders();

  // Methods of AVCodec
  // This method provides free output port buffer to AVCodec.
  status_t GetBuffer(CodecBuffer& codec_buffer) override;

  // This method provides filled output buffer to TrackEncoder.
  status_t ReturnBuffer(CodecBuffer& codec_buffer) override;

  // Method to handle returned buffers from client.
  status_t OnBufferReturnFromClient(std::vector<BnBuffer> &buffers);

 private:

  status_t AllocOutputPortBufs();

  // This methos Notifies bitstream buffer to remote client.
  void NotifyBufferToClient(CodecBuffer& codec_buffer);

  void EventCallback(OMX_EVENTTYPE event, OMX_U32 data1, OMX_U32 data2);

#ifdef DUMP_BITSTREAM
  void DumpBitStream(CodecBuffer& codec_buffer);
#endif

  uint32_t TrackId() { return track_params_.track_id; }

  sp<TrackSource>  track_source_;
  VideoTrackParams track_params_;
  sp<AVCodec>      avcodec_;

  Vector<CodecBuffer>   output_buffer_list_;

  TSQueue<CodecBuffer>  output_free_buffer_queue_;
  TSQueue<CodecBuffer>  output_occupy_buffer_queue_;
  Mutex                 lock_;
  Condition             wait_for_frame_;
  int32_t               ion_device_;
  Mutex                 queue_lock_;
  bool                  eos_atoutput_;
#ifdef DUMP_BITSTREAM
  int32_t               file_fd_;
#endif
};

}; // namespace recorder

}; // name space qmmf
