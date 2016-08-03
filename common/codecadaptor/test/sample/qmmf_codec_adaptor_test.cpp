/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *     Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *     Neither the name of The Linux Foundation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.

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

#define TAG "CodecTest"

#include "qmmf_codec_adaptor_test.h"

CodecTest::CodecTest()
    :avcodec_(nullptr),
     ion_device_(-1),
     stop_(false)
{
  QMMF_INFO("%s:%s: Enter ", TAG, __func__);

  ion_device_ = open("/dev/ion", O_RDONLY);
  if (ion_device_ <= 0) {
    QMMF_ERROR("%s:%s Ion dev open failed %s", TAG, __func__,strerror(errno));
    ion_device_ = -1;
  }

  QMMF_INFO("%s:%s: Exit", TAG, __func__);
}

CodecTest::~CodecTest()
{
  QMMF_INFO("%s:%s: Enter ", TAG, __func__);

  if(avcodec_)
    delete avcodec_;

  close(ion_device_);
  ion_device_ = -1;

  QMMF_INFO("%s:%s: Exit", TAG, __func__);
}

void CodecTest::CodecEventCallback(OMX_EVENTTYPE event
                                   __attribute__((__unused__)))
{
  QMMF_INFO("%s:%s Error from Codec. Stop encoding ", TAG, __func__);
  StopCodec();
  QMMF_INFO("%s:%s Exit", TAG, __func__);
}

status_t CodecTest::CreateCodec(int argc, char *argv[])
{
  QMMF_INFO("%s:%s: Enter ", TAG, __func__);
  status_t ret = 0;

  if((argc < 2 ) || (strcmp(argv[1], "-c"))) {
    QMMF_INFO("%s:%s Usage: %s -c config.txt", TAG, __func__, argv[0]);
    return -1;
  }

  TestInitParams params;
  memset(&params, 0x0, sizeof(params));

  ret = ParseConfig(argv[2], &params);
  if(ret != OK) {
    QMMF_ERROR("%s:%s Failed to parse config file %s", TAG, __func__, argv[2]);
    return ret;
  }

  avcodec_ = new AVCodec();
  if(avcodec_ ==  nullptr) {
    QMMF_ERROR("%s:%s avcodec creation failed", TAG, __func__);
    return NO_MEMORY;
  }

  params.create_param.event_cb = [&] (OMX_EVENTTYPE event)
      { CodecEventCallback(event);};

  ret = avcodec_->ConfigureCodec(params.codec_type, params.create_param);
  if(ret != OK) {
    QMMF_ERROR("%s:%s Failed to configure Codec", TAG, __func__);
    return ret;
  }

  ret = AllocateBuffer(kPortIndexInput);
  if(ret != OK) {
    QMMF_ERROR("%s:%s Failed to allocate buffer on OMX_PORT_NAME(%d)", TAG,
        __func__, kPortIndexInput);
    return ret;
  }

  input_source_impl_= new InputCodecSourceImpl(params.input_file,
      params.record_frame);
  if(input_source_impl_.get() == nullptr) {
    QMMF_ERROR("%s:%s failed to create input source",TAG, __func__);
    return NO_MEMORY;
  }

  ret = avcodec_->UseBuffer(kPortIndexInput, input_source_impl_.get());
  if(ret != OK) {
    QMMF_ERROR("%s:%s Failed to Call Use buffer on OMX_PORT_NAME(%d)", TAG,
        __func__, kPortIndexInput);
    ReleaseBuffer();
    return ret;
  }

  input_source_impl_->AddBufferList(input_buffer_list_);

  ret = AllocateBuffer(kPortIndexOutput);
  if(ret != OK) {
    QMMF_ERROR("%s:%s Failed to allocate buffer on OMX_PORT_NAME(%d)", TAG,
        __func__, kPortIndexOutput);
    ReleaseBuffer();
    return ret;
  }

  output_source_impl_ = new OutputCodecSourceImpl(params.output_file);
  if(output_source_impl_.get() == nullptr) {
    QMMF_ERROR("%s:%s failed to create output source",TAG, __func__);
    return NO_MEMORY;
  }

  ret = avcodec_->UseBuffer(kPortIndexOutput, output_source_impl_.get());
  if(ret != OK) {
    QMMF_ERROR("%s:%s Failed to Call Use buffer on OMX_PORT_NAME(%d)", TAG,
        __func__, kPortIndexOutput);
    ReleaseBuffer();
    return ret;
  }

  output_source_impl_->AddBufferList(output_buffer_list_);

  QMMF_INFO("%s:%s: Exit", TAG, __func__);
  return ret;
}

status_t CodecTest::DeleteCodec()
{
  QMMF_INFO("%s:%s: Enter ", TAG, __func__);
  status_t ret = 0;

  ret = avcodec_->ReleaseBuffer();
  assert(ret == OK);

  ret = ReleaseBuffer();
  assert(ret == OK);

  if(input_source_impl_.get())
    input_source_impl_.clear();

  if(output_source_impl_.get())
    output_source_impl_.clear();

  QMMF_INFO("%s:%s Exit", TAG, __func__);
  return ret;
}

status_t CodecTest::StartCodec()
{
  QMMF_INFO("%s:%s Enter ", TAG, __func__);
  status_t ret = 0;

  {
    Mutex::Autolock l(stop_lock_);
    stop_ = false;
  }

  ret = avcodec_->StartCodec();
  assert(ret == OK);

  QMMF_INFO("%s:%s Exit", TAG, __func__);
  return ret;
}

status_t CodecTest::StopCodec()
{
  QMMF_INFO("%s:%s Enter ", TAG, __func__);
  status_t ret = 0;

  {
    Mutex::Autolock l(stop_lock_);
    stop_ = true;
  }

  ret = avcodec_->StopCodec();
  assert(ret == OK);

  input_source_impl_->BufferStatus();
  output_source_impl_->BufferStatus();

  QMMF_INFO("%s:%s: Exit", TAG, __func__);
  return ret;
}

status_t CodecTest::ResumeCodec()
{
  QMMF_INFO("%s:%s Enter ", TAG, __func__);
  status_t ret = 0;

  ret =  avcodec_->ResumeCodec();
  assert(ret == OK);

  QMMF_INFO("%s:%s Exit", TAG, __func__);
  return ret;
}

status_t CodecTest::PauseCodec()
{
  QMMF_INFO("%s:%s Enter ", TAG, __func__);
  status_t ret = 0;

  ret =  avcodec_->PauseCodec();
  assert(ret == OK);

  QMMF_INFO("%s:%s Exit", TAG, __func__);
  return ret;
}

bool CodecTest::IsStop()
{
   Mutex::Autolock l(stop_lock_);
   return stop_;
}

status_t CodecTest::AllocateBuffer(OMX_U32 index)
{
  QMMF_INFO("%s:%s Enter ", TAG, __func__);
  status_t ret = 0;

  assert(ion_device_ > 0);
  int32_t ionType =  ION_HEAP(ION_IOMMU_HEAP_ID);

  uint32_t count, size;
  ret = avcodec_->GetBufferRequirements(index,  &count, &size);
  if(ret != OK) {
    QMMF_INFO("%s:%s Failed to get Buffer Requirements on %s", TAG, __func__,
        OMX_PORT_NAME(index));
    return ret;
  }

  struct ion_allocation_data alloc;
  struct ion_fd_data ionFdData;

  if(index == kPortIndexInput) {
    OMX_S32 nFds = 1;
    OMX_S32 nInts = 3;
    count = INPUT_MAX_COUNT;

    for(uint32_t i = 0; i < count; i++) {
      StreamBuffer buffer;
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
        QMMF_ERROR("%s:%s ION allocation failed", TAG, __func__);
        goto ION_ALLOC_FAILED;
      }

      ionFdData.handle = alloc.handle;
      ret = ioctl(ion_device_, ION_IOC_SHARE, &ionFdData);
      if (ret < 0) {
        QMMF_ERROR("%s:%s ION map failed %s", TAG, __func__, strerror(errno));
        goto ION_MAP_FAILED;
      }

      ion_handle_data.push_back(alloc);

      //Allocate buffer for MetaData Handle
      native_handle_t* meta_handle = (native_handle_create(1, 16));
      if(meta_handle == nullptr) {
        QMMF_ERROR("%s:%s failed to allocated metabuffer handle", TAG, __func__);
        return NO_MEMORY;
      }

      meta_handle->version = sizeof(native_handle_t);
      meta_handle->numFds  = nFds;
      meta_handle->numInts = nInts;
      meta_handle->data[0] = ionFdData.fd;
      meta_handle->data[1] = 0; //offset
      meta_handle->data[4] = alloc.len;
      buffer.handle = meta_handle;

      QMMF_INFO("%s:%s  buffer native handle = %p", TAG, __func__, meta_handle);
      QMMF_INFO("%s:%s  buffer ionFd = %d", TAG, __func__, meta_handle->data[0]);
      QMMF_INFO("%s:%s  buffer frameLen = %d",TAG,__func__,meta_handle->data[4]);
      input_buffer_list_.push_back(buffer);
    }
  } else {
    count = OUTPUT_MAX_COUNT;
    void *vaddr = nullptr;

    for(uint32_t i = 0; i < count; i++) {
      CodecBuffer buffer;
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
        QMMF_ERROR("%s:%s ION allocation failed", TAG, __func__);
        goto ION_ALLOC_FAILED;
      }

      ionFdData.handle = alloc.handle;
      ret = ioctl(ion_device_, ION_IOC_SHARE, &ionFdData);
      if (ret < 0) {
        QMMF_ERROR("%s:%s ION map failed %s", TAG, __func__, strerror(errno));
        goto ION_MAP_FAILED;
      }

      vaddr = mmap(nullptr, alloc.len, PROT_READ  | PROT_WRITE, MAP_SHARED,
                  ionFdData.fd, 0);
      if(vaddr == MAP_FAILED) {
        QMMF_ERROR("%s:%s  ION mmap failed: %s (%d)", TAG, __func__,
            strerror(errno), errno);
        goto ION_MAP_FAILED;
      }

      buffer.handle_data.handle = ionFdData.handle;
      buffer.fd                 = ionFdData.fd;
      buffer.frame_length       = alloc.len;
      buffer.filled_length      = alloc.len;
      buffer.pointer            = vaddr;

      QMMF_INFO("%s:%s buffer.Fd = %d", TAG, __func__, buffer.fd );
      QMMF_INFO("%s:%s buffer.frameLen = %d", TAG,__func__, buffer.frame_length);
      QMMF_INFO("%s:%s buffer.vaddr = %p", TAG, __func__, buffer.pointer);
      output_buffer_list_.push_back(buffer);
    }
  }

  QMMF_INFO("%s:%s Exit", TAG, __func__);
  return ret;

ION_MAP_FAILED:
  struct ion_handle_data ionHandleData;
  memset(&ionHandleData, 0x0, sizeof(ionHandleData));
  ionHandleData.handle = ionFdData.handle;
  ioctl(ion_device_, ION_IOC_FREE, &ionHandleData);
ION_ALLOC_FAILED:
  close(ion_device_);
  ion_device_ = -1;
  QMMF_ERROR("%s:%s ION Buffer allocation failed!", TAG, __func__);
  QMMF_INFO("%s:%s Exit", TAG, __func__);
  return -1;
}

status_t CodecTest::ReleaseBuffer()
{
  QMMF_INFO("%s:%s Enter ", TAG, __func__);

  for(auto& iter : ion_handle_data) {
      ioctl(ion_device_, ION_IOC_FREE, &iter);
  }

  for(auto& iter: input_buffer_list_) {
    native_handle_t * meta_handle = const_cast<native_handle_t *>((iter).handle);
    if(meta_handle->data[0]) {
      close(meta_handle->data[0]);
      meta_handle->data[0] = -1;
      native_handle_delete(meta_handle);
      meta_handle = nullptr;
    }
  }

  for(auto& iter : output_buffer_list_) {
      if((iter).pointer) {
          munmap((iter).pointer, (iter).frame_length);
          (iter).pointer = nullptr;
      }
      if((iter).fd) {
          ioctl(ion_device_, ION_IOC_FREE, &((iter).handle_data));
          close((iter).fd);
          (iter).fd = -1;
      }
  }

  input_buffer_list_.clear();
  output_buffer_list_.clear();

  QMMF_INFO("%s:%s Exit", TAG, __func__);
  return 0;
}

status_t CodecTest::ParseConfig(char *fileName, TestInitParams* params)
{
  FILE *fp;
  bool isStreamReadCompleted = false;
  const int MAX_LINE = 128;
  char line[MAX_LINE];
  char value[50];
  char key[25];
  uint32_t id = 0;
  bool avc = false;

  if(!(fp = fopen(fileName,"r"))) {
    QMMF_ERROR("%s:%s failed to open config file: %s", TAG, __func__,fileName);
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

    if(!strncmp("RecordingFrame", key, strlen("RecordingFrame"))) {
      params->record_frame = atoi(value);
    } else if(!strncmp("InputFile", key, strlen("InputFile"))) {
      strncpy(params->input_file, value, strlen(value));
      params->input_file[strlen(value)] = '\0';
    } else if(!strncmp("OutputFile", key, strlen("OutputFile"))) {
      strncpy(params->output_file, value, strlen(value));
      params->output_file[strlen(value)] = '\0';
    } else if(!strncmp("CodecType", key, strlen("CodecType"))) {
      if(!strncmp("VideoEncode", value, strlen("VideoEncode"))) {
        params->codec_type = CodecType::kVideoEncoder;
      } else {
        QMMF_ERROR("%s:%s Unknown CodecType(%s)", TAG, __func__, value);
        goto READ_FAILED;
      }
    } else if(!strncmp("Width", key, strlen("Width"))) {
      params->create_param.video_param.width = atoi(value);
    } else if(!strncmp("Height", key, strlen("Height"))) {
      params->create_param.video_param.height = atoi(value);
    } else if(!strncmp("FPS", key, strlen("FPS"))) {
      params->create_param.video_param.frame_rate = atoi(value);
    } else if(!strncmp("Codec", key, strlen("Codec"))) {
      if(!strncmp("AVC", value, strlen("AVC"))) {
        avc = true;
        params->create_param.video_param.format_type = VideoFormat::kAVC;
      } else if(!strncmp("HEVC", value, strlen("HEVC"))) {
        params->create_param.video_param.format_type = VideoFormat::kHEVC;
      } else {
        QMMF_ERROR("%s:%s Unknown Video CodecType(%s)", TAG, __func__, value);
        goto READ_FAILED;
      }
    } else if(!strncmp("IFR", key, strlen("IFR"))) {
      if(avc)
        params->create_param.video_param.codec_param.avc.idr_interval =
            atoi(value);
      else
        params->create_param.video_param.codec_param.hevc.idr_interval =
            atoi(value);
    } else if(!strncmp("Bitrate", key, strlen("Bitrate"))) {
      if(avc)
        params->create_param.video_param.codec_param.avc.bitrate = atoi(value);
      else
       params->create_param.video_param.codec_param.hevc.bitrate = atoi(value);
    } else if(!strncmp("Profile", key, strlen("Profile"))) {
      params->create_param.video_param.codec_param.avc.profile =
        AVCProfileType::kBaseline;
    } else if(!strncmp("Level", key, strlen("Level"))) {
      params->create_param.video_param.codec_param.avc.level =
        AVCLevelType::kLevel3;
    } else if(!strncmp("RateControl", key, strlen("RateControl"))) {
      if(avc)
        params->create_param.video_param.codec_param.avc.ratecontrol_type =
            VideoRateControlType::kVariable;
      else
        params->create_param.video_param.codec_param.hevc.ratecontrol_type =
            VideoRateControlType::kVariable;
    } else if(!strncmp("InitQpI", key, strlen("InitQpI"))) {
      if(avc)
        params->create_param.video_param.codec_param.avc.qp_params.init_qp.init_IQP =
            atoi(value);
      else
        params->create_param.video_param.codec_param.hevc.qp_params.init_qp.init_IQP =
            atoi(value);
    } else if(!strncmp("InitQpP", key, strlen("InitQpP"))) {
      if(avc)
        params->create_param.video_param.codec_param.avc.qp_params.init_qp.init_PQP =
            atoi(value);
      else
        params->create_param.video_param.codec_param.hevc.qp_params.init_qp.init_PQP =
            atoi(value);
    } else if(!strncmp("InitQpB", key, strlen("InitQpB"))) {
      if(avc)
          params->create_param.video_param.codec_param.avc.qp_params.init_qp.init_BQP =
              atoi(value);
      else
          params->create_param.video_param.codec_param.hevc.qp_params.init_qp.init_BQP =
              atoi(value);
    } else {
        QMMF_ERROR("%s:%s Unknown Key %s found", TAG, __func__, key);
        goto READ_FAILED;
    }
  }

  fclose(fp);
  return 0;
READ_FAILED:
  fclose(fp);
  return -1;
}

InputCodecSourceImpl::InputCodecSourceImpl(char* file_name, uint32_t num_frame)
{
  QMMF_INFO("%s:%s  Enter",TAG, __func__);

  input_file_ = fopen(file_name, "r");
  if(input_file_ == nullptr) {
    QMMF_ERROR("%s:%s failed to open input file(%s)", TAG, __func__, file_name);
  }

  num_frame_read = num_frame;

  QMMF_INFO("%s:%s Exit", TAG, __func__);
}

InputCodecSourceImpl::~InputCodecSourceImpl()
{
  QMMF_INFO("%s:%s  Enter", TAG, __func__);
  QMMF_INFO("%s:%s  Exit", TAG, __func__);
}

void InputCodecSourceImpl::AddBufferList(Vector<StreamBuffer>& list)
{
  QMMF_INFO("%s:%s Enter ", TAG, __func__);

  input_list_ = list;
  input_free_buffer_queue_.Clear();
  input_occupy_buffer_queue_.Clear();
  for(auto& iter : input_list_) {
    input_free_buffer_queue_.PushBack(iter);
  }

  QMMF_INFO("%s:%s Exit", TAG, __func__);
}

status_t InputCodecSourceImpl::Stop()
{
  QMMF_INFO("%s:%s Enter", TAG, __func__);
  QMMF_INFO("%s:%s Exit", TAG, __func__);
  return 0;
}

status_t InputCodecSourceImpl::Read(StreamBuffer& stream_buffer)
{
  status_t ret = 0;

  static OMX_TICKS time_stamp = 0;
  static int32_t frame_count = 1;

  if(input_free_buffer_queue_.Size() <= 0) {
    QMMF_WARN("%s:%s No buffer available. Wait for new buffer", TAG, __func__);
    Mutex::Autolock autoLock(wait_for_frame_lock_);
    wait_for_frame_.wait(wait_for_frame_lock_);
  }

  StreamBuffer buffer = *input_free_buffer_queue_.Begin();
  assert(buffer.handle != nullptr);

  int32_t byte_read = 0;
  if(input_file_) {
    ret = ReadFile(buffer.handle->data[0], buffer.handle->data[4], &byte_read);
  } else {
    QMMF_ERROR("%s:%s input file is not opened", TAG, __func__);
    return -1;
  }

  if(ret != OK) {
    QMMF_INFO("%s:%s Read completed. Read from start..", TAG, __func__);
    ret = fseek(input_file_, 0, SEEK_SET);
    if(ret != OK) {
      QMMF_ERROR("%s:%s Failed to seek file", TAG, __func__);
    } else {
      ret = ReadFile(buffer.handle->data[0],buffer.handle->data[4], &byte_read);
    }
  }

  if((num_frame_read != -1) && (frame_count > num_frame_read)) {
    QMMF_INFO("%s:%s Number of Frame read completed %d. Send EOS", TAG, __func__,
        frame_count);
    ret = -1;
  }

  stream_buffer.handle = buffer.handle;

  input_occupy_buffer_queue_.PushBack(buffer);
  input_free_buffer_queue_.Erase(input_free_buffer_queue_.Begin());

  time_stamp = time_stamp + (OMX_TICKS)(1000000 / 30);
  stream_buffer.timestamp = time_stamp;

  QMMF_INFO("%s:%s ETB buffer handle(%p), fd(%d)", TAG, __func__,
      stream_buffer.handle, stream_buffer.handle->data[0]);

  return ret;
}

status_t InputCodecSourceImpl::ReadFile(int32_t fd, uint32_t frame_length,
                                        int32_t *read)
{
  //TODO: map only first time buffer comes.
  void *buffer = mmap(nullptr, frame_length, PROT_READ  | PROT_WRITE,
                     MAP_SHARED, fd, 0);
  assert(buffer != nullptr);

  char *yuv = (char *)(buffer);
  int32_t width = 1280;
  int32_t height = 720;
  int32_t i, lscanl, lstride, cscanl, cstride;
  int32_t should = 0;
  int32_t actual = 0;

  lstride = VENUS_Y_STRIDE(COLOR_FMT_NV12, width);
  lscanl = VENUS_Y_SCANLINES(COLOR_FMT_NV12, height);
  cstride = VENUS_UV_STRIDE(COLOR_FMT_NV12, width);
  cscanl = VENUS_UV_SCANLINES(COLOR_FMT_NV12, height);
  for (i = 0; i < height; i++) {
    actual += (int)fread(yuv, 1, width, input_file_);
    should += lstride;
    yuv += lstride;
  }
  yuv = (char *)(buffer) + (lscanl * lstride);
  for (i = 0; i < ((height + 1) >> 1); i++) {
    actual += (int)fread(yuv, 1, width, input_file_);
    yuv += cstride;
    should += cstride;
  }

  munmap(buffer, frame_length);

  if(should == actual) {
    *read = actual;
    return 0;
  } else {
    *read = -1;
    return -1;
  }
}

status_t InputCodecSourceImpl::SignalBufferReturned(StreamBuffer& buffer)
{
  status_t ret = 0;
  QMMF_INFO("%s:%s ETB buffer handle(%p), fd(%d)", TAG, __func__, buffer.handle,
      buffer.handle->data[0]);

  bool found = false;
  List<StreamBuffer>::iterator it = input_occupy_buffer_queue_.Begin();
  for (; it != input_occupy_buffer_queue_.End(); ++it) {
    if ((*it).handle ==  buffer.handle) {
      input_free_buffer_queue_.PushBack(*it);
      wait_for_frame_.signal();
      found = true;
      break;
    }
  }
  assert(found == true);
  input_occupy_buffer_queue_.Erase(it);

  return ret;
}

void InputCodecSourceImpl::BufferStatus()
{
  QMMF_INFO("%s:%s Total Buffer(%d), free(%d), occupy(%d)", TAG, __func__,
      input_list_.size(), input_free_buffer_queue_.Size(),
      input_occupy_buffer_queue_.Size());
  assert(input_occupy_buffer_queue_.Size() == 0);
}

OutputCodecSourceImpl::OutputCodecSourceImpl(char* file_name)
{
  QMMF_INFO("%s:%s Enter ", TAG, __func__);

  file_fd_ = open(file_name, O_CREAT | O_WRONLY | O_TRUNC, 0655);
  if(file_fd_ < 0) {
    QMMF_ERROR("%s:%s Failed to open o/p file(%s)", TAG, __func__, file_name);
  }

  QMMF_INFO("%s:%s Exit", TAG, __func__);
}

OutputCodecSourceImpl::~OutputCodecSourceImpl()
{
  QMMF_INFO("%s:%s Enter", TAG, __func__);
  QMMF_INFO("%s:%s Exit", TAG, __func__);
}

void OutputCodecSourceImpl::AddBufferList(Vector<CodecBuffer>& list)
{
  QMMF_INFO("%s:%s Enter ", TAG, __func__);

  output_list_ = list;
  output_free_buffer_queue_.Clear();
  output_occupy_buffer_queue_.Clear();

  for(auto& iter : output_list_) {
    output_free_buffer_queue_.PushBack(iter);
  }

  QMMF_INFO("%s:%s Exit", TAG, __func__);
}

status_t OutputCodecSourceImpl::GetBuffer(CodecBuffer& codec_buffer)
{
  status_t ret = 0;

  if(output_free_buffer_queue_.Size() <= 0) {
    QMMF_WARN("%s:%s No buffer available to notify. Wait for new buffer", TAG,
        __func__);
    Mutex::Autolock autoLock(wait_for_frame_lock_);
    wait_for_frame_.wait(wait_for_frame_lock_);
  }

  CodecBuffer iter = *output_free_buffer_queue_.Begin();
  codec_buffer.fd = (iter).fd;
  codec_buffer.pointer = (iter).pointer;
  output_occupy_buffer_queue_.PushBack(iter);
  output_free_buffer_queue_.Erase(output_free_buffer_queue_.Begin());

  QMMF_INFO("%s:%s FTB buffer(%p), fd(%d)", TAG, __func__, codec_buffer.pointer,
      codec_buffer.fd);
  return ret;
}

status_t OutputCodecSourceImpl::ReturnBuffer(CodecBuffer& codec_buffer)
{
  status_t ret = 0;

  assert(codec_buffer.pointer != nullptr);
  QMMF_INFO("%s:%s FBD buffer(%p), filled length(%d)", TAG, __func__,
      codec_buffer.pointer, codec_buffer.filled_length);

  if(file_fd_ > 0) {
    ssize_t expSize = (ssize_t) codec_buffer.filled_length;
    if (expSize != write(file_fd_, codec_buffer.pointer,
        codec_buffer.filled_length)) {
        QMMF_ERROR("%s:%s Bad Write error (%d) %s", TAG, __func__,
            errno, strerror(errno));
        close(file_fd_);
        file_fd_ = -1;
    }
  } else {
    QMMF_ERROR("%s:%s File is not open to write", TAG, __func__);
  }

  if(codec_buffer.flag & OMX_BUFFERFLAG_EOS) {
    QMMF_INFO("%s:%s This is last buffer from encoder.Close file", TAG,__func__);
    close(file_fd_);
    file_fd_ = -1;
  }

  List<CodecBuffer>::iterator it = output_occupy_buffer_queue_.Begin();
  bool found = false;
  for (; it != output_occupy_buffer_queue_.End(); ++it) {
    if (((*it).pointer) ==  (codec_buffer.pointer)) {
      output_free_buffer_queue_.PushBack(*it);
      output_occupy_buffer_queue_.Erase(it);
      wait_for_frame_.signal();
      found = true;
      break;
    }
  }

  assert(found == true);
  return ret;
}

void OutputCodecSourceImpl::BufferStatus()
{
  QMMF_INFO("%s:%s Total Buffer(%d), free(%d), occupy(%d)", TAG, __func__,
      output_list_.size(), output_free_buffer_queue_.Size(),
      output_occupy_buffer_queue_.Size());
  assert(output_occupy_buffer_queue_.Size() == 0);
}

void CmdMenu::PrintMenu()
{
  printf("\n\n=========== QIPCAM TEST MENU ===================\n\n");

  printf(" \n\nCodec Test Application commands \n");
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

CmdMenu::Command CmdMenu::GetCommand()
{
  PrintMenu();
  return CmdMenu::Command(static_cast<CmdMenu::CommandType>(getchar()));
}

int main(int argc,char *argv[])
{
  QMMF_INFO("%s:%s Enter", TAG, __func__);

  CodecTest test_context;

  CmdMenu cmd_menu(test_context);

  int32_t testRunning = true;

  while (testRunning) {
    CmdMenu::Command command = cmd_menu.GetCommand();

    switch (command.cmd) {
      case CmdMenu::CREATE_CODEC_CMD:
      {
        test_context.CreateCodec(argc, argv);
      }
      break;
      case CmdMenu::DELETE_CODEC_CMD:
      {
        test_context.DeleteCodec();
      }
      break;
      case CmdMenu::START_CODEC_CMD:
      {
        test_context.StartCodec();
      }
      break;
      case CmdMenu::STOP_CODEC_CMD:
      {
        test_context.StopCodec();
      }
      break;
      case CmdMenu::PAUSE_CODEC_CMD:
      {
        test_context.PauseCodec();
      }
      break;
      case CmdMenu::RESUME_CODEC_CMD:
      {
        test_context.ResumeCodec();
      }
      break;
       case CmdMenu::EXIT_CMD:
      {
        QMMF_INFO("%s:%s exit from test", TAG, __func__);
        testRunning = false;
      }
      break;
      default:
      break;
    }
  }
  QMMF_INFO("%s:%s Exit", TAG, __func__);
  return 0;
}
