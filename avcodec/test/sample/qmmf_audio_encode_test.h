/*
* Copyright (c) 2018, The Linux Foundation. All rights reserved.
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

#include <fcntl.h>
#include <linux/msm_ion.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstring>
#include <condition_variable>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <media/msm_media_info.h>
#include <utils/Log.h>

#include "common/utils/qmmf_log.h"
#include "qmmf-sdk/qmmf_avcodec.h"

namespace audioencodetest {

typedef struct ion_allocation_data IonHandleData;
typedef int32_t status_t;

static const uint32_t kIdRiff = 0x46464952;
static const uint32_t kIdWave = 0x45564157;
static const uint32_t kIdFmt  = 0x20746d66;
static const uint32_t kIdData = 0x61746164;
static const uint16_t kFormatPcm = 1;

struct TestInitParams {
  char input_file[50];
  char output_file[50];
  qmmf::avcodec::CodecParam create_param;
};

class InputCodecSourceImpl;
class OutputCodecSourceImpl;

class CodecTest {
 public:
  CodecTest();

  ~CodecTest();

  status_t CreateCodec(int argc, char* argv[]);

  status_t DeleteCodec();

  status_t StartCodec();

  status_t StopCodec();

  status_t PauseCodec();

  status_t ResumeCodec();

 private:
  status_t ParseConfig(const std::string& fileName, TestInitParams* params);

  status_t ParseWavHeader(TestInitParams* params);

  bool IsStop();

  status_t AllocateBuffer(uint32_t port);

  status_t ReleaseBuffer();

  qmmf::avcodec::IAVCodec* avcodec_;
  int32_t ion_device_;
  std::mutex stop_lock_;
  bool stop_;
  std::vector<qmmf::BufferDescriptor> input_buffer_list_;
  std::vector<qmmf::BufferDescriptor> output_buffer_list_;
  std::vector<IonHandleData> input_ion_handle_data_;
  std::vector<IonHandleData> output_ion_handle_data_;
  std::shared_ptr<InputCodecSourceImpl> input_source_impl_;
  std::shared_ptr<OutputCodecSourceImpl> output_source_impl_;

  struct __attribute__((packed)) WavRiffHeader {
    uint32_t riff_id;
    uint32_t riff_size;
    uint32_t wave_id;

    ::std::string ToString() const {
      ::std::stringstream stream;
      stream << "riff_id[" << ::std::setbase(16) << riff_id
            << ::std::setbase(10) << "] ";
      stream << "riff_size[" << riff_size << "] ";
      stream << "wave_id[" << ::std::setbase(16) << wave_id
            << ::std::setbase(10) << "] ";
      return stream.str();
    }
  };

  struct __attribute__((packed)) WavChunkHeader {
    uint32_t format_id;
    uint32_t format_size;

    ::std::string ToString() const {
      ::std::stringstream stream;
      stream << "format_id[" << ::std::setbase(16) << format_id
            << ::std::setbase(10) << "] ";
      stream << "format_size[" << format_size << "] ";
      return stream.str();
    }
  };

  struct __attribute__((packed)) WavChunkFormat {
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;

    ::std::string ToString() const {
      ::std::stringstream stream;
      stream << "audio_format[" << audio_format << "] ";
      stream << "num_channels[" << num_channels << "] ";
      stream << "sample_rate[" << sample_rate << "] ";
      stream << "byte_rate[" << byte_rate << "] ";
      stream << "block_align[" << block_align << "] ";
      stream << "bits_per_sample[" << bits_per_sample << "] ";
      return stream.str();
    }
  };

  struct __attribute__((packed)) WavHeader {
    WavRiffHeader riff_header;
    WavChunkHeader chunk_header;
    WavChunkFormat chunk_format;

    ::std::string ToString() const {
      ::std::stringstream stream;
      stream << "riff_header[" << riff_header.ToString() << "] ";
      stream << "chunk_header[" << chunk_header.ToString() << "] ";
      stream << "chunk_format[" << chunk_format.ToString() << "] ";
      return stream.str();
    }
  };
  WavHeader header_;
};  // class CodecTest

class InputCodecSourceImpl : public  qmmf::avcodec::ICodecSource {
 public:
  InputCodecSourceImpl(const std::string& filename);

  ~InputCodecSourceImpl();

  status_t GetBuffer(qmmf::BufferDescriptor& stream_buffer,
                     void* client_data) override;

  status_t ReturnBuffer(qmmf::BufferDescriptor& stream_buffer,
                        void* client_data) override;

  status_t NotifyPortEvent(qmmf::avcodec::PortEventType event_type, void* event_data) override;

  void BufferStatus();

  void AddBufferList(std::vector<qmmf::BufferDescriptor>& list);

 private:
  void ReadFile(qmmf::BufferDescriptor* buffer);

  int32_t in_file_fd_;
  std::mutex wait_for_frame_lock_;
  std::condition_variable wait_for_frame_;
  std::vector<qmmf::BufferDescriptor> input_list_;
  std::mutex input_free_buffer_vector_lock_;
  std::mutex input_occupy_buffer_vector_lock_;
  std::vector<qmmf::BufferDescriptor> input_free_buffer_vector_;
  std::vector<qmmf::BufferDescriptor> input_occupy_buffer_vector_;
};  // Class InputCodecSourceImpl

class OutputCodecSourceImpl : public qmmf::avcodec::ICodecSource {
 public:
  OutputCodecSourceImpl(const std::string& filename);

  ~OutputCodecSourceImpl();

  status_t GetBuffer(qmmf::BufferDescriptor& codec_buffer,
                     void* client_data) override;

  status_t ReturnBuffer(qmmf::BufferDescriptor& codec_buffer,
                        void* client_data) override;

  status_t NotifyPortEvent(qmmf::avcodec::PortEventType event_type, void* event_data) override;

  void BufferStatus();

  void AddBufferList(std::vector<qmmf::BufferDescriptor>& list);

 private:
  int32_t out_file_fd_;
  std::mutex wait_for_frame_lock_;
  std::condition_variable wait_for_frame_;
  std::vector<qmmf::BufferDescriptor> output_list_;
  std::mutex output_free_buffer_vector_lock_;
  std::mutex output_occupy_buffer_vector_lock_;
  std::vector<qmmf::BufferDescriptor> output_free_buffer_vector_;
  std::vector<qmmf::BufferDescriptor> output_occupy_buffer_vector_;
};  // Class OutputCodecSourceImpl

class CmdMenu {
 public:
  enum CommandType {
    CREATE_CODEC_CMD = '1',
    DELETE_CODEC_CMD = '2',
    START_CODEC_CMD = '3',
    STOP_CODEC_CMD = '4',
    PAUSE_CODEC_CMD = '5',
    RESUME_CODEC_CMD = '6',
    SET_CODEC_PARAM_CMD = '7',
    EXIT_CMD = 'X',
    INVALID_CMD = '0'
  };

  struct Command {
    Command(CommandType cmd) : cmd(cmd) {}
    Command() : cmd(INVALID_CMD) {}
    CommandType cmd;
  };

  CmdMenu(CodecTest& ctx) : ctx_(ctx){};

  ~CmdMenu(){};

  Command GetCommand();

  void PrintMenu();

  void PrintDynamicParams();

  CodecTest& ctx_;
};
}; //namespace audioencodetest
