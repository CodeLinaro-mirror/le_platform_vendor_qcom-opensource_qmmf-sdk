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

#define TAG "ReprocessPipe"

#include <utils/Vector.h>

#include "recorder/src/service/qmmf_recorder_utils.h"

#include "../interface/qmmf_camera_reprocess.h"
#include "../node/qmmf_camera_node.h"

#include "qmmf_camera_pipe.h"

namespace qmmf {

namespace recorder {

ReprocessPipe::ReprocessPipe(IPostProcCameraContext* context) :
    context_(context) {
  QMMF_VERBOSE("%s:%s: Enter", TAG, __func__);
  state_ = ReprocessPipeState::CREATED;
  QMMF_VERBOSE("%s:%s: Exit (%p)", TAG, __func__, this);
}

ReprocessPipe::~ReprocessPipe() {
  QMMF_INFO("%s:%s: Enter", TAG, __func__);
  for (auto iter : reproc_node_pipe_) {
    if (iter.get() != nullptr) {
      iter.clear();
    }
  }
  QMMF_INFO("%s:%s: Exit (%p)", TAG, __func__, this);
}

int32_t ReprocessPipe::Initialize(int32_t stream_id,
                                  ReprocessNodeCreate& reproc_node_create_param,
                                  void* static_meta) {
  ReprocessNodeCreate create_param = reproc_node_create_param;

  Vector<sp<ReprocessNode>>::iterator iter = reproc_node_pipe_.end();
  while (iter != reproc_node_pipe_.begin()) {
    --iter;
    ReprocessNodeParams reproc_node_param;
    (*iter)->getDefaultParam(reproc_node_param, create_param);

    reprocess_stream_id_ = (*iter)->Initialize(stream_id,
        reproc_node_param, static_meta);
    assert(reprocess_stream_id_ >= 0);

    /* update input params for next node */
    create_param.in.format = reproc_node_param.out.format;
    create_param.in.width = reproc_node_param.out.width;
    create_param.in.height = reproc_node_param.out.height;
  }

  reproc_node_create_param.out.format = create_param.out.format;
  reproc_node_create_param.out.width = create_param.out.width;
  reproc_node_create_param.out.height = create_param.out.height;

  return reprocess_stream_id_;
}

int32_t ReprocessPipe::Create(int32_t stream_id,
                              const char* pipe[],
                              const uint32_t pipe_size,
                              CameraStreamParameters &stream_param,
                              void* static_meta) {
  init_params_ = stream_param;

  for (uint32_t i = 0; i < pipe_size; i++) {
    sp<ReprocessNode>
        reproc_node = new ReprocessNode(String8(pipe[i]), context_);
    assert(reproc_node.get() != nullptr);
    reproc_node_pipe_.push_back(reproc_node);
  }

  ReprocessNodeCreate reproc_node_create_param;
  memset(&reproc_node_create_param, 0x0, sizeof reproc_node_create_param);

  reproc_node_create_param.in.format  = stream_param.format;
  reproc_node_create_param.in.width   = stream_param.width;
  reproc_node_create_param.in.height  = stream_param.height;

  reproc_node_create_param.out = reproc_node_create_param.in;

  reproc_node_create_param.frame_rate = 30; // todo
  reproc_node_create_param.max_buffer_count = stream_param.bufferCount;

  auto steram_id = Initialize(stream_id, reproc_node_create_param, static_meta);

  state_ = ReprocessPipeState::INITIALIZED;

  return steram_id;
}

sp<IBufferConsumer>& ReprocessPipe::GetConsumerIntf() {
  return pipe_consumer_;
}

void ReprocessPipe::PipeNotifyBufferReturn(StreamBuffer& buffer) {
  QMMF_VERBOSE("%s:%s: Enter", TAG, __func__);
  if (!reproc_node_pipe_.isEmpty()) {
    reproc_node_pipe_[0]->NotifyBufferReturned(buffer);
  }
  QMMF_VERBOSE("%s:%s: Exit", TAG, __func__);
}

void ReprocessPipe::LinkPipe(sp<IBufferConsumer>& consumer) {
  sp<IBufferConsumer>& tmp_c = consumer;
  for (auto iter : reproc_node_pipe_) {
    if (iter.get() != nullptr) {
      iter->AddConsumer(tmp_c);
      tmp_c = iter->GetConsumerIntf();
    }
  }
  pipe_consumer_ = tmp_c;
}

void ReprocessPipe::UnlinkPipe(sp<IBufferConsumer>& consumer) {
  sp<IBufferConsumer>& tmp_c = consumer;

  Vector<sp<ReprocessNode>>::iterator iter = reproc_node_pipe_.end();
  while (iter != reproc_node_pipe_.begin()) {
    --iter;
    (*iter)->RemoveConsumer(tmp_c);
    tmp_c = (*iter)->GetConsumerIntf();
  }
  pipe_consumer_.clear();
  pipe_consumer_ = nullptr;
}

void ReprocessPipe::Start() {
  for (auto iter : reproc_node_pipe_) {
    if (iter.get() != nullptr) {
      iter->Start();
    }
  }
}

void ReprocessPipe::Stop() {
  Vector<sp<ReprocessNode>>::iterator iter = reproc_node_pipe_.end();
  while (iter != reproc_node_pipe_.begin()) {
    --iter;
    (*iter)->Stop();
  }
}

status_t ReprocessPipe::AddConsumer(sp<IBufferConsumer>& consumer) {

  if (state_ != ReprocessPipeState::INITIALIZED) {
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

  state_ = ReprocessPipeState::READYTOSTART;
  return NO_ERROR;
}

status_t ReprocessPipe::RemoveConsumer(sp<IBufferConsumer>& consumer) {
  if (state_ != ReprocessPipeState::READYTOSTART) {
    QMMF_ERROR("%s:%s: Incorrect state: %d", TAG, __func__, state_);
    return INVALID_OPERATION;
  }

  UnlinkPipe(consumer);

  state_ = ReprocessPipeState::READYTOSTOP;
  return NO_ERROR;
}

void ReprocessPipe::AddResult(const void* result) {
  for (auto iter : reproc_node_pipe_) {
    if (iter.get() != nullptr) {
      iter->AddResult(result);
    }
  }
}

}; //namespace recorder.

}; //namespace qmmf.

