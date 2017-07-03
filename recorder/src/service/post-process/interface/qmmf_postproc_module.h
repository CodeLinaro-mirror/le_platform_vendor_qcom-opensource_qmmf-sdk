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

#include <functional>
#include <utils/RefBase.h>
#include "common/qmmf_common_utils.h"

#include "recorder/src/service/post-process/common/qmmf_camera_base_types.h"
#include "recorder/src/service/post-process/interface/qmmf_postproc.h"

namespace qmmf {

namespace recorder {

class IPostProcEventListener {
 public:
  virtual ~IPostProcEventListener(){};

  /** OnFrameProcessed
   *    @input_buffer: input buffer
   *
   * Indicates that input buffer is processed
   *
   * return: void
   **/
  virtual status_t OnFrameProcessed(StreamBuffer &input_buffer) = 0;

  /** OnFrameReady
   *    @output_buffer: output buffer
   *
   * Indicates that output buffer is processed
   *
   * return: void
   **/
  virtual status_t OnFrameReady(StreamBuffer &output_buffer) = 0;
};

class IPostProcModule : public RefBase {

 public:

   virtual ~IPostProcModule() {};

   virtual status_t Create(const int32_t stream_id,
                           const ReprocParam& input,
                           const ReprocParam& output,
                           const uint32_t frame_rate,
                           const uint32_t num_images,
                           const void* static_data,
                           const void* context,
                           int32_t &out_stream_id) = 0;

   virtual status_t Delete() = 0;

   virtual void SetCallbacks(IPostProcEventListener *cb) = 0;

   virtual status_t Process(StreamBuffer &input_buffers,
                            StreamBuffer &output_buffers) = 0;

   virtual status_t ReturnBuff(StreamBuffer buffer) = 0;

   virtual void AddResult(const void* result) = 0;

   virtual status_t Start() = 0;

   virtual status_t Stop() = 0;

   virtual status_t GetCapabilities(ReprocCaps *caps) = 0;
};

}; //namespace recorder

}; //namespace qmmf
