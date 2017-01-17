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

#include "qmmf_transcode_params.h"

#pragma once


namespace qmmf {
namespace transcode {

class TransCoderPipe : public enable_shared_from_this<TransCoderPipe> {

public:

  TransCoderPipe(TransCodeType track_type);
  ~TransCoderPipe();

  status_t PreparePipeline();
  status_t RemovePipe();
  void PassInputCodec(shared_ptr<IAVCodec>& arg) {in_avcodec_ = arg;}
  void PassOutputCodec(shared_ptr<IAVCodec>& arg) {out_avcodec_ = arg;}
  void Sendforward(TransCodeBuffer& buffer);
  void Sendbackward(TransCodeBuffer& buffer);
  static void* Transport(void*arg);

#ifdef DEBUG_PIPE
  void PipeStatusThread();
#endif

private:

  class TransCoderPipeIn : public ICodecSource,
                           public enable_shared_from_this<TransCoderPipeIn> {
  public:
    TransCoderPipeIn(shared_ptr<IAVCodec> arg,
        shared_ptr<TransCoderPipe> parent, CodecType type);
    ~TransCoderPipeIn();

    status_t PreparePipeline();
    ::std::string BufferStatus();
    void ReleaseCodec() {avcodec_.reset();}
    void ReceiveBuffer(TransCodeBuffer& buffer);
    status_t  GetBuffer(BufferDescriptor& buffer_descriptor,
                        void* client_data) override;
    status_t  ReturnBuffer(BufferDescriptor& buffer_descriptor,
                        void* client_data) override;
    status_t  NotifyPortEvent(PortEventType event_type,
                            void* event_data) override;
  private:
    status_t AddBufferList(vector<TransCodeBuffer>& list);

    vector<TransCodeBuffer>             buffer_list_;
    TSQueue<TransCodeBuffer>            free_buffer_queue_;
    TSQueue<TransCodeBuffer>            occupy_buffer_queue_;
    shared_ptr<IAVCodec>                avcodec_;
    weak_ptr<TransCoderPipe>            pipe_;
    const uint32_t                      port_index_;
    Mutex                               wait_for_frame_lock_;
    Condition                           wait_for_frame_;
    CodecType                           codec_type_;
  };

  class TransCoderPipeOut : public ICodecSource,
                            public enable_shared_from_this<TransCoderPipeOut> {
  public:
    TransCoderPipeOut(shared_ptr<IAVCodec> arg,
        shared_ptr<TransCoderPipe> parent, CodecType type);
    ~TransCoderPipeOut();

    status_t PreparePipeline();
    ::std::string BufferStatus();
    void ReleaseCodec() {avcodec_.reset();}
    void ReceiveBuffer(TransCodeBuffer& buffer);
    status_t GetBuffer(BufferDescriptor& buffer_descriptor,
                      void* client_data) override;
    status_t ReturnBuffer(BufferDescriptor& buffer_descriptor,
                          void* client_data) override;
    status_t NotifyPortEvent(PortEventType event_type,
                             void* event_data) override;
  private:
    status_t AddBufferList(vector<TransCodeBuffer>& list);

    vector<TransCodeBuffer>             buffer_list_;
    TSQueue<TransCodeBuffer>            free_buffer_queue_;
    TSQueue<TransCodeBuffer>            occupy_buffer_queue_;
    shared_ptr<IAVCodec>                avcodec_;
    weak_ptr<TransCoderPipe>            pipe_;
    const uint32_t                      port_index_;
    Mutex                               wait_for_frame_lock_;
    Condition                           wait_for_frame_;
    CodecType                           codec_type_;
  };

  TransCodeType                         track_type_;
  shared_ptr<TransCoderPipeIn>          pipe_in_;
  shared_ptr<TransCoderPipeOut>         pipe_out_;
  weak_ptr<IAVCodec>                    in_avcodec_;
  weak_ptr<IAVCodec>                    out_avcodec_;
  TSQueue<TransCodeBuffer>              forward_dir_queue_;
  TSQueue<TransCodeBuffer>              backward_dir_queue_;
  Mutex                                 transport_lock_;
  Mutex                                 wait_frame_fwqueue_lock_;
  Condition                             wait_frame_fwqueue_;
  Mutex                                 wait_frame_bwqueue_lock_;
  Condition                             wait_frame_bwqueue_;
  pthread_t                             transport_thread_;
};

};  //namespace transcode
};  //namespace qmmf