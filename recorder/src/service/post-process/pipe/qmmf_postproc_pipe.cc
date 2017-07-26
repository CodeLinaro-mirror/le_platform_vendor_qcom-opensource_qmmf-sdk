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

#define TAG "RecorderPostProcPipe"

#include <utils/Vector.h>

#include "recorder/src/service/qmmf_recorder_utils.h"

#include "../interface/qmmf_postproc.h"
#include "../node/qmmf_postproc_node.h"

#include "qmmf_postproc_pipe.h"

namespace qmmf {

namespace recorder {

PostProcPipe::PostProcPipe(IPostProc* context,
                           const std::vector<uint32_t> &plugins)
    : context_(context),
      use_hal_jpeg_(false) {

  QMMF_VERBOSE("%s:%s: Enter", TAG, __func__);

  factory_ = PostProcFactory::getInstance();
  assert(factory_ != nullptr);

  for (auto& plugin_uid : plugins) {
    sp<PostProcNode> node = factory_->GetProcNode(plugin_uid);
    assert(node.get() != nullptr);
    pipe_.push_back(node);
  }

  char prop_val[PROPERTY_VALUE_MAX];
  property_get("persist.qmmf.postproc.haljpeg", prop_val, "0");
  use_hal_jpeg_ = (0 == atoi(prop_val)) ? false : true;

  state_ = PostProcPipeState::CREATED;
  QMMF_VERBOSE("%s:%s: Exit (%p)", TAG, __func__, this);
}

PostProcPipe::~PostProcPipe() {
  QMMF_INFO("%s:%s: Enter", TAG, __func__);
  for (auto& node : pipe_) {
    QMMF_INFO("%s:%s: node(%p) uid(%d)", TAG, __func__, node.get(), node->GetId());
    factory_->ReturnProcNode(node->GetId());
  }
  pipe_consumer_.clear();

  QMMF_INFO("%s:%s: Exit (%p)", TAG, __func__, this);
}

status_t PostProcPipe::Init(int32_t stream_id, const PipeIOParam &input) {
  if (state_ != PostProcPipeState::CREATED) {
    QMMF_ERROR("%s:%s: Incorrect state: %d", TAG, __func__, state_);
    return INVALID_OPERATION;
  }

  input_param_ = input;

  PostProcIOParam proc_param;
  proc_param.width      = input_param_.width;
  proc_param.height     = input_param_.height;
  proc_param.format     = Common::FromHalToQmmfFormat(input_param_.format);
  proc_param.frame_rate = input_param_.frame_rate;

  /* Forward iteration over the pipe */
  size_t idx = 0;
  sp<PostProcNode> node;

  /* Validate pipeline and create internal processing nodes if necessary */
  while (idx < pipe_.size()) {
    node = pipe_.at(idx);

    /* Check compatibility with the previous node */
    auto ret = node->ValidateInput(proc_param);
    if (ret == BAD_TYPE) {
      /* Unsupported format, try to fix this */
      auto reqs = node->GetRequirements();
      sp<PostProcNode> proc_node = FindInternalNode(proc_param, reqs);

      if (proc_node.get() != nullptr) {
        /* Insert new internal post processing node at current index */
        pipe_.insert(pipe_.begin() + idx, proc_node);
        continue;
      } else {
        QMMF_ERROR("%s:%s: Node format incompatibility!", TAG, __func__);
        return ret;
      }
    } else if (ret != NO_ERROR) {
      QMMF_ERROR("%s:%s: Node dimensions incompatibility!", TAG, __func__);
      return ret;
    }

    /* Update the output parameters for the next node */
    proc_param = node->GetOutput(proc_param);

    /* Increment node index */
    ++idx;
  }

  int32_t usage_flags = GRALLOC_USAGE_SW_WRITE_OFTEN |
      GRALLOC_USAGE_SW_READ_OFTEN;
  usage_flags |= output_param_.gralloc_flags;

  /* Initialize pipeline nodes */
  for (auto const& node : pipe_) {
    auto ret = node->Initialize(stream_id, output_param_.buffer_count, usage_flags);
    if (ret != NO_ERROR) {
      QMMF_ERROR("%s:%s: Failed to initialize node!", TAG, __func__);
      return ret;
    }
  }

  /* Set the consumer of the 1st node as the pipe consumer */
  pipe_consumer_ = pipe_.front()->GetConsumerIntf();

  state_ = PostProcPipeState::INITIALIZED;
  return NO_ERROR;
}

status_t PostProcPipe::GetInput(PipeIOParam &input,
                                const PipeIOParam &output) {
  output_param_ = output; // todo remove. real output should come during init

  PostProcIOParam proc_param = {};
  proc_param.width         = output_param_.width;
  proc_param.height        = output_param_.height;
  proc_param.stride        = output_param_.stride;
  proc_param.scanline      = output_param_.scanline;
  proc_param.frame_rate    = output_param_.frame_rate;
  proc_param.format        = Common::FromHalToQmmfFormat(output_param_.format);
  proc_param.gralloc_flags = output_param_.gralloc_flags;
  proc_param.buffer_count  = output_param_.buffer_count;

  /* If there are no plugins check if JPEG encoding is needed */
  if (pipe_.empty() && proc_param.format == BufferFormat::kBLOB) {
    sp<PostProcNode> node;
    if (use_hal_jpeg_) {
      node = factory_->GetProcNode("HALJpegEncode", context_);
    } else {
      node = factory_->GetProcNode("JpegEncode", context_);
    }
    pipe_.push_back(node);
  }

  /* Backward iteration over the pipe */
  ssize_t idx = pipe_.size() - 1;
  sp<PostProcNode> node;

  /* Validate pipeline and create internal processing nodes if necessary */
  while (idx >= 0) {
    node = pipe_.at(idx);

    /* Check compatibility with the previous node or pipe output */
    auto ret = node->ValidateOutput(proc_param);
    if (ret == BAD_TYPE) {
      /* Unsupported format, try to fix this */
      auto caps = node->GetCapabilities();
      sp<PostProcNode> proc_node = FindInternalNode(proc_param, caps);

      if (proc_node.get() != nullptr) {
        /* Increment index and insert new internal post processing node */
        ++idx;
        pipe_.insert(pipe_.begin() + idx, proc_node);
        continue;
      } else {
        QMMF_ERROR("%s:%s: Node format incompatibility!", TAG, __func__);
        return ret;
      }
    } else if (ret != NO_ERROR) {
      QMMF_ERROR("%s:%s: Node dimensions incompatibility!", TAG, __func__);
      return ret;
    }

    /* Update the output parameters for the next node */
    proc_param = node->GetInput(proc_param);

    /* Decrement node index */
    --idx;
  }

  /* Save the input params from the first node in the pipe */
  input_param_.width = proc_param.width;
  input_param_.height = proc_param.height;
  input_param_.stride = proc_param.stride;
  input_param_.scanline = proc_param.scanline;
  input_param_.frame_rate = proc_param.frame_rate;
  input_param_.format = Common::FromQmmfToHalFormat(proc_param.format);
  input_param_.gralloc_flags = proc_param.gralloc_flags;
  input_param_.buffer_count = proc_param.buffer_count;
  input = input_param_;

  return NO_ERROR;
}

sp<PostProcNode> PostProcPipe::FindInternalNode(const PostProcIOParam &output,
                                                const PostProcCaps &caps) {
  sp<PostProcNode> node;

  /* Check if a RAW re-processing or JPEG encoding node is required */
  if (IsYUVFormat(output.format) && SupportsRAWFormat(caps.formats_)) {
    node = factory_->GetProcNode("HALReprocess", context_);
  } else if (IsJPEGFormat(output.format) && SupportsYUVFormat(caps.formats_)) {
    if (use_hal_jpeg_) {
      node = factory_->GetProcNode("HALJpegEncode", context_);
    } else {
      node = factory_->GetProcNode("JpegEncode", context_);
    }
  }

  return node;
}

sp<PostProcNode> PostProcPipe::FindInternalNode(const PostProcIOParam &output,
                                                const PostProcReqs &reqs) {
  sp<PostProcNode> node;

  /* Check if a RAW re-processing or JPEG encoding node is required */
  if (IsRAWFormat(output.format) && (SupportsYUVFormat(reqs.formats_) ||
      SupportsJPEGFormat(reqs.formats_))) {
    node = factory_->GetProcNode("HALReprocess", context_);
  } else if (IsYUVFormat(output.format) &&
      SupportsJPEGFormat(reqs.formats_)) {
    if (use_hal_jpeg_) {
      node = factory_->GetProcNode("HALJpegEncode", context_);
    } else {
      node = factory_->GetProcNode("JpegEncode", context_);
    }
  }

  return node;
}

sp<IBufferConsumer>& PostProcPipe::GetConsumerIntf() {
  return pipe_consumer_;
}

void PostProcPipe::PipeNotifyBufferReturn(StreamBuffer& buffer) {
  QMMF_VERBOSE("%s:%s: Enter", TAG, __func__);
  if (pipe_.empty() == false) {
    pipe_.back()->NotifyBufferReturned(buffer);
  }
  QMMF_VERBOSE("%s:%s: Exit", TAG, __func__);
}

void PostProcPipe::LinkPipe(sp<IBufferConsumer>& consumer) {
  sp<IBufferConsumer> tmp_c = consumer;
  auto iter = pipe_.end();
  while (iter != pipe_.begin()) {
    --iter;
    (*iter)->AddConsumer(tmp_c);
    tmp_c = (*iter)->GetConsumerIntf();
  }
  QMMF_INFO("%s:%s: Pipe is linked! last node consumer (%p)", TAG, __func__,
      consumer.get());
}

void PostProcPipe::UnlinkPipe(sp<IBufferConsumer>& consumer) {
  sp<IBufferConsumer> tmp_c = consumer;
  auto iter = pipe_.end();
  while (iter != pipe_.begin()) {
    --iter;
    (*iter)->RemoveConsumer(tmp_c);
    tmp_c = (*iter)->GetConsumerIntf();
  }
  QMMF_INFO("%s:%s: Pipe is unlinked!", TAG, __func__);
}

bool PostProcPipe::IsRAWFormat(const BufferFormat &format) {
  switch (format) {
    case BufferFormat::kRAW10:
    case BufferFormat::kRAW12:
    case BufferFormat::kRAW16:
      return true;
    default:
      return false;
  }
}

bool PostProcPipe::IsYUVFormat(const BufferFormat &format) {
  switch (format) {
    case BufferFormat::kNV12:
    case BufferFormat::kNV21:
    case BufferFormat::kNV12UBWC:
      return true;
    default:
      return false;
  }
}

bool PostProcPipe::IsJPEGFormat(const BufferFormat &format) {
  switch (format) {
    case BufferFormat::kBLOB:
      return true;
    default:
      return false;
  }
}

bool PostProcPipe::SupportsRAWFormat(const std::set<BufferFormat> &formats) {
  if (formats.count(BufferFormat::kRAW10) != 0 ||
      formats.count(BufferFormat::kRAW12) != 0 ||
      formats.count(BufferFormat::kRAW16) != 0) {
    return true;
  }
  return false;
}

bool PostProcPipe::SupportsYUVFormat(const std::set<BufferFormat> &formats) {
  if (formats.count(BufferFormat::kNV12) != 0 ||
      formats.count(BufferFormat::kNV21) != 0 ||
      formats.count(BufferFormat::kNV12UBWC) != 0) {
    return true;
  }
  return false;
}

bool PostProcPipe::SupportsJPEGFormat(const std::set<BufferFormat> &formats) {
  if (formats.count(BufferFormat::kBLOB) != 0) {
    return true;
  }
  return false;
}

status_t PostProcPipe::Start() {
  if (pipe_.empty()) {
    QMMF_ERROR("%s:%s: Pipe is empty", TAG, __func__);
    return BAD_VALUE;
  }
  auto iter = pipe_.end();
  while (iter != pipe_.begin()) {
    --iter;
    (*iter)->Start();
  }
  return NO_ERROR;
}

status_t PostProcPipe::Stop() {
  if (pipe_.empty()) {
    QMMF_ERROR("%s:%s: Pipe is empty", TAG, __func__);
    return BAD_VALUE;
  }

  auto iter = pipe_.end();
  while (iter != pipe_.begin()) {
    --iter;
    (*iter)->Stop();
  }
  return NO_ERROR;
}

status_t PostProcPipe::AddConsumer(sp<IBufferConsumer>& consumer) {

  QMMF_INFO("%s:%s: Enter (%p)", TAG, __func__, consumer.get());
  if (state_ != PostProcPipeState::INITIALIZED) {
    QMMF_ERROR("%s:%s: Incorrect state: %d", TAG, __func__, state_);
    return INVALID_OPERATION;
  }

  if (consumer == nullptr) {
    QMMF_ERROR("%s:%s: Consumer is NULL", TAG, __func__);
    return BAD_VALUE;
  }

  LinkPipe(consumer);

  QMMF_VERBOSE("%s:%s: Consumer(%p) has been added.", TAG, __func__,
             consumer.get());

  state_ = PostProcPipeState::READYTOSTART;
  QMMF_INFO("%s:%s: Exit (%p)", TAG, __func__, consumer.get());
  return NO_ERROR;
}

status_t PostProcPipe::RemoveConsumer(sp<IBufferConsumer>& consumer) {

  QMMF_INFO("%s:%s: Enter consumer=%p", TAG, __func__, consumer.get());
  if (state_ != PostProcPipeState::READYTOSTART) {
    QMMF_ERROR("%s:%s: Incorrect state: %d", TAG, __func__, state_);
    return INVALID_OPERATION;
  }

  UnlinkPipe(consumer);

  state_ = PostProcPipeState::READYTOSTOP;
  QMMF_INFO("%s:%s: Exit consumer=%p", TAG, __func__, consumer.get());
  return NO_ERROR;
}

void PostProcPipe::AddResult(const void* result) {
  for (auto const& node : pipe_) {
    node->AddResult(result);
  }
}

}; //namespace recorder.

}; //namespace qmmf.

