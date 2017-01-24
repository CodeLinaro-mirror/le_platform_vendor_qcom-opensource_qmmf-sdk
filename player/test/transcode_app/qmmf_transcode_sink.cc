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

#include "qmmf_transcode_sink.h"

namespace qmmf {
namespace transcode {

TransCoderSink::TransCoderSink(CodecType type, CodecParam& codec_params,
    shared_ptr<TransCoderPipe>& pipe):  pipe_(pipe), codec_type_(type),
    params_(codec_params), port_index_(kPortIndexOutput) {

  TEST_INFO("%s:%s:%s Enter", TAG, __func__, "SINK");
  TEST_INFO("%s:%s:%s Exit", TAG, __func__, "SINK");
}

TransCoderSink::~TransCoderSink() {
  TEST_INFO("%s:%s:%s Enter", TAG, __func__, "SINK");
  TEST_INFO("%s:%s:%s Exit", TAG, __func__, "SINK");
}

status_t TransCoderSink::PreparePipeline() {
  TEST_INFO("%s:%s:%s Enter", TAG, __func__, "SINK");
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
      TEST_ERROR("%s:%s:%s CodecType not Supported", TAG, __func__, "SINK");
      return -1;
  }

  ret = avcodec_->ConfigureCodec(mime_,params_);
  if (ret != 0) {
    TEST_ERROR("%s:%s:%s Failed to Configure Codec", TAG, __func__, "SINK");
    return ret;
  }

  ret = AllocateBuffers(buffer_list_, avcodec_, BufferOwner::kTransCoderSink,
      port_index_);
  if (ret != 0) {
    TEST_ERROR("%s:%s:%s Failed to Allocate Input Buffers",
        TAG, __func__, "SINK");
    return ret;
  }

  vector<BufferDescriptor> temp_out;
  for (auto& iter: buffer_list_) {
    BufferDescriptor temp_buffer;
    memset(&temp_buffer, 0x0, sizeof(temp_buffer));
    ret = iter.getAVCodecBuffer(AVCodecBufferType::kNormal,temp_buffer);
    assert(ret == 0);
    temp_out.push_back(temp_buffer);
  }

  ret = avcodec_->RegisterOutputBuffers(temp_out);
  if (ret != 0) {
    TEST_ERROR("%s:%s:%s Failed to register Output Buffers",
        TAG, __func__, "SINK");
    return ret;
  }

  shared_ptr<TransCoderSink> output_source_impl = shared_from_this();

  ret = avcodec_->AllocateBuffer(port_index_, 0, 0,
      ::std::static_pointer_cast<ICodecSource>(output_source_impl),
      temp_out);
  if(ret != OK) {
    TEST_ERROR("%s:%s:%s Failed to Call Allocate buffer on SinkSide",
        TAG, __func__, "SINK");
    ReleaseBuffers(buffer_list_);
    return ret;
  }

  pipe_->PassOutputCodec(avcodec_);

  ret = AddBufferList(buffer_list_);
  assert(ret == OK);

  TEST_INFO("%s:%s:%s Exit", TAG, __func__, "SINK");
  return ret;
}

status_t TransCoderSink::StartCodec() {
  status_t ret = 0;
  TEST_INFO("%s:%s:%s Enter", TAG, __func__, "SINK");
  ret = avcodec_->StartCodec();
  assert(ret == OK);
  TEST_INFO("%s:%s:%s Exit", TAG, __func__, "SINK");
  return ret;
}

status_t TransCoderSink::StopCodec() {
  status_t ret = 0;
  TEST_INFO("%s:%s:%s Enter", TAG, __func__, "SINK");

  ret = avcodec_->StopCodec();
  assert(ret == OK);

  TEST_INFO("%s:%s:%s Exit", TAG, __func__, "SINK");
  return ret;
}

status_t TransCoderSink::DeleteCodec() {
  status_t ret = 0;
  TEST_INFO("%s:%s:%s Enter", TAG, __func__, "SINK");

  ret = avcodec_->ReleaseBuffer();
  assert(ret == OK);

  ret = ReleaseBuffers(buffer_list_);
  assert(ret == OK);

  avcodec_.reset();

  TEST_INFO("%s:%s:%s Exit", TAG, __func__, "SINK");
  return ret;
}

status_t TransCoderSink::AddBufferList(vector<TransCodeBuffer>& list) {

  free_buffer_queue_.Clear();
  occupy_buffer_queue_.Clear();
  filled_buffer_queue_.Clear();
  being_read_buffer_queue_.Clear();

  for (auto& iter : list)
    free_buffer_queue_.PushBack(iter);
  return 0;
}

status_t TransCoderSink::GetBuffer(BufferDescriptor& buffer_descriptor,
                                   void* client_data) {
  TEST_VERBOSE("%s:%s:%s Enter", TAG, __func__, "SINK");
  status_t ret = 0;

  if (free_buffer_queue_.Size() <= 0) {
    TEST_DEBUG("%s:%s:%s No buffer available to notify Wait for new buffer",
        TAG, __func__, "SINK");
    Mutex::Autolock autoLock(wait_for_frame_lock_);
    if (free_buffer_queue_.Size() <= 0)
      wait_for_frame_.wait(wait_for_frame_lock_);
  }

  TransCodeBuffer iter = *free_buffer_queue_.Begin();
  ret = iter.getAVCodecBuffer(AVCodecBufferType::kNormal, buffer_descriptor);
  assert(ret == 0);
  free_buffer_queue_.Erase(free_buffer_queue_.Begin());
  occupy_buffer_queue_.PushBack(iter);

  TEST_VERBOSE("%s:%s:%s Exit", TAG, __func__, "SINK");
  return ret;
}

status_t TransCoderSink::ReturnBuffer(BufferDescriptor& buffer_descriptor,
                                      void* client_data) {
  TEST_VERBOSE("%s:%s:%s Enter", TAG, __func__, "SINK");
  status_t ret = 0;
  bool found = false;

  List<TransCodeBuffer>::iterator it = occupy_buffer_queue_.Begin();

  for (; it != occupy_buffer_queue_.End(); ++it) {
    //for now data pointer will be used instead of buf_id
    if (it->data() == buffer_descriptor.data) {
    // if (it->getID() == buffer_descriptor.buf_id) {
      TEST_VERBOSE("%s:%s:%s Buffer found", TAG, __func__, "SINK");
      it->UpdateTransCodeBuffer(buffer_descriptor);
      filled_buffer_queue_.PushBack(*it);
      occupy_buffer_queue_.Erase(it);
      Mutex::Autolock autoLock(wait_for_filled_frame_lock_);
      wait_for_filled_frame_.signal();
      found = true;
      break;
    }
  }

  assert(found == true);
  TEST_VERBOSE("%s:%s:%s Exit", TAG, __func__, "SINK");
  return ret;
}

status_t TransCoderSink::NotifyPortEvent(PortEventType event_type,
                                         void* event_data) {
  TEST_INFO("%s:%s:%s Enter", TAG, __func__, "SINK");
  status_t ret = 0;

  return ret;
  TEST_INFO("%s:%s:%s Exit", TAG, __func__, "SINK");
}

status_t TransCoderSink::DequeInputBuffer(TransCodeBuffer& buffer) {
  TEST_VERBOSE("%s:%s:%s Enter", TAG, __func__, "SINK");
  status_t ret = 0;

  if (filled_buffer_queue_.Size() <= 0) {
    TEST_DEBUG("%s:%s:%s No buffer available to notify Wait for new buffer",
        TAG, __func__, "SINK");
    Mutex::Autolock autoLock(wait_for_filled_frame_lock_);
    if (filled_buffer_queue_.Size() <= 0)
      wait_for_filled_frame_.wait(wait_for_filled_frame_lock_);
  }

  buffer = *filled_buffer_queue_.Begin();
  filled_buffer_queue_.Erase(filled_buffer_queue_.Begin());
  being_read_buffer_queue_.PushBack(buffer);

  TEST_VERBOSE("%s:%s:%s Exit", TAG, __func__, "SINK");
  return ret;
}

status_t TransCoderSink::QueueInputBuffer(TransCodeBuffer& buffer) {
  TEST_VERBOSE("%s:%s:%s Enter", TAG, __func__, "SINK");
  status_t ret = 0;
  bool found = false;

  List<TransCodeBuffer>::iterator it = being_read_buffer_queue_.Begin();

  for (; it != being_read_buffer_queue_.End(); ++it) {
    if (it->getID() == buffer.getID()) {
      TEST_VERBOSE("%s:%s:%s Buffer found", TAG, __func__, "SINK");
      *it = buffer;
      free_buffer_queue_.PushBack(*it);
      being_read_buffer_queue_.Erase(it);
      Mutex::Autolock autoLock(wait_for_frame_lock_);
      wait_for_frame_.signal();
      found = true;
      break;
    }
  }
  assert(found == true);
  TEST_VERBOSE("%s:%s:%s Exit", TAG, __func__, "SINK");
  return ret;
}

};  //namespace qmmf
};  //namespace transcode