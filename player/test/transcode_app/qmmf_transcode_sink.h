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


#include "qmmf_transcode_pipe.h"
#include "qmmf_transcode_params.h"

#pragma once

namespace qmmf {
namespace transcode {

class TransCoderPipe;

class TransCoderSink: public ICodecSource,
                      public enable_shared_from_this<TransCoderSink> {

public:
  TransCoderSink(CodecType type, CodecParam& codec_params,
                 shared_ptr<TransCoderPipe>& pipe);

  ~TransCoderSink();

  status_t PreparePipeline();

  status_t StartCodec();

  status_t StopCodec();

  status_t DeleteCodec();

  status_t PauseCodec();

  status_t ResumeCodec();

  status_t DequeInputBuffer(TransCodeBuffer& buffer);

  status_t QueueInputBuffer(TransCodeBuffer& buffer);

  status_t SetCodecParameters();

  status_t AddBufferList(vector<TransCodeBuffer>& list);

  status_t GetBuffer(BufferDescriptor& buffer_descriptor,
                    void* client_data) override;

  status_t ReturnBuffer(BufferDescriptor& buffer_descriptor,
                        void* client_data) override;

  status_t NotifyPortEvent(PortEventType event_type,
                           void* event_data) override;

private:

  shared_ptr<TransCoderPipe>            pipe_;
  shared_ptr<IAVCodec>                  avcodec_;
  CodecType                             codec_type_;
  CodecParam                            params_;
  CodecMimeType                         mime_;
  vector<TransCodeBuffer>               buffer_list_;
  const uint32_t                        port_index_;
  TSQueue<TransCodeBuffer>              free_buffer_queue_;
  TSQueue<TransCodeBuffer>              occupy_buffer_queue_;
  TSQueue<TransCodeBuffer>              filled_buffer_queue_;
  TSQueue<TransCodeBuffer>              being_read_buffer_queue_;
  Mutex                                 wait_for_frame_lock_;
  Condition                             wait_for_frame_;
  Mutex                                 wait_for_filled_frame_lock_;
  Condition                             wait_for_filled_frame_;
};

};  //namespace transcode
};  //namespace qmmf