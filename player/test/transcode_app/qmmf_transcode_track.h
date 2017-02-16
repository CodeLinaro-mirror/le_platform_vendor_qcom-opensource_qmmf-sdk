/*
* Copyright (c) 2017, The Linux Foundation. All rights reserved.
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



#include "qmmf_transcode_core.h"
#include "qmmf_transcode_sink.h"
#include "qmmf_transcode_pipe.h"
#include "qmmf_transcode_params.h"

#include "player/test/demuxer/qmmf_demuxer_mediadata_def.h"
#include "player/test/demuxer/qmmf_demuxer_intf.h"
#include "player/test/demuxer/qmmf_demuxer_sourceport.h"

#pragma once

namespace qmmf {
namespace transcode {

class TransCoderCore;
class TransCoderSink;
class TransCoderPipe;

class TranscoderTrack {
public:

  TranscoderTrack(char* file);

  ~TranscoderTrack();

  status_t PreparePipeline();

  status_t Start();

  status_t Stop();

  status_t Delete();

private:

  bool IsStop();

  static status_t ParseFile(char*filename, void*arg);

  status_t FillParams();

  status_t CreateDataSource();

  status_t ReadMediaInfo();

  status_t ReadAudioTrackMediaInfo(uint32 ulTkId,
      FileSourceMnMediaType eTkMnType);

  status_t ReadVideoTrackMediaInfo(uint32 ulTkId,
      FileSourceMnMediaType eTkMnType);

  static void* DeliverInput(void *ptr);
  static void* ReceiveOutput(void *ptr);
  static void* StopTransCoding(void* ptr);

  inline shared_ptr<TransCoderCore>& getInputBufferSource() {
    return transcoder_core_;
  }

  inline shared_ptr<TransCoderSink>& getOutputBufferSource() {
    return transcoder_sink_;
  }

  inline bool IsInputPortStop();

  shared_ptr<TransCoderCore>            transcoder_core_;
  shared_ptr<TransCoderSink>            transcoder_sink_;
  shared_ptr<TransCoderPipe>            transcoder_pipe_;
  TransCodeParams                       params_;
  Mutex                                 stop_lock_;
  bool                                  stop_;

  MM_TRACK_INFOTYPE                     m_sTrackInfo_;
  CMM_MediaSourcePort*                  m_pIStreamPort_;
  CMM_MediaDemuxInt*                    m_pDemux_;
  TrackTypes                            track_type_;

  bool                                  isFirstFrame_;
  bool                                  isLastFrame_;
  bool                                  input_stop_;
  Mutex                                 input_stop_lock_;

  pthread_t                             deliver_thread_;
  pthread_t                             receiver_thread_;
  pthread_t                             stop_thread_;

  uint64_t                              num_delivered_frames;
  uint64_t                              num_received_frames;
};

};  //namespace transcode
};  //namespace qmmf