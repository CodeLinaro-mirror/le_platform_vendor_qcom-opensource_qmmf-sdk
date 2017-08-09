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
                           const std::vector<std::string> &pipe) :
    context_(context) {
  QMMF_VERBOSE("%s:%s: Enter", TAG, __func__);

  for (auto node : pipe) {
    sp<PostProcNode> reproc_node = new PostProcNode(node, context_);
    assert(reproc_node.get() != nullptr);
    pipe_.push_back(reproc_node);
  }

  state_ = PostProcPipeState::CREATED;
  QMMF_VERBOSE("%s:%s: Exit (%p)", TAG, __func__, this);
}

PostProcPipe::~PostProcPipe() {
  QMMF_INFO("%s:%s: Enter", TAG, __func__);
  for (auto iter : pipe_) {
    if (iter.get() != nullptr) {
      iter.clear();
    }
  }
  QMMF_INFO("%s:%s: Exit (%p)", TAG, __func__, this);
}

PostProcCreateParam PostProcPipe::GetInput(PostProcCreateParam &out) {
  PostProcCreateParam params = out;

  // iterate nodes from last to first node
  // each node return input requirement based on output
  // give each node input as output of previous node
  // return first node input to client
  auto iter = pipe_.end();
  while (iter != pipe_.begin()) {
    --iter;
    params = (*iter)->GetInput(params);
  }
  return params;
}

status_t PostProcPipe::CreatePipe(int32_t in_stream_id,
                                  CameraStreamParameters &stream_param,
                                  const ImageParam &param, // kmotov: todo:
                                  void* static_meta,
                                  int32_t &out_stream_id) {
  init_params_ = stream_param;

  PostProcNodeCreate create_param;
  memset(&create_param, 0x0, sizeof create_param);

  create_param.in.format  = stream_param.format;
  create_param.in.width   = stream_param.width;
  create_param.in.height  = stream_param.height;

  create_param.out.format  = create_param.in.format;
  create_param.out.width   = param.width;
  create_param.out.height  = param.height;

  create_param.frame_rate = 30; // todo
  create_param.max_buffer_count = stream_param.bufferCount;

  for (auto iter : pipe_) {
    PostProcNodeParams reproc_node_param;
    iter->getDefaultParam(reproc_node_param, create_param);

    auto ret = iter->Initialize(in_stream_id, reproc_node_param,
        static_meta, out_stream_id);
    if (ret != NO_ERROR) {
      QMMF_ERROR("%s:%s: fail to create ret: %d", TAG, __func__, ret);
      return ret;
    }

    // update input params for next node
    create_param.in.format = reproc_node_param.out.format;
    create_param.in.width  = reproc_node_param.out.width;
    create_param.in.height = reproc_node_param.out.height;
  }

  state_ = PostProcPipeState::INITIALIZED;

  return NO_ERROR;
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
  pipe_consumer_ = tmp_c;
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
  pipe_consumer_.clear();
  pipe_consumer_ = nullptr;
  QMMF_INFO("%s:%s: Pipe is unlinked!", TAG, __func__);
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
  for (auto iter : pipe_) {
    if (iter.get() != nullptr) {
      iter->AddResult(result);
    }
  }
}

}; //namespace recorder.

}; //namespace qmmf.

