/*
 * Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
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

/*! @file qmmf_postproc_pipe.h
*/

#pragma once

#include <string>
#include <vector>

#include "common/utils/qmmf_condition.h"
#include "../node/qmmf_postproc_node.h"
#include "../factory/qmmf_postproc_factory.h"

namespace qmmf {

namespace recorder {

class IBufferConsumer;
class IBufferProducer;

struct PipeIOParam {
  uint32_t width;
  uint32_t height;
  uint32_t stride;
  uint32_t scanline;
  uint32_t frame_rate;
  int32_t format;
  MemAllocFlags alloc_flags;
  uint32_t buffer_count;
  uint32_t max_internal_buffers;
  BufferFormat internal_format;
  bool frame_skip;
  bool exif_en;
};

enum class PostProcPipeState {
  CREATED,
  INITIALIZE,
  INITIALIZED,
  CONFIGURED,
  READYTOSTART,
  READYTOSTOP,
};

/// @brief This class implements post-processing by managing plugins.
/// Plugins may be user provided or internal.
/// There is plugin factory which exposes the supported plugins.
class PostProcPipe {

 public:

   /// PostProcPipe Constructor
   PostProcPipe(IPostProc* context);

   /// PostProcPipe Destructor
   ~PostProcPipe();

   /// Create pipeline, validate it and add all required plugins.
   status_t CreatePipe(const PipeIOParam &pipe_out_param,
       const std::vector<uint32_t> &plugins, PipeIOParam &pipe_in_param);

   /// Delete pipeline
   status_t DeletePipe();

   /// Configure all nodes of a pipeline
   status_t Configure(const std::string &config_json_data);

   /// Add consumer to pipeline
   status_t AddConsumer(sp<IBufferConsumer>& consumer);

   /// Remove consumer from pipeline
   status_t RemoveConsumer(sp<IBufferConsumer>& consumer);

   /// Propagate camera result to all plugins
   void AddResult(const void* result);

   /// Start all plugins
   status_t Start(const int32_t stream_id);

   /// Stop all plugins. After this API buffers are returned.
   status_t Stop();

   /// Abort current processing. This is the blocking API and returns when
   /// all processing is done and buffers are returned
   status_t Abort();

   /// Return pipe consumer interface
   sp<IBufferConsumer>& GetConsumerIntf();

   /// Return buffer to the pipe
   void PipeNotifyBufferReturn(StreamBuffer& buffer);

   /// @cond PRIVATE
 private:

   void LinkPipe(sp<IBufferConsumer>& consumer);

   void UnlinkPipe(sp<IBufferConsumer>& consumer);

   std::shared_ptr<PostProcNode> FindInternalNode(const PostProcIOParam &output);

   bool IsRAWFormat(const BufferFormat &format);

   bool IsYUVFormat(const BufferFormat &format);

   bool IsJPEGFormat(const BufferFormat &format);

   bool IsFormatSupported(const std::set<BufferFormat> &formats,
                          const BufferFormat &format);

   bool IsFormatSupported(const std::set<BufferFormat> &formats,
                          const int32_t format);

   bool SupportsRAWFormat(const std::set<BufferFormat> &formats);

   bool SupportsYUVFormat(const std::set<BufferFormat> &formats);

   bool SupportsJPEGFormat(const std::set<BufferFormat> &formats);

   static const uint32_t         kWaitAbortTimeout = 2000000000; // 2 sec.

   PostProcPipeState             state_;

   std::vector<std::shared_ptr<PostProcNode>> pipe_;

   IPostProc*                    context_;

   std::shared_ptr<PostProcFactory> factory_;

   bool                          use_hal_jpeg_;

   bool                          abort_done_;
   QCondition                    abort_signal_;
   std::mutex                    abort_lock_;

   /// @endcond
};

}; //namespace recorder

}; //namespace qmmf
