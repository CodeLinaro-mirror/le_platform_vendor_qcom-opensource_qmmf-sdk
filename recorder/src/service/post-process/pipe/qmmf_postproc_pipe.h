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

#include <string>
#include <vector>

#include "../node/qmmf_postproc_node.h"
#include "../factory/qmmf_postproc_factory.h"

namespace qmmf {

namespace recorder {

class IBufferConsumer;
class IBufferProducer;

struct PipeInputParam {
  uint32_t width;
  uint32_t height;
  uint32_t frame_rate;
  int32_t format;
};

struct PipeOutputParam {
  uint32_t width;
  uint32_t height;
  uint32_t frame_rate;
  int32_t format;
  uint32_t image_quality;
};

enum class PostProcPipeType {
  kVideo,
  kPreview,
  kSnapshot,
};

enum class PostProcPipeState {
  CREATED,
  INITIALIZE,
  INITIALIZED,
  CONFIGURED,
  READYTOSTART,
  READYTOSTOP,
};

class PostProcPipe : public virtual  RefBase {

 public:

   PostProcPipe(IPostProc* context, const PostProcPipeType &type,
                const std::vector<uint32_t> &plugins);

   ~PostProcPipe();

   status_t BeginInit(const PipeOutputParam &output);

   int32_t EndInit(int32_t stream_id, const PipeInputParam &input,
                   uint32_t max_buffer_count);

   status_t GetInput(PipeInputParam &input);

   status_t AddConsumer(sp<IBufferConsumer>& consumer);

   status_t RemoveConsumer(sp<IBufferConsumer>& consumer);

   void AddResult(const void* result);

   status_t Start();

   status_t Stop();

   sp<IBufferConsumer>& GetConsumerIntf();

   void PipeNotifyBufferReturn(StreamBuffer& buffer);

 private:

   void LinkPipe(sp<IBufferConsumer>& consumer);

   void UnlinkPipe(sp<IBufferConsumer>& consumer);

   sp<PostProcNode> FindInternalNode(const PostProcIOParam &param,
                                     const PostProcCaps &caps);

   sp<PostProcNode> FindInternalNode(const PostProcIOParam &param,
                                     const PostProcReqs &reqs);

   bool IsRAWFormat(const BufferFormat &format);

   bool IsYUVFormat(const BufferFormat &format);

   bool IsJPEGFormat(const BufferFormat &format);

   bool SupportsRAWFormat(const std::set<BufferFormat> &formats);

   bool SupportsYUVFormat(const std::set<BufferFormat> &formats);

   bool SupportsJPEGFormat(const std::set<BufferFormat> &formats);

   PipeInputParam                input_param_;
   PipeOutputParam               output_param_;

   PostProcPipeState             state_;
   PostProcPipeType              type_;

   std::vector<sp<PostProcNode>> pipe_;

   IPostProc*                    context_;

   sp<IBufferConsumer>           pipe_consumer_;

   sp<PostProcFactory>           factory_;

};

}; //namespace recorder

}; //namespace qmmf
