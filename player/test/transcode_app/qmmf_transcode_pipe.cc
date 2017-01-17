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

#include "qmmf_transcode_pipe.h"

namespace qmmf {
namespace transcode {

TransCoderPipe::TransCoderPipe(TransCodeType track_type) :
    track_type_(track_type) {
  TEST_INFO("%s:%s:%s Enter", TAG, __func__, "PIPE");
  TEST_INFO("%s:%s:%s Exit", TAG, __func__, "PIPE");
}

TransCoderPipe::~TransCoderPipe() {
  TEST_INFO("%s:%s:%s Enter", TAG, __func__, "PIPE");
  TEST_INFO("%s:%s:%s Exit", TAG, __func__, "PIPE");
};

status_t TransCoderPipe::PreparePipeline() {
  status_t ret = 0;
  TEST_INFO("%s:%s:%s Enter", TAG, __func__, "PIPE");

  CodecType pipe_in_codec_type;
  CodecType pipe_out_codec_type;
  if (track_type_ == TransCodeType::kVideoDecodeVideoEncode) {
    pipe_in_codec_type = CodecType::kVideoDecoder;
    pipe_out_codec_type = CodecType::kVideoEncoder;
  } else if (track_type_ == TransCodeType:: kVideoEncodeVideoDecode) {
    pipe_in_codec_type = CodecType::kVideoEncoder;
    pipe_out_codec_type = CodecType::kVideoDecoder;
  }
  pipe_in_ = make_shared<TransCoderPipeIn>(in_avcodec_.lock(),
      shared_from_this(), pipe_in_codec_type);
  pipe_out_ = make_shared<TransCoderPipeOut>(out_avcodec_.lock(),
      shared_from_this(), pipe_out_codec_type);

  ret = pipe_in_->PreparePipeline();
  assert(ret == 0);
  ret = pipe_out_->PreparePipeline();
  assert(ret == 0);

  pthread_create(&transport_thread_, nullptr, Transport, (void*)this);

  TEST_INFO("%s:%s:%s Exit", TAG, __func__, "PIPE");
  return ret;
}

status_t TransCoderPipe::RemovePipe() {
  TEST_INFO("%s:%s:%s Detaching PipeIn and PipeOut from CodecAdaptors",
      TAG, __func__, "PIPE");
  pipe_in_->ReleaseCodec();
  pipe_out_->ReleaseCodec();
  if (in_avcodec_.expired() && out_avcodec_.expired()) {
    TEST_INFO("%s:%s:%s Both the CodecAdaptors has been released",
        TAG, __func__, "PIPE");
    return 0;
  } else {
    TEST_ERROR("%s:%s:%s CodecAdaptors are not relased", TAG, __func__, "PIPE");
    return -1;
  }
}

TransCoderPipe::TransCoderPipeIn::TransCoderPipeIn(shared_ptr<IAVCodec> arg,
    shared_ptr<TransCoderPipe> parent, CodecType type) :
  port_index_(kPortIndexOutput) {
  TEST_INFO("%s:%s:%s Enter", TAG, __func__, "PIP_IN");
  avcodec_ = arg;
  pipe_= parent;
  codec_type_ = type;
  TEST_INFO("%s:%s:%s Exit", TAG, __func__, "PIP_IN");
}

TransCoderPipe::TransCoderPipeIn::~TransCoderPipeIn() {
  TEST_INFO("%s:%s:%s Enter", TAG, __func__, "PIP_IN");
  ReleaseBuffers(buffer_list_);
  TEST_INFO("%s:%s:%s Exit", TAG, __func__, "PIP_IN");
}

status_t TransCoderPipe::TransCoderPipeIn::PreparePipeline() {
  status_t ret = 0;
  TEST_INFO("%s:%s:%s Enter", TAG, __func__, "PIP_IN");

  ret = AllocateBuffers(buffer_list_, avcodec_, BufferOwner::kTransCoderPipeIn,
      port_index_);
  if (ret != 0) {
    TEST_ERROR("%s:%s:%s Failed to Allocate PipeIn Buffers",
        TAG, __func__, "PIP_IN");
    return ret;
  }

  vector<BufferDescriptor> temp_pipe_in;
  for (auto& iter: buffer_list_) {
    BufferDescriptor temp_buffer;
    memset(&temp_buffer, 0x0, sizeof(temp_buffer));
    ret = iter.getAVCodecBuffer(AVCodecBufferType::kNormal, temp_buffer);
    assert(ret == 0);
    temp_pipe_in.push_back(temp_buffer);
  }

  ret = avcodec_->RegisterOutputBuffers(temp_pipe_in);
  if (ret != 0) {
    TEST_ERROR("%s:%s:%s Failed to register PipeIn Buffers",
        TAG, __func__, "PIP_IN");
    return ret;
  }

  shared_ptr<TransCoderPipeIn> output_source_impl = shared_from_this();

  ret = avcodec_->AllocateBuffer(port_index_, 0, 0,
      ::std::static_pointer_cast<ICodecSource>(output_source_impl),
      temp_pipe_in);
  if(ret != OK) {
    TEST_ERROR("%s:%s:%s Failed to Call Allocate buffer on PipeInSide",
        TAG, __func__, "PIP_IN");
    ReleaseBuffers(buffer_list_);
    return ret;
  }

  ret = AddBufferList(buffer_list_);

  TEST_INFO("%s:%s:%s Exit", TAG, __func__, "PIP_IN");
  return ret;
}

status_t TransCoderPipe::TransCoderPipeIn::AddBufferList(
    vector<TransCodeBuffer>& list) {
  free_buffer_queue_.Clear();
  occupy_buffer_queue_.Clear();
  for (auto& iter : list) {
    free_buffer_queue_.PushBack(iter);
  }
  return 0;
}


TransCoderPipe::TransCoderPipeOut::TransCoderPipeOut(shared_ptr<IAVCodec> arg,
    shared_ptr<TransCoderPipe> parent, CodecType type) :
  port_index_(kPortIndexInput) {
  TEST_INFO("%s:%s:%s Enter", TAG, __func__, "PIP_OUT");
  avcodec_ = arg;
  pipe_ = parent;
  codec_type_ = type;
  TEST_INFO("%s:%s:%s Exit", TAG, __func__, "PIP_OUT");
}

TransCoderPipe::TransCoderPipeOut::~TransCoderPipeOut() {
  TEST_INFO("%s:%s:%s Enter", TAG, __func__, "PIP_OUT");
  ReleaseBuffers(buffer_list_);
  TEST_INFO("%s:%s:%s Exit", TAG, __func__, "PIP_OUT");
}

status_t TransCoderPipe::TransCoderPipeOut::PreparePipeline() {
  status_t ret = 0;
  TEST_INFO("%s:%s:%s Enter", TAG, __func__, "PIP_OUT");

  ret = AllocateBuffers(buffer_list_, avcodec_,
      BufferOwner::kTransCoderPipeOut, port_index_);
  if (ret != 0) {
    TEST_ERROR("%s:%s:%s Failed to Allocate PipeOut Buffers",
        TAG, __func__, "PIP_OUT");
    return ret;
  }

  vector<BufferDescriptor> temp_pipe_out;
  for (auto& iter: buffer_list_) {
    BufferDescriptor temp_buffer;
    memset(&temp_buffer, 0x0, sizeof(temp_buffer));
    ret = iter.getAVCodecBuffer(AVCodecBufferType::kNormal,temp_buffer);
    assert(ret == 0);
    temp_pipe_out.push_back(temp_buffer);
  }

  ret = avcodec_->RegisterInputBuffers(temp_pipe_out);
  if (ret != 0) {
    TEST_ERROR("%s:%s:%s Failed to register PipeOut Buffers",
        TAG, __func__, "PIP_OUT");
    return ret;
  }

  shared_ptr<TransCoderPipeOut> input_source_impl = shared_from_this();

  ret = avcodec_->AllocateBuffer(port_index_, 0, 0,
      ::std::static_pointer_cast<ICodecSource>(input_source_impl),
      temp_pipe_out);
  if(ret != OK) {
    TEST_ERROR("%s:%s:%s Failed to Call Allocate buffer on PipeInSide",
        TAG, __func__, "PIP_OUT");
    ReleaseBuffers(buffer_list_);
    return ret;
  }

  ret = AddBufferList(buffer_list_);
  assert(ret == OK);

  TEST_INFO("%s:%s:%s Exit", TAG, __func__, "PIP_OUT");
  return ret;
}

status_t TransCoderPipe::TransCoderPipeOut::AddBufferList(
    vector<TransCodeBuffer>& list) {
  free_buffer_queue_.Clear();
  occupy_buffer_queue_.Clear();
  for (auto& iter : list) {
    if (pipe_.expired()) {
        TEST_ERROR("%s:%s:%s Could not find pipe to send the buffer forward",
            TAG, __func__, "PIP_OUT");
        return -1;
      } else {
        shared_ptr<TransCoderPipe> sp_pipe = pipe_.lock();
        sp_pipe->Sendbackward(iter);
      }
  }
  return 0;
}

status_t TransCoderPipe::TransCoderPipeIn::GetBuffer(
    BufferDescriptor& buffer_descriptor, void* client_data) {
  TEST_VERBOSE("%s:%s:%s Enter", TAG, __func__, "PIP_IN");
  status_t ret = 0;

  if (free_buffer_queue_.Size() <= 0) {
    TEST_DEBUG("%s:%s:%s No buffer available to notify Wait for new buffer",
        TAG, __func__, "PIP_IN");
    Mutex::Autolock autoLock(wait_for_frame_lock_);
    if (free_buffer_queue_.Size() <= 0)
      wait_for_frame_.wait(wait_for_frame_lock_);
  }

  TransCodeBuffer iter = *free_buffer_queue_.Begin();
  ret = iter.getAVCodecBuffer(AVCodecBufferType::kNormal, buffer_descriptor);
  assert(ret == 0);
  free_buffer_queue_.Erase(free_buffer_queue_.Begin());
  occupy_buffer_queue_.PushBack(iter);

  TEST_VERBOSE("%s:%s:%s Exit", TAG, __func__, "PIP_IN");
  return ret;
}

status_t TransCoderPipe::TransCoderPipeIn::ReturnBuffer(
    BufferDescriptor& buffer_descriptor, void* client_data) {
  TEST_VERBOSE("%s:%s:%s Enter", TAG, __func__, "PIP_IN");
  status_t ret = 0;
  bool found = false;

  List<TransCodeBuffer>::iterator it = occupy_buffer_queue_.Begin();

  for (; it != occupy_buffer_queue_.End(); ++it) {
    //for now fd will be used instead of buf_id
    if (it->Fd() == buffer_descriptor.fd) {
    // if (it->getID() == buffer_descriptor.buf_id) {
      TEST_VERBOSE("%s:%s:%s Buffer found", TAG, __func__, "PIP_IN");
      if (codec_type_ == CodecType::kVideoDecoder)
        buffer_descriptor.size = (buffer_descriptor.size == 0 ? \
          buffer_descriptor.size : buffer_descriptor.capacity);
      it->UpdateTransCodeBuffer(buffer_descriptor);
      if (pipe_.expired()) {
        TEST_ERROR("%s:%s:%s Could not find pipe to send the buffer forward",
            TAG, __func__, "PIP_IN");
        assert(0);
      } else {
        shared_ptr<TransCoderPipe> sp_pipe = pipe_.lock();
        sp_pipe->Sendforward(*it);
      }
      occupy_buffer_queue_.Erase(it);
      found = true;
      break;
    }
  }

  assert(found == true);
  TEST_VERBOSE("%s:%s:%s Exit", TAG, __func__, "PIP_IN");
  return ret;
}

status_t TransCoderPipe::TransCoderPipeIn::NotifyPortEvent(
    PortEventType event_type, void* event_data) {
  TEST_INFO("%s:%s:%s Enter", TAG, __func__, "PIP_IN");
  status_t ret = 0;
  List<TransCodeBuffer>::iterator it;
  vector<BufferDescriptor> temp_pipe_in;
  switch(event_type) {
    case PortEventType::kPortStatus:
    break;
    case PortEventType::kPortSettingsChanged:
      switch(static_cast<PortreconfigData*>(event_data)->reconfig_type) {
        case PortreconfigData::PortReconfigType::kCropParametersChanged:
        break;
        case PortreconfigData::PortReconfigType::kBufferRequirementsChanged:
          TEST_INFO("%s:%s:%s Releasing pipeIn buffer_list_",
              TAG, __func__, "PIP_IN");
          ret = ReleaseBuffers(buffer_list_);
          if (ret != 0) {
            TEST_ERROR("%s:%s:%s Buffer Release Failed",
                TAG, __func__, "PIP_IN");
            return ret;
          }
          TEST_INFO("%s:%s:%s Allocating New set of Buffers",
              TAG, __func__, "PIP_IN");
          ret = AllocateBuffers(buffer_list_, avcodec_,
              BufferOwner::kTransCoderPipeIn, port_index_);
          assert(ret == 0);
          for (auto& iter: buffer_list_) {
            BufferDescriptor temp_buffer;
            memset(&temp_buffer, 0x0, sizeof(temp_buffer));
            ret = iter.getAVCodecBuffer(AVCodecBufferType::kNormal,temp_buffer);
            assert(ret == 0);
            temp_pipe_in.push_back(temp_buffer);
          }
          ret = avcodec_->RegisterOutputBuffers(temp_pipe_in);
          if (ret != 0) {
            TEST_ERROR("%s:%s:%s Buffer Registration Failed",
                TAG, __func__, "PIP_IN");
            return ret;
          }
          it = free_buffer_queue_.Begin();
          for (; it != free_buffer_queue_.End(); ++it) {
            if (it->Owner() == BufferOwner::kTransCoderPipeIn) {
              free_buffer_queue_.Erase(it);
            }
          }
          it = occupy_buffer_queue_.Begin();
          for (; it != occupy_buffer_queue_.End(); ++it) {
            if (it->Owner() == BufferOwner::kTransCoderPipeIn) {
              occupy_buffer_queue_.Erase(it);
            }
          }
          for (auto& iter : buffer_list_) {
            free_buffer_queue_.PushBack(iter);
          }
          wait_for_frame_.signal();
          break;
        default:
          return -1;
      }
    break;
    default:
    return -1;
  }
  TEST_INFO("%s:%s:%s Exit", TAG, __func__, "PIP_IN");
  return 0;
}

status_t TransCoderPipe::TransCoderPipeOut::GetBuffer(
    BufferDescriptor& buffer_descriptor, void* client_data) {
  TEST_VERBOSE("%s:%s:%s Enter", TAG, __func__, "PIP_OUT");
  status_t ret = 0;

  if (free_buffer_queue_.Size() <= 0) {
    TEST_DEBUG("%s:%s:%s No buffer available to notify Wait for new buffer",
        TAG, __func__, "PIP_OUT");
    Mutex::Autolock autoLock(wait_for_frame_lock_);
    if (free_buffer_queue_.Size() <= 0)
      wait_for_frame_.wait(wait_for_frame_lock_);
  }

  TransCodeBuffer iter = *free_buffer_queue_.Begin();
  ret = iter.getAVCodecBuffer(AVCodecBufferType::kNativeHandle,
      buffer_descriptor);
  assert(ret == 0);
  free_buffer_queue_.Erase(free_buffer_queue_.Begin());
  occupy_buffer_queue_.PushBack(iter);

  if (buffer_descriptor.flag & EOS_FLAG) {
    TEST_INFO("%s:%s:%s Last Buffer", TAG, __func__, "PIP_OUT");
    ret = -1;
  }

  TEST_VERBOSE("%s:%s:%s Exit", TAG, __func__, "PIP_OUT");
  return ret;
}

status_t TransCoderPipe::TransCoderPipeOut::ReturnBuffer(
    BufferDescriptor& buffer_descriptor, void* client_data) {
  TEST_VERBOSE("%s:%s:%s Enter", TAG, __func__, "PIP_OUT");
  status_t ret = 0;
  bool found = false;

  List<TransCodeBuffer>::iterator it = occupy_buffer_queue_.Begin();

  for (; it != occupy_buffer_queue_.End(); ++it) {
    //for now native handle will be used instead of buf_id
    if (it->MetaHandle() == buffer_descriptor.data) {
    // if (it->getID() == buffer_descriptor.buf_id) {
      TEST_VERBOSE("%s:%s:%s Buffer found", TAG, __func__, "PIP_OUT");
      if (pipe_.expired()) {
        TEST_ERROR("%s:%s:%s Could not find pipe to send the buffer forward",
            TAG, __func__, "PIP_OUT");
        assert(0);
      } else {
        shared_ptr<TransCoderPipe> sp_pipe = pipe_.lock();
        sp_pipe->Sendbackward(*it);
      }
      occupy_buffer_queue_.Erase(it);
      found = true;
      break;
    }
  }

  assert(found == true);
  TEST_VERBOSE("%s:%s:%s Exit", TAG, __func__, "PIP_OUT");
  return ret;
}

status_t TransCoderPipe::TransCoderPipeOut::NotifyPortEvent(
    PortEventType event_type, void* event_data) {
  return 0;
}

void* TransCoderPipe::Transport(void*arg) {
  TEST_INFO("%s:%s:%s Enter", TAG, __func__, "PIPE");
  TransCoderPipe*ptr = static_cast<TransCoderPipe*>(arg);

  while(1) {
    if (ptr->forward_dir_queue_.Size() <= 0) {
      TEST_DEBUG("%s:%s:%s No buffer available in forward Queue",
          TAG, __func__, "PIPE");
      Mutex::Autolock autoLock(ptr->wait_frame_fwqueue_lock_);
      if (ptr->forward_dir_queue_.Size() <= 0)
        ptr->wait_frame_fwqueue_.wait(ptr->wait_frame_fwqueue_lock_);
    }
    TransCodeBuffer iter_forward = *(ptr->forward_dir_queue_.Begin());
    if (ptr->backward_dir_queue_.Size() <= 0) {
      TEST_DEBUG("%s:%s:%s No buffer available in backward Queue",
          TAG, __func__, "PIPE");
      Mutex::Autolock autoLock(ptr->wait_frame_bwqueue_lock_);
      if (ptr->backward_dir_queue_.Size() <= 0)
        ptr->wait_frame_bwqueue_.wait(ptr->wait_frame_bwqueue_lock_);
    }
    TransCodeBuffer iter_backward = *(ptr->backward_dir_queue_.Begin());
    if (ptr->track_type_ == TransCodeType::kVideoDecodeVideoEncode) {
      // Buffer can be shared
      if (iter_forward.FilledSize() <= 0 && !(iter_forward.Flag() && EOS_FLAG)) {
        ptr->pipe_in_->ReceiveBuffer(iter_forward);
      } else {
        ptr->pipe_out_->ReceiveBuffer(iter_forward);
      }
      ptr->forward_dir_queue_.Erase(ptr->forward_dir_queue_.Begin());
      ptr->pipe_in_->ReceiveBuffer(iter_backward);
      ptr->backward_dir_queue_.Erase(ptr->backward_dir_queue_.Begin());
      if (iter_forward.Flag() & EOS_FLAG) {
        TEST_INFO("%s:%s:%s Transporting Last Buffer", TAG, __func__, "PIPE");
        break;
      }
    }
  }
  TEST_INFO("%s:%s:%s Exit", TAG, __func__, "PIPE");
  return nullptr;
}

void TransCoderPipe::Sendforward(TransCodeBuffer& buffer) {
  Mutex::Autolock autoLock(wait_frame_fwqueue_lock_);
  forward_dir_queue_.PushBack(buffer);
  wait_frame_fwqueue_.signal();
}

void TransCoderPipe::Sendbackward(TransCodeBuffer& buffer) {
  Mutex::Autolock autoLock(wait_frame_bwqueue_lock_);
  backward_dir_queue_.PushBack(buffer);
  wait_frame_bwqueue_.signal();
}

void TransCoderPipe::TransCoderPipeIn::ReceiveBuffer(TransCodeBuffer& buffer) {
  Mutex::Autolock autoLock(wait_for_frame_lock_);
  free_buffer_queue_.PushBack(buffer);
  wait_for_frame_.signal();
}

void TransCoderPipe::TransCoderPipeOut::ReceiveBuffer(TransCodeBuffer& buffer) {
  Mutex::Autolock autoLock(wait_for_frame_lock_);
  free_buffer_queue_.PushBack(buffer);
  wait_for_frame_.signal();
}

};  //namespace transcode
};  //namespace qmmf