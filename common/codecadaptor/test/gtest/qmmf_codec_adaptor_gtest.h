/*
* Copyright (c) 2016, The Linux Foundation. All rights reserved.
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

#include <vector>
#include <fcntl.h>
#include <dirent.h>
#include <functional>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <utils/Log.h>
#include <gtest/gtest.h>
#include <linux/msm_ion.h>
#include <media/msm_media_info.h>


#include "common/codecadaptor/src/qmmf_avcodec.h"
#include "common/qmmf_log.h"

using namespace qmmf;

#define MAX_FILE_NAME 80
typedef  struct ion_allocation_data IonHandleData;

class InputCodecSourceImpl;
class OutputCodecSourceImpl;

class CodecGtest : public ::testing::Test {

 public:
  CodecGtest() {};

  ~CodecGtest() {};

 protected:
  const ::testing::TestInfo* test_info_;

  void SetUp() override;

  void TearDown() override;

  status_t CreateCodec();

  status_t DeleteCodec();

  status_t AllocateBuffer(OMX_U32 port);

  status_t ReleaseBuffer();

  void CodecEventCallback(OMX_EVENTTYPE event, OMX_U32 data1, OMX_U32 data2);

  AVCodec*                  avcodec_;
  int32_t                   ion_device_;
  Mutex                     stop_lock_;
  bool                      stop_;
  Vector<StreamBuffer>      input_buffer_list_;
  Vector<CodecBuffer>       output_buffer_list_;
  Vector<IonHandleData>     ion_handle_data;
  sp<InputCodecSourceImpl>  input_source_impl_;
  sp<OutputCodecSourceImpl> output_source_impl_;
};

class InputCodecSourceImpl : public IInputCodecSource {

public:
  InputCodecSourceImpl();

  ~InputCodecSourceImpl();

  status_t Read(StreamBuffer& stream_buffer) override;

  status_t SignalBufferReturned(StreamBuffer& stream_buffer) override;

  status_t NotifyStatus(CodecInputPortStatus status) override;

  void BufferStatus();

  void AddBufferList(Vector<StreamBuffer>& list);

private:
  status_t   ReadFile(int32_t fd, uint32_t size, int32_t *byte_read);

  Mutex                 wait_for_frame_lock_;
  Condition             wait_for_frame_;
  Vector<StreamBuffer>  input_list_;
  TSQueue<StreamBuffer> input_free_buffer_queue_;
  TSQueue<StreamBuffer> input_occupy_buffer_queue_;
};

class OutputCodecSourceImpl : public IOutputCodecSource {

public:
  OutputCodecSourceImpl();

  ~OutputCodecSourceImpl();

  status_t GetBuffer(CodecBuffer& codec_buffer) override;

  status_t ReturnBuffer(CodecBuffer& codec_buffer) override;

  void BufferStatus();

  void AddBufferList(Vector<CodecBuffer>& list);

private:
  Mutex                wait_for_frame_lock_;
  Condition            wait_for_frame_;
  Vector<CodecBuffer>  output_list_;
  TSQueue<CodecBuffer> output_free_buffer_queue_;
  TSQueue<CodecBuffer> output_occupy_buffer_queue_;
};

