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
#include <vector>

#include "qmmf-plugin/qmmf_alg_plugin.h"

#include "include/qmmf-sdk/qmmf_codec.h"

#include "common/qmmf_common_utils.h"

#include "recorder/src/service/post-process/interface/qmmf_postproc.h"

namespace qmmf {

namespace recorder {

using namespace qmmf_alg_plugin;

struct PostProcCreateParam {
  uint32_t stride;
  uint32_t scanline;
  uint32_t width;
  uint32_t height;
  int32_t  format;
};


/** PostProcCaps:
 *    @internal_buff: internal buffers
 *    @in_formats_: supported input formats
 *    @out_formats_: supported output formats
 *    @min_width_: min supported frame width dimension
 *    @min_height_: min supported frame height dimension
 *    @max_width_: max supported frame width dimension
 *    @max_height_: max supported frame height dimension
 *    @crop_support_: image crop capability flag
 *    @scale_support_: image scale capability flag
 *    @inplace_processing_: inplace processing is required
 *    @lib_version_: library version
 *    @usage_: specific for allocator usage flags
 *
 *  This class defines the post processing module capabilities
 **/
struct PostProcCaps {
  uint32_t                  output_buff_;
  std::vector<BufferFormat> in_formats_;
  std::vector<BufferFormat> out_formats_;
  uint32_t                  min_width_;
  uint32_t                  min_height_;
  uint32_t                  max_width_;
  uint32_t                  max_height_;
  bool                      crop_support_;
  bool                      scale_support_;
  bool                      inplace_processing_;
  std::string               lib_version_;
  uint32_t                  usage_;
};


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
  virtual void OnFrameProcessed(const StreamBuffer &input_buffer) = 0;

  /** OnFrameReady
   *    @output_buffer: output buffer
   *
   * Indicates that output buffer is processed
   *
   * return: void
   **/
  virtual void OnFrameReady(const StreamBuffer &output_buffer) = 0;

  /** OnError
   *    @err: error id
   *
   * Indicates runtime error
   *
   * return: void
   **/
  virtual void OnError(RuntimeError err) = 0;
};

class IPostProcModule : public RefBase {

 public:

   virtual ~IPostProcModule() {};

   virtual status_t Create(const int32_t stream_id,
                           const PostProcCreateParam& input,
                           const PostProcCreateParam& output,
                           const uint32_t frame_rate,
                           const uint32_t num_images,
                           const void* static_data,
                           const void* context,
                           int32_t &out_stream_id) = 0;

   virtual status_t Delete() = 0;

   virtual void SetCallbacks(IPostProcEventListener *cb) = 0;

   virtual status_t Configure(const std::string config_json_data) = 0;

   virtual status_t Process(const std::vector<StreamBuffer> &in_buffers,
                            const std::vector<StreamBuffer> &out_buffers) = 0;

   virtual status_t ReturnBuff(StreamBuffer &buffer) = 0;

   virtual void AddResult(const void* result) = 0;

   virtual status_t Start() = 0;

   virtual status_t Stop() = 0;

   virtual PostProcCreateParam GetInput(const PostProcCreateParam &out) = 0;

   virtual PostProcCreateParam GetOutput(const PostProcCreateParam &in) = 0;

   virtual status_t GetCapabilities(PostProcCaps &caps) = 0;
};

}; //namespace recorder

}; //namespace qmmf
