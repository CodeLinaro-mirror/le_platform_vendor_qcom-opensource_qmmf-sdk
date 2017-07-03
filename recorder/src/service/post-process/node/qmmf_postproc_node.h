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

#include <utils/RefBase.h>
#include <utils/Log.h>
#include <map>
#include <mutex>
#include <condition_variable>

#include "recorder/src/service/qmmf_recorder_common.h"
#include "common/qmmf_common_utils.h"

#include "../interface/qmmf_postproc.h"
#include "../interface/qmmf_postproc_module.h"
#include "../plugin/qmmf_postproc_plugin.h"
#include "../memory/qmmf_postproc_memory_pool.h"
#include "../common/qmmf_postproc_thread.h"
#include "../factory/qmmf_postproc_factory.h"
namespace qmmf {

namespace recorder {

class IBufferConsumer;
class IBufferProducer;

struct ReprocessCreate {
  uint32_t           width;
  uint32_t           height;
  int32_t            format;
};

struct PostProcNodeCreate {
  ReprocessCreate    in;
  ReprocessCreate    out;
  uint32_t           frame_rate;
  uint32_t           max_buffer_count;
};

struct PostProcImgParams {
  uint32_t           width;
  uint32_t           height;
  int32_t            format;
  int32_t            gralloc_flags;
  uint32_t           max_buffer_count;
  uint32_t           max_size;
  uint32_t           frame_rate;
};

struct PostProcNodeParams {
  PostProcImgParams  in;
  PostProcImgParams  out;
};

enum class PostProcNodeState {
  CREATED,
  INITIALIZED,
  LINKED,
  STARTING,
  ACTIVE,
  STOPPING,
};

class PostProcNode;

class InputHandler : public PostProcThread {
 public:

  InputHandler(PostProcNode *node) : node_(node) {}

  void AddBuf(StreamBuffer& buffer);

  void FlushBufs(std::function<void(StreamBuffer&)> BuffHandler);

  void* MapBuf(StreamBuffer& buffer);

  void UnMapBufs();

 protected:

  bool ThreadLoop() override;

 private:

  struct map_data_t {
    void* addr;
    size_t size;
  };

  static const nsecs_t              kFrameTimeout  = 50000000;  // 50 ms.
  PostProcNode                      *node_;
  std::map<uint32_t, map_data_t>    mapped_buffs_;
  List<StreamBuffer>                bufs_list_;
  std::mutex                        wait_lock_;
  std::condition_variable           wait_;

};

class OutputHandler : public PostProcThread {
 public:

  OutputHandler(PostProcNode *node) : node_(node) {}

  void AddBuf(StreamBuffer& buffer);

  void FlushBufs(std::function<void(StreamBuffer&)> BuffHandler);

 protected:

  bool ThreadLoop() override;

 private:

  static const nsecs_t              kFrameTimeout  = 50000000;  // 50 ms.
  PostProcNode                      *node_;
  std::vector<StreamBuffer>         bufs_list_;
  std::mutex                        wait_lock_;
  std::condition_variable           wait_;

};

class PostProcNode : public PostProcPlugin<PostProcNode>,
                     public IPostProcEventListener,
                     public RefBase {
   friend class InputHandler;
   friend class OutputHandler;

 public:
   PostProcNode(const char* srt, IPostProc* context);

   ~PostProcNode();


   int32_t Initialize(int32_t input_stream_id,
                      PostProcNodeParams& reproc_node_param,
                      void* static_meta);

   void OnFrameAvailable(StreamBuffer& buffer) override;

   void NotifyBufferReturned(StreamBuffer& buffer) override;

   status_t OnFrameProcessed(StreamBuffer &input_buffer) override;

   status_t OnFrameReady(StreamBuffer &output_buffer) override;

   status_t AddConsumer(sp<IBufferConsumer>& consumer);

   status_t RemoveConsumer(sp<IBufferConsumer>& consumer);

   void AddResult(const void* result);

   void getDefaultParam(PostProcNodeParams& reproc_node_param,
                        PostProcNodeCreate& create_params);

   status_t Start();

   status_t Stop();

 private:

   status_t ReturnBufferToClient(StreamBuffer &buffer);

   status_t ReturnBuffers();

   InputHandler                      in_;
   OutputHandler                     out_;

   sp<MemPool>                       mem_pool_;
   sp<PostProcFactory>               reprocess_factory_;
   sp<IPostProcModule>               module_;

   PostProcNodeParams                init_params_;

   int32_t                           id_;
   String8                           name_;
   ReprocCaps                        caps_;

   PostProcNodeState                 state_;
   std::mutex                        state_lock_;
};

}; //namespace recorder

}; //namespace qmmf
