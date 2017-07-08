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
#include <utils/String8.h>

#include "recorder/src/service/qmmf_recorder_common.h"

#include "../interface/qmmf_camera_reprocess.h"
#include "../plugin/qmmf_camera_plugin.h"
#include "../memory/qmmf_camera_memory_pool.h"
#include "../common/qmmf_camera_thread.h"
#include "../common/qmmf_camera_module.h"

namespace qmmf {

namespace recorder {

class IBufferConsumer;
class IBufferProducer;

struct ReprocessCreate {
  uint32_t width;
  uint32_t height;
  int32_t  format;
};

struct ReprocessNodeCreate {
  ReprocessCreate in;
  ReprocessCreate out;
  uint32_t frame_rate;
  uint32_t max_buffer_count;
};

struct ReprocessImgParams {
  uint32_t width;
  uint32_t height;
  int32_t  format;
  int32_t  gralloc_flags;
  uint32_t max_buffer_count;
  uint32_t max_size;
  uint32_t frame_rate;
};

struct ReprocessNodeParams {
  ReprocessImgParams in;
  ReprocessImgParams out;
};


enum class ReprocessNodeState {
  CREATED,
  INITIALIZED,
  READYTOSTART,
  READYTOSTOP,
};

class ReprocessNode : public CameraThread,
                      public ReprocessPlugin<ReprocessNode>,
                      public IReprocessCallbacks {

 public:

   ReprocessNode(const char* srt, IPostProcCameraContext* context);

   ~ReprocessNode();

   ReprocCaps GetCapabilities() {return caps_;};

   int32_t Initialize(int32_t input_stream_id,
                      ReprocessNodeParams& reproc_node_param,
                      void* static_meta);

   void OnFrameAvailable(StreamBuffer& buffer);

   void NotifyBufferReturned(StreamBuffer& buffer);

   status_t AddConsumer(sp<IBufferConsumer>& consumer);

   status_t RemoveConsumer(sp<IBufferConsumer>& consumer);

   status_t ReturnBuffers();

   void AddResult(const void* result);

   void getDefaultParam(ReprocessNodeParams& reproc_node_param,
                        ReprocessNodeCreate& create_params);

   void Start();

   void Stop();

 protected:

   bool ThreadLoop() override;

 private:

   status_t ReturnBufferToClient(StreamBuffer &buffer);

   void ReprocessLibCallback(StreamBuffer in_buff, StreamBuffer out_buff) override;

   status_t GetBuffer(StreamBuffer* buffer) override;

   void SetCallBacks(sp<IReprocessCallbacks>& cb) override;

   void ClearCallBacks() override;

   ReprocessNodeParams           init_params_;
   ReprocessNodeState            state_;
   Vector<StreamBuffer>          buffer_list_;
   Mutex                         wait_lock_;
   Condition                     wait_for_frames_;

   static const nsecs_t kFrameTimeout  = 50000000;  // 50 ms.

   int32_t                       reprocess_stream_id_;
   sp<CameraModule>              camera_module_;

   sp<MemPool>                   mem_pool_;
   String8                       name_;
   ReprocCaps                    caps_;

   bool                          stop_;
   Mutex                         stop_lock_;
};

}; //namespace recorder

}; //namespace qmmf
