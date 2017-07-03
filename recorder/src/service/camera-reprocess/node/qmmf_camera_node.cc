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

#define TAG "ReprocessNode"

#include <algorithm>
#include <libgralloc/gralloc_priv.h>

#include "recorder/src/service/qmmf_recorder_utils.h"

#include "../common/qmmf_camera_module.h"

#include "qmmf_camera_node.h"

namespace qmmf {

namespace recorder {

ReprocessNode::ReprocessNode(const char* srt, IPostProcCameraContext* context)
    : ReprocessPlugin<ReprocessNode>(this),
      name_(srt) {
  QMMF_INFO("%s:%s: Enter", TAG, __func__);

  mem_pool_ = new MemPool();
  assert(mem_pool_.get() != nullptr);

  camera_module_ = new CameraModule(name_, context);
  assert(camera_module_.get() != nullptr);


  memset(&caps_, 0x0, sizeof(ReprocCaps));
  camera_module_->GetCapabilities(&caps_);

  state_ = ReprocessNodeState::CREATED;
  QMMF_INFO("%s:%s: Exit (%p) name: %s", TAG, __func__, this, name_.string());
}

ReprocessNode::~ReprocessNode() {
  QMMF_INFO("%s:%s: Enter", TAG, __func__);

  RequestExitAndWait();

  if (camera_module_.get() != nullptr) {
    camera_module_->Delete();
    camera_module_.clear();
  }

  if (mem_pool_.get() != nullptr) {
    mem_pool_.clear();
  }

  ReprocessFactory::releaseInstance();
  QMMF_INFO("%s:%s: Exit (%p)", TAG, __func__, this);
}

void ReprocessNode::getDefaultParam(ReprocessNodeParams& reproc_node_param,
                                    ReprocessNodeCreate& create_params) {
  memset(&reproc_node_param, 0x0, sizeof reproc_node_param);

  /* input frame params */
  reproc_node_param.in.format        = create_params.in.format;
  reproc_node_param.in.width         = create_params.in.width;
  reproc_node_param.in.height        = create_params.in.height;

  /* fill output frame params to default */
  reproc_node_param.out              = reproc_node_param.in;

  /* fill default in/out gralloc usage flag*/
  reproc_node_param.out.gralloc_flags = GRALLOC_USAGE_HW_FB |
      private_handle_t::PRIV_FLAGS_VIDEO_ENCODER;
  reproc_node_param.in.gralloc_flags = GRALLOC_USAGE_SW_READ_OFTEN;

  /* frame rate */
  reproc_node_param.in.frame_rate =
  reproc_node_param.out.frame_rate = create_params.frame_rate;

  /* buffer cnt */
  reproc_node_param.in.max_buffer_count =
  reproc_node_param.out.max_buffer_count = create_params.max_buffer_count;

  /* update out data depends on capabilities */
  if (caps_.scale_en) {
    reproc_node_param.out.width = create_params.out.width;
    reproc_node_param.out.height = create_params.out.height;
  }

  if (caps_.usage) {
    reproc_node_param.out.gralloc_flags = caps_.usage;
  }

  if (caps_.out_format > 0) {
    reproc_node_param.out.format = caps_.out_format;
  }

  if (reproc_node_param.out.format == HAL_PIXEL_FORMAT_BLOB) {
    reproc_node_param.out.max_size =
        reproc_node_param.out.width*reproc_node_param.out.height;
  }

  if (!caps_.internal_buff) {
    // disable extra buffer allocation
    reproc_node_param.out.max_buffer_count = 0;
  } else {
    reproc_node_param.out.max_buffer_count =
        std::max(caps_.internal_buff, create_params.max_buffer_count);
  }

}

int32_t ReprocessNode::Initialize(int32_t input_stream_id,
                                  ReprocessNodeParams& reproc_node_param,
                                  void* static_meta) {
  init_params_ = reproc_node_param;

  auto ret = mem_pool_->Initialize(init_params_.out.width,
                                   init_params_.out.height,
                                   init_params_.out.format,
                                   init_params_.out.gralloc_flags,
                                   init_params_.out.max_buffer_count,
                                   init_params_.out.max_size);
  assert(ret >= 0);

  ReprocParam in, out;
  memset(&in, 0x0, sizeof(ReprocParam));
  memset(&out, 0x0, sizeof(ReprocParam));

  in.stride = init_params_.in.width;
  in.scanline = init_params_.in.height;

  in.width = init_params_.in.width;
  in.height = init_params_.in.height;
  in.format = init_params_.in.format;

  out.width = init_params_.out.width;
  out.height = init_params_.out.height;
  out.format = init_params_.out.format;
  reprocess_stream_id_ = camera_module_->Create(input_stream_id, in, out,
                                  init_params_.in.frame_rate,
                                  init_params_.in.max_buffer_count,
                                  reinterpret_cast<void*>(static_meta), this);
  assert(reprocess_stream_id_ >= 0);

  state_ = ReprocessNodeState::INITIALIZED;

  QMMF_VERBOSE("%s:%s: reprocess_stream_id_: %d name: %s", TAG, __func__,
      reprocess_stream_id_, name_.string());

  return reprocess_stream_id_;
}

status_t ReprocessNode::AddConsumer(sp<IBufferConsumer>& consumer) {
  if (state_ != ReprocessNodeState::INITIALIZED) {
    QMMF_ERROR("%s:%s: Incorrect state: %d", TAG, __func__, state_);
    return INVALID_OPERATION;
  }

  if (consumer == nullptr) {
    QMMF_ERROR("%s:%s: Consumer is NULL", TAG, __func__);
    return BAD_VALUE;
  }

  Mutex::Autolock lock(stop_lock_);
  stop_ = false;
  AttachConsumer(consumer);

  QMMF_VERBOSE("%s:%s: Consumer(%p) has been added.", TAG, __func__,
      consumer.get());

  return NO_ERROR;
}

status_t ReprocessNode::RemoveConsumer(sp<IBufferConsumer>& consumer) {

  if (state_ != ReprocessNodeState::READYTOSTART) {
    QMMF_ERROR("%s:%s: Incorrect state: %d", TAG, __func__, state_);
    return INVALID_OPERATION;
  }

  Mutex::Autolock lock(stop_lock_);
  stop_ = true;
  DetachConsumer(consumer);

  return NO_ERROR;
}

void ReprocessNode::Start() {
  if (camera_module_.get() != nullptr) {
    sp<IReprocessCallbacks> cb = this;
    SetCallBacks(cb);
    camera_module_->Start();
  }
  state_ = ReprocessNodeState::READYTOSTART;
  Run("ReprocessNode");
}

void ReprocessNode::Stop() {
  QMMF_INFO("%s:%s: Enter stop name:%s state: %d", TAG, __func__,
      name_.string(), state_);

  if (camera_module_.get() != nullptr) {
    camera_module_->Stop();
  }

  RequestExitAndWait();
  QMMF_INFO("%s:%s: The Node thread is stopped name: %s", TAG,
      __func__, name_.string());

  Mutex::Autolock lock(stop_lock_);
  state_ = ReprocessNodeState::READYTOSTOP;
  ReturnBuffers();

  ClearCallBacks();

  QMMF_INFO("%s:%s: Exit stop name:%s state: %d", TAG, __func__,
      name_.string(), state_);
}

status_t ReprocessNode::ReturnBuffers() {
  if (state_ != ReprocessNodeState::READYTOSTOP) {
    QMMF_ERROR("%s:%s: Incorrect state: %d", TAG, __func__, state_);
    return INVALID_OPERATION;
  }
  Mutex::Autolock lock(wait_lock_);
  for (auto iter : buffer_list_) {
    StreamBuffer buf = iter;
    NotifyBufferReturned(buf);
  }
  buffer_list_.clear();
  return NO_ERROR;
}

void ReprocessNode::OnFrameAvailable(StreamBuffer& buffer) {
  QMMF_VERBOSE("%s:%s: Frame %" PRId64 " buff:%p ts: %lld FD: %d name: %s", TAG,
            __func__, buffer.frame_number, buffer.handle, buffer.timestamp,
            buffer.fd, name_.string());

  Mutex::Autolock lock(stop_lock_);
  if (state_ == ReprocessNodeState::READYTOSTOP ||
      state_ == ReprocessNodeState::CREATED ||
      stop_ == true) {
    NotifyBufferReturn(buffer);
  } else {
    /*AddBuff*/
    if (camera_module_.get() != nullptr) {
      camera_module_->AddBuff(buffer);
    }
  }
}

void ReprocessNode::AddResult(const void* result) {
    if (camera_module_.get() != nullptr) {
      camera_module_->AddResult(result);
    }
}

bool ReprocessNode::ThreadLoop() {
  status_t ret = NO_ERROR;

  {
    Mutex::Autolock lock(stop_lock_);
    if (stop_ == true) {
      return true;
    }
  }

  StreamBuffer buffer;
  {
    Mutex::Autolock lock(wait_lock_);
    if (buffer_list_.empty() && state_ == ReprocessNodeState::READYTOSTART) {
      ret = wait_for_frames_.waitRelative(wait_lock_, kFrameTimeout);
      if (ret == TIMED_OUT) {
        QMMF_DEBUG("%s:%s: Wait for frame available timed out", TAG, __func__);
        return true;
      }
    }
    // todo error check here
    buffer = buffer_list_.editTop();
    buffer_list_.pop();
  }

  ReturnBufferToClient(buffer);

  return true;
}

status_t ReprocessNode::ReturnBufferToClient(StreamBuffer &buffer) {
  // Give buffer ownership to the CameraSource
  Mutex::Autolock lock(stop_lock_);
  if (GetNumConsumer() > 0 && stop_ == false) {
    QMMF_VERBOSE("%s:%s: StreamBuffer(handle %p) returned to client name: %s",
        TAG, __func__, buffer.handle, name_.string());
    NotifyBuffer(buffer);
  } else {
    NotifyBufferReturned(buffer);
  }
  return NO_ERROR;
}

void ReprocessNode::NotifyBufferReturned(StreamBuffer& buffer) {
  QMMF_VERBOSE("%s:%s: StreamBuffer(%p) FD: %d stream_id: %d moduleID:%d name: %s",
      TAG, __func__, buffer.handle, buffer.fd, buffer.stream_id,
      reprocess_stream_id_, name_.string());

  if (buffer.stream_id == reprocess_stream_id_) {
    if (init_params_.out.max_buffer_count == 0) {
        QMMF_VERBOSE("%s:%s: Buffer count is 0. Return to lib.", TAG, __func__);
        camera_module_->ReturnBuff(buffer);
        return;
    }
    status_t ret = mem_pool_->ReturnBufferLocked(buffer);
    if (ret != NO_ERROR) {
      QMMF_ERROR("%s:%s: Buffer return Error", TAG, __func__);
    }
  } else {
    NotifyBufferReturn(buffer);
  }
  QMMF_VERBOSE("%s:%s: Exit!", TAG, __func__);
}

void ReprocessNode::ReprocessLibCallback(StreamBuffer in_buff,
                                         StreamBuffer out_buff) {
  QMMF_VERBOSE("%s:%s:", TAG, __func__);
  if (in_buff.fd >= 0) {
    QMMF_VERBOSE("%s:%s: Not need return FD: %d name: %s", TAG, __func__,
        in_buff.fd, name_.string());
    Mutex::Autolock lock(stop_lock_);
    NotifyBufferReturn(in_buff);
  }

  Mutex::Autolock lock(wait_lock_);
  buffer_list_.push_back(out_buff);
  wait_for_frames_.signal();
}

status_t ReprocessNode::GetBuffer(StreamBuffer* buffer) {
  buffer->stream_id = reprocess_stream_id_;
  return mem_pool_->GetBuffer(buffer);
}

void ReprocessNode::SetCallBacks(sp<IReprocessCallbacks>& cb) {
  camera_module_->SetCallBacks(cb);
}

void ReprocessNode::ClearCallBacks() {
  camera_module_->ClearCallBacks();
}

}; //namespace recorder.

}; //namespace qmmf.
