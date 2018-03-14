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

* THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
* WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
* ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORSAA
* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#define LOG_TAG "CodecTest"

#include <chrono>

#include "avcodec/test/sample/qmmf_audio_encode_test.h"

using qmmf::AACFormat;
using qmmf::AACMode;
using qmmf::AudioDeviceId;
using qmmf::AudioFormat;
using qmmf::avcodec::CodecParam;
using qmmf::avcodec::IAVCodec;
using qmmf::avcodec::ICodecSource;
using qmmf::avcodec::kPortIndexInput;
using qmmf::avcodec::kPortIndexOutput;
using qmmf::avcodec::PortEventType;
using qmmf::BufferDescriptor;
using qmmf::BufferFlags;
using qmmf::CodecMimeType;
using qmmf::DeviceId;
using std::chrono::seconds;
using std::ios;
using std::lock_guard;
using std::make_shared;
using std::mutex;
using std::ifstream;
using std::shared_ptr;
using std::string;
using std::to_string;
using std::unique_lock;
using std::vector;

using namespace audioencodetest;

CodecTest::CodecTest()
  : avcodec_(nullptr),
    ion_device_(-1) {
  QMMF_INFO("%s: Enter", __func__);
  ion_device_ = open("/dev/ion", O_RDONLY);
  if (ion_device_ <= 0) {
    QMMF_ERROR("%s: Ion dev open failed %s", __func__, strerror(errno));
    ion_device_ = -1;
  }
  QMMF_INFO("%s: Exit", __func__);
}

CodecTest::~CodecTest() {
  QMMF_INFO("%s: Enter", __func__);
  if (ion_device_ > 0) {
    close(ion_device_);
    ion_device_ = -1;
  }
  QMMF_INFO("%s: Exit", __func__);
}

status_t CodecTest::AllocateBuffer(uint32_t index) {
  status_t ret = 0;

  assert(ion_device_ > 0);
  int32_t ionType = ION_HEAP(ION_IOMMU_HEAP_ID);

  uint32_t count, size;
  ret = avcodec_->GetBufferRequirements(index, &count, &size);
  if (ret != 0) {
    QMMF_ERROR("%s: Failed to get Buffer Requirements on %s", __func__,
               PORT_NAME(index));
    return ret;
  }

  struct ion_allocation_data alloc;
  struct ion_fd_data ionFdData;
  void* vaddr = nullptr;

  for (uint32_t i = 0; i < count; i++) {
    BufferDescriptor buffer;
    vaddr = nullptr;

    memset(&buffer, 0x0, sizeof(buffer));
    memset(&alloc, 0x0, sizeof(ion_allocation_data));
    memset(&ionFdData, 0x0, sizeof(ion_fd_data));

    alloc.len = size;
    alloc.len = (alloc.len + 4095) & (~4095);
    alloc.align = 4096;
    alloc.flags = ION_FLAG_CACHED;
    alloc.heap_id_mask = ionType;

    ret = ioctl(ion_device_, ION_IOC_ALLOC, &alloc);
    if (ret < 0) {
      QMMF_ERROR("%s: ION allocation failed", __func__);
      goto ION_ALLOC_FAILED;
    }

    ionFdData.handle = alloc.handle;
    ret = ioctl(ion_device_, ION_IOC_SHARE, &ionFdData);
    if (ret < 0) {
      QMMF_ERROR("%s: ION map failed %s", __func__, strerror(errno));
      goto ION_MAP_FAILED;
    }

    vaddr = mmap(nullptr, alloc.len, PROT_READ | PROT_WRITE, MAP_SHARED,
                  ionFdData.fd, 0);
    if (vaddr == MAP_FAILED) {
      QMMF_ERROR("%s: ION mmap failed: %s (%d)", __func__, strerror(errno),
                  errno);
      goto ION_MAP_FAILED;
    }
    buffer.data = vaddr;
    buffer.capacity = alloc.len;
    buffer.size = 0;
    buffer.fd = ionFdData.fd;
    input_ion_handle_data_.push_back(alloc);
    QMMF_DEBUG("%s: buffer.Fd(%d)\n", __func__, buffer.fd);
    QMMF_DEBUG("%s: buffer.capacity(%d)\n", __func__, buffer.capacity);
    QMMF_DEBUG("%s: buffer.vaddr(%p)\n", __func__, buffer.data);

    if (index == kPortIndexInput) {
      input_buffer_list_.push_back(buffer);
    } else {
      output_buffer_list_.push_back(buffer);
    }
  }
  QMMF_INFO("%s: Exit", __func__);
  return ret;

ION_MAP_FAILED:
  struct ion_handle_data ionHandleData;
  memset(&ionHandleData, 0x0, sizeof(ionHandleData));
  ionHandleData.handle = ionFdData.handle;
  ioctl(ion_device_, ION_IOC_FREE, &ionHandleData);
ION_ALLOC_FAILED:
  close(ion_device_);
  ion_device_ = -1;
  QMMF_ERROR("%s ION Buffer allocation failed!", __func__);
  QMMF_INFO("%s: Exit", __func__);
  return -1;
}

status_t CodecTest::ReleaseBuffer() {
  QMMF_INFO("%s: Enter", __func__);

  int i = 0;
  input_source_impl_->BufferStatus();
  output_source_impl_->BufferStatus();
  for (auto& iter : input_buffer_list_) {
    if ((iter).data) {
      munmap((iter).data, (iter).capacity);
      (iter).data = nullptr;
    }
    if ((iter).fd) {
      ioctl(ion_device_, ION_IOC_FREE, &(input_ion_handle_data_[i]));
      close((iter).fd);
      (iter).fd = -1;
    }
    i++;
  }

  i = 0;
  for (auto& iter : output_buffer_list_) {
    if ((iter).data) {
      munmap((iter).data, (iter).capacity);
      (iter).data = nullptr;
    }
    if ((iter).fd) {
      ioctl(ion_device_, ION_IOC_FREE, &(output_ion_handle_data_[i]));
      close((iter).fd);
      (iter).fd = -1;
    }
    ++i;
  }

  input_buffer_list_.clear();
  output_buffer_list_.clear();
  input_ion_handle_data_.clear();
  output_ion_handle_data_.clear();

  QMMF_INFO("%s: Exit", __func__);
  return 0;
}

status_t CodecTest::ParseConfig(const string& filename, TestInitParams* params) {

  QMMF_INFO("%s: Enter", __func__);
  FILE *fp;
  bool isStreamReadCompleted = false;
  const int MAX_LINE = 128;
  char line[MAX_LINE];
  char value[70];
  char key[30];
  uint32_t id = 0;

  if(!(fp = fopen(filename.c_str(),"r"))) {
    QMMF_ERROR("%s: failed to open config file: %s", __func__, filename.c_str());
    return -1;
  }

  while(fgets(line,MAX_LINE-1,fp)) {
    if((line[0] == '\n') || (line[0] == '/') || line[0] == ' ')
        continue;
    memset(value, 0x0, sizeof(value));
    memset(key, 0x0, sizeof(key));
    if(isStreamReadCompleted) {
        isStreamReadCompleted = false;
    }
    int len = strlen(line);
    int i,j = 0;

    //This assumes new stream params always start with #
    if(!strcspn(line,"#")) {
        id++;
        continue;
     }

    int pos = strcspn(line,":");
    for(i = 0; i< pos; i++){
        if(line[i] != ' ') {
            key[j] = line[i];
            j++;
        }
    }

    key[j] = '\0';
    j = 0;
    for(i = pos+1; i< len-1; i++) {
        if(line[i] != ' ') {
            value[j] = line[i];
             j++;
        }
    }
    value[j] = '\0';
    QMMF_VERBOSE("%s: Key:%s", __func__, key);
    QMMF_VERBOSE("%s: Value:%s", __func__, value);
    string temp(value);
    if(!strncmp("InputFile", key, strlen("InputFile"))) {
      temp.copy(params->input_file, strlen(value));
      params->input_file[strlen(value)] = '\0';
    } else if(!strncmp("OutputFile", key, strlen("OutputFile"))) {
      temp.copy(params->output_file, strlen(value));
      params->output_file[strlen(value)] = '\0';
    } else if (!strncmp("AACFormat", key, strlen("AACFormat"))) {
       if(!strncmp("ADTS", value, strlen("ADTS"))) {
        params->create_param.audio_enc_param.codec_params.aac.format = AACFormat::kADTS;
       } else  if(!strncmp("Raw", value, strlen("Raw"))) {
         params->create_param.audio_enc_param.codec_params.aac.format = AACFormat::kRaw;
       }
    } else if (!strncmp("AACMode", key, strlen("AACMode"))) {
      if(!strncmp("AALC", value, strlen("AALC"))) {
        params->create_param.audio_enc_param.codec_params.aac.mode = AACMode::kAALC;
       } else if(!strncmp("HEV1", value, strlen("HEV1"))) {
         params->create_param.audio_enc_param.codec_params.aac.mode = AACMode::kHEVC_v1;
       } else if (!strncmp("HEV2", value, strlen("HEV2"))) {
         params->create_param.audio_enc_param.codec_params.aac.mode = AACMode::kHEVC_v2;
       }
    } else {
        QMMF_ERROR("%s: Unknown Key %s found", __func__, key);
        goto READ_FAILED;
    }
    QMMF_VERBOSE("%s: Iteration Done", __func__);
  }

  QMMF_INFO("%s: Exit", __func__);
  fclose(fp);
  return 0;
READ_FAILED:
  QMMF_INFO("%s: Exit", __func__);
  fclose(fp);
  return -1;
}

status_t CodecTest::ParseWavHeader(TestInitParams* params) {
  ifstream wav;
  wav.open(params->input_file, ios::in | ios::binary);
  if (!wav.is_open()) {
    QMMF_ERROR("%s: error opening file[%s]", __func__,
                  params->input_file);
    return -EBADF;
  }

  wav.read(reinterpret_cast<char*>(&header_.riff_header),
                 sizeof header_.riff_header);
  if (header_.riff_header.riff_id != kIdRiff ||
         header_.riff_header.wave_id != kIdWave) {
    wav.close();
    QMMF_ERROR("%s() file[%s] is not WAV format", __func__,
                  params->input_file);
    return -EDOM;
  }

  bool read_more_chunks = true;
  do {
    wav.read(reinterpret_cast<char*>(&header_.chunk_header),
                sizeof header_.chunk_header);
    switch (header_.chunk_header.format_id) {
      case kIdFmt:
        wav.read(reinterpret_cast<char*>(&header_.chunk_format),
                    sizeof header_.chunk_format);
        // if the format header is larger, skip the rest
        if (header_.chunk_header.format_size > sizeof header_.chunk_format)
          wav.seekg(header_.chunk_header.format_size -
                      sizeof header_.chunk_format, ios::cur);
        break;
    case kIdData:
        // stop looking for chunks
        read_more_chunks = false;
        break;
    default:
       // unknown chunk, skip bytes
      wav.seekg(header_.chunk_header.format_size, ios::cur);
    }
  } while (read_more_chunks);

  if (header_.chunk_format.audio_format == kFormatPcm) {
    params->create_param.audio_enc_param.channels = header_.chunk_format.num_channels;
    params->create_param.audio_enc_param.sample_rate = header_.chunk_format.sample_rate;
    params->create_param.audio_enc_param.bit_depth = header_.chunk_format.bits_per_sample;
  } else {
    QMMF_ERROR("%s: WAV file[%s] is not PCM format", __func__,
               params->input_file);
    return -EDOM;
  }
  wav.close();
  QMMF_VERBOSE("%s: No of channels:%d, sample-rate:%d, Bit-depth:%d", __func__,
      params->create_param.audio_enc_param.channels,
      params->create_param.audio_enc_param.sample_rate,
      params->create_param.audio_enc_param.bit_depth);

  QMMF_VERBOSE("%s: wav-header[%s]", __func__,
              header_.ToString().c_str());
  return 0;
}

status_t CodecTest::CreateCodec(int argc, char* argv[]) {
  QMMF_INFO("%s: Enter", __func__);
  int ret = 0;

  if((argc < 2 ) || (strcmp(argv[1], "-c"))) {
    QMMF_INFO("%s: Usage: %s -c config.txt", __func__, argv[0]);
    return -1;
  }

  TestInitParams params;
  memset(&params, 0x0, sizeof(params));

  params.create_param.audio_enc_param.in_devices_num = 0;
  params.create_param.audio_enc_param.in_devices[0] =
   static_cast<DeviceId>(AudioDeviceId::kBuiltIn);
  params.create_param.audio_enc_param.format = AudioFormat::kAAC;
  params.create_param.audio_enc_param.out_device = 0;
  params.create_param.audio_enc_param.flags = 0;
  params.create_param.audio_enc_param.codec_params.aac.bit_rate = 128000;
  params.create_param.audio_enc_param.codec_params.aac.frame_length = 1024;

  ret = ParseConfig(argv[2], &params);
  if(ret != 0) {
    QMMF_ERROR("%s: Failed to parse config file(%s)", __func__, argv[2]);
    return ret;
  }

  ret = ParseWavHeader(&params);
  if(ret != 0) {
    QMMF_ERROR("%s: Failed to parse wav header for file(%s)", __func__, params.input_file);
    return ret;
  }

  avcodec_ = IAVCodec::CreateAVCodec();
  if (avcodec_ == nullptr) {
    QMMF_ERROR("%s: avcodec creation failed\n", __func__);
    return -1;
  }

  ret = avcodec_->ConfigureCodec(CodecMimeType::kMimeTypeAudioEncAAC,
                                 params.create_param);
  if (ret != 0) {
    QMMF_ERROR("%s: Failed to configure Codec", __func__);
    return ret;
  }

  ret = AllocateBuffer(kPortIndexInput);
  if (ret != 0) {
    QMMF_ERROR("%s: Failed to allocate buffer on PORT_NAME(%d)", __func__,
               kPortIndexInput);
    return ret;
  }

  uint32_t buf_count = 0;
  uint32_t buf_size = 0;

  QMMF_VERBOSE("%s: Input file name: %s", __func__, params.input_file);
  string input(params.input_file);
  input_source_impl_ = make_shared<InputCodecSourceImpl>(input);
  if (input_source_impl_.get() == nullptr) {
    QMMF_ERROR("%s: failed to create input source", __func__);
    return -ENOMEM;
  }

  ret = avcodec_->GetBufferRequirements(kPortIndexInput, &buf_count, &buf_size);
  if (ret != 0) {
    QMMF_ERROR("%s: Failed to get Buffer Requirements on %s", __func__,
               PORT_NAME(kPortIndexInput));
    return ret;
  }
  QMMF_INFO("%s:Port:%d Buf Count: %d Buf Size: %d list size: %d\n", __func__,
            kPortIndexInput, buf_count, buf_size, input_buffer_list_.size());

  ret = avcodec_->AllocateBuffer(kPortIndexInput, buf_count, buf_size,
                                 shared_ptr<ICodecSource>(input_source_impl_),
                                 input_buffer_list_);
  if (ret != 0) {
    QMMF_ERROR("%s: Failed to allocate buffer in AVCodec on PORT_NAME(%d)",
               __func__, kPortIndexInput);
    return ret;
  }
  input_source_impl_->AddBufferList(input_buffer_list_);
  QMMF_INFO("%s: Port:%d ret %d list size: %d\n", __func__, kPortIndexInput,
            ret, input_buffer_list_.size());

  ret = AllocateBuffer(kPortIndexOutput);
  if (ret != 0) {
    QMMF_ERROR("%s Failed to allocate buffer on PORT_NAME(%d)", __func__,
               kPortIndexOutput);
    return ret;
  }

  string output(params.output_file);
  QMMF_VERBOSE("%s: Output file name:%s", __func__, params.output_file);
  output_source_impl_ = make_shared<OutputCodecSourceImpl>(output);
  if (output_source_impl_.get() == nullptr) {
    QMMF_ERROR("%s: failed to create input source", __func__);
    return -ENOMEM;
  }

  ret = avcodec_->GetBufferRequirements(kPortIndexOutput, &buf_count, &buf_size);
  if (ret != 0) {
    QMMF_ERROR("%s: Failed to get Buffer Requirements on %s", __func__,
               PORT_NAME(kPortIndexOutput));
    return ret;
  }
  QMMF_INFO("%s: Port:%d Buf Count: %d Buf Size: %d", __func__,
            kPortIndexOutput, buf_count, buf_size);

  ret = avcodec_->AllocateBuffer(kPortIndexOutput, buf_count, buf_size,
                                 shared_ptr<ICodecSource>(output_source_impl_),
                                 output_buffer_list_);
  if (ret != 0) {
    QMMF_ERROR("%s Failed to allocate buffer in AVCodec on PORT_NAME(%d)",
               __func__, kPortIndexOutput);
    return ret;
  }
  output_source_impl_->AddBufferList(output_buffer_list_);
  QMMF_INFO("%s: Port:%d ret %d list size: %d", __func__, kPortIndexOutput, ret,
            output_buffer_list_.size());
  QMMF_INFO("%s: Exit", __func__);
  return 0;
}

status_t CodecTest::DeleteCodec() {
  QMMF_INFO("%s: Enter", __func__);
  if (avcodec_) {
    ReleaseBuffer();
    delete avcodec_;
  }
  if(input_source_impl_.get() != nullptr) {
    input_source_impl_ = nullptr;
  }
  if(output_source_impl_.get() != nullptr) {
    output_source_impl_ = nullptr;
  }

  QMMF_INFO("%s: Exit", __func__);
  return 0;
}

status_t CodecTest::StartCodec() {
  QMMF_INFO("%s: Enter", __func__);
  int ret = avcodec_->StartCodec();
  if (ret != 0) {
    QMMF_ERROR("%s: Failed to start codec", __func__);
    return ret;
  }
  QMMF_INFO("%s: Exit", __func__);
  return ret;
}

status_t CodecTest::StopCodec() {
  QMMF_INFO("%s: Enter", __func__);
  int ret = avcodec_->StopCodec(true);
  if (ret != 0) {
    QMMF_ERROR("%s: Failed to stop codec", __func__);
    return ret;
  }
  input_source_impl_->BufferStatus();
  output_source_impl_->BufferStatus();

  QMMF_INFO("%s: Exit", __func__);
  return ret;
}

status_t CodecTest::PauseCodec() {
  QMMF_INFO("%s: Enter", __func__);
  int ret = avcodec_->PauseCodec();
  if (ret != 0) {
    QMMF_ERROR("%s: Failed to pause codec", __func__);
  }
  QMMF_INFO("%s: Exit", __func__);
  return ret;
}

status_t CodecTest::ResumeCodec() {
  QMMF_INFO("%s: Enter", __func__);
  int ret = avcodec_->ResumeCodec();
  if (ret != 0) {
    QMMF_ERROR("%s: Failed to pause codec", __func__);
  }
  QMMF_INFO("%s: Exit", __func__);
  return ret;
}

/*Input port*/
InputCodecSourceImpl::InputCodecSourceImpl(const string& filename) {
  QMMF_INFO("%s: Enter", __func__);

  in_file_fd_ = open(filename.c_str(), O_RDONLY);
  if (in_file_fd_ < 0) {
    printf("Failed to open i/p file(%s), therefore exiting..\n", filename.c_str());
    QMMF_ERROR("%s: Failed to open i/p file(%s)", __func__, filename.c_str());
    exit(1);
  }
  QMMF_VERBOSE("%s: Input file opened", __func__);
  auto curpos = lseek(in_file_fd_, 0, SEEK_CUR);
  auto ret = lseek(in_file_fd_, 0, SEEK_END);
  QMMF_VERBOSE("%s: Input file size in bytes: %ld", __func__, ret);
  if(ret < 0) {
     QMMF_ERROR("%s: lseek to find size of file failed (%d) %s", __func__,
      errno, strerror(errno));
  } else if(ret == 0) {
      printf("Input file is an empty file (%s)\n", filename.c_str());
      QMMF_ERROR("%s: Input file is an empty file (%s)", __func__, filename.c_str());
      close(in_file_fd_);
      exit(1);
  }
  ret = lseek(in_file_fd_, curpos, SEEK_SET);
  if(ret < 0) {
     QMMF_ERROR("%s: Unable to seek position to beginning (%d) %s", __func__,
      errno, strerror(errno));
  }
  QMMF_INFO("%s: Exit", __func__);
}

InputCodecSourceImpl::~InputCodecSourceImpl() {
  QMMF_INFO("%s: Enter", __func__);
  if(in_file_fd_ > 0) {
    close(in_file_fd_);
    in_file_fd_ = -1;
  }
  QMMF_INFO("%s: Exit", __func__);
}

void InputCodecSourceImpl::ReadFile(BufferDescriptor* buffer) {
  QMMF_INFO("InputCodecSourceImpl:%s: Enter", __func__);
  int ret;
  do {
    ret = read(in_file_fd_, buffer->data, buffer->capacity);
    QMMF_DEBUG("%s: No of bytes read = %d and No of bytes to be read = %d",
                __func__, ret, buffer->capacity);
    if (ret < 0) {
      QMMF_ERROR("InputCodecSourceImpl:%s: Improper Read (%d) %s", __func__,
        errno, strerror(errno));
      close(in_file_fd_);
      in_file_fd_ = -1;
      assert(0);
    } else if (ret == 0) {
      auto status = lseek(in_file_fd_, 0, SEEK_SET);
      if(status < 0) {
        QMMF_ERROR("InputCodecSourceImpl:%s: Unable to seek position to "
          "beginning (%d) %s", __func__, errno, strerror(errno));
        assert(0);
      }
    }
  } while(ret == 0);
  QMMF_INFO("InputCodecSourceImpl:%s: Exit", __func__);
}

status_t InputCodecSourceImpl::GetBuffer(BufferDescriptor& stream_buffer,
                                         void* client_data) {
  QMMF_INFO("InputCodecSourceImpl:%s: Enter", __func__);
  while (input_free_buffer_vector_.size() <= 0) {
    QMMF_ERROR("InputCodecSourceImpl:%s:No buffer available. Wait for new buffer",
        __func__);
    unique_lock<mutex> lock(wait_for_frame_lock_);
    wait_for_frame_.wait_for(lock, seconds(1));
  }
  BufferDescriptor buffer;
  {
    lock_guard<mutex> lg(input_free_buffer_vector_lock_);
    buffer = *(input_free_buffer_vector_).begin();
  }
  QMMF_VERBOSE("InputCodecSourceImpl:%s: Buffer-descriptor: %s", __func__,
          buffer.ToString().c_str());
  assert(buffer.data != nullptr);
  QMMF_DEBUG("InputCodecSourceImpl:%s: In file fd:%d", __func__, in_file_fd_);
  /* Read Data and Copy to buffer
   * Send Buffer to Avcodec */
  ReadFile(&buffer);
  QMMF_VERBOSE("InputCodecSourceImpl:%s: Buffer-descriptor after file read: %s",
        __func__, buffer.ToString().c_str());
  stream_buffer.data = buffer.data;
  stream_buffer.size = buffer.capacity;
  stream_buffer.capacity = buffer.capacity;
  stream_buffer.fd = buffer.fd;
  {
    lock_guard<mutex> lg(input_occupy_buffer_vector_lock_);
    input_occupy_buffer_vector_.push_back(buffer);
  }
  {
    lock_guard<mutex> lg(input_free_buffer_vector_lock_);
    input_free_buffer_vector_.erase(input_free_buffer_vector_.begin());
  }
  QMMF_DEBUG("InputCodecSourceImpl:%s: Frame size: %d id: %d addr: %0x pts: "
      "%Ld cap: %d", __func__, stream_buffer.size, stream_buffer.fd,
      (unsigned int)stream_buffer.data, stream_buffer.timestamp,
      stream_buffer.capacity);

  QMMF_INFO("InputCodecSourceImpl:%s: Exit", __func__);
  return 0;
}

status_t InputCodecSourceImpl::ReturnBuffer(BufferDescriptor& buffer,
                                            void* client_data) {
  QMMF_INFO("InputCodecSourceImpl:%s: Enter", __func__);
  status_t ret = 0;
  bool found = false;
  {
    lock_guard<mutex> lg(input_occupy_buffer_vector_lock_);
    auto it = input_occupy_buffer_vector_.begin();
    for (; it != input_occupy_buffer_vector_.end(); ++it) {
      if ((*it).data == buffer.data) {
        lock_guard<mutex> lg(input_free_buffer_vector_lock_);
        input_free_buffer_vector_.push_back(*it);
        wait_for_frame_.notify_one();
        found = true;
        break;
      }
    }
    assert(found == true);
    input_occupy_buffer_vector_.erase(it);
  }
  QMMF_INFO("InputCodecSourceImpl:%s: Exit", __func__);
  return ret;
}

status_t InputCodecSourceImpl::NotifyPortEvent(PortEventType event_type,
                                               void* event_data) {
  QMMF_INFO("InputCodecSourceImpl:%s: Enter", __func__);
  QMMF_INFO("InputCodecSourceImpl:%s: Exit", __func__);
  return 0;
}
void InputCodecSourceImpl::AddBufferList(vector<BufferDescriptor>& list) {
  QMMF_INFO("InputCodecSourceImpl:%s: Enter", __func__);
  input_list_ = list;
  {
    lock_guard<mutex> lg(input_free_buffer_vector_lock_);
    input_free_buffer_vector_.clear();
  }
  {
    lock_guard<mutex> lg(input_occupy_buffer_vector_lock_);
    input_occupy_buffer_vector_.clear();
  }
  {
    lock_guard<mutex> lg(input_free_buffer_vector_lock_);
    for (auto& iter : input_list_) {
      input_free_buffer_vector_.push_back(iter);
    }
  }
  QMMF_INFO("InputCodecSourceImpl:%s: Exit", __func__);
}

void InputCodecSourceImpl::BufferStatus() {
  QMMF_INFO("InputCodecSourceImpl:%s: Enter", __func__);

  QMMF_INFO("Input-Port:%s: Total Buffer(%d), free(%d), occupy(%d)\n", __func__,
            input_list_.size(), input_free_buffer_vector_.size(),
            input_occupy_buffer_vector_.size());

  QMMF_INFO("InputCodecSourceImpl:%s: Exit", __func__);
}

OutputCodecSourceImpl::OutputCodecSourceImpl(const string& filename) {
  QMMF_INFO("%s: Enter", __func__);

  out_file_fd_ = open(filename.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0655);
  if (out_file_fd_ < 0) {
    QMMF_ERROR("%s: Failed to open o/p file(%s)", __func__, filename.c_str());
  }

  QMMF_INFO("%s: Exit", __func__);
}

OutputCodecSourceImpl::~OutputCodecSourceImpl() {
  QMMF_INFO("%s: Enter", __func__);
  if(out_file_fd_ > 0) {
    close(out_file_fd_);
    out_file_fd_ = -1;
  }
  QMMF_INFO("%s: Exit", __func__);
}

status_t OutputCodecSourceImpl::GetBuffer(BufferDescriptor& codec_buffer,
                                          void* client_data) {
  QMMF_INFO("OutputCodecSourceImpl:%s: Enter", __func__);

  while (output_free_buffer_vector_.size() <= 0) {
    QMMF_WARN(
        "OutputCodecSourceImpl:%s: No buffer available to notify. Wait for new "
        "buffer",
        __func__);
    unique_lock<mutex> lock(wait_for_frame_lock_);
    wait_for_frame_.wait_for(lock, seconds(1));
  }
  {
    lock_guard<mutex> lg(output_free_buffer_vector_lock_);
    BufferDescriptor iter = *output_free_buffer_vector_.begin();
    codec_buffer.fd = (iter).fd;
    codec_buffer.data = (iter).data;
    {
      lock_guard<mutex> lg(output_occupy_buffer_vector_lock_);
      output_occupy_buffer_vector_.push_back(iter);
    }
    output_free_buffer_vector_.erase(output_free_buffer_vector_.begin());
  }
  QMMF_INFO("OutputCodecSourceImpl:%s: Exit", __func__);
  return 0;
}

status_t OutputCodecSourceImpl::ReturnBuffer(BufferDescriptor& codec_buffer,
                                             void* client_data) {
  QMMF_INFO("%s: Enter", __func__);

  assert(codec_buffer.data != nullptr);
  QMMF_VERBOSE("OutputCodecSourceImpl:%s: Buffer-descriptor: %s", __func__,
        codec_buffer.ToString().c_str());
  QMMF_DEBUG("OutputCodecSourceImpl:%s: Out file fd:%d", __func__, out_file_fd_);
  if (out_file_fd_ > 0) {
    int expSize = (int)codec_buffer.size;
    QMMF_VERBOSE("OutputCodecSourceImpl:%s: Offset:%d, size:%d", __func__,
          codec_buffer.offset, codec_buffer.size);
    QMMF_INFO("OutputCodecSourceImpl:FillBufferDone size writen to file  %zd",
              expSize);
    if (expSize != write(out_file_fd_,
                         reinterpret_cast<uint8_t*>(codec_buffer.data),
                         codec_buffer.size)) {
      QMMF_ERROR("OutputCodecSourceImpl:%s: Bad Write error (%d) %s", __func__,
                 errno, strerror(errno));
      close(out_file_fd_);
      out_file_fd_ = -1;
    }
  } else {
    QMMF_ERROR("OutputCodecSourceImpl:%s File is not open to write", __func__);
  }

  if (codec_buffer.flag & static_cast<uint32_t>(BufferFlags::kFlagEOS)) {
    printf("Reached EOS, this is last buffer from encoder\n");
    QMMF_INFO(
        "OutputCodecSourceImpl:%s: This is last buffer from encoder.Close file",
        __func__);
    if (out_file_fd_ > 0) {
      close(out_file_fd_);
      out_file_fd_ = -1;
    }
  }
  bool found = false;
  {
    lock_guard<mutex> lg(output_occupy_buffer_vector_lock_);
    auto it = output_occupy_buffer_vector_.begin();
    for (; it != output_occupy_buffer_vector_.end(); ++it) {
      if (((*it).data) == (codec_buffer.data)) {
        lock_guard<mutex> lg(output_free_buffer_vector_lock_);
        output_free_buffer_vector_.push_back(*it);
        output_occupy_buffer_vector_.erase(it);
        wait_for_frame_.notify_one();
        found = true;
        break;
      }
    }
  }
  assert(found == true);
  QMMF_INFO("OutputCodecSourceImpl:%s: Exit", __func__);
  return 0;
}

status_t OutputCodecSourceImpl::NotifyPortEvent(PortEventType event_type,
                                                void* event_data) {
  QMMF_INFO("OutputCodecSourceImpl:%s: Enter", __func__);
  QMMF_INFO("OutputCodecSourceImpl:%s: Exit", __func__);
  return 0;
}

void OutputCodecSourceImpl::AddBufferList(vector<BufferDescriptor>& list) {
  QMMF_INFO("OutputCodecSourceImpl:%s: Enter", __func__);
  output_list_ = list;
  {
    lock_guard<mutex> lg(output_free_buffer_vector_lock_);
    output_free_buffer_vector_.clear();
  }
  {
    lock_guard<mutex> lg(output_occupy_buffer_vector_lock_);
    output_occupy_buffer_vector_.clear();
  }
  {
    lock_guard<mutex> lg(output_free_buffer_vector_lock_);
    for (auto& iter : output_list_) {
      output_free_buffer_vector_.push_back(iter);
    }
  }
  QMMF_INFO("OutputCodecSourceImpl:%s: Exit", __func__);
}

void OutputCodecSourceImpl::BufferStatus() {
  QMMF_INFO("%s: Enter", __func__);

  QMMF_INFO("Output-Port:%s: Total Buffer(%d), free(%d), occupy(%d)", __func__,
            output_list_.size(), output_free_buffer_vector_.size(),
            output_occupy_buffer_vector_.size());

  QMMF_INFO("%s: Exit", __func__);
}

void CmdMenu::PrintMenu() {
  printf("\n=========== AUDIO ENCODE TEST MENU ============\n");

  printf("\n Codec Test Application commands \n");
  printf(" -----------------------------\n");
  printf("   %c. Create Codec\n", CmdMenu::CREATE_CODEC_CMD);
  printf("   %c. Delete Codec\n", CmdMenu::DELETE_CODEC_CMD);
  printf("   %c. Start Codec\n", CmdMenu::START_CODEC_CMD);
  printf("   %c. Stop Codec\n", CmdMenu::STOP_CODEC_CMD);
  printf("   %c. Pause Codec\n", CmdMenu::PAUSE_CODEC_CMD);
  printf("   %c. Resume Codec\n", CmdMenu::RESUME_CODEC_CMD);
  printf("   %c. Exit\n", CmdMenu::EXIT_CMD);
  printf("\n   Choice: ");
}

CmdMenu::Command CmdMenu::GetCommand() {
  PrintMenu();
  return CmdMenu::Command(static_cast<CmdMenu::CommandType>(getchar()));
}

int main(int argc, char* argv[]) {
  QMMF_GET_LOG_LEVEL();

  QMMF_INFO("%s Enter", __func__);

  CodecTest test_context;

  CmdMenu cmd_menu(test_context);

  int32_t testRunning = true;

  while (testRunning) {
    CmdMenu::Command command = cmd_menu.GetCommand();

    switch (command.cmd) {
      case CmdMenu::CREATE_CODEC_CMD: {
        test_context.CreateCodec(argc, argv);
      } break;
      case CmdMenu::DELETE_CODEC_CMD: {
        test_context.DeleteCodec();
      } break;
      case CmdMenu::START_CODEC_CMD: {
        test_context.StartCodec();
      } break;
      case CmdMenu::STOP_CODEC_CMD: {
        test_context.StopCodec();
      } break;
      case CmdMenu::PAUSE_CODEC_CMD: {
        test_context.PauseCodec();
      } break;
      case CmdMenu::RESUME_CODEC_CMD: {
        test_context.ResumeCodec();
      } break;
      case CmdMenu::EXIT_CMD: {
        QMMF_INFO("%s exit from test", __func__);
        testRunning = false;
      } break;
      default:
        break;
    }
  }
  QMMF_INFO("%s Exit", __func__);
  return 0;
}
