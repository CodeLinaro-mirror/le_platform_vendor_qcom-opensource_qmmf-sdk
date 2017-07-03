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

#define TAG "PostProcNode"

#include <sys/mman.h>
#include <libgralloc/gralloc_priv.h>

#include "qmmf_postproc_node.h"

namespace qmmf {

namespace recorder {

PostProcNode::PostProcNode(std::string name, IPostProc* context)
    : PostProcPlugin<PostProcNode>(this),
      in_(this),
      out_(this),
      id_(-1),
      name_(name) {
  QMMF_INFO("%s:%s: Enter", TAG, __func__);

  mem_pool_ = new MemPool();
  assert(mem_pool_.get() != nullptr);

  reprocess_factory_ = PostProcFactory::getInstance();
  assert(reprocess_factory_ != nullptr);

  module_ = reprocess_factory_->getReprocEngine(name_, context);
  assert(module_.get() != nullptr);

  module_->SetCallbacks(this);

  module_->GetCapabilities(caps_);

  state_ = PostProcNodeState::CREATED;
  QMMF_INFO("%s:%s: Exit (%p) name: %s", TAG, __func__, this, name_.c_str());
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
                                   const PostProcNodeCreate& create_params) {
  QMMF_VERBOSE("%s:%s:%s: Enter", TAG, __func__, name_.c_str());

  memset(&reproc_node_param, 0x0, sizeof reproc_node_param);

  /* input frame params */
  reproc_node_param.in.format           = create_params.in.format;
  reproc_node_param.in.width            = create_params.in.width;
  reproc_node_param.in.height           = create_params.in.height;

  /* fill output frame params to default */
  PostProcCreateParam out_create_params = module_->GetOutput(create_params.in);
  reproc_node_param.out.format          = out_create_params.format;
  reproc_node_param.out.width           = out_create_params.width;
  reproc_node_param.out.height          = out_create_params.height;

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
  if (caps_.scale_support_) {
    reproc_node_param.out.width = create_params.out.width;
    reproc_node_param.out.height = create_params.out.height;
  }

  if (caps_.usage_) {
    reproc_node_param.out.gralloc_flags = caps_.usage_;
  }

  if (reproc_node_param.out.format == HAL_PIXEL_FORMAT_BLOB) {
    reproc_node_param.out.max_size =
        reproc_node_param.out.width * reproc_node_param.out.height;
  }

  if (caps_.output_buff_ == 0) {
    // disable extra buffer allocation
    reproc_node_param.out.max_buffer_count = 0;
  } else {
    reproc_node_param.out.max_buffer_count =
        std::max(caps_.output_buff_, create_params.max_buffer_count);
  }

  QMMF_VERBOSE("%s:%s:%s: Exit", TAG, __func__, name_.c_str());
}

status_t PostProcNode::Initialize(int32_t in_stream_id,
                                  PostProcNodeParams& reproc_node_param,
                                  void* static_meta,
                                  int32_t &out_stream_id) {
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

  PostProcCreateParam in;
  in.width     = init_params_.in.width;
  in.height    = init_params_.in.height;
  in.format    = init_params_.in.format;
  in.stride    = init_params_.in.width;
  in.scanline  = init_params_.in.height;

  PostProcCreateParam out;
  out.width    = init_params_.out.width;
  out.height   = init_params_.out.height;
  out.format   = init_params_.out.format;
  out.stride   = init_params_.out.width;
  out.scanline = init_params_.out.height;

  QMMF_INFO("%s:%s:%s: Input:  dim: %dx%d stride %d scanline %d fmt: %x",
      TAG, __func__, name_.c_str(),
      in.width, in.height, in.stride, in.scanline, in.format);
  QMMF_INFO("%s:%s:%s: Output: dim: %dx%d stride %d scanline %d fmt: %x",
      TAG, __func__, name_.c_str(),
      out.width, out.height, out.stride, out.scanline, out.format);

  ret = module_->Create(in_stream_id, in, out,
                        init_params_.in.frame_rate,
                        init_params_.in.max_buffer_count,
                        reinterpret_cast<void*>(static_meta), this, id_);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s:%s:%s: fail to create ret: %d", TAG, __func__,
        name_.c_str(), ret);
    return ret;
  }
  out_stream_id = id_;

  state_ = PostProcNodeState::INITIALIZED;

  QMMF_VERBOSE("%s:%s:%s: Exit id: %d", TAG, __func__, name_.c_str(), id_);

  return NO_ERROR;
}

PostProcCreateParam PostProcNode::GetInput(const PostProcCreateParam &out) {
  return module_->GetInput(out);
}

status_t PostProcNode::AddConsumer(sp<IBufferConsumer>& consumer) {

  std::lock_guard<std::mutex> lock(state_lock_);
  if (state_ != PostProcNodeState::INITIALIZED) {
    QMMF_ERROR("%s:%s:%s: Incorrect state: %d", TAG, __func__,
        name_.c_str(), state_);
    return INVALID_OPERATION;
  }

  if (consumer == nullptr) {
    QMMF_ERROR("%s:%s:%s: Consumer is NULL", TAG, __func__, name_.c_str());
    return BAD_VALUE;
  }

  AttachConsumer(consumer);

  state_ = PostProcNodeState::LINKED;

  QMMF_ERROR("%s:%s:%s: Consumer(%p) has been added.", TAG, __func__,
      name_.c_str(), consumer.get());

  return NO_ERROR;
}

status_t PostProcNode::RemoveConsumer(sp<IBufferConsumer>& consumer) {

  std::lock_guard<std::mutex> lock(state_lock_);
  if (state_ != PostProcNodeState::LINKED) {
    QMMF_ERROR("%s:%s:%s: Incorrect state: %d", TAG, __func__,
        name_.c_str(), state_);
    return INVALID_OPERATION;
  }

  DetachConsumer(consumer);

  state_ = PostProcNodeState::INITIALIZED;

  return NO_ERROR;
}

status_t PostProcNode::Start() {
  status_t ret = NO_ERROR;

  QMMF_INFO("%s:%s:%s: Enter Start. State: %d", TAG, __func__,
      name_.c_str(), state_);

  std::lock_guard<std::mutex> lock(state_lock_);
  if (state_ != PostProcNodeState::LINKED) {
    QMMF_ERROR("%s:%s:%s: wrong state_: %d ", TAG, __func__,
        name_.c_str(), state_);
    return BAD_VALUE;
  }

  state_ = PostProcNodeState::STARTING;

  ret = module_->Start();
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s:%s:%s: fail to start module ret: %d", TAG, __func__,
        name_.c_str(), ret);
    return ret;
  }

  in_.Run("InputHandler");
  out_.Run("OutputHandler");

  state_ = PostProcNodeState::ACTIVE;

  QMMF_INFO("%s:%s:%s: Exit", TAG, __func__, name_.c_str());

  return ret;
}

status_t PostProcNode::Stop() {
  status_t ret = NO_ERROR;

  QMMF_INFO("%s:%s:%s: Enter stop. State: %d", TAG, __func__,
      name_.c_str(), state_);

  {
    std::lock_guard<std::mutex> lock(state_lock_);
    state_ = PostProcNodeState::STOPPING;
  }

  in_.RequestExitAndWait();
  in_.FlushBufs([this] (StreamBuffer &buf) -> void
             { NotifyBufferReturn(buf); } );

  QMMF_VERBOSE("%s:%s:%s: The Lip thread is stopped Id_: %d", TAG, __func__,
      name_.c_str(), id_);

  ret = module_->Stop();
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s:%s:%s: fail to stop module ret: %d", TAG, __func__,
        name_.c_str(), ret);
    return ret;
  }

  out_.RequestExitAndWait();
  QMMF_VERBOSE("%s:%s:%s: The Node thread is stopped", TAG, __func__,
      name_.c_str());

  out_.FlushBufs([this] (StreamBuffer &buf) -> void
          { NotifyBufferReturned(buf); } );

  {
    std::lock_guard<std::mutex> lock(state_lock_);
    state_ = PostProcNodeState::LINKED;
  }

  QMMF_INFO("%s:%s:%s: Exit stop. State: %d", TAG, __func__,
      name_.c_str(), state_);

  return ret;
}

void PostProcNode::OnFrameAvailable(StreamBuffer& buffer) {
  QMMF_VERBOSE("%s:%s:%s: StreamBuffer(0x%p) fd: %d stream_id: %d ts: %lld",
      TAG, __func__, name_.c_str(), buffer.handle, buffer.fd,
      buffer.stream_id, buffer.timestamp);

  std::lock_guard<std::mutex> lock(state_lock_);
  if (state_ != PostProcNodeState::ACTIVE) {
    QMMF_ERROR("%s:%s:%s: Buffer not processed. Incorrect state: %d", TAG,
        __func__, name_.c_str(), state_);
    NotifyBufferReturn(buffer);
  } else {
    in_.AddBuf(buffer);
  }
  QMMF_VERBOSE("%s:%s:%s: Exit", TAG, __func__, name_.c_str());
}

void PostProcNode::OnFrameProcessed(const StreamBuffer &input_buffer) {
  QMMF_VERBOSE("%s:%s:%s: StreamBuffer(0x%p) fd: %d stream_id: %d ts: %lld",
      TAG, __func__, name_.c_str(), input_buffer.handle, input_buffer.fd,
      input_buffer.stream_id, input_buffer.timestamp);

  NotifyBufferReturn(const_cast<StreamBuffer&>(input_buffer));
}

void PostProcNode::OnFrameReady(const StreamBuffer &output_buffer) {
  QMMF_VERBOSE("%s:%s:%s: StreamBuffer(0x%p) fd: %d stream_id: %d ts: %lld",
      TAG, __func__, name_.c_str(), output_buffer.handle, output_buffer.fd,
      output_buffer.stream_id, output_buffer.timestamp);

  out_.AddBuf(const_cast<StreamBuffer&>(output_buffer));
}

void PostProcNode::OnError(RuntimeError err) {
  QMMF_ERROR("%s:%s:%s: Error %d", TAG, __func__, name_.c_str(), err);
}

void PostProcNode::AddResult(const void* result) {
  module_->AddResult(result);
}

void PostProcNode::NotifyBufferReturned(StreamBuffer& buffer) {
  QMMF_VERBOSE("%s:%s:%s: StreamBuffer(0x%p) fd: %d stream_id: %d ts: %lld", TAG,
    __func__, name_.c_str(), buffer.handle, buffer.fd,
    buffer.stream_id, buffer.timestamp);

  if (caps_.inplace_processing_ == false) {
    if (caps_.output_buff_ == 0) {
      module_->ReturnBuff(buffer);
    } else {
      status_t ret = mem_pool_->ReturnBufferLocked(buffer);
      if (ret != NO_ERROR) {
        QMMF_ERROR("%s:%s:%s Buffer return Error", TAG, __func__, name_.c_str());
        assert(0);
      }
    }
  } else {
    NotifyBufferReturn(buffer);
  }
  QMMF_VERBOSE("%s:%s:%s: Exit", TAG, __func__, name_.c_str());
}

status_t PostProcNode::ProcessOutputBuffer(StreamBuffer &buffer) {
  QMMF_VERBOSE("%s:%s:%s: StreamBuffer(0x%p) fd: %d stream_id: %d ts: %lld",
      TAG, __func__, name_.c_str(), buffer.handle, buffer.fd,
      buffer.stream_id, buffer.timestamp);

  // Give buffer ownership to the CameraSource
  std::lock_guard<std::mutex> lock(state_lock_);
  if (state_ == PostProcNodeState::ACTIVE) {
    QMMF_VERBOSE("%s:%s:%s: StreamBuffer(handle %p) returned to client",
        TAG, __func__, name_.c_str(), buffer.handle);
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

status_t InputHandler::MapBuf(StreamBuffer& buffer) {
  void *vaaddr = nullptr;

  if (buffer.fd == -1) {
    QMMF_ERROR("%s:%s: Error Invalid FD", TAG, __func__);
    return BAD_VALUE;
  }

  if (mapped_buffs_.count(buffer.fd) == 0) {
    vaaddr = mmap(nullptr, buffer.size, PROT_READ  | PROT_WRITE,
        MAP_SHARED, buffer.fd, 0);
    if (vaaddr == MAP_FAILED) {
        QMMF_ERROR("%s:%s  ION mmap failed: %s (%d)", TAG, __func__,
            strerror(errno), errno);;
        return BAD_VALUE;
    }
    buffer.data = vaaddr;
    map_data_t map;
    map.addr = vaaddr;
    map.size = buffer.size;
    mapped_buffs_[buffer.fd] = map;
    buffer.data = vaaddr;
  } else {
    buffer.data = mapped_buffs_[buffer.fd].addr;
  }

  return NO_ERROR;
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

status_t InputHandler::GetInputBuffers(std::vector<StreamBuffer> &in_buffs) {
  std::unique_lock<std::mutex> lock(wait_lock_);

  if (bufs_list_.empty()) {
    size_t wait_time = kFrameTimeout;
    auto ret = wait_.wait_for(lock, std::chrono::nanoseconds(wait_time));
    if (ret == std::cv_status::timeout) {
      QMMF_VERBOSE("%s:%s: Wait for frame available timed out Copy",
          TAG, __func__);
      return BAD_VALUE;
    }
  }

  auto iter = bufs_list_.begin();
  StreamBuffer buff = *iter;
  bufs_list_.erase(iter);

  auto ret = MapBuf(buff);
  if (ret != NO_ERROR) {
    assert(0);
  }

  in_buffs.push_back(buff);

  return NO_ERROR;
}

status_t InputHandler::GetOutputBuffers(std::vector<StreamBuffer> &out_buffs,
    const std::vector<StreamBuffer> &in_buffs) {
  if (node_->caps_.output_buff_ == 0) {
    return NO_ERROR;
  }

  for (auto buff : in_buffs) {
    StreamBuffer out_buff;
    memset(&out_buff, 0x0, sizeof(out_buff));
    node_->mem_pool_->GetBuffer(&out_buff);

    out_buff.stream_id = node_->id_;
    out_buff.timestamp = buff.timestamp;
    out_buff.frame_number = buff.frame_number;
    out_buff.camera_id = buff.camera_id;
    out_buff.flags = buff.flags;
    out_buff.info = buff.info;

    auto ret = MapBuf(out_buff);
    if (ret != NO_ERROR) {
      assert(0);
    }

    out_buffs.push_back(out_buff);
  }

  return NO_ERROR;
}

bool InputHandler::ThreadLoop() {

  {
    std::lock_guard<std::mutex> lock(node_->state_lock_);
    if (node_->state_ != PostProcNodeState::ACTIVE) {
      // exit from main loop
      return false;
    }
  }

  std::vector<StreamBuffer> in_buffs;
  auto ret = GetInputBuffers(in_buffs);
  if (ret != NO_ERROR) {
    // timeout loop again
    return true;
  }

  std::vector<StreamBuffer> out_buffs;
  ret = GetOutputBuffers(out_buffs, in_buffs);
  if (ret != NO_ERROR) {
    assert(0);
  }

  QMMF_VERBOSE("%s:%s: Process: FD: %d %d name: %s", TAG, __func__,
    in_buffs[0].fd, out_buffs.size() == 0 ? -1 : out_buffs[0].fd,
    node_->name_.c_str());

  ret = node_->module_->Process(in_buffs, out_buffs);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s:%s Error %d while algo process", TAG, __func__, ret);
    assert(0);
  }

  // loop again
  return true;
}

void OutputHandler::FlushBufs(std::function<void(StreamBuffer&)> BuffHandler) {
  std::unique_lock<std::mutex> lock(wait_lock_);
  for (auto iter : bufs_list_) {
    StreamBuffer buf = iter;
    BuffHandler(buf);
  }
  bufs_list_.clear();
}

void OutputHandler::AddBuf(StreamBuffer& buffer) {
  std::unique_lock<std::mutex> lock(wait_lock_);
  bufs_list_.push_back(buffer);
  wait_.notify_one();
}

bool OutputHandler::ThreadLoop() {
  {
    std::lock_guard<std::mutex> lock(node_->state_lock_);
    if (node_->state_ != PostProcNodeState::ACTIVE) {
      // exit main loop
      return false;
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
        // timeout loop again
        return true;
      }
    }

    buffer = bufs_list_.back();
    bufs_list_.pop_back();
  }

  node_->ProcessOutputBuffer(buffer);

  // loop again
  return true;
}

}; //namespace recorder.

}; //namespace qmmf.
