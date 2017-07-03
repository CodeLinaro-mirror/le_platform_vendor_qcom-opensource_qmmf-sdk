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

#define TAG "ProcessingNode"

#include <sys/mman.h>
#include <libgralloc/gralloc_priv.h>

#include "qmmf_postproc_node.h"

namespace qmmf {

namespace recorder {

PostProcNode::PostProcNode(const char* srt, IPostProc* context)
    : PostProcPlugin<PostProcNode>(this),
      in_(this),
      out_(this),
      id_(-1),
      name_(srt) {
  QMMF_INFO("%s:%s: Enter", TAG, __func__);

  mem_pool_ = new MemPool();
  assert(mem_pool_.get() != nullptr);

  reprocess_factory_ = PostProcFactory::getInstance();
  assert(reprocess_factory_ != nullptr);

  module_ = reprocess_factory_->getReprocEngine(name_, context);
  assert(module_.get() != nullptr);

  module_->SetCallbacks(this);

  memset(&caps_, 0x0, sizeof(ReprocCaps));
  module_->GetCapabilities(&caps_);

  state_ = PostProcNodeState::CREATED;
  QMMF_INFO("%s:%s: Exit (%p) name: %s", TAG, __func__, this, name_.string());
}

PostProcNode::~PostProcNode() {
  QMMF_INFO("%s:%s: Enter", TAG, __func__);

  in_.RequestExitAndWait();
  in_.UnMapBufs();

  out_.RequestExitAndWait();

  module_->Delete();

  if (mem_pool_.get() != nullptr) {
    mem_pool_.clear();
  }

  PostProcFactory::releaseInstance();

  QMMF_INFO("%s:%s: Exit (%p)", TAG, __func__, this);
}

void PostProcNode::getDefaultParam(PostProcNodeParams& reproc_node_param,
                                   PostProcNodeCreate& create_params) {
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

int32_t PostProcNode::Initialize(int32_t input_stream_id,
                                 PostProcNodeParams& reproc_node_param,
                                 void* static_meta) {
  std::lock_guard<std::mutex> lock(state_lock_);
  if (state_ != PostProcNodeState::CREATED) {
    QMMF_ERROR("%s:%s: wrong state: %d", TAG, __func__, state_);
    return BAD_VALUE;
  }

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

  // kmotov todo error handling
        module_->Create(input_stream_id, in, out,
                        init_params_.in.frame_rate,
                        init_params_.in.max_buffer_count,
                        reinterpret_cast<void*>(static_meta), this, id_);
  assert(id_ >= 0);

  state_ = PostProcNodeState::INITIALIZED;

  QMMF_VERBOSE("%s:%s: id_: %d name: %s", TAG, __func__,
      id_, name_.string());

  return id_;
}

status_t PostProcNode::AddConsumer(sp<IBufferConsumer>& consumer) {

  std::lock_guard<std::mutex> lock(state_lock_);
  if (state_ != PostProcNodeState::INITIALIZED) {
    QMMF_ERROR("%s:%s: Incorrect state: %d", TAG, __func__, state_);
    return INVALID_OPERATION;
  }

  if (consumer == nullptr) {
    QMMF_ERROR("%s:%s: Consumer is NULL", TAG, __func__);
    return BAD_VALUE;
  }

  AttachConsumer(consumer);

  state_ = PostProcNodeState::LINKED;

  QMMF_VERBOSE("%s:%s: Consumer(%p) has been added.", TAG, __func__,
      consumer.get());

  return NO_ERROR;
}

status_t PostProcNode::RemoveConsumer(sp<IBufferConsumer>& consumer) {

  std::lock_guard<std::mutex> lock(state_lock_);
  if (state_ != PostProcNodeState::LINKED) {
    QMMF_ERROR("%s:%s: Incorrect state: %d", TAG, __func__, state_);
    return INVALID_OPERATION;
  }

  DetachConsumer(consumer);

  state_ = PostProcNodeState::INITIALIZED;

  return NO_ERROR;
}

status_t PostProcNode::Start() {
  status_t ret = NO_ERROR;

  std::lock_guard<std::mutex> lock(state_lock_);
  if (state_ != PostProcNodeState::LINKED) {
    QMMF_ERROR("%s:%s: wrong state_: %d ", TAG, __func__, state_);
    return BAD_VALUE;
  }

  state_ = PostProcNodeState::STARTING;

  ret = module_->Start();
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s:%s: fail to start module ret: %d", TAG, __func__, ret);
    return ret;
  }

  in_.Run("InputHandler");
  out_.Run("OutputHandler");

  state_ = PostProcNodeState::ACTIVE;

  return ret;
}

status_t PostProcNode::Stop() {
  status_t ret = NO_ERROR;

  QMMF_INFO("%s:%s: Enter stop name:%s state: %d", TAG, __func__,
      name_.string(), state_);

  {
    std::lock_guard<std::mutex> lock(state_lock_);
    state_ = PostProcNodeState::STOPPING;
  }

  in_.RequestExitAndWait();
  in_.FlushBufs([this] (StreamBuffer &buf) -> void
             { NotifyBufferReturn(buf); } );

  QMMF_INFO("%s:%s: The Lip thread is stopped Id_: %d", TAG, __func__, id_);

  ret = module_->Stop();
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s:%s: fail to stop module ret: %d", TAG, __func__, ret);
    return ret;
  }

  out_.RequestExitAndWait();
  QMMF_INFO("%s:%s: The Node thread is stopped name: %s", TAG, __func__,
      name_.string());

  out_.FlushBufs([this] (StreamBuffer &buf) -> void
          { NotifyBufferReturned(buf); } );

  {
    std::lock_guard<std::mutex> lock(state_lock_);
    state_ = PostProcNodeState::LINKED;
  }

  QMMF_INFO("%s:%s: Exit stop name:%s state: %d", TAG, __func__,
      name_.string(), state_);

  return ret;
}

void OutputHandler::FlushBufs(std::function<void(StreamBuffer&)> BuffHandler) {
  std::unique_lock<std::mutex> lock(wait_lock_);
  for (auto iter : bufs_list_) {
    StreamBuffer buf = iter;
    BuffHandler(buf);
  }
  bufs_list_.clear();
}

void PostProcNode::OnFrameAvailable(StreamBuffer& buffer) {
  QMMF_VERBOSE("%s:%s: Frame %" PRId64 " buff:%p ts: %lld FD: %d name: %s", TAG,
            __func__, buffer.frame_number, buffer.handle, buffer.timestamp,
            buffer.fd, name_.string());

  std::lock_guard<std::mutex> lock(state_lock_);
  if (state_ != PostProcNodeState::ACTIVE) {
    QMMF_ERROR("%s:%s: Buffer not processed. Incorrect state: %d", TAG,
        __func__, state_);
    NotifyBufferReturn(buffer);
  } else {
    in_.AddBuf(buffer);
  }
}

status_t PostProcNode::OnFrameProcessed(StreamBuffer &input_buffer) {
  QMMF_INFO("%s:%s: Return FD: %d name: %s", TAG, __func__,
    input_buffer.fd, name_.string());

  NotifyBufferReturn(input_buffer);

  return NO_ERROR;
}

status_t PostProcNode::OnFrameReady(StreamBuffer &output_buffer) {
  QMMF_INFO("%s:%s: Return FD: %d name: %s", TAG, __func__,
    output_buffer.fd, name_.string());

  out_.AddBuf(output_buffer);

  return NO_ERROR;
}

void OutputHandler::AddBuf(StreamBuffer& buffer) {
  std::unique_lock<std::mutex> lock(wait_lock_);
  bufs_list_.push_back(buffer);
  wait_.notify_one();
}

void PostProcNode::AddResult(const void* result) {
  module_->AddResult(result);
}

void PostProcNode::NotifyBufferReturned(StreamBuffer& buffer) {
  QMMF_VERBOSE("%s:%s: StreamBuffer(%p) FD: %d stream_id: %d moduleID:%d name: %s",
      TAG, __func__, buffer.handle, buffer.fd, buffer.stream_id,
      id_, name_.string());

  if (buffer.stream_id == id_) {
    if (init_params_.out.max_buffer_count == 0) {
        QMMF_VERBOSE("%s:%s: Buffer count is 0. Return to lib.", TAG, __func__);
        if (module_ == nullptr) {
          QMMF_ERROR("%s:%s: Error", TAG, __func__);
        }
        module_->ReturnBuff(buffer);
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

status_t PostProcNode::ReturnBufferToClient(StreamBuffer &buffer) {
  // Give buffer ownership to the CameraSource
  std::lock_guard<std::mutex> lock(state_lock_);
  if (state_ == PostProcNodeState::ACTIVE) {
    QMMF_VERBOSE("%s:%s: StreamBuffer(handle %p) returned to client name: %s",
        TAG, __func__, buffer.handle, name_.string());
    NotifyBuffer(buffer);
  } else {
    NotifyBufferReturned(buffer);
  }
  return NO_ERROR;
}

void InputHandler::AddBuf(StreamBuffer& buffer) {
  std::unique_lock<std::mutex> lock(wait_lock_);
  bufs_list_.push_back(buffer);
  wait_.notify_one();
}


void InputHandler::FlushBufs(std::function<void(StreamBuffer&)> BuffHandler) {
  std::unique_lock<std::mutex> lock(wait_lock_);

  StreamBuffer buffer;
  auto iter = bufs_list_.begin();
  while (iter != bufs_list_.end()) {
    iter = bufs_list_.begin();
    buffer = *iter;
    bufs_list_.erase(iter);
    QMMF_INFO("%s:%s: back to client node: FD: %d", TAG, __func__, buffer.fd);
    BuffHandler(buffer);
  }
}

void* InputHandler::MapBuf(StreamBuffer& buffer) {
  void *vaaddr = nullptr;

  if (buffer.fd == -1) {
    QMMF_ERROR("%s:%s: Error Invalid FD", TAG, __func__);
    return vaaddr;
  }

  if (mapped_buffs_.count(buffer.fd) == 0) {
    vaaddr = mmap(nullptr, buffer.size, PROT_READ  | PROT_WRITE,
        MAP_SHARED, buffer.fd, 0);
    if (vaaddr == MAP_FAILED) {
        QMMF_ERROR("%s:%s  ION mmap failed: %s (%d)", TAG, __func__,
            strerror(errno), errno);
    }
    buffer.data = vaaddr;
    map_data_t map;
    map.addr = vaaddr;
    map.size = buffer.size;
    mapped_buffs_[buffer.fd] = map;
  } else {
    vaaddr = mapped_buffs_[buffer.fd].addr;
  }

  return vaaddr;
}

void InputHandler::UnMapBufs() {
  for (auto iter : mapped_buffs_) {
    auto map = iter.second;
    if (map.addr) {
      QMMF_INFO("%s:%s: Unmap %p size %d", TAG, __func__, map.addr, map.size);
      munmap(map.addr, map.size);
    }
  }

  mapped_buffs_.clear();
}

bool InputHandler::ThreadLoop() {

  {
    std::lock_guard<std::mutex> lock(node_->state_lock_);
    if (node_->state_ != PostProcNodeState::ACTIVE) {
      return true;
    }
  }

  StreamBuffer in_buff;
  {
    std::unique_lock<std::mutex> lock(wait_lock_);
    if (bufs_list_.empty()) {
      size_t wait_time = kFrameTimeout;
      auto ret = wait_.wait_for(lock, std::chrono::nanoseconds(wait_time));
      if (ret == std::cv_status::timeout) {
        QMMF_VERBOSE("%s:%s: Wait for frame available timed out Copy",
            TAG, __func__);
        return true;
      }
    }
    auto iter = bufs_list_.begin();
    in_buff = *iter;
    bufs_list_.erase(iter);
  }

  // kmotov: todo:
  StreamBuffer out_buff;
  {
    memset(&out_buff, 0x0, sizeof(out_buff));
    node_->mem_pool_->GetBuffer(&out_buff);

    out_buff.stream_id = node_->id_;
    out_buff.timestamp = in_buff.timestamp;
    out_buff.frame_number = in_buff.frame_number;
    out_buff.camera_id = in_buff.camera_id;
    out_buff.flags = in_buff.flags;
    out_buff.info = in_buff.info;
    out_buff.data = MapBuf(out_buff);
  }


  status_t ret = node_->module_->Process(in_buff, out_buff);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s:%s Error %d while algo process", TAG, __func__, ret);
    assert(1);
  }

  return true;
}

bool OutputHandler::ThreadLoop() {
  {
    std::lock_guard<std::mutex> lock(node_->state_lock_);
    if (node_->state_ != PostProcNodeState::ACTIVE) {
      return true;
    }
  }

  StreamBuffer buffer;
  {
    std::unique_lock<std::mutex> lock(wait_lock_);
    if (bufs_list_.empty()) {
      size_t wait_time = kFrameTimeout;
      auto ret = wait_.wait_for(lock, std::chrono::nanoseconds(wait_time));
      if (ret == std::cv_status::timeout) {
        QMMF_DEBUG("%s:%s: Wait for frame available timed out", TAG, __func__);
        return true;
      }
    }

    buffer = bufs_list_.back();
    bufs_list_.pop_back();
  }

  node_->ReturnBufferToClient(buffer);

  return true;
}

}; //namespace recorder.

}; //namespace qmmf.
