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

#include "qmmf_transcode_core.h"

namespace qmmf {
namespace transcode {

TransCoderCore::TransCoderCore(CodecType type, CodecParam& codec_params,
    shared_ptr<TransCoderPipe>& pipe):  pipe_(pipe), codec_type_(type),
    params_(codec_params), port_index_(kPortIndexInput) {
  TEST_INFO("%s:%s:%s Enter", TAG, __func__, "CORE");
  TEST_INFO("%s:%s:%s Exit", TAG, __func__, "CORE");
}

TransCoderCore::~TransCoderCore() {
  TEST_INFO("%s:%s:%s Enter", TAG, __func__, "CORE");
  TEST_INFO("%s:%s:%s Exit", TAG, __func__, "CORE");
}

status_t TransCoderCore::PreparePipeline() {
  TEST_INFO("%s:%s:%s Enter", TAG, __func__, "CORE");
  status_t ret = 0;

  avcodec_.reset(IAVCodec::CreateAVCodec());

  switch (codec_type_) {
    case CodecType::kVideoEncoder:
      mime_ = CodecMimeType::kMimeTypeVideoEncAVC;
    break;
    case CodecType::kVideoDecoder:
      mime_ = CodecMimeType::kMimeTypeVideoDecAVC;
    break;
    case CodecType::kAudioEncoder:
      mime_ = CodecMimeType::kMimeTypeAudioEncAAC;
    break;
    case CodecType::kAudioDecoder:
      mime_ = CodecMimeType::kMimeTypeAudioDecAAC;
    break;
    case CodecType::kImageEncoder:
    case CodecType::kImageDecoder:
    default:
      TEST_ERROR("%s:%s:%s CodecType not Supported", TAG, __func__, "CORE");
      return -1;
  }

  ret = avcodec_->ConfigureCodec(mime_,params_);
  if (ret != 0) {
    TEST_ERROR("%s:%s:%s Failed to Configure Codec", TAG, __func__, "CORE");
    return ret;
  }

  ret = AllocateBuffers(buffer_list_, avcodec_,
      BufferOwner::kTransCoderCore, port_index_);
  if (ret != 0) {
    TEST_ERROR("%s:%s:%s Failed to Allocate Input Buffers",
        TAG, __func__, "CORE");
    return ret;
  }

  vector<BufferDescriptor> temp_in;
  for (auto& iter: buffer_list_) {
    BufferDescriptor temp_buffer;
    memset(&temp_buffer, 0x0, sizeof(temp_buffer));
    ret = iter.getAVCodecBuffer(AVCodecBufferType::kNormal,temp_buffer);
    assert(ret == 0);
    temp_in.push_back(temp_buffer);
  }

  ret = avcodec_->RegisterInputBuffers(temp_in);
  if (ret != 0) {
    TEST_ERROR("%s:%s:%s Failed to register Input Buffers",
        TAG, __func__, "CORE");
    return ret;
  }

  shared_ptr<TransCoderCore> input_source_impl = shared_from_this();

  // TODO: As of now these values (0, 0) after port_index_ will be ignored by
  // AVCodec. Hence, support is needed in this function in AVCodec to modify
  // the buffer requirements as per the wish of AVcodec Client
  // This means that in case of VIDEO ENCODER Input Buffer Requirements
  // and Output Buffer Requiremnts are HARDCODED. Although still the
  // avcodec client for input port can allocate the buffers as suggested
  // by GetBufferRequirements function, This flexibility is not there in
  // case of Output Port. That is why AllocateBuffers function in
  // qmmf_transcode_utils.cc has an If statement
  ret = avcodec_->AllocateBuffer(port_index_, 0, 0,
      ::std::static_pointer_cast<ICodecSource>(input_source_impl),temp_in);
  if(ret != OK) {
    TEST_ERROR("%s:%s:%s Failed to Call Allocate buffer on CoreSide",
        TAG, __func__, "CORE");
    ReleaseBuffers(buffer_list_);
    return ret;
  }

  pipe_->PassInputCodec(avcodec_);

  ret = AddBufferList(buffer_list_);
  assert(ret == OK);

  TEST_INFO("%s:%s:%s Exit", TAG, __func__, "CORE");
  return ret;
}

status_t TransCoderCore::StartCodec() {
  status_t ret = 0;
  TEST_INFO("%s:%s:%s Enter", TAG, __func__, "CORE");
  ret = avcodec_->StartCodec();
  assert(ret == OK);
  TEST_INFO("%s:%s:%s Exit", TAG, __func__, "CORE");
  return ret;
}

status_t TransCoderCore::StopCodec() {
  status_t ret = 0;
  TEST_INFO("%s:%s:%s Enter", TAG, __func__, "CORE");

  ret = avcodec_->StopCodec();
  assert(ret == OK);

  TEST_INFO("%s:%s:%s Exit", TAG, __func__, "CORE");
  return ret;
}

status_t TransCoderCore::DeleteCodec() {
  status_t ret = 0;
  TEST_INFO("%s:%s:%s Enter", TAG, __func__, "CORE");

  ret = avcodec_->ReleaseBuffer();
  assert(ret == OK);

  ret = ReleaseBuffers(buffer_list_);
  assert(ret == OK);

  avcodec_.reset();

  TEST_INFO("%s:%s:%s Exit", TAG, __func__, "CORE");
  return ret;
}

status_t TransCoderCore::PauseCodec() {
  status_t ret = 0;
  TEST_INFO("%s:%s:%s Enter", TAG, __func__, "CORE");

  ret = avcodec_->PauseCodec();
  assert(ret == OK);

  TEST_INFO("%s:%s:%s Exit", TAG, __func__, "CORE");
  return ret;
}

status_t TransCoderCore::ResumeCodec() {
  status_t ret = 0;
  TEST_INFO("%s:%s:%s Enter", TAG, __func__, "CORE");

  ret = avcodec_->ResumeCodec();
  assert(ret == OK);

  TEST_INFO("%s:%s:%s Exit", TAG, __func__, "CORE");
  return ret;
}

status_t TransCoderCore::AddBufferList(vector<TransCodeBuffer>& list) {

  free_buffer_queue_.Clear();
  occupy_buffer_queue_.Clear();
  unfilled_frame_queue.Clear();
  being_filled_frame_queue.Clear();


  for (auto& iter : list) {
    unfilled_frame_queue.PushBack(iter);
  }

  return 0;
}

status_t TransCoderCore::GetBuffer(BufferDescriptor& buffer_descriptor,
                                   void* client_data) {
  TEST_VERBOSE("%s:%s:%s Enter", TAG, __func__, "CORE");
  status_t ret = 0;

  if (free_buffer_queue_.Size() <= 0) {
    TEST_DEBUG("%s:%s:%s No buffer available to notify Wait for new buffer",
        TAG, __func__, "CORE");
    Mutex::Autolock autoLock(wait_for_frame_lock_);
    if (free_buffer_queue_.Size() <= 0)
      wait_for_frame_.wait(wait_for_frame_lock_);
  }

  TransCodeBuffer iter = *free_buffer_queue_.Begin();
  ret = iter.getAVCodecBuffer(AVCodecBufferType::kNormal, buffer_descriptor);
  assert(ret == 0);
  free_buffer_queue_.Erase(free_buffer_queue_.Begin());
  occupy_buffer_queue_.PushBack(iter);

  if (buffer_descriptor.flag & EOS_FLAG) {
    TEST_INFO("%s:%s:%s Last Buffer", TAG, __func__, "CORE");
    ret = -1;
  }

  TEST_VERBOSE("%s:%s:%s Exit", TAG, __func__, "CORE");
  return ret;
}

status_t TransCoderCore::ReturnBuffer(BufferDescriptor& buffer_descriptor,
                                      void* client_data) {
  TEST_VERBOSE("%s:%s:%s Enter", TAG, __func__, "CORE");
  status_t ret = 0;
  bool found = false;

  List<TransCodeBuffer>::iterator it = occupy_buffer_queue_.Begin();

  for (; it != occupy_buffer_queue_.End(); ++it) {
    // for now data pointer will be used instead of buf_id
    if (it->data() == buffer_descriptor.data) {
    // if (it->getID() == buffer_descriptor.buf_id) {
      TEST_VERBOSE("%s:%s:%s Buffer found", TAG, __func__, "CORE");
      unfilled_frame_queue.PushBack(*it);
      occupy_buffer_queue_.Erase(it);
      Mutex::Autolock autoLock(wait_for_unfilled_frame_lock_);
      wait_for_unfilled_frame_.signal();
      found = true;
      break;
    }
  }

  assert(found == true);
  TEST_VERBOSE("%s:%s:%s Exit", TAG, __func__, "CORE");
  return ret;
}

status_t TransCoderCore::NotifyPortEvent(PortEventType event_type,
                                         void* event_data) {
  TEST_INFO("%s:%s:%s Enter", TAG, __func__, "CORE");
  status_t ret = 0;

  return ret;
  TEST_INFO("%s:%s:%s Exit", TAG, __func__, "CORE");
}

status_t TransCoderCore::DequeInputBuffer(TransCodeBuffer& buffer) {
  TEST_VERBOSE("%s:%s:%s Enter", TAG, __func__, "CORE");
  status_t ret = 0;

  if (unfilled_frame_queue.Size() <= 0) {
    TEST_DEBUG("%s:%s:%s No buffer available to notify Wait for new buffer",
        TAG, __func__, "CORE");
    Mutex::Autolock autoLock(wait_for_unfilled_frame_lock_);
    if (unfilled_frame_queue.Size() <= 0)
      wait_for_unfilled_frame_.wait(wait_for_unfilled_frame_lock_);
  }

  buffer = *unfilled_frame_queue.Begin();
  unfilled_frame_queue.Erase(unfilled_frame_queue.Begin());
  being_filled_frame_queue.PushBack(buffer);

  TEST_VERBOSE("%s:%s:%s Exit", TAG, __func__, "CORE");
  return ret;
}

status_t TransCoderCore::QueueInputBuffer(TransCodeBuffer& buffer) {
  TEST_VERBOSE("%s:%s:%s Enter", TAG, __func__, "CORE");
  status_t ret = 0;
  bool found = false;

  List<TransCodeBuffer>::iterator it = being_filled_frame_queue.Begin();

  for (; it != being_filled_frame_queue.End(); ++it) {
    if (it->getID() == buffer.getID()) {
      TEST_VERBOSE("%s:%s:%s Buffer found", TAG, __func__, "CORE");
      *it = buffer;
      free_buffer_queue_.PushBack(*it);
      being_filled_frame_queue.Erase(it);
      Mutex::Autolock autoLock(wait_for_frame_lock_);
      wait_for_frame_.signal();
      found = true;
      break;
    }
  }
  assert(found == true);
  TEST_VERBOSE("%s:%s:%s Exit", TAG, __func__, "CORE");
  return ret;
}

};  //namespace transcode
};  //namespace qmmf