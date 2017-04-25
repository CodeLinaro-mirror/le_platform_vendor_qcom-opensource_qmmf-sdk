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

#pragma once

#include "../node/qmmf_camera_node.h"

namespace qmmf {

namespace recorder {

class IBufferConsumer;
class IBufferProducer;

enum class ReprocessPipeState {
  CREATED,
  INITIALIZED,
  READYTOSTART,
  READYTOSTOP,
};

class ReprocessPipe : public virtual  RefBase {

 public:

   ReprocessPipe(IPostProcCameraContext * context);

   ~ReprocessPipe();

   int32_t Create(int32_t stream_id,
                  const char* pipe[],
                  const uint32_t pipe_size,
                  CameraStreamParameters &stream_param,
                  void* static_meta);

   status_t AddConsumer(sp<IBufferConsumer>& consumer);

   status_t RemoveConsumer(sp<IBufferConsumer>& consumer);

   void AddResult(const void* result);

   void Start();

   void Stop();

   sp<IBufferConsumer>& GetConsumerIntf();

   void PipeNotifyBufferReturn(StreamBuffer& buffer);

 private:

   int32_t Initialize(int32_t stream_id,
                      ReprocessNodeCreate& reproc_node_create_param,
                      void* static_meta);

   void LinkPipe(sp<IBufferConsumer>& consumer);

   void UnlinkPipe(sp<IBufferConsumer>& consumer);

   CameraStreamParameters        init_params_;

   ReprocessPipeState            state_;

   int32_t                       reprocess_stream_id_;

   Vector<sp<ReprocessNode>>     reproc_node_pipe_;

   IPostProcCameraContext*       context_;

   sp<IBufferConsumer>           pipe_consumer_;

};

}; //namespace recorder

}; //namespace qmmf
